CC = gcc
CCFLAGS = -Wall -std=gnu99 -O2 -lm

filter: filter.c
	$(CC) $(CCFLAGS) -o $@ $^

.PHONY: clean
clean:
	@rm -vf filter

