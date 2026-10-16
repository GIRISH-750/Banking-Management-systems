#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <ctype.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include "config.h"
#include "net_util.h"

#define DEFAULT_HOST "127.0.0.1"

static int connect_to_server(const char* host) {
    int fd = socket(AF_INET, SOCK_STREAM, 0);
    if (fd < 0) {
        perror("socket");
        return -1;
    }
    struct sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_port = htons(SERVER_PORT);
    if (inet_pton(AF_INET, host, &addr.sin_addr) != 1) {
        fprintf(stderr, "Invalid server address: %s\n", host);
        close(fd);
        return -1;
    }
    if (connect(fd, (struct sockaddr*)&addr, sizeof(addr)) < 0) {
        perror("connect");
        close(fd);
        return -1;
    }
    return fd;
}

/* Reads one line from the keyboard. Returns 0 = ok, -1 = input ended. */
static int ask(const char* prompt, char* buf, size_t size) {
    printf("%s", prompt);
    fflush(stdout);
    if (fgets(buf, (int)size, stdin) == NULL) {
        return -1;
    }
    char* nl = strchr(buf, '\n');
    if (nl != NULL) {
        *nl = '\0';
    } else {
        int c;   /* line was too long: throw away the rest of it */
        while ((c = getchar()) != '\n' && c != EOF) {
        }
    }
    return 0;
}

/* Sends one request line, reads one reply line. -1 if the connection broke. */
static int request(int fd, const char* req, char* reply, size_t size) {
    if (send_line(fd, req) < 0) {
        return -1;
    }
    if (read_line(fd, reply, size) <= 0) {
        return -1;
    }
    return 0;
}

static int has_space(const char* s) {
    for (; *s != '\0'; s++) {
        if (isspace((unsigned char)*s)) {
            return 1;
        }
    }
    return 0;
}

static const char* role_name(int role) {
    switch (role) {
        case 1:  return "Admin";
        case 2:  return "Manager";
        case 3:  return "Employee";
        case 4:  return "Customer";
        default: return "User";
    }
}

/* Returns 1 = logged in, 0 = login failed, -1 = stop (connection lost / input ended). */
static int do_login(int fd, int* role_out, int* id_out) {
    char role_s[32], id_s[32], pw[128], req[MAX_LINE_LEN], reply[MAX_LINE_LEN];
    int role, id;

    printf("Roles: 1 Admin, 2 Manager, 3 Employee, 4 Customer\n");
    if (ask("Role: ", role_s, sizeof(role_s)) < 0) {
        return -1;
    }
    if (ask("ID: ", id_s, sizeof(id_s)) < 0) {
        return -1;
    }
    if (ask("Password: ", pw, sizeof(pw)) < 0) {
        return -1;
    }

    if (sscanf(role_s, "%d", &role) != 1 || role < 1 || role > 4) {
        printf("Invalid role.\n");
        return 0;
    }
    if (sscanf(id_s, "%d", &id) != 1 || id < 1) {
        printf("Invalid ID.\n");
        return 0;
    }
    if (pw[0] == '\0' || strlen(pw) > 63 || has_space(pw)) {
        printf("Password must be 1-63 characters, no spaces.\n");
        return 0;
    }

    snprintf(req, sizeof(req), "LOGIN %d %d %s\n", role, id, pw);
    if (request(fd, req, reply, sizeof(reply)) < 0) {
        return -1;
    }
    printf("Server: %s\n", reply);

    if (strncmp(reply, "OK", 2) == 0) {
        *role_out = role;
        *id_out = id;
        return 1;
    }
    return 0;
}

/* Menu shown after login. Role-specific features will be added here later.
   Returns 0 = logged out (back to main menu), -1 = stop. */
static int session_menu(int fd, int role, int id) {
    char choice[16], reply[MAX_LINE_LEN];
    while (1) {
        printf("\n--- %s %d ---\n", role_name(role), id);
        printf("1. Logout\n");
        if (ask("Choice: ", choice, sizeof(choice)) < 0) {
            return -1;
        }
        if (strcmp(choice, "1") == 0) {
            if (request(fd, "LOGOUT\n", reply, sizeof(reply)) < 0) {
                return -1;
            }
            printf("Server: %s\n", reply);
            return 0;
        }
        printf("Invalid choice.\n");
    }
}

int main(int argc, char* argv[]) {
    const char* host = (argc > 1) ? argv[1] : DEFAULT_HOST;
    int fd = connect_to_server(host);
    if (fd < 0) {
        return 1;
    }

    char line[MAX_LINE_LEN], choice[16];
    if (read_line(fd, line, sizeof(line)) <= 0) {   /* the server's welcome line */
        fprintf(stderr, "Server closed the connection.\n");
        close(fd);
        return 1;
    }
    printf("Connected to server.\n");

    while (1) {
        printf("\n=== Banking System ===\n1. Login\n2. Exit\n");
        if (ask("Choice: ", choice, sizeof(choice)) < 0) {
            break;
        }
        if (strcmp(choice, "1") == 0) {
            int role = 0, id = 0;
            int r = do_login(fd, &role, &id);
            if (r < 0) {
                break;
            }
            if (r == 1 && session_menu(fd, role, id) < 0) {
                break;
            }
        } else if (strcmp(choice, "2") == 0) {
            break;
        } else {
            printf("Invalid choice.\n");
        }
    }

    send_line(fd, "QUIT\n");
    read_line(fd, line, sizeof(line));
    close(fd);
    printf("Goodbye.\n");
    return 0;
}
