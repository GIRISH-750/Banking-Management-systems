#ifndef ADMIN_HANDLER_H
#define ADMIN_HANDLER_H
/* Returns 1 if it was an admin command (reply sent), else 0. */
int handle_admin(int fd, int id, const char* line);
#endif
