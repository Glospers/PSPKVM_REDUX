# PSPKVM Phase 0 — reproducible build environment

A Docker image that builds the PSPKVM `EBOOT.PBP` from source, plus a GitHub
Actions workflow (`.github/workflows/build.yml`) that runs it in CI. This is the
Phase 0 deliverable from `HANDOFF.md`: get the existing source building on a
modern system before touching M3G.

## Usage

```sh
# 1. Build the environment image (slow, cached — rebuild only when pins change)
docker build -t pspkvm-build ./docker

# 2. Get the pinned source (or use your working checkout)
git clone https://github.com/vadosnaprimer/pspkvm.git src
git -C src checkout 15b93ccb82048d4ae12510ef65666bc13c79c252

# 3. Build the EBOOT.PBP — source is mounted, output lands in src/psp/EBOOT.PBP
docker run --rm -v "$PWD/src:/work/pspkvm" pspkvm-build
```

The image holds only the **environment**; the source is bind-mounted at
`/work/pspkvm` so you can iterate on source/patches without rebuilding the image.

## Pinned versions

| Component | Pin | Why this pin |
|---|---|---|
| Base image | `ubuntu:24.04` | Matches the host the pspdev Ubuntu tarballs are built on, so their glibc-linked binaries run. |
| pspdev toolchain | `v20260701` (arch-selected asset) | Prebuilt glibc/Ubuntu build → `/usr/local/pspdev` (where the source hardcodes it). *Not* the Alpine/musl Docker image, whose binaries won't run on glibc. |
| JDK | Eclipse Temurin **8u462-b08** (Adoptium versioned API, arch-selected) | See "Why JDK 8, not 1.4.2" below. |

## Host architecture (x86_64 and ARM64 / Raspberry Pi 5)

The image builds natively on both **amd64** and **arm64** hosts — no edits, same
`docker build`. `psp-gcc` cross-compiles to PSP MIPS regardless of host CPU; the
Dockerfile just selects host-native pspdev + JDK builds via BuildKit's
`TARGETARCH` (`amd64` → `pspdev-ubuntu-latest-x86_64` + Temurin `x64`; `arm64` →
`pspdev-ubuntu-24.04-arm-arm64` + Temurin `aarch64`). So a Raspberry Pi 5 (arm64,
Docker installed) builds the same EBOOT as an x86_64 PC. Docker Desktop on the PC
reports `linux/amd64`; Docker on Pi OS reports `linux/arm64` — both are covered.
| GNU make | distro **4.x** | The "must be 3.80" rule is a Cygwin/DOS-path constraint that doesn't apply on Linux. |
| pspkvm source | `15b93ccb82048d4ae12510ef65666bc13c79c252` | vadosnaprimer/pspkvm @ master. |

### Why JDK 8, not 1.4.2 (which `BUILDING.TXT` specifies)

The host JDK only **compiles the class library to 1.4-target bytecode** and runs
the phoneME romizer/jcc; the actual Java runtime on the PSP is the C interpreter
in `cldc`, not the host JVM. So a modern JDK emitting `-source/-target 1.4`
bytecode reproduces upstream behavior. Meanwhile a 32-bit JDK 1.4.2 barely runs
on modern glibc (the classic NPTL/TLS startup failure). JDK 8 is the last
release whose `javac` still accepts `-source 1.4`, so it's the sweet spot of
"runs reliably" + "emits the right bytecode".

If exact-fidelity 1.4.2 is ever required, add a `jdk` build stage that vendors
`j2sdk-1_4_2_19-linux-i586` (Internet Archive) and repoint `JDK_DIR` — but
expect to fight glibc. Not recommended.

## Known risks (ranked — each is plausibly a multi-hour blocker)

1. **Toolchain era gap.** pspkvm is 2010-era (GCC ~4.3); the pinned pspdev
   toolchain is ~GCC 14. Expect a batch of compile-error fixes (implicit
   declarations, `-fcommon`, stricter C). Land them as `docker/patches/*.patch`.
   Fallback: build the historic ps2dev toolchain instead.
2. **SDL 1.2 vs SDL2.** `psp/Makefile` links `-lSDL -lSDL_mixer` (SDL 1.2). Modern
   pspdev centers on SDL2. If the SDL 1.2 port is unavailable, either add it or
   shim the handful of SDL calls in `psp/pspkvm.c`.
3. **FreeType version drift.** `BUILDING.TXT` wanted FreeType **2.3.9**; the
   toolchain now ships a much newer FreeType with API changes. pspkvm's font
   code may need adjusting (the old `tools/freetype_239_patch` is now moot).
4. **`javac` 8 vs the old sources / build flags.** JDK 8 warns on (or, for a few
   removed flags, rejects) 1.4-isms. Bounded and fixable, unlike (1)–(3).

`build.sh` prints the resolved `javac` / `psp-gcc` / `make` versions at the top
of every run so drift is visible in logs.

## What "done" looks like for Phase 0

`docker run ... pspkvm-build` exits 0 and produces `src/psp/EBOOT.PBP`, byte-for-
behavior comparable to the upstream 0.5.5 release. Only then do we start Phase 1
(M3G stubs).
