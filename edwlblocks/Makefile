PREFIX ?= /usr/local
CFLAGS ?= -g -Wall -Wextra -Werror -Wno-unused-parameter -Wno-sign-compare -Wno-unused-function -Wno-unused-variable -Wdeclaration-after-statement
CC ?= cc

include config.mk

PKGS = $(DBUS_LIBS)
CFLAGS += $(foreach p,$(PKGS),$(shell pkg-config --cflags $(p)))
LDLIBS += $(foreach p,$(PKGS),$(shell pkg-config --libs $(p)))
CFLAGS += $(DBUS_FLAGS)

all: edwlblocks

clean:
	rm -f *.o *.gch edwlblocks

install: output
	mkdir -p $(DESTDIR)$(PREFIX)/bin
	install -m 0755 edwlblocks $(DESTDIR)$(PREFIX)/bin/edwlblocks
uninstall:
	rm -f $(DESTDIR)$(PREFIX)/bin/edwlblocks

.PHONY: all clean install uninstall

blocks.h: | blocks.def.h
	cp blocks.def.h $@

edwlblocks.o: blocks.h

edwlblocks: edwlblocks.o
