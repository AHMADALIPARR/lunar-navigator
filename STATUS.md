# STATUS — lunar-navigator (Jessica's team entry)

Built: 2026-11-15, local only. No remote, no push, no submission yet.

## Verified in this build

- Region coordinates: NASA's October 28, 2024 nine-region update, as
  tabulated in Table 4 of the 1D-CNN preprint
  (doi:10.20944/preprints202511.1488.v1 — preprint, not
  peer-reviewed), cross-checked against the
  USGS ScienceBase values (agreement within ~1 deg; regions are
  ~10–38 km across). Source files: data/sites.tsv, data/science.tsv.
- Horizons data path: the old endpoint https://ssd-api.jpl.nasa.gov/horizons.api
  returns 404; the current docs (Horizons API v1.3, 2025 June) give
  https://ssd.jpl.nasa.gov/api/horizons.api, which returns 200.
  Probe 2026-11-15 ~10:11 PST: Sun at lunar lon 0, lat -89.5, el +1.35 deg.
- Data pull: 18 queries (9 sites x Sun + Earth), 2026-11-15 to
  2026-11-29, every 6 hours, QUANTITIES='4' (airless apparent
  azimuth/elevation, degrees). All 18 files returned 57 rows;
  1,026 rows total. Raw output kept in data/raw/, extracted tables
  in data/csv/, fetch script in data/fetch.sh.
- Citation correction 2026-11-15 ~13:10 PST: coordinates and oxides
  were cited as "Bian et al. 2025" — wrong authorship. The DOI
  10.20944/preprints202511.1488.v1 is "Oxide Abundance Inversion in
  the Lunar Polar Regions Using a 1D-CNN Algorithm" (Preprints.org,
  preprint, not peer-reviewed); Bian et al. (2025) is a separate
  paper whose oxide maps the preprint used as training samples.
  Tables 4 and 5 are in the preprint and their values are unchanged
  — only the attribution was fixed, in every file, before push.
- Navigator: gcc -std=c11 -O2 -Wall -Wextra, zero warnings.
  Metrics cross-checked by an independent count: Sun sweep ~170 deg
  for every site over the 14 days (half a lunar month); Earth sweep
  ~12 deg (libration). Mons Mouton Plateau: Sun up 336.0 h, Earth up
  336.0 h. Peak near Cabeus B: Sun up 179.2 h. Slater Plain: Earth
  up 238.0 h, max +4.44 deg.
- Decision model: sites_filled.csv parses as 11 columns x 9 rows;
  guards and decisions match the numbers in out/summary.txt.
- Page: out/index.html renders with zero page errors (headless
  Chromium); 9 sky views; 30 table rows; comparison table and
  sky views visually checked by screenshot.
- 3D sky domes (2026-11-15 ~14:52 PST, owner order: OpenGL host +
  GLSL kernels): sky/host.c is the shader-raw-demo host plus a
  sky-data-file argument (uSun/uEarth vec2 arrays, uCount);
  sky/kernel_skydome.frag renders a fisheye dome per site from the
  site's own 57 Horizons samples (center zenith, bold horizon ring,
  edge -10 deg, north up; gold Sun path solid above / dashed grey
  below the horizon; blue Earth dots + line; bright discs at
  mid-window sample 28, Nov 22 00:00 UTC, drawn larger than true
  size and labeled as such). nav.c writes out/sky/<slug>.txt,
  sky/render.sh renders 640x640 PNGs under xvfb-run, nav re-runs
  and embeds them base64 (SVG plot remains as fallback when no
  render exists). Clean rebuild: zero warnings, 9/9 renders,
  numbers byte-identical (MMP still #1, 336.0 h / +7.30). Renders
  viewed: MMP (unbroken gold arc, Sun never sets), Peak near
  Cabeus B (gold arc breaking to dashed grey for the dark stretch),
  Slater Plain (low Sun + Earth, both hugging the horizon). Page
  re-checked headless: 9 images all complete, zero page errors.
  Demo video rebuilt against the 3D page (29.52 s, 1920x1080,
  H.264 + AAC, peak -3.06 dB) and frame-checked at t=5/17/27.
- Demo video: DONE 2026-11-15 ~13:10 PST — demo/lunar-navigator-demo-30s.mp4, 29.52 s, 1920x1080 H.264 + AAC (peak -3.06 dB, RMS -25.9 dB), narration AI text-to-speech (Magnus, speed 100, 28.0 s), three shots of the page with lower-third labels, frame-checked at t=5/17/27. One fix during verification: a stale '342 h' in the guard-rules paragraph was corrected to '336 h' (the window is 14 days = 336 h) and the video rebuilt.
- Repo packaging: LICENSE verified canonical Sovereign (SHA-256 matches), README.md 2,854 words (walkthrough, over the 2,500 minimum), WATERMARK.md hash-chained over the tree (re-chained after the final edits).

## Still owed (not done by this repo)

- Registration: Jessica's team and Team NOREPLY register on
  spaceappschallenge.org (Universal Event). The assistant cannot
  register anyone. Both teams still unregistered as of 2026-11-15
  ~13:00 PST.
- Team name: Jessica's team name is unpicked. The repo uses the
  working title "Lunar Landing Site Navigator" until she names it.
- Repository name + push: no remote exists. Push needs the owner to
  name the destination repo and drop a fresh token; push public and
  submit in the same session, never early.
- Submission: Project tab, before Sun Nov 15 23:59 local. Demo video
  public link and repo public link go into SUBMISSION.md first.
- Terrain: the LOLA horizon profile is NOT computed. The terrain
  guard is UNKNOWN for every site, by design, until that work is
  done. No site claims a terrain pass.
- Elevations are target-center above a flat horizon. The Sun's and
  Earth's angular radii (~0.27 deg and ~0.95 deg) are not added.
  Rise/set times are linearly interpolated between 6-hour samples.

## What this repo is not

- Not the owner's Team NOREPLY entry. That entry is manual, no AI,
  and is not built here.
- Not a landing recommendation. It is a comparison of sky geometry
  over one 14-day window, and the decision column says which site
  wins that comparison.
- Not a ranking of science value. The oxide table is a second axis
  for the reader; the decision ranks on sky access only.

## README images (2026-11-15)

The README now shows the work: a screenshot of the generated page at the top, the nine rendered sky domes in a gallery, and the Mons Mouton Plateau and Peak near Cabeus B domes beside their worked readings. The frames are the sky-dome kernel's output, re-rendered 2026-11-15 and byte-identical to the first run. The renders are software OpenGL (Mesa llvmpipe) on this VM; the host and kernel run unchanged on a GPU.
