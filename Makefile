# Makefile for xhisper

CC = gcc
CFLAGS = -O2 -Wall -Wextra
PREFIX = /usr/local
BINDIR = $(PREFIX)/bin

all: xhispertool test test-keymap

xhispertool: xhispertool.c keymap.c keymap.h
	$(CC) $(CFLAGS) xhispertool.c keymap.c -o xhispertool
	ln -sf xhispertool xhispertoold

test: test.c keymap.c keymap.h
	$(CC) $(CFLAGS) test.c keymap.c -o test

test-keymap: test_keymap.c keymap.c keymap.h
	$(CC) $(CFLAGS) -o test-keymap test_keymap.c keymap.c

install: xhispertool xhisper.sh
	install -d $(DESTDIR)$(BINDIR)
	install -m 755 xhispertool $(DESTDIR)$(BINDIR)/xhispertool
	ln -sf xhispertool $(DESTDIR)$(BINDIR)/xhispertoold
	install -m 755 xhisper.sh $(DESTDIR)$(BINDIR)/xhisper

uninstall:
	rm -f $(DESTDIR)$(BINDIR)/xhisper
	rm -f $(DESTDIR)$(BINDIR)/xhispertool
	rm -f $(DESTDIR)$(BINDIR)/xhispertoold

clean:
	rm -f xhispertool xhispertoold test

.PHONY: all install uninstall clean
