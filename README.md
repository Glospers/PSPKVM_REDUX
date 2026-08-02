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

**M3G games run.** Deep 3D: Submarine Odyssey plays start to finish — menus, the
station, gameplay, HUD, music and sound effects — on PPSSPP and on real PSP-2000
hardware, which behave identically.

| Phase | Scope | Status |
|-------|-------|--------|
| 0 | Reproducible build of unmodified PSPKVM (Docker + CI) | Done — boots to the app manager; installs, runs and removes MIDlets |
| 1 | `javax.microedition.m3g` classes + native stubs; reconstruct `M3G/m3g_core.h` | Done — classes load; the header is reconstructed and verified |
| 2 | Wire in the backend-agnostic core of Nokia's M3G engine | Done — scenes load, animate and render |
| 3 | GL ES 1.x → `sceGu` rendering shim | Done — via pspgl, built from source with fixes |
| 4 | Per-game compatibility pass | In progress — frame rate and edge cases |

Getting here took a long run of fixes for behaviour that changed in the toolchain
since 2010 — `lseek`, `fstat`, `access` and `readdir` all misreport on the current
newlib, so the file and directory layers call the PSP kernel directly — and then a
second run of fixes in the graphics stack itself. The ones worth naming:

- **pspgl is built from source** rather than used as the toolchain's prebuilt
  `libGL.a`, because the texture matrix could not be kept coherent from outside
  it. Its lazy matrix cache skips the per-draw compensation it applies for
  integral texture coordinates, so consecutive draws inherited each other's
  texture transforms. The texture stack now bypasses that cache entirely.
- **Register-cache corruption.** This port re-marks pspgl's cached GE registers
  dirty after every context switch, because PSPKVM drives the same hardware for
  its own 2D. Marking *all* of them re-issued the recorded draw command, so every
  mid-frame context switch replayed the previous draw with the next one's
  matrices. It now uses pspgl's own mask, which deliberately excludes the
  trigger registers.
- **Frame delivery costs three CPU passes fewer.** The finished frame is already
  in the screen's own pixel layout, so it is copied straight out of the read-back
  cache instead of being expanded to RGBA8, copied by the engine, and packed back
  down again.
- **Sound works**: the `.amr` effects are transcoded at install time, and the
  MIDI path needed both a `$gp` fix for the small-data-addressed SDL_mixer and a
  full General MIDI patch set.

Frame rate is the current work. It is quantised by the vblank wait in the screen
flush, so the achievable rates are 60/N — the renderer's remaining cost is being
cut to move it up a step.

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
  patches-pspgl/      Fixes applied to pspgl, which is rebuilt from source
  README.md           Environment docs, pinned versions, known risks
jsr184/               javax.microedition.m3g classes and their KNI natives
m3g/                  The port itself: reconstructed public header, PSP
                      platform layer, and the corrections applied to the
                      GL and EGL calls the engine makes
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
| pspgl (GL ES over sceGu) | [pspdev/pspgl](https://github.com/pspdev/pspgl) | `de4260adf56d06516ec46018d404ca77e0b61748` |

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
