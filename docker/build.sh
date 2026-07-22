#!/usr/bin/env bash
###############################################################################
# PSPKVM Phase 0 build driver (runs inside the container).
#
#   pspkvm-build [SRC_DIR]
#
# SRC_DIR defaults to /work/pspkvm (bind-mounted). Optional patches in
# $PSPKVM_PATCHES (default /work/patches) are applied before building.
#
# Steps, matching BUILDING.TXT:
#   1. ./build-psp-cldc.sh -J $JDK_DIR   (javacall -> pcsl -> cldc -> midp libs)
#   2. cd psp && make BUILD_SLIM=true     (link + pack EBOOT.PBP for 3.xx CFW)
###############################################################################
set -euo pipefail

SRC="${1:-/work/pspkvm}"
PATCHES="${PSPKVM_PATCHES:-/work/patches}"
export JDK_DIR="${JDK_DIR:-/opt/jdk}"

if [ ! -d "$SRC" ]; then
  echo "ERROR: source dir '$SRC' not found. Mount the pspkvm tree at /work/pspkvm." >&2
  exit 2
fi

echo "=================== PSPKVM Phase 0 build ==================="
echo "  SRC     = $SRC"
echo "  JDK_DIR = $JDK_DIR"
echo "  javac   = $(javac -version 2>&1)"
echo "  PSPDEV  = ${PSPDEV:-<unset>}"
echo "  psp-gcc = $(psp-gcc --version 2>/dev/null | head -1)"
echo "  make    = $(make --version | head -1)"
echo "==========================================================="

# --- optional source patches -------------------------------------------------
if [ -d "$PATCHES" ]; then
  shopt -s nullglob
  for p in "$PATCHES"/*.patch; do
    echo ">> applying patch: $(basename "$p")"
    git -C "$SRC" apply "$p" 2>/dev/null || patch -p1 -d "$SRC" < "$p"
  done
  shopt -u nullglob
fi

cd "$SRC"

# The upstream shell scripts can arrive without the executable bit (depending on
# how the source tree was checked out / mounted). Restore it before we invoke
# them, or the first `./build-psp-cldc.sh` fails with exit 126 (Permission denied).
find "$SRC" -type f -name '*.sh' -exec chmod +x {} + 2>/dev/null || true

# --- 1) phoneME libraries ----------------------------------------------------
echo ">> [1/2] building phoneME libraries (javacall/pcsl/cldc/midp)"
./build-psp-cldc.sh -J "$JDK_DIR"

# --- 2) link + package EBOOT.PBP --------------------------------------------
echo ">> [2/2] linking + packaging EBOOT.PBP (BUILD_SLIM=true)"
cd psp
make BUILD_SLIM=true

if [ -f EBOOT.PBP ]; then
  echo "== SUCCESS: $(pwd)/EBOOT.PBP =="
  ls -la EBOOT.PBP
else
  echo "== FAILURE: EBOOT.PBP was not produced ==" >&2
  exit 1
fi
