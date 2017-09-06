.PHONY: all

EXE := lm4flash

CC ?= gcc
CFLAGS += -g -DDEBUG

LIBUSB_CFLAGS = $(shell pkg-config --cflags libusb-1.0)
LIBUSB_LDFLAGS = $(shell pkg-config --libs libusb-1.0)

all: $(EXE) getver

release: CFLAGS += -O2
release: $(EXE)

$(EXE): $(EXE).c
	$(CC) $(CFLAGS) $(LIBUSB_CFLAGS) $^ $(LDFLAGS) $(LIBUSB_LDFLAGS) -o $@

getver: get_icdi_version.o icdi.o
	$(LINK.o) $^ -o $@

clean:
	rm -f *.o $(EXE) getver

.PHONY: all clean
