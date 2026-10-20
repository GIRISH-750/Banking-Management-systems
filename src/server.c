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

/* One thread runs this per connected client. */
static void* client_thread(void* arg) {
    int fd = *(int*)arg;
    free(arg);

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
    return NULL;
}

int main(void) {
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

    printf("Server listening on port %d (max %d clients)\n", SERVER_PORT, MAX_CLIENTS);

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

        int* arg = malloc(sizeof(int));
        if (arg == NULL) {
            close(client_fd);
            sem_post(&conn_sem);
            continue;
        }
        *arg = client_fd;

        pthread_t tid;
        if (pthread_create(&tid, NULL, client_thread, arg) != 0) {
            perror("pthread_create");
            free(arg);
            close(client_fd);
            sem_post(&conn_sem);
            continue;
        }
        pthread_detach(tid);   /* thread cleans up itself, no join needed */
    }

    close(server_fd);
    return 1;
}
