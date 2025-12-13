.POSIX:

XCFLAGS = ${CFLAGS} -std=c99 -fPIC \
		  -Wall -Wextra -Wpedantic \
		  -Wno-unused-parameter
XLDFLAGS = ${LDFLAGS} -shared -Wl,-soname,dumb_gbm.so

LIBDIR ?= /lib64

OBJ = dumb_gbm.o

LIB = dumb_gbm.so

all: ${LIB}

.c.o:
	${CC} ${XCFLAGS} -c -o $@ $<

dumb_gbm.so: ${OBJ}
	${CC} ${XCFLAGS} -o $@ ${OBJ} ${XLDFLAGS}

install: ${LIB}
	mkdir -p ${DESTDIR}/usr${LIBDIR}
	cp -f ${LIB} ${DESTDIR}/usr${LIBDIR}/${LIB}

uninstall:
	rm -f ${DESTDIR}/usr${LIBDIR}/${LIB}
clean:
	rm -f ${LIB} ${OBJ}

.PHONY: all clean install uninstall
