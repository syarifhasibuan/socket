CC  		= gcc
SRC 		= server.c client.c
OBJ 		= ${SRC:.c=.o}
LIBS    = 
CFLAGS  = -Wall
LDFLAGS = ${LIBS}

all: server client

%.o: %.c
	${CC} ${CFLAGS} -c $<

server: server.o
	${CC} -o $@ $+

client: client.o
	${CC} -o $@ $+

clean:
	rm -f server client ${OBJ}
