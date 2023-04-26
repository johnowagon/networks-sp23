CC = gcc
CFLAGS = -g -Wall -Wextra
LDFLAGS = 
OBJFILES = helpers.o dfc.o
OBJFILES2 = helpers.o dfs.o
TARGET1 = dfc
TARGET2 = dfs

all: $(TARGET1) $(TARGET2)

$(TARGET1): $(OBJFILES)
	$(CC) $(CFLAGS) -o $(TARGET1) $(OBJFILES) $(LDFLAGS)

$(TARGET2): $(OBJFILES2)
	$(CC) $(CFLAGS) -o $(TARGET2) $(OBJFILES2) $(LDFLAGS)

clean:
	rm -f $(OBJFILES) $(OBJFILES2) $(TARGET1) $(TARGET2) *~