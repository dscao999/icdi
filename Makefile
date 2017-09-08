.PHONY: all clean release

CC ?= gcc

all: dumpflash txicdi flashbin

release: CFLAGS += -O2

all: CFLAGS += -g -DDEBUG

release: dumpflash

dumpflash: dumpflash.o icdi.o
	$(LINK.o) $^ -o $@

txicdi: tx_icdi.o icdi.o
	$(LINK.o) $^ -o $@

flashbin: bin2flash.o icdi.o
	$(LINK.o) $^ -o $@

clean:
	rm -f *.o dumpflash txicdi
