PKGCONFIG ?= pkg-config
CC ?= gcc

cflags := $(shell ${PKGCONFIG} --cflags glib-2.0 libxml-2.0)
libs := $(shell ${PKGCONFIG} --libs glib-2.0 libxml-2.0)

.PHONY: all clean tests

all: libfeedparser.so

libfeedparser.so: feedparser.o
	${CC} feedparser.o -o libfeedparser.so -shared ${libs}

feedparser.o: feedparser.c feedparser.h
	${CC} -c feedparser.c -o feedparser.o -O4 -Wall -fPIC ${cflags}

clean:
	rm -f feedparser.o libfeedparser.so

tests: libfeedparser.so
	python cfeedparsertest.py
