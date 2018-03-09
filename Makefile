-include config.mk

CFLAGS 	?= -O2
LDFLAGS ?= -L $(CURDIR)

ifdef SystemDrive
	EXE := .exe
endif
TESTS := tests$(EXE)

all: libjspp.a $(TESTS)

libjspp.a: jspp.o
	$(AR) rc $@ $^

jspp.o: jspp.c jspp.h
	$(CC) -c $(CFLAGS) $(filter %.c,$^) -o $@

$(TESTS): tests.o test.o libjspp.a
	$(CC) $(LDFLAGS) $(filter %.o,$^) -ljspp -o $@

clean:
	$(RM) *.o *.a $(TESTS)
