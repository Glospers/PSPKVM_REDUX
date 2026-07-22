# Investigation

Analysis of the PSPKVM build system and Nokia's M3G engine that shaped the plan
in [DESIGN.md](DESIGN.md). Sources examined at their pinned commits:

- PSPKVM @ `15b93ccb82048d4ae12510ef65666bc13c79c252` (vadosnaprimer, master)
- m3gcore @ `1b921b3ae476b27d7359083babcbfab81d6e532f` (toaarnio)

## 1. PSPKVM build system

- Driven by **GNU make + POSIX shell** (`build-psp-cldc.sh`); ~496 `.gmk` files,
  no Ant.
- Four modules build in order, then a manual package step:
  `javacall → pcsl → cldc → midp`, then `cd psp && make BUILD_SLIM=true` →
  `EBOOT.PBP`.
- `cldc` on MIPS is a **pure C interpreter, no JIT** (`javacall_psp_gcc.cfg`).
- `midp` emits a static `libmidp.a`; `psp/Makefile` links all four modules'
  archives into `pspkvm.elf` and packs the EBOOT via `pack-pbp` / `mksfo`.
- Expected environment: Cygwin/Linux, a JDK (`JDK_DIR`), pspsdk at
  `/usr/local/pspdev` (located via `psp-config`), plus SDL/SDL_mixer/libvorbis/
  libogg and a patched FreeType 2.3.9. The "make 3.80, not 3.81" requirement in
  `BUILDING.TXT` is a Cygwin/DOS-path constraint and does not apply on Linux.

### Native binding: KNI, statically registered

phoneME uses **KNI** (not JNI). Natives are declared
`KNIEXPORT KNI_RETURNTYPE_* KNIDECL(class_method)`
(`cldc/src/vm/share/natives/kni.h:66`) and bound through a **generated**
`NativesTable.cpp` — the JCC romizer emits it from `classes.zip`
(`cldc/src/tools/jcc/runtime/CLDC_HI_NativesWriter.java:364`). There is no dynamic
`registerNatives` and no `.kdef`. M3G natives therefore only need to follow the
`KNIDECL(javax_microedition_m3g_...)` naming convention; the JCC pass regenerates
the table.

### Where M3G wires in

The build already reserves hooks for JSR-184, all currently disabled, with no
implementation bundled:

- `USE_JSR_184 = false` — `midp/build/javacall_psp/Options.gmk:50`
- Build hook expecting `JSR_184_DIR/src/config/<subsystem>.gmk` —
  `midp/build/common/makefiles/Subsystems.gmk:347-352`
- Java init guarded by `ENABLE_JSR_184` →
  `com.sun.midp.jsr184.Initializer.init()` (`.../JSRInitializer.jpp:45`) —
  package absent
- The verify machinery references a `SWERVE_DIR` (Sun's own "Swerve" M3G
  engine) — the slot the original build reserved for a reference M3G, which this
  project fills with Nokia's engine.

## 2. m3gcore structure and backend split

- **Unity build:** `src/m3g_core.c` `#include`s every other `.c` (a single
  translation unit).
- **GL is not entangled with the scene graph** — the split is clean:
  - **Backend-agnostic (reusable as-is):** math, `.m3g` loader, node/group/world,
    mesh/morphing/skinned, transform/tcache, render queue, animation, keyframe,
    object/array/memory. ~18 of 34 `.c` files, including the two largest
    (`m3g_math.c`, `m3g_loader.c`).
  - **Backend-specific (GL ES 1.x fixed-function):** rendercontext, image,
    appearance, material, compositingmode, polygonmode, fog, light, lightmanager,
    camera, texture, background, sprite, vertexarray, vertexbuffer, indexbuffer.
- Platform abstraction is clean: an `M3Gparams` function-pointer struct passed to
  `m3gCreateInterface` (malloc/free/begin/endRender/error) plus four `m3ggl*`
  native-surface hooks (`inc/m3g_gl.h:87-101`). The two Symbian `.cpp` files are a
  working template for PSP glue (bitmap→surface, inflate, log, assert).

## Two findings that shaped the plan

### No software rasterizer {#no-software-rasterizer}

An obvious "slow but correct first" milestone would be to run the engine's own
software rasterizer before touching `sceGu`. This drop has none: it renders
**only** through GL ES 1.x + EGL. The alternative NGL ("Gerbera") software-GL
path is hard-`#error`'d out (`src/m3g_rendercontext.inl:25-26`, "This file is for
the OES API only") and its `ngl.h` / `.inl` files are absent. Consequently the
first working milestone is "the GL ES 1.x → `sceGu` shim renders, unoptimized",
not "software rasterizer renders".

### Missing public header {#missing-public-header}

`M3G/m3g_core.h` is included by four headers (`inc/m3g_gl.h:27`, `m3g_defs.h:28`,
`m3g_memory.h:31`, `m3g_image.h`), but no `M3G/` directory exists in the drop. It
defines `M3Gparams`, the handle typedefs, `M3GPixelFormat`, and every `M3G_API`
prototype — the entire host-facing contract. It must be reconstructed, or lifted
from a phoneME JSR-184 tree, before anything compiles against the engine.

## Licensing {#licensing}

The two upstreams are under **incompatible** licenses:

- **PSPKVM / phoneME:** GPL-2.0 *only* (`cldc/src/vm/share/natives/kni.h` header:
  "GNU General Public License version 2 only").
- **m3gcore:** Eclipse Public License v1.0 (`src/m3g_core.c` header).

EPL-1.0 and the GPL are recognized as mutually incompatible. Hosting both source
trees is fine (mere aggregation), but the plan statically links the M3G engine
into the GPL-2.0 binary — combining EPL and GPL-only code in one distributable
work, which the licenses do not permit. This affects **binary distribution**, not
private development or source hosting.

Open question, to resolve before any binary release: reimplement the small set of
engine pieces actually needed against a compatible license, seek relicensing, or
treat built binaries as non-distributable. Tracked here rather than resolved.
