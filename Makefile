.POSIX:

XCFLAGS = ${CFLAGS} -std=c99 -fPIC \
		  -Wall -Wextra -Wpedantic -Wmissing-prototypes \
		  -Wno-unused-parameter \
		  $(shell pkg-config --cflags libdrm)

XLDFLAGS = ${LDFLAGS} $(shell pkg-config --libs libdrm) \
		  -shared -Wl,-soname,dumb_gbm.so

LIBDIR ?= /lib64

OBJ = dumb_gbm.o

LIB = dumb_gbm.so

ifeq ($(STRICT), 1)
	XCFLAGS += -DSTRICT
endif

all: ${LIB}

.c.o:
	${CC} ${XCFLAGS} -c -o $@ $<

dumb_gbm.so: ${OBJ}
	${CC} ${XCFLAGS} -o $@ ${OBJ} ${XLDFLAGS}

install: ${LIB}
	mkdir -p ${DESTDIR}/usr${LIBDIR}/gbm
	cp -f ${LIB} ${DESTDIR}/usr${LIBDIR}/gbm/${LIB}

uninstall:
	rm -f ${DESTDIR}/usr${LIBDIR}/gbm/${LIB}
clean:
	rm -f ${LIB} ${OBJ}

.PHONY: all clean install uninstall
