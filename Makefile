-include config.mk

CFLAGS 	?= -O2
LDFLAGS ?= -L $(CURDIR)

all: libjspp.a

libjspp.a: jspp.o
	$(AR) rc $@ $^

jspp.o: jspp.c jspp.h
	$(CC) -c $(CFLAGS) $(filter %.c,$^) -o $@

test.o: test.c

tests: tests.c test.o test.h libjspp.a
	$(CC) $(CFLAGS) $(LDFLAGS) $(filter %.c %.o,$^) -ljspp -o $@

clean:
	$(RM) *.o *.a tests *.exe
