CC = gcc
CFLAGS = -Wall
LIBS =  -levent

PROGS =	ptp-server ptp-serverd ptp-client

all:	${PROGS}

ptp-server: ptp-server.c
		${CC} ${CFLAGS} -o $@ $^ ${LIBS}

ptp-serverd: ptp-server.c
		${CC} ${CFLAGS} -DDAEMON -o $@ $^ ${LIBS}

ptp-client: ptp-client.c
		${CC} ${CFLAGS} -o $@ $^ ${LIBS} -lpthread


clean:
		rm -f ${PROGS}
