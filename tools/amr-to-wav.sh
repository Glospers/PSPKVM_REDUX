#!/usr/bin/env bash
###############################################################################
# amr-to-wav.sh -- make a MIDlet's AMR sound effects playable on this runtime.
#
#   tools/amr-to-wav.sh <suite.jar> [output.jar]
#
# Rewrites every .amr entry in the JAR as a RIFF/WAVE file of the same name.
# With no output path the JAR is converted in place (the original is kept
# alongside as <name>.orig).
#
# WHY THIS EXISTS
#
# AMR-NB is a 3GPP speech codec that phones of the era decoded in hardware.
# The PSP has no AMR decoder -- sceAudiocodec offers ATRAC3, MP3 and AAC and
# nothing else -- and neither MIDP's media layer nor SDL_mixer can read it, so
# a player created for an AMR resource returns nothing and the effect is
# silent. The runtime already routes audio/amr through the WAVE path
# (docker/patches/0055), which is what makes this conversion sufficient: the
# entries keep their .amr names, so the MIDlet's own resource lookups are
# untouched, and only the bytes behind them change.
#
# That is a per-title data fix, not a runtime capability. A JAR downloaded
# fresh from anywhere still contains AMR and will still be silent until it is
# run through this script. Adding real AMR decoding to the runtime is the
# proper fix and is a separate piece of work -- note that opencore-amr is
# Apache-2.0, which cannot be linked into a GPL-2.0-only binary; an
# LGPL-licensed decoder (such as the one in FFmpeg) can.
#
# Requires ffmpeg on PATH. 11025 Hz mono 16-bit matches what the mixer wants
# without inflating the JAR: the sources are 8 kHz speech-band recordings, so
# nothing is gained by going higher, and every doubling costs memory at load
# time (SDL_mixer expands each clip to the output format in RAM).
#
# This program is free software; you can redistribute it and/or modify it
# under the terms of the GNU General Public License version 2 only, as
# published by the Free Software Foundation.
###############################################################################
set -euo pipefail

RATE=11025

usage() { echo "usage: $0 <suite.jar> [output.jar]" >&2; exit 2; }
[ $# -ge 1 ] || usage

SRC=$1
OUT=${2:-}

[ -f "$SRC" ] || { echo "no such file: $SRC" >&2; exit 1; }
command -v ffmpeg >/dev/null || { echo "ffmpeg not found on PATH" >&2; exit 1; }
command -v zip    >/dev/null || { echo "zip not found on PATH" >&2; exit 1; }

WORK=$(mktemp -d)
trap 'rm -rf "$WORK"' EXIT

if [ -z "$OUT" ]; then
  [ -f "$SRC.orig" ] || cp -p "$SRC" "$SRC.orig"
  OUT=$SRC
else
  cp -p "$SRC" "$OUT"
fi

# zip is run from inside the scratch directory, so the archive it updates has
# to be named absolutely.
OUT_ABS=$(cd "$(dirname "$OUT")" && pwd)/$(basename "$OUT")

mapfile -t ENTRIES < <(unzip -Z1 "$SRC" | grep -i '\.amr$' || true)

if [ ${#ENTRIES[@]} -eq 0 ]; then
  echo "no .amr entries in $(basename "$SRC") -- nothing to do"
  exit 0
fi

echo "converting ${#ENTRIES[@]} AMR entries at ${RATE} Hz mono 16-bit"

converted=0
for e in "${ENTRIES[@]}"; do
  rm -rf "$WORK/x"; mkdir -p "$WORK/x"
  ( cd "$WORK/x" && unzip -qo "$SRC" "$e" )

  # Already converted by an earlier run? RIFF means it is a WAVE already.
  if head -c 4 "$WORK/x/$e" | grep -q RIFF; then
    echo "  skip (already WAVE): $e"
    continue
  fi

  if ! ffmpeg -v error -y -i "$WORK/x/$e" \
        -ar "$RATE" -ac 1 -acodec pcm_s16le -f wav "$WORK/x/$e.wav" 2>/dev/null; then
    echo "  FAILED to decode, left as-is: $e" >&2
    continue
  fi
  mv -f "$WORK/x/$e.wav" "$WORK/x/$e"

  # -X keeps the archive free of platform extra fields; the entry name, and so
  # the MIDlet's getResourceAsStream() path, is unchanged.
  ( cd "$WORK/x" && zip -qX "$OUT_ABS" "$e" )
  converted=$((converted + 1))
done

echo "done: $converted converted -> $(basename "$OUT")"
