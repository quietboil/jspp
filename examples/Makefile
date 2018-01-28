JSPPDIR = $(realpath $(dir $(CURDIR)))
CFLAGS  = -I $(JSPPDIR) -I $(CURDIR)/include -g
LDFLAGS = -L $(JSPPDIR) -L $(CURDIR)/lib
LDLIBS  = -ljspp -lhttpget
ifeq ($(OS),Windows_NT)
	LDLIBS += -lws2_32
endif

all: sunrise-sunset

sunrise-sunset: sunrise-sunset.c $(JSPPDIR)/libjspp.a $(CURDIR)/lib/libhttpget.a

$(CURDIR)/lib/libhttpget.a:
	$(MAKE) -C $(CURDIR)/lib

$(JSPPDIR)/libjspp.a:
	$(MAKE) -C $(JSPPDIR)

clean:
	$(RM) sunrise-sunset sunrise-sunset.exe
