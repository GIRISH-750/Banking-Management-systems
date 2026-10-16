#ifndef NET_UTIL_H
#define NET_UTIL_H

#include <stddef.h>

/* Sends the whole string. Returns 0 on success, -1 on error. */
int send_line(int fd, const char* msg);

/* Reads one line (without the newline). Returns 1 = got a line,
   0 = peer closed, -1 = error. Longer lines are cut to fit buf. */
int read_line(int fd, char* buf, size_t size);

#endif
