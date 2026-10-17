#ifndef COUNTER_H
#define COUNTER_H

#include <stddef.h>

#define COUNTER_FILE "data/counter.bin"

/* Hands out the next free ID for one entity and increments it in
   counter.bin, under a write lock (so two threads never get the same ID).
   field = offsetof(struct Counter, next_xxx_id).
   Returns the ID (>= 1), or -1 on error. */
int counter_next(size_t field);

#endif
