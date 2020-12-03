CFLAGS=-c -Wall -O0 
LIBS = 

all: image_to_c 

image_to_c: main.o 
	$(CC) main.o $(LIBS) -o image_to_c 

main.o: main.c
	$(CC) $(CFLAGS) main.c

clean:
	rm -rf *.o image_to_c
