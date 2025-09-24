CC ?= gcc
CFLAGS ?= -std=c99 -Wall -Wextra -pedantic -O2
LDFLAGS ?=

BIN := pa1
SRC := main.c ipc.c
HDR := ipc_impl.h ipc.h common.h pa1.h

all: $(BIN)

$(BIN): $(SRC) $(HDR)
	$(CC) $(CFLAGS) -o $@ $(SRC) $(LDFLAGS)

clean:
	rm -f $(BIN) *.o *.log

.PHONY: all clean
