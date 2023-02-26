.POSIX:
.SUFFIXES:

include config.mk

# flags for compiling
EDWLCPPFLAGS = -I. -DWLR_USE_UNSTABLE -D_POSIX_C_SOURCE=200809L -DVERSION=\"$(VERSION)\" $(XWAYLAND) $(DBUS_FLAGS)
EDWLDEVCFLAGS = -pedantic -Wall -Wextra -Wdeclaration-after-statement -Wno-unused-parameter -Wno-sign-compare -Wshadow -Wunused-macros

# Wayland utils
WAYLAND_PROTOCOLS = `pkg-config --variable=pkgdatadir wayland-protocols`
WAYLAND_SCANNER   = `pkg-config --variable=wayland_scanner wayland-scanner`

# CFLAGS / LDFLAGS
PKGS      = wlroots wayland-server xkbcommon libinput xcb cairo pango pangocairo libjpeg pixman-1 $(XLIBS) $(DBUS_LIBS)
EDWLCFLAGS = `pkg-config --cflags $(PKGS)` $(EDWLCPPFLAGS) $(CFLAGS) $(XWAYLAND) $(DBUS_FLAGS)
LDLIBS    = `pkg-config --libs $(PKGS)` $(LIBS)
LDLIBS		+= -lpthread -lm

# build rules

# wayland-scanner is a tool which generates C headers and rigging for Wayland
# protocols, which are specified in XML. wlroots requires you to rig these up
# to your build system yourself and provide them in the include path.
all: edwl
edwl: edwl.o util.o tray.o
	$(CC) edwl.o util.o tray.o $(LDLIBS) $(LDFLAGS) $(EDWLCFLAGS) -o $@
edwl.o: edwl.c config.mk config.h client.h xdg-shell-protocol.h wlr-layer-shell-unstable-v1-protocol.h pointer-constraints-unstable-v1-protocol.h
util.o: util.c util.h
tray.o: tray.c tray.h

# wayland-scanner is a tool which generates C headers and rigging for Wayland
# protocols, which are specified in XML. wlroots requires you to rig these up
# to your build system yourself and provide them in the include path.
WAYLAND_SCANNER   = `pkg-config --variable=wayland_scanner wayland-scanner`
WAYLAND_PROTOCOLS = `pkg-config --variable=pkgdatadir wayland-protocols`

xdg-shell-protocol.h:
	$(WAYLAND_SCANNER) server-header \
		$(WAYLAND_PROTOCOLS)/stable/xdg-shell/xdg-shell.xml $@
wlr-layer-shell-unstable-v1-protocol.h:
	$(WAYLAND_SCANNER) server-header \
		protocols/wlr-layer-shell-unstable-v1.xml $@
pointer-constraints-unstable-v1-protocol.h:
	$(WAYLAND_SCANNER) server-header \
		$(WAYLAND_PROTOCOLS)/unstable/pointer-constraints/pointer-constraints-unstable-v1.xml $@


config.h:
	cp config.def.h $@
clean:
	rm -f edwl *.o *-protocol.h

# distribution archive
dist: clean
	mkdir -p edwl-$(VERSION)
	cp -R LICENSE* Makefile README.md client.h config.def.h\
		config.mk protocols edwl.1 edwl.c util.c util.h\
		edwl-$(VERSION)
	tar -caf dwl-$(VERSION).tar.gz edwl-$(VERSION)
	rm -rf edwl-$(VERSION)

# install rules

install: edwl
	mkdir -p $(DESTDIR)$(PREFIX)/bin
	cp -f edwl $(DESTDIR)$(PREFIX)/bin
	chmod 755 $(DESTDIR)$(PREFIX)/bin/edwl
	mkdir -p $(DESTDIR)$(PREFIX)/share/edwl  
	cp -f wallpaper.png $(DESTDIR)$(PREFIX)/share/edwl 
	chmod 644 $(DESTDIR)$(PREFIX)/share/edwl/wallpaper.png
uninstall:
	rm -f $(DESTDIR)$(PREFIX)/bin/edwl

.SUFFIXES: .c .o
.c.o:
	$(CC) $(CPPFLAGS) $(EDWLCFLAGS) -ggdb -c $<
