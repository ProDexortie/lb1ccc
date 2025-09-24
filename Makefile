CC=clang
CFLAGS=-std=c99 -Wall -Wextra -pedantic -O2
LDFLAGS=-L. -lruntime

TARGET=pa2
SRCS=pa23.c
OBJS=$(SRCS:.c=.o)

all: $(TARGET)

$(TARGET): $(OBJS)
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS)

%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	rm -f $(OBJS) $(TARGET)

.PHONY: all clean
