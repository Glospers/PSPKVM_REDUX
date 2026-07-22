# Design

## Goal

Implement JSR-184 (Mobile 3D Graphics, "M3G") in PSPKVM so that M3G-based J2ME
games render on real PSP hardware. PSPKVM implements MIDP 2.0 and a range of JSRs
(75, 120, 172, 179, 226, Nokia UI, audio) but leaves JSR-184 as an unimplemented
`TODO`, so any MIDlet that initializes an M3G 3D scene hangs or crashes.

## Why it's tractable

- The PSP has hardware 3D through `sceGu`, a fixed-function pipeline roughly
  equivalent to OpenGL 1.x. It comfortably exceeds the GPU capabilities of the
  phones these games targeted.
- Nokia's M3G 1.1 reference engine is open source (`m3gcore`) and handles the
  scene graph, animation, `.m3g` parsing, and math. Its GL dependencies are
  confined to the render backend — the scene graph itself is backend-agnostic.
- phoneME already exposes a native bridge (KNI). Adding the M3G package is a
  matter of registering the native methods and wiring them to the engine.

## Constraints

- **Hardware:** MIPS R4000 CPU at 222/333 MHz; 32 MB RAM (PSP-1000) or 64 MB
  (2000/3000/Go); 480×272 screen; fixed-function GPU, no shaders.
- **`sceGu` vs GL:** differing coordinate/handedness conventions, matrix-stack
  semantics, power-of-two and swizzled texture formats, and a display-list model
  rather than immediate mode.
- **Toolchain:** modern pinned pspdev (MIPS GCC cross-compiler). The original
  build system assumed Cygwin + JDK 1.4.2; reproducing it in a modern container
  is the hardest part of Phase 0.
- **Target device profiles:** the games expect Sony Ericsson W810 (176×208) up
  through K800/W950 (240×320). Primary test title: Deep 3D: Submarine Odyssey.

## Architecture decisions

1. **Rendering backend: a GL ES 1.x fixed-function shim over `sceGu`.**
   Nokia's engine renders only through GL ES 1.x — this drop ships no software
   rasterizer (see [INVESTIGATION.md](INVESTIGATION.md#no-software-rasterizer)).
   Rather than rewrite the engine's backend from scratch, implement a small GL
   ES 1.x shim on top of `sceGu` and run the engine's existing backend on it.
   M3G only exercises fixed-function GL ES 1.x, so the surface to implement is
   bounded. This merges what were originally two separate phases (software
   rasterizer, then `sceGu`) into a single "shim, then optimize" effort.

2. **JDK 8, not 1.4.2.** The host JDK only compiles the class library to
   1.4-target bytecode and runs the phoneME romizer; the actual runtime on the
   PSP is the C interpreter, not the host JVM. Modern JDK 8 emits correct
   1.4-target bytecode and runs reliably on current glibc, whereas a 32-bit JDK
   1.4.2 barely starts on a modern system. Exact-fidelity 1.4.2 remains an
   opt-in fallback only.

3. **Pinned modern toolchain + source patches**, rather than reconstructing a
   period-accurate 2010 toolchain. Modifications to upstream live as discrete
   patches under `docker/patches/`, preserving a clean line back to upstream.

4. **Reconstruct `M3G/m3g_core.h`.** This public header — the entire host-facing
   contract for the engine — is missing from the m3gcore drop. It is sourced
   from a phoneME JSR-184 tree where possible, otherwise reconstructed. See
   [INVESTIGATION.md](INVESTIGATION.md#missing-public-header).

## Phase plan

| Phase | Scope | Deliverable |
|-------|-------|-------------|
| 0 | Reproducible build of unmodified PSPKVM | `docker run` produces an `EBOOT.PBP` behaving like the upstream 0.5.5 release |
| 1 | `javax.microedition.m3g` classes + KNI no-op native stubs; reconstruct `M3G/m3g_core.h` | Deep 3D boots past its hang into a black 3D viewport without crashing |
| 2 | Wire in the backend-agnostic core of the M3G engine | Scene graph, loader, and animation driven from the runtime |
| 3 | GL ES 1.x → `sceGu` rendering shim under the engine's backend | Deep 3D renders at a playable framerate (target 20+ fps) |
| 4 | Per-game compatibility pass | Additional M3G titles (Galaxy on Fire, Asphalt 3D, Rayman Kart, Splinter Cell 3D) fixed |

## Out of scope

- **JSR-239** (OpenGL ES bindings for J2ME) — different API, almost no J2ME game
  uses it.
- **M3G 2.0 (JSR-297)** — drafted but never widely adopted; no shipped games
  target it.
- **Rewriting phoneME** — treated as a fixed dependency.
