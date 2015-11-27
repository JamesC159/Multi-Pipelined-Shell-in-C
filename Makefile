CC=gcc
CFLAGS=-c -Wall -g
SOURCES=shell.c
OBJECTS=$(SOURCES:.c=.o)
EXEC=driver

all: $(EXEC)

$(EXEC): $(OBJECTS)
	$(CC) $(OBJECTS) -o $@ 

shell.o: shell.c
	$(CC) $(CFLAGS) shell.c

clean:
	-rm *.o $(EXEC)
