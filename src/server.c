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

static sem_t conn_sem;   /* counts free connection slots */

/* Sends the whole string. MSG_NOSIGNAL: no SIGPIPE if the client has gone. */
static int send_line(int fd, const char* msg) {
    size_t len = strlen(msg);
    size_t sent = 0;
    while (sent < len) {
        ssize_t n = send(fd, msg + sent, len - sent, MSG_NOSIGNAL);
        if (n < 0) {
            if (errno == EINTR) {
                continue;
            }
            return -1;
        }
        sent += (size_t)n;
    }
    return 0;
}

/* Reads one line. Returns 1 = got a line, 0 = client closed, -1 = error.
   Extra characters beyond `size` are dropped, so a huge line cannot overflow buf. */
static int read_line(int fd, char* buf, size_t size) {
    size_t i = 0;
    while (1) {
        char c;
        ssize_t n = read(fd, &c, 1);
        if (n == 0) {
            return 0;
        }
        if (n < 0) {
            if (errno == EINTR) {
                continue;
            }
            return -1;
        }
        if (c == '\n') {
            break;
        }
        if (c != '\r' && i < size - 1) {
            buf[i++] = c;
        }
    }
    buf[i] = '\0';
    return 1;
}

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

        int role, id;
        char pw[64];
        if (sscanf(line, "LOGIN %d %d %63s", &role, &id, pw) == 3) {
            if (logged_in) {
                send_line(fd, "ERR Already logged in\n");
                continue;
            }
            int rc = authenticate(role, id, pw);
            if (rc == AUTH_OK) {
                logged_in = 1;
            }
            send_line(fd, auth_message(rc));
        } else {
            send_line(fd, "ERR Unknown command\n");
        }
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
