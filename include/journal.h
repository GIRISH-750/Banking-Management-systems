#ifndef JOURNAL_H
#define JOURNAL_H
/* Undo journal for operations that change 2 records (transfer, loan approval,
   promote, demote). Before changing anything we save the OLD bytes of both
   records here and fsync; after both changes are on disk we mark the entry
   DONE. On startup, any entry still ACTIVE is undone (old bytes written back). */

#define JOURNAL_FILE   "data/journal.bin"
#define JR_MAX         256   /* biggest record we journal (bytes) */
#define JR_MAX_TARGETS 2

enum { JF_CUSTOMERS = 1, JF_LOANS, JF_EMPLOYEES, JF_MANAGERS };

struct jr_target { int file; int id; };

/* Saves old values of the targets (caller already holds their locks).
   Returns a journal slot (>= 0) or -1 on error. */
int jr_begin(const struct jr_target* t, int n);
/* fsyncs the touched files, then marks the slot DONE. Call on success AND failure. */
void jr_end(int slot);
/* Run once at server start, before accepting clients.
   Returns how many unfinished operations were rolled back, or -1 on error. */
int jr_recover(void);
/* Test hook: if env BANK_CRASH == name, the process dies here (simulated crash). */
void crash_point(const char* name);
#endif
