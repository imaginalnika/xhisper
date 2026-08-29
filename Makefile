# Makefile for xhisper

CC = gcc
CFLAGS = -O2 -Wall -Wextra
PREFIX = /usr/local
BINDIR = $(PREFIX)/bin

all: xhispertool tests/test_uinput_integration

config:
	bash configure.sh

setup:
	sudo usermod -aG input $(USER)
	@echo "User added to 'input' group. Please LOG OUT and LOG IN again for changes to take effect."

show:
	@cat $(HOME)/.config/xhisper/xhisperrc 2>/dev/null || echo "Config file not found at ~/.config/xhisper/xhisperrc"

xhispertool: xhispertool.c
	$(CC) $(CFLAGS) xhispertool.c -o xhispertool
	ln -sf xhispertool xhispertoold

tests/test_uinput_integration: tests/test_uinput_integration.c
	$(CC) $(CFLAGS) tests/test_uinput_integration.c -o tests/test_uinput_integration

tests/test_xhispertool: tests/test_xhispertool.c xhispertool.c
	$(CC) $(CFLAGS) -I tests tests/test_xhispertool.c -o tests/test_xhispertool

install: xhispertool xhisper.sh
	install -d $(DESTDIR)$(BINDIR)
	install -m 755 xhispertool $(DESTDIR)$(BINDIR)/xhispertool
	ln -sf xhispertool $(DESTDIR)$(BINDIR)/xhispertoold
	install -m 755 xhisper.sh $(DESTDIR)$(BINDIR)/xhisper

uninstall:
	rm -f $(DESTDIR)$(BINDIR)/xhisper
	rm -f $(DESTDIR)$(BINDIR)/xhispertool
	rm -f $(DESTDIR)$(BINDIR)/xhispertoold

check: tests/test_xhispertool
	@echo "=== C tests ===" && tests/test_xhispertool
	@echo "=== Shell tests ===" && bash tests/test_paste.sh

clean:
	rm -f xhispertool xhispertoold
	rm -f tests/test_uinput_integration tests/test_xhispertool

.PHONY: all install uninstall clean check
