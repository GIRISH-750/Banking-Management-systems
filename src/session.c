#include <pthread.h>
#include "config.h"
#include "session.h"

#define MAX_SESSIONS MAX_CLIENTS   /* at most one session per connection */

struct Slot {
    int in_use;
    int role;
    int id;
};

static struct Slot slots[MAX_SESSIONS];
static pthread_mutex_t session_lock = PTHREAD_MUTEX_INITIALIZER;

int session_start(int role, int id) {
    int result = SESSION_FULL;
    int free_index = -1;

    pthread_mutex_lock(&session_lock);

    /* Check and insert happen under ONE lock, so two threads cannot both pass. */
    for (int i = 0; i < MAX_SESSIONS; i++) {
        if (slots[i].in_use) {
            if (slots[i].role == role && slots[i].id == id) {
                result = SESSION_ALREADY_ACTIVE;
                break;
            }
        } else if (free_index < 0) {
            free_index = i;
        }
    }

    if (result != SESSION_ALREADY_ACTIVE && free_index >= 0) {
        slots[free_index].in_use = 1;
        slots[free_index].role = role;
        slots[free_index].id = id;
        result = SESSION_OK;
    }

    pthread_mutex_unlock(&session_lock);
    return result;
}

void session_end(int role, int id) {
    pthread_mutex_lock(&session_lock);
    for (int i = 0; i < MAX_SESSIONS; i++) {
        if (slots[i].in_use && slots[i].role == role && slots[i].id == id) {
            slots[i].in_use = 0;
            break;
        }
    }
    pthread_mutex_unlock(&session_lock);
}
