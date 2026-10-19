#ifndef EMPLOYEE_HANDLER_H
#define EMPLOYEE_HANDLER_H

/* Returns 1 if it was an employee command (reply sent), else 0. */
int handle_employee(int fd, int id, const char* line);

#endif
