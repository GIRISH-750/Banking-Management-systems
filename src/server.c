#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <pthread.h>
#include <semaphore.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include "config.h"
#include "auth.h"
#include "net_util.h"
#include "session.h"
#include "customer_handler.h"
#include "employee_handler.h"
#include "admin_handler.h"
#include "staff_handler.h"
#include "manager_handler.h"
#include "journal.h"
#include "thread_pool.h"

static sem_t conn_sem;   /* counts free connection slots */

static const char* auth_message(int rc) {
    switch (rc) {
        case AUTH_OK:             return "OK Login successful\n";
        case AUTH_NO_SUCH_USER:
        case AUTH_WRONG_PASSWORD: return "ERR Invalid ID or password\n";
        case AUTH_INACTIVE:       return "ERR Account is inactive\n";
        default:                  return "ERR Internal error\n";
    }
}

/* A pool worker runs this for one client session, then goes back to the queue. */
static void serve_client(int fd) {

    char line[MAX_LINE_LEN];
    int logged_in = 0;
    int cur_role = 0, cur_id = 0;   /* who this connection is logged in as */

    send_line(fd, "Welcome. Commands: LOGIN <role 1-4> <id> <password> | QUIT\n");

    while (1) {
        int r = read_line(fd, line, sizeof(line));
        if (r <= 0) {
            break;
        }
        if (strcmp(line, "QUIT") == 0) {
            send_line(fd, "Bye\n");
            break;
        }

        if (strcmp(line, "LOGOUT") == 0) {
            if (!logged_in) {
                send_line(fd, "ERR Not logged in\n");
            } else {
                session_end(cur_role, cur_id);
                logged_in = 0;
                send_line(fd, "OK Logged out\n");
            }
            continue;
        }

        /* customer commands: id is taken from the session, not from the client */
        if (logged_in && cur_role == ROLE_CUSTOMER && handle_customer(fd, cur_id, line)) {
            continue;
        }
        /* employee commands: id is taken from the session, not from the client */
        if (logged_in && cur_role == ROLE_EMPLOYEE && handle_employee(fd, cur_id, line)) {
            continue;
        }
        /* admin commands */
        if (logged_in && cur_role == ROLE_ADMIN && handle_admin(fd, cur_id, line)) {
            continue;
        }
        /* manager commands */
        if (logged_in && cur_role == ROLE_MANAGER && handle_manager(fd, cur_id, line)) {
            continue;
        }
        /* PASSWD for admin, manager, employee (customers have their own) */
        if (logged_in && cur_role != ROLE_CUSTOMER && handle_staff(fd, cur_role, cur_id, line)) {
            continue;
        }

        int role, id;
        char pw[64];
        if (sscanf(line, "LOGIN %d %d %63s", &role, &id, pw) == 3) {
            if (logged_in) {
                send_line(fd, "ERR Already logged in\n");
                continue;
            }
            int rc = authenticate(role, id, pw);
            if (rc == AUTH_OK) {
                /* password is correct, only now check for an existing session */
                int sr = session_start(role, id);
                if (sr == SESSION_OK) {
                    logged_in = 1;
                    cur_role = role;
                    cur_id = id;
                    send_line(fd, auth_message(rc));
                } else if (sr == SESSION_ALREADY_ACTIVE) {
                    send_line(fd, "ERR User already logged in\n");
                } else {
                    send_line(fd, "ERR Server busy\n");
                }
            } else {
                send_line(fd, auth_message(rc));
            }
        } else {
            send_line(fd, "ERR Unknown command\n");
        }
    }

    if (logged_in) {
        session_end(cur_role, cur_id);   /* client left without LOGOUT */
    }
    close(fd);
    sem_post(&conn_sem);   /* free the slot for the next client */
}

/* Pool size: BANK_POOL env (1..MAX_CLIENTS) for demos, else POOL_SIZE. */
static int pool_size_from_env(void) {
    const char* s = getenv("BANK_POOL");
    if (s == NULL || *s == '\0') {
        return POOL_SIZE;
    }
    char* end;
    long n = strtol(s, &end, 10);
    if (*end != '\0' || n < 1 || n > MAX_CLIENTS) {
        fprintf(stderr, "BANK_POOL must be 1-%d, using %d\n", MAX_CLIENTS, POOL_SIZE);
        return POOL_SIZE;
    }
    return (int)n;
}

int main(void) {
    /* Crash recovery: undo operations that were cut off by a crash.
       Runs before bind/listen, so no client can see half-done data. */
    int fixed = jr_recover();
    if (fixed < 0) {
        fprintf(stderr, "Journal recovery failed\n");
        return 1;
    }
    fprintf(stderr, "Recovery: rolled back %d unfinished operation(s)\n", fixed);
    int server_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (server_fd < 0) {
        perror("socket");
        return 1;
    }

    int yes = 1;   /* lets us restart the server without "Address already in use" */
    setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(yes));

    struct sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = htonl(INADDR_ANY);
    addr.sin_port = htons(SERVER_PORT);

    if (bind(server_fd, (struct sockaddr*)&addr, sizeof(addr)) < 0) {
        perror("bind");
        return 1;
    }
    if (listen(server_fd, 128) < 0) {
        perror("listen");
        return 1;
    }
    if (sem_init(&conn_sem, 0, MAX_CLIENTS) < 0) {
        perror("sem_init");
        return 1;
    }

    int nworkers = pool_init(pool_size_from_env(), serve_client);
    if (nworkers < 0) {
        fprintf(stderr, "Thread pool start failed\n");
        return 1;
    }

    printf("Server listening on port %d (max %d clients, pool of %d workers)\n",
           SERVER_PORT, MAX_CLIENTS, nworkers);
    fflush(stdout);

    while (1) {
        if (sem_wait(&conn_sem) < 0) {   /* blocks when all slots are used */
            if (errno == EINTR) {
                continue;
            }
            perror("sem_wait");
            break;
        }

        int client_fd = accept(server_fd, NULL, NULL);
        if (client_fd < 0) {
            sem_post(&conn_sem);
            if (errno != EINTR) {
                perror("accept");
            }
            continue;
        }

        /* Hand the client to the pool: an idle worker takes it,
           or it waits in the queue until one is free. */
        if (pool_submit(client_fd) == POOL_QUEUED) {
            fprintf(stderr, "All workers busy: client queued (waiting: %d)\n", pool_waiting());
        }
    }

    close(server_fd);
    return 1;
}
