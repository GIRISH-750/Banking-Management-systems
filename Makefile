CC      = gcc
CFLAGS  = -Wall -Wextra -Iinclude
LIBS    = -pthread -lcrypto
SRV_SRC = $(filter-out src/client.c src/seed.c,$(wildcard src/*.c))
HDRS    = $(wildcard include/*.h)

all: seed server client

seed: src/seed.c src/hash.c $(HDRS)
	$(CC) $(CFLAGS) src/seed.c src/hash.c -o $@ -lcrypto

server: $(SRV_SRC) $(HDRS)
	$(CC) $(CFLAGS) $(SRV_SRC) -o $@ $(LIBS)

client: src/client.c src/net_util.c $(HDRS)
	$(CC) $(CFLAGS) src/client.c src/net_util.c -o $@ -pthread

test: all
	bash tests/run_all.sh
	bash tests/day4_check.sh
	bash tests/day5_check.sh
	bash tests/day6_crash.sh

clean:
	rm -f seed server client

.PHONY: all test clean

# ================= easy commands (clone -> make deps -> make run) =================
.PHONY: help deps setup run run-server run-client reset
help:                 ## list the easy commands
	@echo 'make deps        install compiler + OpenSSL (once, needs sudo)'
	@echo 'make setup       build everything + create demo data'
	@echo 'make run         ONE terminal: start server in background + open a client'
	@echo 'make run-server  start the server in this terminal'
	@echo 'make run-client  open another client (in a new terminal)'
	@echo 'make reset       reset all data to the demo data'
	@echo 'make test        run all tests'
deps:                 ## install build tools (Ubuntu / WSL)
	sudo apt-get update && sudo apt-get install -y build-essential libssl-dev
setup: all            ## build + demo data
	./seed
run: all              ## server in background + one client, all in one terminal
	@[ -f data/customers.bin ] || ./seed
	@./server > server.log 2>&1 & SRV=$$!; sleep 0.5; \
	if kill -0 $$SRV 2>/dev/null; then \
	  echo 'Server running in background (log: server.log). More users: make run-client in a new terminal.'; \
	  ./client; kill $$SRV 2>/dev/null; echo 'Server stopped.'; \
	else \
	  echo 'A server is already running on port 5000 - connecting to it.'; ./client; \
	fi
run-server: all       ## server in this terminal
	@[ -f data/customers.bin ] || ./seed
	./server
run-client: client    ## one more client
	./client
reset: seed           ## demo data again
	./seed
