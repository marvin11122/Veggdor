CC      ?= cc
CFLAGS  ?= -O2 -Wall -Wextra -Wpedantic -Wshadow -fstack-protector-strong \
           -D_FORTIFY_SOURCE=2 -std=c11

.PHONY: all test clean run mc demo-dot

all: veg_test veg2_test veg3_test veg_mc

veg_test: veg.c veg_test.c veg.h
	$(CC) $(CFLAGS) -o $@ veg.c veg_test.c

veg2_test: veg.c veg2.c veg2_test.c veg2.h veg.h
	$(CC) $(CFLAGS) -o $@ veg.c veg2.c veg2_test.c

veg3_test: veg.c veg3.c veg3_test.c veg3.h veg2.h veg.h
	$(CC) $(CFLAGS) -o $@ veg.c veg3.c veg3_test.c

veg_mc: veg.c veg_mc.c veg.h
	$(CC) $(CFLAGS) -o $@ veg.c veg_mc.c

host_demo: veg.c veg_host.c veg.h
	$(CC) $(CFLAGS) -DVEG_HOST_MAIN -o $@ veg.c veg_host.c

test: all
	./veg_test
	./veg2_test
	./veg3_test

mc: veg_mc
	./veg_mc example.mc

demo-dot: veg_mc
	./veg_mc --trace examples/dot_product.mc

clean:
	rm -f veg_test veg2_test veg3_test veg_mc v2run host_demo veg3.o
