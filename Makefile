CC   = gcc
OPTS = -Wall

all: server client libmfs.so

# this generates the target executables
server: server.o udp.o
	$(CC) -o server server.o udp.o 

client: client.o mfs.o udp.o
	$(CC) -o client client.o mfs.o udp.o

libmfs.so: mfs.o udp.o
	$(CC) -c -fpic mfs.c udp.c -Wall -Werror
	$(CC) -shared -o libmfs.so mfs.o udp.o

# this is a generic rule for .o files 
%.o: %.c 
	$(CC) $(OPTS) -c $< -o $@

clean:
	rm -f server.o udp.o client.o mfs.o server client libmfs.so



