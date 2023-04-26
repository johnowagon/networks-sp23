CC = gcc
CFLAGS = -g -Wall -Wextra
LDFLAGS =
OBJFILES = helpers.o proxy.o cache.o
TARGET = server

all: $(TARGET)

$(TARGET): $(OBJFILES)
	$(CC) $(CFLAGS) -o $(TARGET) $(OBJFILES) $(LDFLAGS)

clean:
	rm -f $(OBJFILES) $(TARGET) *~