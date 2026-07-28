# pspgl as an OpenGL ES 1.x backend for m3gcore — findings

Standalone de-risking exercise. Nothing here is wired into the VM; it exists to
answer whether GL ES 1.x over `sceGu`/`sceGe` is a viable substrate for the M3G
render backend before that work is committed to.

**Verdict: yes, it works, and the gap is small.** pspgl builds, links, and
renders under `psp-gcc 15.2.0`; the ten missing GL entry points and six missing
EGL ones are all shallow. The real integration risk is not the API surface, it
is VRAM ownership — see §4.

---

## 1. What is in the image

| | |
|---|---|
| Package | `pspgl r12-7`, https://github.com/pspdev/pspgl |
| Library | `/usr/local/pspdev/psp/lib/libGL.a` — 137 objects, 199 exported `gl*`/`egl*` symbols (178 GL, 21 EGL) |
| Headers | `/usr/local/pspdev/psp/include/GLES/{gl.h,egl.h,egltypes.h,glut.h}` — no `EGL/` directory; EGL lives under `GLES/` |
| Reports itself as | `GL_RENDERER = "OpenGL ES-CM 1.1"` |
| Licence | **BSD-3-Clause** |
| Link deps | `-lGL -lpspvfpu`, plus `sceGe*` / `sceDisplay*` / `sceRtc*` / `sceKernelDcache*` |

The headers are the stock Khronos GL ES 1.1 ones, so **every** function below is
*declared* — only the implementation is absent. A missing entry point therefore
shows up as an undefined reference at link time, never as a compile error. That
makes the gap trivially plugged from outside the library.

Two things worth flagging up front:

- **pspgl does not use `libpspgu`.** It emits GE display lists itself and
  submits them with `sceGeListEnQueue`/`sceGeListSync`. It shares no state with
  `sceGu*` — no duplicate symbols, and the two link together cleanly.
- **BSD-3-Clause is GPL-2.0 compatible**, unlike m3gcore's EPL-1.0. Linking
  pspgl into the EBOOT adds no new licence problem, and its source can be
  vendored and patched if that turns out to be the cleanest route.

## 2. Does it build and render? Yes

`main.c` here draws a spinning, per-face-coloured cube plus a 2D bar, packaged
as `EBOOT.PBP` and installed at
`ppsspp_win/memstick/PSP/GAME/PSPGLTEST/EBOOT.PBP`. Built clean, no warnings.
Confirmed rendering correctly in PPSSPP 1.20.4 at a vsync-locked ~60 fps
(1030 `sceGeListEnQueue` + `sceDisplaySetFrameBuf` pairs in ~17 s).

Init path is plain EGL 1.0, no pspgl-specific entry point required:

```
eglGetDisplay(EGL_DEFAULT_DISPLAY) -> eglInitialize -> eglChooseConfig
  -> eglCreateContext -> eglCreateWindowSurface(dpy, cfg, 0, NULL)
  -> eglMakeCurrent -> ... -> eglSwapBuffers
```

The demo deliberately routes its clear colour, its 2D projection and its flat
colour through the *shimmed* entry points (`glClearColorx`, `glClearDepthx`,
`glFogxv`, `glOrthox`, `glColor4x`, `glHint`, `glActiveTexture`,
`glClientActiveTexture`, `eglBindAPI`, `eglQuerySurface`), so a correct picture
proves the shim approach end to end, not merely that pspgl works.

## 3. The gap for m3gcore — verified

m3gcore's requirement is the declaration set in `m3g/inc/GLES/gl.h` (63
functions) and `m3g/inc/EGL/egl.h` (21 functions). Diffed against `psp-nm` on
`libGL.a`:

**53 of 63 GL and 15 of 21 EGL are already implemented.** Missing:

### GL — 10 functions

| Function | Fix | Difficulty |
|---|---|---|
| `glColor4x` | → `glColor4f`, 16.16 → float | trivial |
| `glClearColorx` | → `glClearColor` | trivial |
| `glClearDepthx` | → `glClearDepthf` | trivial |
| `glOrthox` | → `glOrthof` | trivial |
| `glFogxv` | → `glFogfv`; 4 components for `GL_FOG_COLOR`, 1 otherwise, and `GL_FOG_MODE` is an enum not a 16.16 scalar | trivial, one `switch` |
| `glTexEnvx` | → `glTexEnvf`; only `GL_RGB_SCALE`/`GL_ALPHA_SCALE` are genuinely fixed-point, the rest are enums | trivial, one `switch` |
| `glTexParameterx` | → `glTexParameteri`; every ES 1.x pname here is an enum/boolean, so the value passes as an integer, **not** as 16.16 | trivial — but easy to get wrong by scaling it |
| `glActiveTexture` | no-op — the GE has one texture unit and pspgl exposes one | trivial |
| `glClientActiveTexture` | no-op, same reason | trivial |
| `glHint` | no-op — hints are advisory and the GE has no equivalent control | trivial |

All ten are implemented in `gles1x_shim.c` (86 lines) and proven working.

### EGL — 6 functions

| Function | Fix | Difficulty |
|---|---|---|
| `eglBindAPI` | return TRUE for `EGL_OPENGL_ES_API` | trivial |
| `eglQuerySurface` | `EGL_WIDTH`/`EGL_HEIGHT` — but pspgl keeps surface dimensions in file-static structs we cannot reach, so an out-of-library shim has to hardcode 480×272 | trivial for a window surface, **wrong for pbuffers** — see below |
| `eglQueryContext` | report client version 1 | trivial |
| `eglGetCurrentDisplay` | pspgl has exactly one display and ignores `eglGetDisplay`'s argument | trivial |
| `eglCopyBuffers` | return FALSE — no native pixmaps on PSP | trivial |
| `eglCreatePixmapSurface` | return `EGL_NO_SURFACE` — ditto | trivial |

`eglQuerySurface` is the only one that is not honestly implementable from
outside the library, because m3gcore calls it on **pbuffer** surfaces whose
dimensions it chose itself. Two clean ways out: track the dimensions in the M3G
backend (it created the surface, so it knows them), or vendor pspgl and add the
real accessor. The former is a handful of lines.

### Not a gap, but relevant

- **`eglCreatePbufferSurface` is present and real** (allocates via
  `__pspgl_buffer_new` against `__pspgl_pixconfigs`). This matters: m3gcore's
  `SURFACE_IMAGE` path — rendering into an `Image2D`, and by extension the
  route to getting 3D into MIDP's 16-bit screen buffer — goes through
  pbuffer + `glReadPixels` (`m3gcore/src/m3g_rendercontext.inl:361`,
  `:1084`; `m3g_image.inl:318`). Both halves exist.
- `eglCreatePixmapSurface` + `eglCopyBuffers` are m3gcore's `SURFACE_BITMAP`
  path only. It is optional; the pbuffer path covers the same ground.
- **Type compatibility.** `m3g/inc/GLES/gl.h` typedefs match pspgl's exactly.
  `m3g/inc/EGL/egl.h` differs cosmetically (`EGLConfig` is `void*` vs pspgl's
  `int`, `EGLBoolean` is `unsigned` vs `int`) but every one of them is 32 bits,
  so the o32 ABI is identical. The two headers must not be included in the same
  translation unit; in practice m3gcore compiles against its own and links
  against pspgl's objects, which is fine.

## 4. Coexistence with PSPKVM's `sceGu` usage — the actual risk

PSPKVM drives the GE itself: `sceGuInit()` and the buffer setup at
`pspkvm/psp/pspkvm.c:364-392`, and a textured-quad blit of the MIDP screen
every flush at
`pspkvm/javacall/implementation/psp_mips/midp/lcd.c:121-138`, ending in
`sceGuSwapBuffers()`.

`coexist/` reproduces that setup exactly, brings pspgl up on top of the live
`sceGu` context, and then interleaves a GL frame and a `sceGu` frame
indefinitely. Installed at `PSP/GAME/PSPGLCOEX/`. Results:

**What works:**

- pspgl initialises fine after `sceGuInit()`. `eglInitialize`,
  `eglCreateContext`, `eglCreateWindowSurface` and `eglMakeCurrent` all
  succeed (`eglGetError() == EGL_SUCCESS`) with the `sceGu` context live.
- 600+ interleaved frames with no crash, no GE hang, no error.
- **pspgl re-emits its full GE context per display list**, so another `sceGu`
  client changing hardware state does not corrupt it. Demonstrated: a single
  `glShadeModel(GL_SMOOTH)` at init survives an unbounded number of intervening
  `sceGuStart(GU_DIRECT)` state blocks — the triangle stays smooth-shaded.
  (Without that call it renders flat, i.e. **pspgl's default shade model is
  flat, not the `GL_SMOOTH` the ES 1.1 spec mandates** — a small conformance
  deviation worth knowing about, since m3gcore may rely on the default.)
- The VFPU is not a problem. pspgl uses it (`pspvfpu_initcontext`,
  `pspvfpu_use_matrices`) and PSPKVM's Java thread is already created with
  `PSP_THREAD_ATTR_VFPU` (`pspkvm/psp/pspkvm.c:708`).

**What does not work — VRAM ownership:**

pspgl has its own edram allocator (`__pspgl_vidmem_alloc` /
`_free` / `_evict` / `_compact` in `pspgl_vidmem.o`) keyed off
`sceGeEdramGetAddr()`, and it starts at offset 0. PSPKVM's
`getStaticVramBuffer` allocator (`pspkvm/psp/vram.c`) also starts at offset 0.
Neither knows the other exists. From the PPSSPP trace of the coexistence test:

```
COEX: sceGu buffers fbp0=+0x000000 fbp1=+0x088000 zbp=+0x110000  (used 0x154000 of 0x200000)
sceDisplaySetFrameBuf(04000000, 512, 3, 0)   x334   <- pspgl
sceDisplaySetFrameBuf(04088000, 512, 3, 0)   x333   <- pspgl
sceDisplaySetFrameBuf(04000000, 512, 3, 1)   x320   <- sceGuSwapBuffers
sceDisplaySetFrameBuf(04088000, 512, 3, 1)   x320   <- sceGuSwapBuffers
```

Both are using the **same two framebuffers**, and both call
`sceDisplaySetFrameBuf` to claim the scanout. It only looked harmless because
the sizes and allocation order happened to coincide (512×272×4 twice, then a
depth buffer) and both were clearing the whole screen every frame. It is
coincidence, not cooperation. Consequences for a real integration:

1. Every M3G frame overwrites the MIDP UI and vice versa. Composition is not
   possible in this arrangement.
2. `__pspgl_vidmem_evict` / `__pspgl_vidmem_compact` will *relocate* pspgl's
   texture allocations under pressure, and can only reason about its own map —
   so it can move data onto PSPKVM's buffers.
3. Any change to PSPKVM's VRAM layout (different pixel format, an extra buffer,
   VRAM-resident textures) turns the coincidence into corruption.

The only VRAM PSPKVM claims today is those three buffers, `0x000000`–`0x154000`
of the 2 MB edram; its UI texture (`swizzled_pixels`, `lcd.c:69`) is a static
array in main RAM. So ~704 KB of edram is free, and a partition is feasible —
but pspgl offers no API to relocate its heap base, so imposing one means either
vendoring pspgl (BSD-3, so this is allowed and easy) or bringing pspgl up
*first* and letting PSPKVM allocate after it.

**Threading:** pspgl has a single global context (`__pspgl_curctx`, common
`B` symbol) and there is not one mutex, semaphore or lock symbol in the entire
archive. It is strictly single-threaded — all GL calls must come from one
thread. That is workable (PSPKVM's Java thread is the natural home) but it is a
constraint, not a detail.

## 5. Recommendation

**Extend pspgl. Do not write a direct `sceGu` backend for m3gcore.**

- The API gap is 16 functions, all but one of which are three-line wrappers,
  and they are already written and proven here. A `sceGu` backend means
  implementing the whole fixed-function pipeline m3gcore drives — matrix stack,
  lighting, materials, fog, texture environment, vertex arrays with arbitrary
  formats and strides, `glReadPixels` — from scratch. That is the difference
  between an afternoon and several weeks.
- pspgl is BSD-3-Clause, so it can be vendored into the tree and patched
  directly. That removes the two things a pure out-of-library shim cannot fix:
  a truthful `eglQuerySurface`, and a settable VRAM heap base. Both are small,
  local changes to a library we are allowed to fork.
- pspgl already solves the hard parts a hand-written backend would have to
  redo: GE display-list construction and pinning, VRAM allocation with eviction
  and compaction, texture swizzling, VFPU matrix handling, and vsync/swap.
- It reports `OpenGL ES-CM 1.1`, which is what m3gcore's `M3G_GL_ES_1_1`
  configuration expects (`m3gcore/inc/m3g_defs.h:489`).

Sequencing suggestion:

1. Land the shim (`gles1x_shim.c` + the EGL half) as-is — that alone takes the
   ~65-entry-point no-op stub layer to a real renderer.
2. Solve VRAM ownership before wiring anything into the MIDP display path. The
   cheapest experiment is to bring pspgl up before `sceGuInit()` and have
   `getStaticVramBuffer` start from `__pspgl_vidmem_avail`'s watermark; if that
   is awkward, vendor pspgl and add a base-offset setter.
3. Target m3gcore's pbuffer + `glReadPixels` path for `Graphics3D.bindTarget`,
   not the window surface. It sidesteps the scanout conflict entirely: M3G
   renders offscreen, reads back into MIDP's 16-bit screen buffer, and PSPKVM's
   existing `lcd.c` blit puts it on screen unchanged. Slower, but it composes
   correctly with the 2D UI, which the window-surface route cannot.

## Files

| | |
|---|---|
| `main.c`, `Makefile`, `build.sh` | the spinning-cube proof; `EBOOT.PBP` → `PSP/GAME/PSPGLTEST/` |
| `gles1x_shim.c` | the 10 missing GL entry points |
| `egl_shim.c`, `pspgl_shims.h` | the 6 missing EGL entry points |
| `coexist/` | the pspgl-alongside-`sceGu` experiment; `EBOOT.PBP` → `PSP/GAME/PSPGLCOEX/` |

Build: `./build.sh`, or
`docker run --rm --entrypoint bash -v <dir>:/work pspkvm-build -c 'cd /work && make'`.
