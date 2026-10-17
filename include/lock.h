#ifndef LOCK_H
#define LOCK_H

#include <stddef.h>

/* Record locking on fixed-size records (record for id sits at (id-1)*size).
   Uses OFD locks: owned by the open file description, so two threads that
   each open() the file get separate locks and block each other. */

/* Blocks until the lock is granted. type = F_RDLCK or F_WRLCK.
   Returns 0 on success, -1 on error. */
int lock_record(int fd, int id, size_t size, short type);

/* Releases the lock on that record. Returns 0 on success, -1 on error. */
int unlock_record(int fd, int id, size_t size);

/* Locks the WHOLE file (offset 0, length 0 = "to end of file, including
   records added later"). Used when scanning every record. */
int lock_file(int fd, short type);
int unlock_file(int fd);

#endif
