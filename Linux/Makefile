CC = gcc
CFLAGS = -Wall -Wextra -O2 -D_GNU_SOURCE
LDFLAGS = -ldl

TARGET = python-caller

.PHONY: all clean

all: $(TARGET)

$(TARGET): python-caller.c
	$(CC) $(CFLAGS) -o $@ $< $(LDFLAGS)

clean:
	rm -f $(TARGET)
