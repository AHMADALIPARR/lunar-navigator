#!/bin/sh
# render.sh — render one 3D sky dome per site with the OpenGL host.
# Reads out/sky/<slug>.txt (written by ./nav from the Horizons CSVs),
# writes out/sky/<slug>.png via sky/host + sky/kernel_skydome.frag.
# "Now" is sample 28 (Nov 22 00:00 UTC, mid-window). Needs Xvfb
# (xvfb-run) and ffmpeg (PPM -> PNG). Run from the repo root or sky/.
set -e
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
export LIBGL_ALWAYS_SOFTWARE=1
mkdir -p "$ROOT/out/sky"
make -C "$ROOT/sky" host
for data in "$ROOT/out/sky/"*.txt; do
  slug="$(basename "$data" .txt)"
  xvfb-run -a "$ROOT/sky/host" "$ROOT/sky/kernel_skydome.frag" 640 640 \
    "$ROOT/out/sky/$slug.ppm" 28 1.0 "$ROOT/sky/passthrough.vert" "$data"
  ffmpeg -y -v error -i "$ROOT/out/sky/$slug.ppm" "$ROOT/out/sky/$slug.png"
  rm -f "$ROOT/out/sky/$slug.ppm"
done
ls -la "$ROOT/out/sky/"
