CC = clang
CFLAGS = -std=c99 -Wall -pedantic
LDFLAGS = -L. -lruntime

SOURCES = pa23.c ipc.c bank_robbery.c
OBJECTS = $(SOURCES:.c=.o)
TARGET = pa2

.PHONY: all clean

all: $(TARGET)

$(TARGET): $(OBJECTS)
	$(CC) $(OBJECTS) -o $@ $(LDFLAGS)

%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	rm -f $(OBJECTS) $(TARGET)

# Dependencies
pa23.o: pa23.c banking.h pa2345.h common.h ipc.h
ipc.o: ipc.c ipc.h common.h
bank_robbery.o: bank_robbery.c banking.h