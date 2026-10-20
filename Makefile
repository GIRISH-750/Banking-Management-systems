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
