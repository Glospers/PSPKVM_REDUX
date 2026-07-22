# Source patches

Drop unified-diff `*.patch` files here (created against the pspkvm tree root,
e.g. `git diff > docker/patches/0001-fix-foo.patch`). `build.sh` applies every
`*.patch` in this directory to the mounted source before building, in sorted
order — so name them `0001-...`, `0002-...`.

These are expected: the pinned modern pspdev toolchain (~GCC 14) is far newer
than the GCC ~4.3 the 2010 pspkvm source assumes, so compile-error fixes will
accumulate here. Keeping them as discrete patches (rather than editing the
mirrored source) preserves a clean, reproducible line back to upstream.
