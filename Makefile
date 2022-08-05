CFLAGS ?= -std=c99 -Wall -Wno-unused-function -pedantic

all: exec test

exec:
	$(CC) $(CFLAGS) lc3exec.c -o lc3exec

forth: lc3frth.c lc3.h
	$(CC) $(CFLAGS) lc3frth.c -o lc3frth
	cat stdlib.fs | sed 's/(.*)//' | sed 's/\\.*$$//' > stdlib.forth
	cat stdlib.forth - | ./lc3frth

test: lc3test
	./lc3test

.PHONY: test forth
