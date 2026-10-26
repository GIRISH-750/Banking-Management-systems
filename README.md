# Banking Management System (CS-513 System Software)

A multi-user banking server and client written in **C**, using only **system calls**
for networking, threads, file I/O, file locking and semaphores.
Many clients can use the bank at the same time, and the data stays correct
even under concurrent use and even if the server crashes.

---

## Features

**4 roles, each with its own menu:**

| Role | What they can do |
|---|---|
| **Customer** | View balance, deposit, withdraw, transfer, transaction history, change password, apply for a loan, view my loans, give feedback |
| **Employee** | Add customer, modify customer, view a customer's passbook, view assigned loans, approve / reject a loan, change password |
| **Manager** | View pending loans, assign a loan to an employee, view all loans, activate / deactivate a customer, view feedback, mark feedback reviewed, change password |
| **Admin** | Add / modify / delete employee, promote employee to manager, demote manager to employee, view / modify / delete customer, list staff, change password |

**System features:**
- TCP socket server, **max 100 clients at once** (counting semaphore)
- **Thread pool** of 100 worker threads (bonus) instead of a new thread per client
- **One session per user** (the same user cannot log in twice)
- Passwords stored as **SHA-256 hashes**
- **Record locking** (OFD locks) so two users never change the same record at the same time
- **Crash safety**: an undo journal rolls back any half-done operation when the server restarts
- **ACID**: Atomicity (journal), Consistency (business rules), Isolation (locks), Durability (fsync)

---

## Requirements

- Linux (tested on Ubuntu under WSL)
- `gcc`, `make`
- OpenSSL dev library (for SHA-256):
  ```bash
  sudo apt install build-essential libssl-dev
  ```

---

## Build

```bash
make            # builds ./seed, ./server and ./client
```

## Create the demo data

```bash
./seed          # 50 customers, 3 employees, 2 managers, 1 admin
```
> Running `./seed` again **resets all data** (accounts, loans, feedback, transactions, journal).

## Run

Terminal 1 (server):
```bash
./server
```
Terminal 2, 3, ... (one per user):
```bash
./client                 # connects to 127.0.0.1:5000
./client <server-ip>     # connect to a server on another machine
```

---

## Demo logins (after `./seed`)

| Role (number) | ID | Password |
|---|---|---|
| Admin (1) | 1 | `admin123` |
| Manager (2) | 1 | `manager123` |
| Manager (2) | 2 | `manager223` |
| Employee (3) | 1 | `emp123` |
| Employee (3) | 2 | `emp223` |
| Employee (3) | 3 | `emp323` |
| Customer (4) | 1 | `alice123` |
| Customer (4) | 2 | `bob123` |
| Customer (4) | 3 - 50 | `cust<ID>`  (e.g. customer 7 = `cust7`) |

---

## Demo: full loan flow

1. **Customer 1** logs in -> 7 Apply for loan -> amount `5000`
2. **Manager 1** logs in -> 1 View pending loans -> 2 Assign loan to employee 1
3. **Employee 1** logs in -> 5 View assigned loans -> 6 Approve loan
4. **Customer 1** -> 1 View balance (increased by 5000) -> 8 My loans (APPROVED)

## Demo: crash recovery (Atomicity)

The server has a test switch that kills it in the **middle** of an operation.

```bash
BANK_CRASH=transfer ./server     # also: approve | promote | demote
```
1. Customer 1 logs in and transfers money to customer 2 -> **the server dies halfway**.
2. Restart normally:
   ```bash
   ./server
   ```
   It prints `Recovery: rolled back 1 unfinished operation(s)`.
3. Check both balances: **no money was lost or created**. The half-done transfer was undone.

## Demo: thread pool queue

```bash
BANK_POOL=2 ./server     # only 2 worker threads (for the demo)
```
Open 3 clients: the 3rd one prints `Server busy, please wait...`.
When one of the first two exits, the 3rd is served at once.
(Normally the pool has 100 workers, the same as the 100-client limit.)

---

## Tests

```bash
make test                        # unit + Day 4/5/6 suites (takes ~4 min: fsync is slow on WSL)
bash tests/day7_pool.sh          # thread pool tests
bash tests/day7_client_wait.sh   # "Server busy" message test
```

| Suite | What it checks | Result |
|---|---|---|
| Unit tests | locks, customer store, deposit/withdraw/transfer, history, passwords | 6/6 pass |
| Day 4 | employee + admin features, role checks, parallel adds/renames | 15 pass |
| Day 5 | full loan flow, feedback, activate/deactivate + 6 threaded race tests | 34 + 6 pass |
| Day 6 | crash in the middle of transfer / approve / promote / demote, then recovery | 30 pass |
| Day 7 | pool queueing, slot reuse, 100-client limit, client busy message | 12 + 4 pass |

---

## Project structure

```
include/          headers (one per module)
src/
  server.c            main(): recovery, socket, semaphore, accept loop
  thread_pool.c       worker threads + job queue (mutex + condition variables)
  client.c            menu-based client
  auth.c, hash.c      login + SHA-256
  session.c           one session per user
  *_handler.c         reads one command line per role and replies
  *_ops.c             business rules (customer, employee, manager, admin, loan)
  lock.c              OFD record / file locks
  journal.c           undo journal + crash recovery
  rec_store.c, customer_store.c, loan_store.c, txn_store.c, counter.c
                      fixed-size record files (pread / pwrite + fsync)
  seed.c              creates demo / test data
tests/            unit tests and test scripts
data/             binary data files (*.bin), created by ./seed
docs/
  design_decisions.md  why each design choice was made
  diagrams/            class diagram + architecture diagram
```

---

## Known limitations

- Records are fixed-size for fast direct access, so names are limited to 49 characters.
- Lists such as pending loans and transaction history scan the whole file; fast at this size, but large data would need an index.
- At most 100 users can be connected at once, and an idle client keeps its slot until it quits.
- Data is stored in local binary files, so the server runs on one machine only.

---

## Design

See [`docs/design_decisions.md`](docs/design_decisions.md) for every design choice and the reason behind it,
and `docs/diagrams/` for the class diagram and the architecture diagram.
