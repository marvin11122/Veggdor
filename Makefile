CC ?= cc
CFLAGS ?= -O2 -Wall -Wextra -Wpedantic -fstack-protector-strong -D_FORTIFY_SOURCE=2 -std=c11

all: veg_test veg2_test

veg_test: veg.c veg_test.c veg.h
	$(CC) $(CFLAGS) -o $@ veg.c veg_test.c

veg2_test: veg.c veg2.c veg2_test.c veg2.h veg.h
	$(CC) $(CFLAGS) -o $@ veg.c veg2.c veg2_test.c

test: all
	./veg_test
	./veg2_test

v2run: veg.c veg2.c v2run.c veg2.h veg.h
	$(CC) $(CFLAGS) -o $@ veg.c veg2.c v2run.c

run: v2run
	./v2run

clean:
	rm -f veg_test veg2_test
