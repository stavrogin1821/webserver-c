CC=gcc
CFLAGS=-Wall -Wextra -O2
TARGET=webserver

all: $(TARGET)

$(TARGET): webserver.c
	$(CC) $(CFLAGS) -o $(TARGET) webserver.c

clean:
	rm -f $(TARGET)