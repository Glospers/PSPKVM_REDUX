# runtime/

Files that replace something in the PSPKVM install package because the
upstream version is wrong. They are not built; they are copied into
`PSP/GAME/PSPKVM/` when a release is assembled.

## timidity.cfg

Upstream PSPKVM ships a Timidity configuration that references **41 patch
files it does not include** — the whole `drums/` directory, plus a handful of
cymbal and hi-hat patches. Only the 16 melodic patches under `inst/` are
actually present.

Standalone TiMidity++ tolerates this: it complains about each missing
instrument and renders the rest, so the melody plays and percussion is
missing. SDL_mixer's built-in Timidity is a different implementation, and on
a stock install music was silent altogether.

The configuration here maps every program to a patch that is genuinely
bundled — percussion to `noise.pat`, or `wood.pat` for the woodblock and
stick sounds — so nothing dangles. It is a poor General MIDI set and it is
meant to be: the point is that a fresh install makes sound at all.

For music that sounds right, drop a full General MIDI patch set into
`PSP/GAME/PSPKVM/gm/` and point `timidity.cfg` at it instead:

```
dir ms0:/PSP/GAME/PSPKVM/gm
source gravis.cfg
source gsdrums.cfg
```

That is a large improvement and is what the development machine runs. Such a
set is tens of megabytes and is not this project's to redistribute, which is
why the bundled fallback exists.
