# Design Decisions (Day 2)

Format: Decision / Why / Trade-off.

## 1. Binary files with fixed-size records
- Why: a struct is written and read with one write()/pread(); fixed size lets us jump to
  record n at offset (n - 1) * sizeof(struct). TA requires binary (.bin) files.
- Trade-off: changing a struct changes the file format; deleting a record would shift others,
  so records are marked inactive instead of removed.

## 2. pread() instead of lseek() + read()
- Why: pread takes the offset as an argument and does not touch the shared file offset, so
  threads cannot disturb each other.
- Trade-off: none that matters here.

## 3. Passwords stored as SHA-256 hashes
- Why: a leaked file does not reveal passwords; the TA prefers hashing.
- Trade-off: unsalted, so equal passwords give equal hashes. Better: per-user salt plus bcrypt
  or Argon2.

## 4. Separate auth module, no socket code in it
- Why: modular design; testable without a network (tests/test_auth.c); one place for login
  rules used by all four roles.

## 5. Same error for unknown ID and wrong password
- Why: stops user enumeration (an attacker cannot learn which IDs exist).
- Trade-off: slightly less helpful message for honest users.

## 6. Thread per client (not fork)
- Why: clients must share state (connection semaphore, session register); threads share
  memory, and creating a thread is cheaper than creating a process.
- Trade-off: no isolation (one bug can crash the server); shared data needs locks. Does not
  scale to ~10,000 clients; a thread pool or epoll would be the next step.

## 7. Counting semaphore, initial value 100
- Why: TA requirement; limits concurrent clients. sem_wait before accept(), sem_post when the
  client's thread ends (also on every failure path so no slot is lost).
- Trade-off: an idle client still holds a slot; a receive timeout would fix that.

## 8. Client fd passed to the thread through malloc
- Why: passing the address of a loop variable is a race, the next accept() could overwrite it.
  Each thread gets its own copy and frees it.

## 9. Shared net_util module (send_line / read_line)
- Why: client and server need the same code; one place to fix bugs; modular.

## 10. Line-based text protocol (LOGIN / LOGOUT / QUIT)
- Why: TCP is a byte stream with no message boundaries; one command per line is simple to
  parse and easy to test with nc.
- Trade-off: text is not compact; passwords travel in plain text (TLS would fix it).

## 11. LOGOUT keeps the connection, QUIT closes it
- Why: after logout the client returns to the main menu on the same connection, and the
  server clears that connection's session state without reconnecting.

## 12. Session register with a mutex (one session per user)
- Why: shared by all threads, so check-and-insert is done under one lock to avoid a race.
  Password is verified before the session check so "already logged in" is not shown to
  strangers. Entries are removed on LOGOUT, QUIT and disconnect.
- Trade-off: in-memory only (empty after a server restart); works for one server process.

## Known limitations
- No limit on login attempts (brute force possible).
- Password is visible while typing (no termios echo off).
- Idle clients can hold a connection slot.
- float is used for money because the TA diagram uses float (real systems use integer paise).
