
CC=gcc
OPTFLAGS=-O2
LDFLAGS=-shared -fPIC

getcwd-autoretry-preload.so: getcwd-autoretry-preload.c
	$(CC) $(OPTFLAGS) $(LDFLAGS) -o $@ $<

clean: 
	-rm getcwd-autoretry-preload.so

.PHONY: clean
