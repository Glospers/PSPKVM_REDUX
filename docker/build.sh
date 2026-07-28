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

# --- our own JSR components --------------------------------------------------
# The upstream tree is fetched fresh at a pinned commit and is never modified in
# place, so components we write ourselves live in this repository and are copied
# in here. JSR 184 (M3G) is delivered this way; the patches above only flip the
# build switches that reference it.
COMPONENTS="${PSPKVM_COMPONENTS:-/work/components}"
if [ -d "$COMPONENTS" ]; then
  for c in "$COMPONENTS"/*; do
    [ -d "$c" ] || continue
    echo ">> adding component: $(basename "$c")"
    rm -rf "${SRC:?}/$(basename "$c")"
    cp -R "$c" "$SRC/"
  done
fi

# The upstream shell scripts can arrive without the executable bit (depending on
# how the source tree was checked out / mounted). Restore it before we invoke
# them, or the first `./build-psp-cldc.sh` fails with exit 126 (Permission denied).
find "$SRC" -type f -name '*.sh' -exec chmod +x {} + 2>/dev/null || true

# The romized class image is regenerated whenever the Java sources change, but
# the object built from it is not reliably rebuilt, so a stale ROMImage.o and
# libmidp.a can be linked and the resulting EBOOT is byte-for-byte identical to
# the previous one. Drop both so the class image always reaches the binary.
rm -f "$SRC"/midp/build/javacall_psp/output/obj/mips/ROMImage.o \
      "$SRC"/midp/build/javacall_psp/output/bin/mips/libmidp.a

# --- 1) libm3g.a (the JSR 184 engine) ---------------------------------------
# Built from two trees that are kept apart on purpose:
#
#   m3g/       ours (this repository): the reconstructed public header, the
#              platform pieces m3gcore expects a port to supply, the null
#              GL ES/EGL backend, and the makefile that ties it together.
#              Arrives with the other components above, so it is at $SRC/m3g.
#   m3gcore/   the Nokia M3G engine, an upstream drop under a different licence
#              (EPL-1.0). Like pspkvm it is fetched at a pinned commit and is
#              never vendored or modified, so it is only ever read from here.
#
# Do this before the long phoneME build: a mistake in the engine or a missing
# m3gcore checkout should be reported in seconds, not twenty minutes in.
M3G_DIR="$SRC/m3g"
if [ ! -d "$M3G_DIR/src" ]; then
  for cand in /work/m3g /m3g; do
    if [ -d "$cand/src" ]; then
      echo ">> adding component: m3g (from $cand)"
      rm -rf "${M3G_DIR:?}"
      cp -R "$cand" "$M3G_DIR"
      break
    fi
  done
fi

if [ -z "${M3GCORE_DIR:-}" ]; then
  for cand in /work/m3gcore /m3gcore "$(dirname "$SRC")/m3gcore"; do
    if [ -d "$cand/src" ]; then M3GCORE_DIR="$cand"; break; fi
  done
fi

if [ ! -d "$M3G_DIR/src" ]; then
  echo "ERROR: the m3g component is missing. Mount this repository's m3g/ at" >&2
  echo "       /work/components/m3g (or /m3g)." >&2
  exit 2
fi
if [ -z "${M3GCORE_DIR:-}" ]; then
  echo "ERROR: m3gcore sources not found. Clone" >&2
  echo "       https://github.com/toaarnio/m3gcore.git at the pinned commit and" >&2
  echo "       mount it at /work/m3gcore (or /m3gcore), or set M3GCORE_DIR." >&2
  exit 2
fi

echo ">> [1/3] building libm3g.a (m3gcore = $M3GCORE_DIR)"
make -C "$M3G_DIR" M3GCORE_DIR="$M3GCORE_DIR"

# --- 2) phoneME libraries ----------------------------------------------------
echo ">> [2/3] building phoneME libraries (javacall/pcsl/cldc/midp)"
./build-psp-cldc.sh -J "$JDK_DIR"

# --- 3) link + package EBOOT.PBP --------------------------------------------
# psp/Makefile picks libm3g.a up from $(ROOT)/m3g/lib and pulls it in with
# --whole-archive: nothing calls into the engine yet, so an ordinary -lm3g
# would let the linker discard every object in it and produce a byte-identical
# EBOOT (see docker/patches/0042-psp-link-m3g.patch).
echo ">> [3/3] linking + packaging EBOOT.PBP (BUILD_SLIM=true)"
cd psp
make BUILD_SLIM=true

if [ -f EBOOT.PBP ]; then
  echo "== SUCCESS: $(pwd)/EBOOT.PBP =="
  ls -la EBOOT.PBP
else
  echo "== FAILURE: EBOOT.PBP was not produced ==" >&2
  exit 1
fi
