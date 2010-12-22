include $(GOROOT)/src/Make.inc

TARG=feedparser
CGOFILES=feedparser.go
CGO_LDFLAGS=../c/feedparser.o -lglib-2.0 -lxml2

include $(GOROOT)/src/Make.pkg
