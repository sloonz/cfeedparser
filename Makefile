libfeedparser.so: feedparser.o
	gcc feedparser.o -o libfeedparser.so -shared -lxml2 -lglib-2.0 

feedparser.o: feedparser.c feedparser.h
	gcc -c feedparser.c -o feedparser.o -O4 -Wall -fPIC  -I/usr/include/libxml2 -I/usr/include/glib-2.0 -I/usr/lib/glib-2.0/include

