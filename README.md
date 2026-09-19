# Banking Management System

A multi-role banking backend built in C, simulating core banking operations with
socket-based client-server communication, system-call-level file management,
file locking, semaphores, and process/thread concurrency.

Course project — CS-513 System Software, IIIT Bangalore.

## Status: In Progress (Day 1 of 27)

## Roles
- **Customer** — balance, deposit, withdraw, transfer, loan application, feedback, transaction history
- **Bank Employee** — add/modify customers, process & approve/reject loans, view passbook
- **Manager** — activate/deactivate accounts, assign loans to employees, review feedback
- **Administrator** — manage employees, manage user roles

## Tech Stack
- C (POSIX system calls only, no high-level library abstractions where avoidable)
- Raw file I/O (`open`, `read`, `write`, `lseek`) for persistent storage
- Sockets (planned) for client-server communication
- `fork()` for per-client concurrency (planned)
- `fcntl()` file locking (planned)
- POSIX semaphores (planned)

## Project Structure
banking_system/
├── include/ # entity struct definitions (entities.h)
├── src/ # source files
├── data/ # binary data files (accounts, loans, etc.)
├── docs/ # class diagram, issue log
└── README.md


## Progress Log
- **Day 1:** Entity design finalized (aligned to TA-provided class diagram — Admin, Manager,
  Employee, Customer, Loan, Feedback, Transaction, Counter). Verified raw file I/O
  (open/write/lseek/read) on a fixed-size struct. Class diagram added to `docs/`.

## Build & Run (current state)
```bash
gcc -c include/entities.h -o /tmp/entities_check.o
gcc -o test_counter src/test_counter.c
./test_counter
```

## Known Issues
See [`docs/ISSUES.md`](docs/ISSUES.md) for tracked mistakes and fixes throughout development.

## Roadmap
- [ ] Customer file layer (create/find/update/list)
- [ ] Socket server-client communication
- [ ] Concurrent client handling via `fork()`
- [ ] File locking for account operations
- [ ] Fund transfer with atomic dual-account locking
- [ ] Semaphore-protected shared counter
- [ ] Loan lifecycle (apply → assign → approve/reject)
- [ ] Feedback & transaction history
- [ ] Role management & account activation/deactivation
- [ ] Integration testing
## Progress Log
- **Day 1:** Entity design finalized (aligned to TA-provided class diagram — Admin, Manager,
  Employee, Customer, Loan, Feedback, Transaction, Counter). Verified raw file I/O
  (open/write/lseek/read) on a fixed-size struct. Class diagram added to `docs/`.

## Build & Run (current state)
```bash
gcc -c include/entities.h -o /tmp/entities_check.o
gcc -o test_counter src/test_counter.c
./test_counter
```

## Known Issues
See [`docs/ISSUES.md`](docs/ISSUES.md) for tracked mistakes and fixes throughout development.

## Roadmap
- [ ] Customer file layer (create/find/update/list)
- [ ] Socket server-client communication
- [ ] Concurrent client handling via `fork()`
- [ ] File locking for account operations
- [ ] Fund transfer with atomic dual-account locking
- [ ] Semaphore-protected shared counter
- [ ] Loan lifecycle (apply → assign → approve/reject)
- [ ] Feedback & transaction history
- [ ] Role management & account activation/deactivation
- [ ] Integration testing
