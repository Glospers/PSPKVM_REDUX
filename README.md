# PSPKVM REDUX 3D

Java ME for the PSP, with the 3D API actually implemented.

PSPKVM is an old open-source J2ME runtime for the PSP. It runs ordinary MIDlets
fine, but it never implemented JSR-184 (M3G) — the 3D graphics API — so every
3D phone game of the mid-2000s just hangs on its loading screen. That's the gap
this fills.

The PSP's GPU is fixed-function and roughly OpenGL 1.x class, which is far more
than these phone games ever asked for. So this was never a performance problem;
it was a plumbing problem.

## What works

Deep 3D: Submarine Odyssey plays from the menu through to gameplay — the
station, the sub, the HUD, music, sound effects, saving and loading. Tested on
**PPSSPP** and on a **real PSP-2000 running ARK-4 CFW**, which behave the same.

That's the honest scope of testing: **one game, one console model.** Other M3G
titles may work, partly work, or not start. If you try one, I'd like to hear
either way.

## Known limitations

Read this before opening an issue.

**Frame rate is low.** Around 15 fps in Deep 3D on the emulator, ~12 on real
hardware. It is also quantised: the screen flush waits for vblank, so you only
ever get 60/N — 30, 20, 15, 12, and nothing in between. Getting to the next step
means fitting the whole frame under 50 ms. A large part of what's left is the
Java bytecode interpreter, and this VM has no JIT for MIPS (the compiler backend
in the phoneME source is empty stubs), so desktop emulators with a JIT will
always be smoother. Work continues, but 30 fps is unlikely.

**AMR sound effects need converting.** AMR-NB is a speech codec the PSP has no
decoder for — `sceAudiocodec` does ATRAC3, MP3 and AAC and nothing else. Games
that use `.amr` for effects will be silent until the JAR's audio is converted to
WAVE with [`tools/amr-to-wav.sh`](tools/amr-to-wav.sh). Music (MIDI) plays
natively and needs nothing. Teaching the runtime to decode AMR itself is still
open.

**Widescreen is a bad idea for Deep 3D.** At 480x272 the game asks for a much
wider view than its sky geometry covers, so you see past the edge of the sky and
the 2D backdrop shows through at the screen edges. It isn't a rendering bug —
the game was never drawn for that shape, and KEmulator doesn't even offer it.
Use a 240x320 profile.

**There is a coloured band along the bottom edge on real hardware.** It shows up
clearly on dark scenes, like the intro cutscene. Not yet fixed; not present on
the emulator.

**Soft keys need the right device profile.** Deep 3D reads Nokia key codes, so
pick a Nokia profile or its menu keys do nothing.

## Install

Assumes a PSP already running custom firmware (PRO, ME, ARK-4 — any modern CFW).

1. Download `PSPKVM-REDUX-3D-alpha.zip` from
   [Releases](https://github.com/Glospers/PSPKVM_REDUX/releases).
2. Unzip it. You'll get a `PSP` folder.
3. Plug the PSP in over USB, or put its memory stick in a card reader.
4. Copy the `PSP` folder onto the **root** of the memory stick and let it merge.
   You should end up with `ms0:/PSP/GAME/PSPKVM/EBOOT.PBP`.
5. Copy your game's `.jar` (and `.jad`, if it has one) anywhere on the stick —
   `ms0:/PSP/` is a good spot.
6. Eject properly, then launch **PSPKVM REDUX 3D** from the XMB under
   Game → Memory Stick.

To install a game, once it's running:

7. Choose **Install/Remove MIDlet** → **Install from memory stick**, then pick
   your `.jar`.
8. Before launching a 3D game, highlight it and open the menu → **Select
   device**. Pick a **240x320** profile — Nokia if the game uses soft keys.
9. Launch it.

Updating later: replace `EBOOT.PBP` only. Everything else — and especially
`appdb`, which holds your installed games and save files — should be left alone.

## If something goes wrong

The runtime writes a log to `ms0:/pspkvm_vm.log` (the root of the stick, not the
game folder). If it crashes, exits to the XMB, or misbehaves, that file is the
first thing worth looking at, and the first thing I'd ask for.

## How this repo is put together

It's a build harness, not a fork. Upstream sources are fetched at pinned commits
and modified through discrete patches, so the line back to upstream stays clean.

```
docker/           Reproducible build environment
  Dockerfile      Pinned toolchain + JDK
  build.sh        In-container build driver
  patches/        Patches applied to the PSPKVM source
  patches-pspgl/  Patches applied to pspgl, which is rebuilt from source
jsr184/           javax.microedition.m3g classes and their native methods
m3g/              The port: public header, PSP platform layer, and the
                  corrections applied to the GL and EGL calls the engine makes
tools/            amr-to-wav.sh — converts a MIDlet's AMR effects to WAVE
docs/             Design notes and the original build/engine investigation
upstream/         Pinned source clones, fetched not vendored (not in git)
```

| Source | Upstream | Pinned commit |
|--------|----------|---------------|
| PSPKVM (phoneME port) | [vadosnaprimer/pspkvm](https://github.com/vadosnaprimer/pspkvm) | `15b93ccb82048d4ae12510ef65666bc13c79c252` |
| M3G engine | [toaarnio/m3gcore](https://github.com/toaarnio/m3gcore) | `1b921b3ae476b27d7359083babcbfab81d6e532f` |
| pspgl (GL ES over sceGu) | [pspdev/pspgl](https://github.com/pspdev/pspgl) | `de4260adf56d06516ec46018d404ca77e0b61748` |

## Building it yourself

Needs Docker, nothing else.

```sh
git clone --filter=blob:none https://github.com/vadosnaprimer/pspkvm.git upstream/pspkvm
git -C upstream/pspkvm checkout 15b93ccb82048d4ae12510ef65666bc13c79c252
git clone --filter=blob:none https://github.com/toaarnio/m3gcore.git upstream/m3gcore
git -C upstream/m3gcore checkout 1b921b3ae476b27d7359083babcbfab81d6e532f
git clone --filter=blob:none https://github.com/pspdev/pspgl.git upstream/pspgl
git -C upstream/pspgl checkout de4260adf56d06516ec46018d404ca77e0b61748

docker build -t pspkvm-build ./docker

docker run --rm \
  -v "$PWD/upstream/pspkvm:/work/pspkvm" \
  -v "$PWD/docker/patches:/work/patches:ro" \
  -v "$PWD/docker/patches-pspgl:/work/patches-pspgl:ro" \
  -v "$PWD/jsr184:/work/components/jsr184:ro" \
  -v "$PWD/m3g:/work/components/m3g:ro" \
  -v "$PWD/upstream/m3gcore:/work/m3gcore:ro" \
  -v "$PWD/upstream/pspgl:/work/pspgl:ro" \
  pspkvm-build
```

The EBOOT lands at `upstream/pspkvm/psp/EBOOT.PBP`. CI runs the same build on
every push.
Pinned versions and the known 2010-source-versus-modern-toolchain hazards are in
[docker/README.md](docker/README.md).

## A note on the 3D engine

The M3G implementation is Nokia's own engine (`m3gcore`, EPL-1.0), which is
fetched separately and never modified. It renders through GL ES 1.x, which on
the PSP means pspgl over `sceGu` — also built from source here, because two
genuine bugs in its matrix handling had to be fixed to get correct output.

Note the licence boundary: phoneME/PSPKVM is GPL-2.0 and m3gcore is EPL-1.0.
Keeping both source trees side by side is fine, and building for yourself is
fine; the two licences are not compatible for redistributing a single linked
binary, which is worth knowing if you plan to ship builds.

## Licence

GPL-2.0, inherited from phoneME/PSPKVM — see [LICENSE](LICENSE).
Credits to the upstream projects are in [CREDITS.md](CREDITS.md).
