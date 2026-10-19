#ifndef STAFF_HANDLER_H
#define STAFF_HANDLER_H

/* PASSWD <old> <new> for Admin, Manager, Employee.
   Returns 1 if handled (reply sent), else 0. */
int handle_staff(int fd, int role, int id, const char* line);
#endif
