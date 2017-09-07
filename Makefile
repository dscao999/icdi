.PHONY: all

CC ?= gcc
CFLAGS += -g -DDEBUG

all: getver

release: CFLAGS += -O2

getver: get_icdi_version.o icdi.o
	$(LINK.o) $^ -o $@

clean:
	rm -f *.o getver

.PHONY: all clean
