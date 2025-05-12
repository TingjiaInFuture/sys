CC=gcc
CFLAGS=-Wall -Wextra -std=c99
TARGET=test_hzk16
SRC=test_hzk16.c

.PHONY: all clean

all: $(TARGET)

$(TARGET): $(SRC)
	$(CC) $(CFLAGS) -o $(TARGET) $(SRC)

clean:
	rm -f $(TARGET)
