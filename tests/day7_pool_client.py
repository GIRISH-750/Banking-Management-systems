#!/usr/bin/env python3
# Day 7 pool test client. Usage: day7_pool_client.py small|full
import socket, sys, time

HOST, PORT = "127.0.0.1", 5000
passed = 0
failed = 0

def check(name, ok):
    global passed, failed
    if ok:
        passed += 1
        print("  PASS", name)
    else:
        failed += 1
        print("  FAIL", name)

def connect():
    end = time.time() + 5
    while True:
        try:
            return socket.create_connection((HOST, PORT), timeout=3)
        except ConnectionRefusedError:
            if time.time() > end:
                raise
            time.sleep(0.1)

def welcome(s, wait):
    s.settimeout(wait)
    try:
        return s.recv(256).startswith(b"Welcome")
    except socket.timeout:
        return False

def quit_(s):
    try:
        s.sendall(b"QUIT\n")
        s.settimeout(2)
        s.recv(256)
    except OSError:
        pass
    s.close()

def small():
    print("Pool of 2 workers:")
    a = connect()
    b = connect()
    check("client 1 served at once", welcome(a, 2))
    check("client 2 served at once", welcome(b, 2))
    c = connect()
    check("client 3 waits in queue (both workers busy)", not welcome(c, 1.5))
    quit_(a)
    check("client 3 served after client 1 quits", welcome(c, 3))
    quit_(b)
    quit_(c)
    ok = True
    for i in range(150):
        s = connect()
        if not welcome(s, 3):
            ok = False
            s.close()
            break
        quit_(s)
    check("150 sessions one after another (workers + slots reused)", ok)

    for i in range(30):
        s = connect()
        welcome(s, 3)
        s.close()   # leave without QUIT
    s = connect()
    check("still serving after 30 clients left without QUIT", welcome(s, 3))
    quit_(s)

def full():
    print("Default pool (100 workers), 100-connection limit:")
    socks = []
    served = 0
    for i in range(100):
        s = connect()
        if welcome(s, 3):
            served += 1
        socks.append(s)
    check("100 clients served at the same time (got %d)" % served, served == 100)
    extra = connect()
    check("client 101 waits (semaphore full)", not welcome(extra, 1.5))
    quit_(socks.pop(0))
    check("client 101 served after one client quits", welcome(extra, 3))
    quit_(extra)
    for s in socks:
        quit_(s)

if sys.argv[1] == "small":
    small()
else:
    full()
print("  -> %d passed, %d failed" % (passed, failed))
sys.exit(failed)
