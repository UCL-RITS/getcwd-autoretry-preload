
CC=gcc
OPTFLAGS=-O2
LDFLAGS=-shared -fPIC

INSTALL_PATH?=/shared/ucl/apps/getcwd-autoretry

getcwd-autoretry-preload.so: getcwd-autoretry-preload.c
	$(CC) $(OPTFLAGS) $(LDFLAGS) -o $@ $<

install: getcwd-autoretry-preload.so
	install -d $(INSTALL_PATH)
	install $< $(INSTALL_PATH)

clean: 
	-rm getcwd-autoretry-preload.so

.PHONY: clean install
