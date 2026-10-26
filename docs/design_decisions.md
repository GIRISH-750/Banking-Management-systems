# Design Decisions

Every important choice in the project, **what** we chose and **why**, in simple words.

---

## 1. Server and concurrency

### 1.1 Threads, not fork()
**Chosen:** one server process with many threads.
**Why:**
- Threads share memory, so the session list and the semaphore are simply shared.
  With fork(), every child is a separate process and we would need extra IPC
  (shared memory + process-shared locks) just to know who is logged in.
- A thread is much cheaper to create than a process.
- **Trade-off:** a crash in one thread stops the whole server. This is covered by
  the journal (section 4): on restart, nothing is left half-done.

### 1.2 Thread pool (bonus) instead of thread-per-client
**Chosen:** 100 worker threads created once at startup + a job queue of client sockets.
**How:** the accept loop puts the client's socket number in the queue; a free worker takes it
and serves that client until they quit, then goes back for the next one.
The queue is protected by **one mutex** and **two condition variables** (`not_empty`, `not_full`).
**Why:**
- No thread is created or destroyed per client (saves time).
- The number of threads is fixed and known (no thread explosion).
**Details:**
- A worker holds the queue lock only while taking a socket out, not while serving,
  otherwise only one client could be served at a time.
- Workers wait in `while (queue empty)`, not `if`, because a thread can wake up without reason
  (spurious wakeup).
- **Pool size = 100 = the client limit.** Our protocol is session-based (one worker stays with
  one client), so a smaller pool would let a few idle users block everyone else.
- `BANK_POOL=2 ./server` shrinks the pool for a live demo of the queue.

### 1.3 Counting semaphore: max 100 clients
**Chosen:** `sem_wait` before `accept`, `sem_post` when a session ends.
**Why:** a hard limit on connected clients (TA requirement). Client 101 is not accepted;
the kernel holds the connection until a slot is free.

### 1.4 Client shows "Server busy, please wait..."
**Chosen:** the **client** waits 2 seconds for the server's Welcome (`poll`); if it does not come,
it prints the message itself and keeps waiting.
**Why:** the server cannot send anything to a client it has not accepted, and the protocol is
"one request -> exactly one reply line". An extra line from the server would shift every
later reply by one.

### 1.5 One session per user
**Chosen:** a shared list of logged-in (role, id) pairs protected by a mutex.
The password is checked **first**, then the session.
**Why:** prevents two people using the same account at once. Checking the password first means
a stranger cannot find out whether someone is online.

---

## 2. Protocol

- **Text lines** over TCP (e.g. `LOGIN 4 1 alice123`, `DEPOSIT 500.00`, reply `OK ...` / `ERR ...`).
  Easy to test with scripts and easy to debug.
- **The user ID is taken from the session, never from the client's message.**
  A customer cannot act on someone else's account by changing an ID.
- Every command is checked against the role: a customer cannot run employee or admin commands.
- Same error for wrong ID and wrong password (`Invalid ID or password`), so IDs cannot be guessed.

---

## 3. Storage and locking

### 3.1 Binary files with fixed-size records
**Chosen:** one `.bin` file per entity; record of ID `n` is at offset `(n - 1) x sizeof(struct)`.
**Why:** reading or writing one record is a single `pread` / `pwrite` with no searching, and
each record has a fixed place, so it can be locked on its own.
`pread`/`pwrite` take the offset as an argument, so threads never share a file position.

### 3.2 OFD locks (not classic fcntl locks)
**Chosen:** `fcntl(F_OFD_SETLKW)` record locks.
**Why:** classic `fcntl` locks belong to the **process**, so two threads of the same server would
never block each other. OFD locks belong to the **open file**, so each operation opens its own
file descriptor and threads really exclude each other.

### 3.3 Read and write locks
- **Read lock** (shared) for viewing, e.g. balance: many readers at once.
- **Write lock** (exclusive) for the whole read-change-write, e.g. deposit: nobody can read a
  half-updated record or overwrite our change (no lost updates).
- Unlock happens on **every** path, including error paths.

### 3.4 Lock order (no deadlocks)
**Global order:** `employees -> loans -> customers`. `counter.bin` and `journal.bin` are
**leaf** locks (taken last, nothing taken after them).
**Transfer:** lock the **lower customer ID first**, unlock in reverse order.
**Why:** if every thread takes locks in the same order, two threads can never wait for each other
in a circle, so a deadlock is impossible.

### 3.5 ID counter
**Chosen:** `counter.bin` holds the next ID for every entity; read and increment under a
whole-file lock.
**Why:** two employees adding customers at the same time always get different IDs.

---

## 4. Crash safety (Atomicity + Durability)

### 4.1 Undo journal
**Chosen:** before changing records, save their **old values** (before-images) in `journal.bin`,
marked `ACTIVE`, with a checksum. After all writes are done, mark it `DONE`.
**Used for:** transfer, loan approval, promote, demote (operations that change more than one record).
**Recovery:** at startup, **before** `bind()`, `jr_recover()` writes back the old values for every
`ACTIVE` entry. No client can see data until recovery is finished.
**Why undo and not redo:** simpler. We only need the old record, and "undo" always brings the
data back to a state that really existed.

### 4.2 fsync after every write
**Why:** without it, data can sit in memory and be lost in a crash. With it, an operation is
really on disk before we reply `OK` (**Durability**).
**Trade-off:** slower (the full test suite takes about 4 minutes on WSL).

### 4.3 Crash test switch
`BANK_CRASH=transfer|approve|promote|demote` makes the server exit at a point **in the middle**
of that operation, to prove recovery works (tested in `tests/day6_crash.sh`).

---

## 5. Data model choices

| Choice | Why |
|---|---|
| **Soft delete** (status `DELETED`, record kept) | Keeps history and IDs; records keep their fixed offset; a deleted user simply cannot log in |
| Status **ACTIVE / INACTIVE / DELETED** | INACTIVE = temporarily blocked by a manager; DELETED = removed by admin and cannot be re-activated |
| A customer with money or an open loan **cannot be deleted** | No money or loan disappears |
| **Loan has `loanID` and `assignedEmpID`** | Each loan can be picked, assigned and approved individually |
| **No `assigned_loans[]` in Employee** | The employee's loans are found from `Loan.assignedEmpID`: one source of truth, no fixed-size array to overflow or keep in sync |
| Loan states **PENDING -> ASSIGNED -> APPROVED / REJECTED** | Clear flow; every action checks the current state under a lock (no double approval) |
| **Max 1 open loan** per customer (PENDING or ASSIGNED) | Prevents loan spam; check + insert happen under one lock, so parallel applies cannot both pass |
| On approval, money is **credited to the balance** and added to `Customer.loan` | Loan money is usable at once; `loan` shows the total borrowed |
| **Promote / demote** create a new record in the other file and set the old one INACTIVE | Managers and employees live in different files; done under the journal so it is never half-done |
| A manager **cannot be demoted if he is the last one** | Someone must always be able to assign loans |
| **Manager has a status** | So a manager can be deactivated / deleted like other staff |
| One `PASSWD` command for all staff | Same logic for admin, manager and employee; less duplicate code |
| Passwords stored as **SHA-256 hashes** | The data file never contains real passwords |

---

## 6. Who can see loans

| Role | Sees |
|---|---|
| Customer | only his own loans |
| Employee | only loans assigned to him |
| Manager | pending loans + all loans |
| Admin | none (he manages people, not loans) |

---

## 7. Known limitations (honest list)

- Password hashes are **not salted**; no limit on wrong login attempts.
- The password is visible while typing in the client.
- An idle logged-in client keeps its slot (and worker) until it quits.
- Money is stored as `float` (small rounding is possible with very large amounts).
- `fsync` after every write makes the system slower but safe.
- Hardening planned before submission: ignore `SIGPIPE` so a client that disconnects while the
  server is writing cannot stop the server.
