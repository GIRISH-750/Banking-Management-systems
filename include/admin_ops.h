#ifndef ADMIN_OPS_H
#define ADMIN_OPS_H
#include "entities.h"

#define ADM_OK            0
#define ADM_NOT_FOUND     1
#define ADM_BAD_INPUT     2
#define ADM_NOT_ACTIVE    3
#define ADM_HAS_LOANS     4
#define ADM_HAS_BALANCE   5
#define ADM_LAST_MANAGER  6
#define ADM_ERROR        -1

int adm_add_employee(const char* first, const char* last, const char* pw, int* new_id);
int adm_modify_employee(int id, const char* first, const char* last);
int adm_delete_employee(int id);
int adm_delete_customer(int id);
int adm_view_customer(int id, struct Customer* out);
int adm_promote(int emp_id, int* new_mgr_id);
int adm_demote(int mgr_id, int* new_emp_id);
const char* adm_message(int rc);
#endif
