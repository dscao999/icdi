.PHONY: all clean

CC ?= gcc
CFLAGS += -g -DDEBUG

all: dumpflash

release: CFLAGS += -O2

dumpflash: dumpflash.o icdi.o
	$(LINK.o) $^ -o $@

clean:
	rm -f *.o dumpflash
