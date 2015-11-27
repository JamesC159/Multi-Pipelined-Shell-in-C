CC=gcc
CFLAGS=-c -Wall -g
SOURCES=main.c
OBJECTS=$(SOURCES:.c=.o)
EXEC=driver

all: $(EXEC)

$(EXEC): $(OBJECTS)
	$(CC) $(OBJECTS) -o $@ 

main.o: main.c
	$(CC) $(CFLAGS) main.c

clean:
	-rm *.o $(EXEC)
