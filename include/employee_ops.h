#ifndef EMPLOYEE_OPS_H
#define EMPLOYEE_OPS_H

#include "entities.h"

#define EMP_OK          0
#define EMP_NOT_FOUND   1
#define EMP_BAD_INPUT   2
#define EMP_ERROR      -1

int emp_add_customer(const char* first, const char* last,
                     const char* password, int* new_id);
int emp_modify_customer(int id, const char* first, const char* last);
int emp_passbook(int id, struct Transaction* out, int max, int* n);
const char* emp_message(int rc);

#endif
