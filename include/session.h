#ifndef SESSION_H
#define SESSION_H

#define SESSION_OK             0
#define SESSION_ALREADY_ACTIVE 1
#define SESSION_FULL           2

/* Registers (role, id) as logged in.
   Returns SESSION_ALREADY_ACTIVE if that user already has a session. */
int session_start(int role, int id);

/* Removes (role, id) from the register. Safe to call if not present. */
void session_end(int role, int id);

#endif
