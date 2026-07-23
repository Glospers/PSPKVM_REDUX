# syntax=docker/dockerfile:1
###############################################################################
# PSPKVM — Phase 0 reproducible build environment
#
# Produces a container with everything needed to build the PSPKVM EBOOT.PBP:
#   - ps2dev pspsdk cross-toolchain (psp-gcc, pack-pbp, mksfo, ...)  @ /usr/local/pspdev
#   - a JDK for the phoneME jcc/romizer host tools                   @ /opt/jdk
#   - GNU make + a host C/C++ toolchain for the build-time tools
#
# The pspkvm SOURCE is NOT baked in; it is mounted at /work/pspkvm at run time
# so the (slow-changing) environment image stays independent of source churn.
#
# Pinned inputs (see docker/README.md for rationale + risk notes):
#   Base image        ubuntu:24.04   (matches the pspdev-ubuntu-latest build host)
#   pspdev toolchain  v20260701      (pspdev-ubuntu-latest-x86_64.tar.gz, glibc)
#   JDK               Temurin 8u462-b08 (emits 1.4-target bytecode; runs on modern glibc)
###############################################################################

ARG UBUNTU_TAG=24.04

########################################
# base — shared host dependencies
########################################
FROM ubuntu:${UBUNTU_TAG} AS base
ARG DEBIAN_FRONTEND=noninteractive

# NOTE on `make`: BUILDING.TXT insists on GNU make 3.80 (not >=3.81). That rule
# exists only because make 3.81 mishandled *DOS-style* paths under Cygwin. In a
# Linux container all paths are POSIX, so the distro's make 4.x is fine. If a
# make-version-specific breakage ever surfaces, build make 3.82 from source and
# put it ahead on PATH (documented in README.md) — do not fight to compile 3.80.
RUN apt-get update && apt-get install -y --no-install-recommends \
      build-essential \
      make \
      git \
      ca-certificates \
      curl \
      unzip \
      zip \
      xxd \
      file \
      python3 \
      zlib1g-dev \
      bash \
    && rm -rf /var/lib/apt/lists/*

########################################
# toolchain — ps2dev pspsdk (prebuilt, glibc/ubuntu build)
########################################
FROM base AS toolchain
# TARGETARCH is provided by BuildKit: "amd64" on x86_64 hosts, "arm64" on a Pi 5.
# psp-gcc cross-compiles to PSP MIPS regardless of host CPU; we only need the
# host-native pspdev build here.
ARG TARGETARCH
ARG PSPDEV_RELEASE=v20260701
RUN case "${TARGETARCH}" in \
      amd64) PSPDEV_ASSET=pspdev-ubuntu-latest-x86_64.tar.gz ;; \
      arm64) PSPDEV_ASSET=pspdev-ubuntu-24.04-arm-arm64.tar.gz ;; \
      *) echo "unsupported TARGETARCH=${TARGETARCH}" >&2; exit 1 ;; \
    esac \
    && curl -fSL -o /tmp/pspdev.tar.gz \
      "https://github.com/pspdev/pspdev/releases/download/${PSPDEV_RELEASE}/${PSPDEV_ASSET}" \
    && tar -xzf /tmp/pspdev.tar.gz -C /usr/local \
    && rm /tmp/pspdev.tar.gz \
    && test -x /usr/local/pspdev/bin/psp-config \
    && test -x /usr/local/pspdev/bin/psp-gcc

########################################
# jdk — Eclipse Temurin 8 (pinned, via Adoptium versioned API)
########################################
FROM base AS jdk
ARG TARGETARCH
ARG JDK_VERSION=jdk8u462-b08
# Adoptium arch slug: amd64 -> x64, arm64 -> aarch64.
RUN case "${TARGETARCH}" in \
      amd64) JDK_ARCH=x64 ;; \
      arm64) JDK_ARCH=aarch64 ;; \
      *) echo "unsupported TARGETARCH=${TARGETARCH}" >&2; exit 1 ;; \
    esac \
    && mkdir -p /opt/jdk \
    && curl -fSL -o /tmp/jdk.tar.gz \
      "https://api.adoptium.net/v3/binary/version/${JDK_VERSION}/linux/${JDK_ARCH}/jdk/hotspot/normal/eclipse" \
    && tar -xzf /tmp/jdk.tar.gz -C /opt/jdk --strip-components=1 \
    && rm /tmp/jdk.tar.gz \
    && test -x /opt/jdk/bin/javac

########################################
# pspkvm-build — final image
########################################
FROM base AS pspkvm-build

COPY --from=toolchain /usr/local/pspdev /usr/local/pspdev
COPY --from=jdk       /opt/jdk          /opt/jdk

# --- Compiler shim: fit the 2010 C sources to a modern (GCC 15) toolchain ------
# Two eras collide here, and both fixes belong at the toolchain level because the
# four build modules invoke the compiler by different spellings (javacall uses
# bare `psp-gcc`; pcsl uses an absolute `.../bin/psp-gcc` path) — so per-module
# CFLAGS edits are fragile and incomplete. We wrap the real driver IN PLACE at
# its toolchain path, which every caller resolves to.
#
#  1. -std=gnu17 (PREPENDED, so any explicit per-file -std still wins): psp-gcc
#     15 defaults to C23, where an empty `()` prototype means `(void)`. The 2010
#     headers use K&R `()` = "unspecified args" and define the same functions
#     WITH parameters (e.g. javacall_network_connect_state), which C23 reads as
#     conflicting types. gnu17 restores the K&R meaning and fixes the whole class.
#  2. -Wno-error=... (APPENDED, so they override any -Werror): GCC promotes these
#     five diagnostics to hard errors by default; demote (not silence) them so the
#     old code compiles while staying visible for later cleanup.
RUN set -eu; cd /usr/local/pspdev/bin; \
    mv psp-gcc psp-gcc.real; \
    { \
      echo '#!/bin/sh'; \
      echo 'exec /usr/local/pspdev/bin/psp-gcc.real -std=gnu17 "$@" -Wno-error=implicit-function-declaration -Wno-error=implicit-int -Wno-error=int-conversion -Wno-error=incompatible-pointer-types -Wno-error=return-mismatch'; \
    } > psp-gcc; \
    chmod +x psp-gcc; \
    head -n2 psp-gcc

ENV PSPDEV=/usr/local/pspdev \
    JAVA_HOME=/opt/jdk \
    JDK_DIR=/opt/jdk \
    PATH=/usr/local/pspdev/bin:/opt/jdk/bin:/usr/local/bin:/usr/bin:/bin

# PSP ports pspkvm links against (psp/Makefile): SDL(1.2), SDL_mixer, libvorbis,
# libogg, freetype. Best-effort install; NON-FATAL so the image always builds.
# Any gaps surface loudly at link time. See README.md "Known risks" — SDL 1.2 vs
# SDL2 and FreeType version drift are the two most likely blockers here.
RUN (psp-pacman -Sy --noconfirm || true) \
 && (psp-pacman -S  --noconfirm sdl sdl_mixer libvorbis libogg freetype || true)

# The cldc host stage is 32-bit-host code:
#   - the prebuilt preverifier shipped in the tree is a 32-bit x86 ELF
#     (build/share/bin/linux_i386/preverify), selected by the host_os=linux patch;
#   - the generator tools romgen/loopgen cast target pointers to `int`, so they
#     must be compiled 32-bit or they fail on a 64-bit host (pointer truncation).
# Install multilib + i386 runtime libs, and wrap NATIVE gcc/g++ (-m32) and the
# assembler `as` (--32, for the generated i386 CPU stubs). This affects only
# native tools, used in this build solely for the host generators — the PSP
# target is built with psp-gcc/psp-as and is untouched.
RUN dpkg --add-architecture i386 \
 && apt-get update \
 && apt-get install -y --no-install-recommends g++-multilib zlib1g:i386 \
 && rm -rf /var/lib/apt/lists/* \
 && for t in gcc g++; do \
      printf '#!/bin/sh\nexec /usr/bin/%s -m32 "$@" -Wno-error\n' "$t" > /usr/local/bin/$t; \
      chmod +x /usr/local/bin/$t; \
    done \
 && printf '#!/bin/sh\nexec /usr/bin/as --32 "$@"\n' > /usr/local/bin/as \
 && chmod +x /usr/local/bin/as

COPY build.sh /usr/local/bin/pspkvm-build
RUN chmod +x /usr/local/bin/pspkvm-build

WORKDIR /work
# Mount the pspkvm source at /work/pspkvm and (optionally) patches at /work/patches.
ENTRYPOINT ["/usr/local/bin/pspkvm-build"]
