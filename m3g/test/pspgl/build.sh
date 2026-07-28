#!/bin/sh
# Build the pspgl proof-of-concept inside the pspkvm-build image.
#
#   From Git Bash on the dev box:
#     export PATH="$PATH:/c/Users/Rasay/AppData/Local/Programs/DockerDesktop/resources/bin"
#     export MSYS_NO_PATHCONV=1
#     ./build.sh
#
# Produces EBOOT.PBP next to the sources.  Install with:
#   cp EBOOT.PBP <memstick>/PSP/GAME/PSPGLTEST/EBOOT.PBP
set -e
HERE=$(cd "$(dirname "$0")" && pwd)
docker run --rm --entrypoint bash -v "$HERE:/work" pspkvm-build \
	-c 'cd /work && make clean >/dev/null 2>&1; make'
