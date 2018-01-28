EXAMPLESDIR = $(realpath $(dir $(CURDIR)))
LDFLAGS = -L $(CURDIR)
ifeq ($(OS),Windows_NT)
	LDLIBS = -lws2_32
	VARIANT = win
else
	VARIANT = bsd
endif
CFLAGS = -I $(EXAMPLESDIR)/include -g

all: libhttpget.a

libhttpget.a: httpget.o
	$(AR) rc $@ $^

httpget.o: httpget-$(VARIANT).c
	$(CC) -c $(CFLAGS) $^ -o $@

clean:
	$(RM) *.o *.a test test.exe
