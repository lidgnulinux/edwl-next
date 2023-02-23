# General
The analog of [dwmblocks](https://github.com/torrinfail/dwmblocks) for [edwl](https://gitlab.com/necrosis/edwl).

# Configuration
All configs are stored in blocks.h header file.
To disable background update, set 'backgroundinterval' to 0. Currently, an edwl supports jpeg and png images as a backgrounds.

# Build
Requirements are:
1. gcc
2. libsystemd (or alternative API, see config.mk)

To build it, just run:

    make
