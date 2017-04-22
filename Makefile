CC=gcc
CFLAGS=-O2 -Wformat=2 `pkg-config --cflags libcurl`
LIBS=`pkg-config --libs libcurl`


all:	lam

lam: 	lametric.c
	$(CC) lametric.c nxjson/nxjson.c -o lam $(CFLAGS) $(LIBS)

install:
	cp lam ~/bin
