.PHONY: all clean release

CC ?= gcc

all: dumpflash txicdi flashbin

release: CFLAGS += -O2
release: LDFLAGS += -Wl,-O2

all: CFLAGS += -g -DDEBUG
all: LDFLAGS += -Wl,-g

release: dumpflash flashbin txicdi

dumpflash: dumpflash.o icdi.o
	$(LINK.o) $^ -o $@

txicdi: tx_icdi.o icdi.o
	$(LINK.o) $^ -o $@

flashbin: bin2flash.o icdi.o
	$(LINK.o) $^ -o $@

clean:
	rm -f *.o dumpflash txicdi flashbin
