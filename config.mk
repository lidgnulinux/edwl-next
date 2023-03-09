# paths
PREFIX = /usr/local

# Default compile flags (overridable by environment)
CFLAGS ?= -g -Wall -Wextra -Werror -Wno-unused-parameter -Wno-sign-compare -Wno-unused-function -Wno-unused-variable -Wdeclaration-after-statement

# Uncomment to build XWayland support
#XWAYLAND = -DXWAYLAND
#XLIBS = xcb

# Uncomment to build with systemd libs
# DBUS_LIBS = libsystemd
# DBUS_FLAGS = -DWITH_SYSTEMD

# Uncomment to build with elogind libs
DBUS_LIBS = libelogind
DBUS_FLAGS = -DWITH_ELOGIND

# Uncomment to build with basu libs
# DBUS_LIBS = basu libdrm
# DBUS_FLAGS = -DWITH_BASU
