#include <errno.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include "net_util.h"

/* MSG_NOSIGNAL: no SIGPIPE if the other side has gone. */
int send_line(int fd, const char* msg) {
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

int read_line(int fd, char* buf, size_t size) {
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
