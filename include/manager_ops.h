#ifndef MANAGER_OPS_H
#define MANAGER_OPS_H
#include "entities.h"

#define FEEDBACK_FILE "data/feedback.bin"
#define FB_NEW      "NEW"
#define FB_REVIEWED "REVIEWED"

#define MG_OK         0
#define MG_NOT_FOUND  1
#define MG_DELETED    2   /* account deleted: cannot be activated/deactivated */
#define MG_ALREADY    3   /* already in that status */
#define MG_BAD_MSG    4
#define MG_BAD_STATE  5   /* feedback already reviewed */
#define MG_ERROR     -1

/* activate = 1 -> ACTIVE, 0 -> INACTIVE. Refuses DELETED accounts. */
int mgr_set_customer_status(int id, int activate);
int fb_submit(int cust_id, const char* msg, int* fb_id);
int fb_list(struct Feedback* out, int max);
int fb_review(int fb_id);
const char* mgr_message(int rc);
#endif
