CC=gcc
CFLAGS=-lcrypto -lpthread
OBJ=sharedMemory.o
EXEC=P1.out P2.out channel.out

all: $(EXEC)

%.out: %.o $(OBJ)
	$(CC) $< $(OBJ) -o $@ $(CFLAGS)

%.o: %.c %.h
	$(CC) -c $< -o $@

%o: %.c
	$(CC) -c $< -o $@

clean:
	rm -rf *.o $(EXEC)
