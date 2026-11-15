# Submission text — Lunar Landing Site Navigator

Copy this into the Project tab on the team's spaceappschallenge.org page.
Fields match the 2026 Project Submission Guide walkthrough
(High-Level Summary, Project Demo, Final Project, Project Details,
Use of AI, Space Agency Data References). Items marked TODO are the
parts only the team can supply: the public demo link, the public
repository link, and the final team/project name.

## Project name (working title)

Lunar Landing Site Navigator

Note: Jessica confirms the final team name and project name before
submitting. The working title is the challenge name because the tool
is the navigator itself.

## High-Level Summary

At the lunar south pole the Sun never rises high — it circles the
horizon, a few degrees up at best. This navigator compares the nine
Artemis III candidate landing regions on real Sun and Earth sky
geometry for a 14-day surface window (November 15–29, 2026), pulled
live from the JPL Horizons system: 1,026 ephemeris rows, one Sun and
one Earth table per site, every 6 hours. It scores each site on
solar-power windows and direct-to-Earth communications windows,
applies stated pass/fail guards, and recommends one site with a
reason and a tradeoff per region. Mons Mouton Plateau wins: the Sun
never sets across the window and Earth never drops below +11
degrees. Peak near Cabeus B loses half the window to darkness.
Every number traces to a Horizons row; the terrain guard stays
UNKNOWN because no local horizon profile was computed — the page
says so instead of pretending.

## Project Demo

30-second video (link to be added after upload):
[TODO: public link — YouTube / Google Drive / GitHub]

Local file: demo/lunar-navigator-demo-30s.mp4 (narration is AI
text-to-speech, labeled in the video description and in
AI_DISCLOSURE.md).

## Final Project

Public repository (link to be added after the push):
[TODO: public repo link]

Local repository: lunar-navigator (this repo). The entry is built by
`src/nav.c` (C11, standard library only) from the Horizons tables in
`data/csv/`; the page it generates is `out/index.html`.

## Project Details

**The problem.** Artemis III lands near the lunar south pole, where
lighting and communications are decided by sky geometry: the Sun
stays within a few degrees of the horizon, and Earth hangs almost
still in the sky, its elevation set by where you stand. A site that
looks good on a map can be dark half the time or deaf to Earth.
Choosing a site means comparing sites *and* dates on those two
windows at once.

**What the navigator does.** It pulls the Sun's and Earth's apparent
azimuth and elevation as seen from each of the nine candidate regions
(NASA's October 28, 2024 list) for November 15–29, 2026, every 6
hours, from JPL Horizons — 18 queries, 1,026 rows. A C11 program
reads the tables and computes, per site: hours the Sun is up (linear
interpolation between samples for rise/set), the longest continuous
sunlit stretch, maximum and mean elevation, the same four numbers
for Earth, and the azimuth sweep of each body. It applies two
stated guards — power passes at 300+ sunlit hours of the 336-hour
window with a maximum of at least +3.0 degrees; comms passes at
300+ Earth-visible hours with a maximum of at least +5.0 degrees —
and ranks the sites by combined up-hours. The result is a
comparison table, one 3D sky dome per site rendered by the OpenGL
host and the GLSL sky-dome kernel in sky/ (Sun path gold, Earth
blue, horizon ring bold), a decision ledger with a reason and a
tradeoff per region, and a science table (oxide abundances per
region, 1D-CNN preprint Table 5).

**The result.** Mons Mouton Plateau is RECOMMENDED: Sun up all 336
hours (maximum +7.30 degrees), Earth up all 336 hours (maximum
+11.07 degrees). Mons Mouton, Nobile Rim 1, Malapert Massif, and
Nobile Rim 2 pass both guards and are VIABLE. Haworth misses the
power guard by 0.8 hours and is MARGINAL. Peak near Cabeus B has
the highest Sun maximum (+7.65 degrees) but the Sun is up only
179.2 hours — half the window in darkness — and is MARGINAL.
Slater Plain and de Gerlache Rim 2 fail both guards and are REJECT.
The science table adds a second axis: Slater Plain and de Gerlache
Rim 2 capture the most compositional diversity, so they stay
interesting even though their sky geometry is the weakest.

**What it does not do.** Elevations are the target's center above a
flat horizon — the Sun's and Earth's angular radii are not added,
and no local terrain is modeled. At these grazing angles a nearby
ridge changes the real illumination, so the terrain guard is UNKNOWN
for every site until a LOLA horizon profile is computed. The
6-hour sampling is interpolated linearly for rise/set times. Nothing
here is a landing recommendation; it is a comparison of sky
geometry, and the decision column says which site wins that
comparison.

**How to re-run it.** `data/fetch.sh` re-pulls the Horizons tables;
`make` rebuilds the navigator, renders the nine 3D sky domes with
the OpenGL host, and regenerates the page. The raw Horizons output
is kept in `data/raw/` so anyone can check the numbers against the
source.

## Use of AI

This entry was built by Jessica Williams' team with Muse (Meta's
personal AI agent) as the AI tool. Muse wrote the navigator program
(`src/nav.c`), generated the page from the Horizons data, and
narrated the demo video with AI text-to-speech. All numeric values
come from JPL Horizons ephemeris rows, not from the AI — the program
only reads, counts, and plots them. No AI-generated images are
used: every diagram is rendered by this repository's own programs
(the navigator and the OpenGL/GLSL sky-dome kernel) from the
measured data. See AI_DISCLOSURE.md for the tool-by-tool record.

## Space Agency Data References

- JPL Horizons System — Sun and Earth ephemeris (observer tables,
  QUANTITIES='4', airless apparent azimuth and elevation) —
  https://ssd.jpl.nasa.gov/horizons/
- NASA Quickmap — LRO terrain and imagery —
  https://quickmap.lroc.asu.edu/
- NASA Moon Trek — interactive lunar mapping —
  https://trek.nasa.gov/moon/
- CSA Moon Exploration — background —
  https://www.asc-csa.gc.ca/eng/exploration/moon-exploration.asp
- NASA Artemis III landing regions (October 28, 2024) — the nine
  candidate regions and their coordinates, as tabulated in Table 4
  of the 1D-CNN preprint (doi:10.20944/preprints202511.1488.v1 —
  preprint, not peer-reviewed)

## Verification checkboxes (confirm on the form)

- [ ] This is original work by the team named on the project page.
- [ ] The project uses at least one NASA data source, linked above.
- [ ] Every AI tool used is disclosed above and in AI_DISCLOSURE.md.
- [ ] No NASA branding (logo, flag, mission identifiers) appears in
      the project. No AI-generated images are used; the demo video's
      narration is AI text-to-speech and is labeled.
- [ ] All team members are registered at the same event (Universal
      Event) and the roster does not overlap with any other team.
