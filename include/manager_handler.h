#ifndef MANAGER_HANDLER_H
#define MANAGER_HANDLER_H
/* Manager commands. Returns 1 if handled (reply sent), else 0. */
int handle_manager(int fd, int id, const char* line);
#endif
