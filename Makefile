CC = gcc
CFLAGS = -Wall -Wextra -pthread -Iinclude
LDFLAGS = -lsqlite3 -lpthread -lm

SRC = src/main.c src/collector.c src/analyzer.c src/display.c
OBJ = $(SRC:.c=.o)
TARGET = dbpulse

all: $(TARGET)

$(TARGET): $(OBJ)
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS)

%.o: %.c
	$(CC) $(CFLAGS) -c -o $@ $<

clean:
	rm -f $(OBJ) $(TARGET)

.PHONY: all clean
