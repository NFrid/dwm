# nwm - undefined window manager
# See LICENSE file for copyright and license details.

include config.mk

SRC = $(wildcard *.c)
OBJ = ${SRC:.c=.o}

all: nwm

%.o: %.c
	${CC} -c ${CFLAGS} $<

${OBJ}: config.h config.mk

nwm: ${OBJ}
	${CC} -o $@ ${OBJ} ${LDFLAGS}

clean:
	rm -rf nwm ${OBJ}

install: all
	mkdir -p ${DESTDIR}${PREFIX}/bin
	cp -f nwm ${DESTDIR}${PREFIX}/bin
	chmod 755 ${DESTDIR}${PREFIX}/bin/nwm

uninstall:
	rm -f ${DESTDIR}${PREFIX}/bin/nwm

.PHONY: all clean install uninstall
