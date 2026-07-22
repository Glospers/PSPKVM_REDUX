# PSPKVM REDUX

Adding **Mobile 3D Graphics (JSR-184 / M3G)** support to **PSPKVM**, the
open-source Java ME (phoneME Feature / CLDC + MIDP) runtime for the Sony PSP.

PSPKVM lets the PSP run J2ME MIDlets, but it never implemented the M3G 3D API —
it's listed as an unimplemented `TODO` in the upstream source. Any J2ME game that
uses M3G for 3D rendering (Deep 3D: Submarine Odyssey, Galaxy on Fire, most
Gameloft 3D mobile titles of ~2006–2010) therefore hangs on its loading screen.
This project implements JSR-184 so those games run on real PSP hardware.

The PSP has a fixed-function 3D GPU (`sceGu`, roughly OpenGL 1.x-class) that is
more than capable of the workloads these phone-era games were designed for — so
the work is a porting problem, not a performance one.

## Status

Early. Phase 0 (a reproducible build of stock PSPKVM) is the current focus.

| Phase | Scope | Status |
|-------|-------|--------|
| 0 | Reproducible build of unmodified PSPKVM (Docker + CI) | Pipeline authored; first build pending |
| 1 | `javax.microedition.m3g` classes + native stubs; reconstruct `M3G/m3g_core.h` | Not started |
| 2 | Wire in the backend-agnostic core of Nokia's M3G engine | Not started |
| 3 | GL ES 1.x → `sceGu` rendering shim | Not started |
| 4 | Per-game compatibility pass | Not started |

See [docs/DESIGN.md](docs/DESIGN.md) for the full plan and the architecture
decisions, and [docs/INVESTIGATION.md](docs/INVESTIGATION.md) for the build-system
and engine analysis that shaped it.

## How it's structured

This repository is a **build-and-integration harness**, not a fork of the runtime.
The upstream sources are not vendored; they're fetched fresh at pinned commits and
modified through discrete patches, keeping a clean line back to upstream.

```
docker/               Reproducible build environment
  Dockerfile          Pinned pspdev toolchain + JDK; builds the EBOOT
  build.sh            In-container build driver
  patches/            Source patches applied to upstream before building
  README.md           Environment docs, pinned versions, known risks
.github/workflows/
  build.yml           CI: fetch pinned source -> build image -> produce EBOOT.PBP
docs/
  DESIGN.md           Goals, phased plan, locked architecture decisions
  INVESTIGATION.md    PSPKVM build system + M3G engine analysis, licensing
LICENSE               GPL-2.0
CREDITS.md            Upstream projects and authors
```

Upstream, fetched at build time:

| Source | Upstream | Pinned commit |
|--------|----------|---------------|
| PSPKVM (phoneME port) | [vadosnaprimer/pspkvm](https://github.com/vadosnaprimer/pspkvm) | `15b93ccb82048d4ae12510ef65666bc13c79c252` |
| M3G core engine | [toaarnio/m3gcore](https://github.com/toaarnio/m3gcore) | `1b921b3ae476b27d7359083babcbfab81d6e532f` |

## Building

Requires Docker. The image builds natively on both x86_64 and ARM64 (e.g. a
Raspberry Pi 5) — same commands, host-native toolchain selected automatically.

```sh
# 1. Fetch the pinned PSPKVM source
git clone --filter=blob:none https://github.com/vadosnaprimer/pspkvm.git pspkvm
git -C pspkvm checkout 15b93ccb82048d4ae12510ef65666bc13c79c252

# 2. Build the environment image (slow, cached)
docker build -t pspkvm-build ./docker

# 3. Build the EBOOT — source is mounted, output lands in pspkvm/psp/EBOOT.PBP
docker run --rm -v "$PWD/pspkvm:/work/pspkvm" \
                -v "$PWD/docker/patches:/work/patches:ro" \
                pspkvm-build
```

Output: `pspkvm/psp/EBOOT.PBP`. Full pinned versions, rationale, and the known
2010-source-vs-modern-toolchain risks are in [docker/README.md](docker/README.md).

## Licensing

This project builds on **phoneME / PSPKVM**, which is **GPL-2.0**, so this
repository is licensed **GPL-2.0** as well — see [LICENSE](LICENSE).

Note that Nokia's M3G engine (`m3gcore`) is licensed under the **Eclipse Public
License v1.0**, which is not compatible with the GPL. The two source trees can
coexist, but linking them into a single distributable binary is a licensing
conflict — this is tracked as an open question in
[docs/INVESTIGATION.md](docs/INVESTIGATION.md#licensing).

## Running homebrew on a PSP

PSPKVM is homebrew and will not run on stock retail firmware, which only executes
signed code. A PSP with 1.50 firmware or custom firmware is required, as with all
PSP homebrew.
