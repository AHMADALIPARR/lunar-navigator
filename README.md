# Lunar Landing Site Navigator

Nine Artemis III candidate landing regions, compared on real Sun and
Earth sky geometry for a 14-day surface window. The navigator pulls
the Sun's and Earth's apparent azimuth and elevation for each region
from the JPL Horizons system, computes solar-power and
direct-to-Earth communications windows, applies stated pass/fail
guards, and recommends one site — with a reason and a tradeoff for
every region. This is Jessica Williams' team entry for the NASA
International Space Apps Challenge 2026, challenge Lunar Landing
Site Navigator. It is built with Muse as the disclosed AI tool; the
numbers come from JPL Horizons, not from the AI.

## What it is

At the lunar south pole the Sun never rises high. It circles the
horizon, a few degrees up at best, and over a two-week stay it can
spend days below it. Earth hangs almost still in the sky — its
elevation is set by where you stand, and from some regions it never
climbs high enough for a comfortable direct link. A site that looks
good on a map can be dark half the time or deaf to Earth, so
choosing a site means comparing sites *and* dates on those two
windows at once. That is the whole job of this navigator.

The entry answers one question: for the window November 15–29,
2026, which of the nine candidate regions gives a surface crew the
best combined sky — the most sunlit hours for power, and the most
Earth-visible hours for communications? The answer it gives is
Mons Mouton Plateau: the Sun never sets across the window, and
Earth never drops below +11 degrees. The page shows the full
comparison so the answer can be checked, not just believed.

## What is in this repository

```
lunar-navigator/
  src/nav.c               the navigator program (C11, standard library only)
  Makefile                builds nav and regenerates the outputs
  data/
    sites.tsv             the nine regions and their coordinates
    science.tsv           oxide abundances per region
    fetch.sh              re-pulls the Horizons tables
    raw/                  raw Horizons output, 18 query files
    csv/                  extracted Sun/Earth tables, 18 files, 1,026 rows
  out/
    index.html            the navigator page (generated)
    sites_filled.csv      the decision-model table, filled from data
    summary.txt           plain-text per-site numbers
  demo/
    lunar-navigator-demo-30s.mp4   the 30-second demo video
  README.md               this file
  SUBMISSION.md           the text for the Project tab form
  AI_DISCLOSURE.md        tool-by-tool record of AI use
  STATUS.md               what is verified and what is still owed
  LICENSE                 SNAPKITTY OPAQUE SOURCE LICENSE v1.0
  WATERMARK.md            hash-chained manifest of the tree
```

## The data pipeline

The pipeline has four stages, and each one is re-runnable from this
repository.

### 1. Region coordinates

The nine regions are NASA's October 28, 2024 update of the Artemis
III candidate landing regions. Their coordinates are tabulated in
Table 4 of "Oxide Abundance Inversion in the Lunar Polar Regions
Using a 1D-CNN Algorithm" (Preprints.org,
doi:10.20944/preprints202511.1488.v1 — preprint, not peer-reviewed)
and cross-checked against the USGS ScienceBase values
carried by nasanow.org. The two sources agree within about one
degree, and the regions themselves are roughly 10–38 km across, so
the agreement is tighter than the regions are wide. The coordinates
live in `data/sites.tsv`:

| Region | Longitude | Latitude |
|---|---|---|
| Peak near Cabeus B | -69.014 | -83.661 |
| Haworth | -22.836 | -86.753 |
| Malapert Massif | -0.125 | -85.990 |
| Mons Mouton Plateau | 29.926 | -84.232 |
| Mons Mouton | 31.586 | -85.423 |
| Nobile Rim 1 | 37.241 | -85.431 |
| Nobile Rim 2 | 58.654 | -83.963 |
| de Gerlache Rim 2 | -65.421 | -88.225 |
| Slater Plain | 126.134 | -87.149 |

### 2. Horizons ephemeris

`data/fetch.sh` pulls the Sun and the Earth for every region from
the JPL Horizons system, observer ephemeris, at
`https://ssd.jpl.nasa.gov/api/horizons.api` (Horizons API v1.3).
Each query asks for the target — Sun (`COMMAND='10'`) or Earth
(`COMMAND='399'`) — as seen from the region (`CENTER='coord@301'`
with the region's longitude, latitude, and 0 km altitude), with
`QUANTITIES='4'`, which is the airless apparent azimuth and
elevation in degrees. The window is 2026-11-15 to 2026-11-29, every
6 hours: 57 rows per query, 18 queries, 1,026 rows in total. The
raw Horizons output is kept in `data/raw/` so anyone can check the
numbers against the source, and the extracted tables live in
`data/csv/`.

One detail matters for anyone re-running the fetch: the older
Horizons host, `ssd-api.jpl.nasa.gov`, returns 404 on
`/horizons.api` now. The current documentation points at
`ssd.jpl.nasa.gov/api/horizons.api`, and that is the endpoint the
script uses.

### 3. The navigator program

`src/nav.c` reads the tables and computes, per site and per body:

- **Hours up.** The time the body's center sits above the local
  flat horizon, computed by linear interpolation between the
  6-hour samples, so rise and set times fall between samples
  instead of snapping to them.
- **Longest continuous stretch.** The longest unbroken run above
  the horizon, in hours — the number that sizes batteries.
- **Maximum and mean elevation.** The peak of the arc and its
  average over the window.
- **Azimuth sweep.** The total angular travel of the body across
  the window, unwrapped so a sweep crossing north reads correctly.
  The Sun sweeps about 170 degrees in 14 days at every site; Earth
  sweeps about 12 degrees, which is lunar libration — Earth barely
  moves in the lunar sky.
- **First and last above-horizon samples.** The datetimes that
  bracket the windows.

The program applies two guards whose rules are stated in the code
and on the page:

- **Power** passes when the Sun is up at least 300 hours of the
  336-hour window and reaches at least +3.0 degrees.
- **Comms** passes when Earth is up at least 300 hours and reaches
  at least +5.0 degrees.
- **Terrain** is UNKNOWN for every site. No local horizon profile
  was computed from LOLA data, so no site claims a terrain pass.
  The guard says so instead of pretending.

The decision ranks the sites by combined up-hours (Sun hours plus
Earth hours, maximum 672), breaking exact ties on the sum of the
two maximum elevations. The top-ranked site that passes both
guards is RECOMMENDED; other passing sites are VIABLE; a site that
fails one guard is MARGINAL; a site that fails both is REJECT. The
program writes one reason and one tradeoff per site, built from
that site's own numbers — never from a template that ignores them.

### 4. The page

The program generates `out/index.html`, a single self-contained
page with no JavaScript and no network calls. It holds the
comparison table, one polar sky plot per site, the decision ledger,
the science table, the method, the data sources, and the AI
disclosure. It also writes `out/sites_filled.csv` — the
decision-model table with all eleven columns filled — and
`out/summary.txt`, the plain-text numbers. `make` rebuilds the
program and regenerates all three outputs.

## How to read the sky plots

Each plot is a polar view of the sky over one region. The rings are
elevation: the bold ring is the horizon (0 degrees), the center is
+12 degrees, and the outer ring is -6 degrees. The gold line is the
Sun's path across the 14 days — solid while the Sun is up, dashed
while it is down, split exactly at the horizon by interpolation.
The blue dots are Earth, one per 6-hour sample; because Earth
barely moves, the dots form a short arc that shows libration. The
gold dot marks where the Sun starts on November 15. North is up.

At the pole the Sun circles the horizon instead of rising and
setting the way it does on Earth, so the gold path is an arc that
hugs the horizon ring. A site where the Sun never sets shows a
single unbroken gold arc; a site where it sets shows the gold arc
breaking into a dashed grey arc for the dark stretch. That visual
difference is the whole power story.

## The results

The window is 2026-11-15 to 2026-11-29, 336 hours, sampled every
6 hours.

| Rank | Region | Sun up | Sun max | Earth up | Earth max | Decision |
|---|---|---|---|---|---|---|
| 1 | Mons Mouton Plateau | 336.0 h | +7.30° | 336.0 h | +11.07° | RECOMMENDED |
| 2 | Mons Mouton | 336.0 h | +6.11° | 336.0 h | +10.00° | VIABLE |
| 3 | Nobile Rim 1 | 336.0 h | +6.11° | 335.1 h | +9.72° | VIABLE |
| 4 | Malapert Massif | 332.6 h | +5.53° | 336.0 h | +10.26° | VIABLE |
| 5 | Nobile Rim 2 | 316.2 h | +7.58° | 321.8 h | +9.08° | VIABLE |
| 6 | Haworth | 299.2 h | +4.75° | 326.2 h | +9.32° | MARGINAL |
| 7 | de Gerlache Rim 2 | 279.2 h | +3.23° | 286.6 h | +7.09° | REJECT |
| 8 | Peak near Cabeus B | 179.2 h | +7.65° | 318.3 h | +8.87° | MARGINAL |
| 9 | Slater Plain | 218.4 h | +4.35° | 238.0 h | +4.44° | REJECT |

Three findings carry the entry.

**Mons Mouton Plateau wins on both windows at once.** The Sun
never sets across the 14 days and Earth never drops below +11
degrees — the highest Earth maximum of the nine. Its next-door
neighbor Mons Mouton ties it on hours (336 and 336) and loses the
tie-break on maxima (+6.11 and +10.00 against +7.30 and +11.07).

**Peak near Cabeus B is the trap.** It has the highest Sun maximum
of all nine regions (+7.65 degrees), which looks like the best
power on a map. But the Sun is up only 179.2 hours of the 336 —
nearly half the window in darkness, with the Sun not rising until
November 21. A surface stay there needs batteries or nuclear power
sized for multi-day nights, not solar alone. A maximum without a
window is not power.

**Slater Plain is the comms trap.** Earth is up only 238.0 hours
and never climbs above +4.44 degrees; it does not rise above the
horizon at all until November 19. Direct-to-Earth communications
from Slater Plain would need a relay. Its science value is real —
Slater Plain and de Gerlache Rim 2 capture the most compositional
diversity of the nine regions — which is why the page keeps the
science table as a second axis instead of letting the sky geometry
decide alone.

Haworth deserves one line of its own: it misses the power guard by
0.8 hours (299.2 against the 300-hour rule). The guard is a stated
rule, not a law of nature, and the ledger shows the number so a
reader can see exactly how close it is.

## The science table

Composition comes from Table 5 of the 1D-CNN preprint (Preprints.org,
doi:10.20944/preprints202511.1488.v1 — preprint, not peer-reviewed),
inverted from KAGUYA spectral data using Bian et al. (2025) oxide
maps as training samples. All nine regions are feldspathic highland
material — iron oxide under 6.2 weight percent, alumina over 25
weight percent — with small differences between them. The table
gives FeO and Al2O3 per region; the paper's finding is that Slater
Plain and de Gerlache Rim 2 capture the most compositional
diversity within feasible operational ranges, which is the reason
those two regions stay in the conversation even though their sky
geometry is the weakest. The decision ranks on sky access only;
the science table is there so the reader can weigh the tradeoff
themselves.

## Why the guards are what they are

The guard thresholds are modeling choices, and they are printed
here so they can be argued with. Power passes at 300 sunlit hours
of the 336-hour window — about 89 percent — with a maximum elevation
of at least +3.0 degrees. Comms passes at 300 Earth-visible hours
with a maximum of at least +5.0 degrees. The hour rule says a site
must be lit and in contact for nearly the whole stay; the elevation
floor says a body that only ever grazes the horizon does not count,
because a grazing Sun casts kilometer-long shadows behind every
rock and a grazing Earth sits behind every ridge. The two rules
together keep a site from passing on duration alone while its peak
stays uselessly low.

The terrain guard is excluded on purpose, not by oversight. A
terrain pass would need a horizon profile computed from LOLA
elevation data — the angle of the actual skyline in every azimuth
from each site — and that computation was not done in this build.
Claiming a terrain pass without it would be inventing a result, so
the guard stays UNKNOWN and the page says so on every row. The
next step for this entry, if the team continues it, is exactly that
computation: ray-march the LOLA grid from each region center, take
the maximum terrain elevation per azimuth, and re-test every
Sun and Earth sample against the real skyline instead of the flat
horizon. The numbers in this build would only go down under that
test — the flat horizon is the most generous case — so the ranking
here is an upper bound on the real illumination.

Haworth shows why the thresholds are printed rather than hidden.
It misses the power guard by 0.8 hours: 299.2 sunlit hours against
the 300-hour rule. That is a knife edge, and the ledger shows the
number so a reader can see it is a knife edge. If the team decides
the rule should be 295 hours, Haworth passes and the ranking
changes by one row; nothing else moves, because no other site sits
within 15 hours of either threshold. A decision rule that cannot
say how close the near-misses are is not a rule, it is a verdict.

## Two plots, worked

The Mons Mouton Plateau plot is the easy one to read. The gold
line is a single unbroken arc across the northern half of the plot,
running from west to east and staying between the +5 and +10 degree
rings for the whole 14 days. There is no dashed segment anywhere,
because the Sun never sets. The blue Earth dots form a short arc
near the top of the plot, around +11 degrees — Earth barely moves,
and it sits high. The plot says in one picture what the table says
in numbers: both bodies up, all window, high.

The Peak near Cabeus B plot is the instructive one. The gold arc
starts on the eastern side, runs for a short stretch, and then the
line turns dashed grey and stays dashed for most of the plot — the
Sun below the horizon from November 15 until November 21. The
solid gold only returns for the last stretch, and the arc climbs
higher than any other site's, touching +7.65 degrees. The blue
Earth dots sit in a tight cluster around +8.87 degrees, high and
steady. So the plot shows a site with the best Sun peak and the
worst Sun window at the same time, and a perfectly good Earth link
throughout. That is why the decision is MARGINAL and not REJECT:
the comms are fine, the power is not, and the tradeoff is a
battery problem rather than a site problem.

## How to re-run it

```
./data/fetch.sh     # re-pull the 18 Horizons tables (needs internet)
make                # build src/nav.c and regenerate out/
```

The build needs only a C11 compiler and the standard math library.
`gcc -std=c11 -O2 -Wall -Wextra` builds with zero warnings. The
page opens from disk in any browser; there is no server, no
JavaScript, and no network call in it.

## Limits, stated plainly

- Elevations are the target's center above a flat horizon. The
  Sun's angular radius (about 0.27 degrees) and Earth's (about
  0.95 degrees) are not added, so the true lit and visible times
  are slightly longer than the numbers here.
- No local terrain is modeled. At these grazing angles a nearby
  ridge changes the real illumination, so the terrain guard is
  UNKNOWN until a LOLA horizon profile is computed. The page and
  the CSV say so on every row.
- Rise and set times are linearly interpolated between 6-hour
  samples; the true crossing can differ by minutes.
- The window is one 14-day span, November 15–29, 2026. A different
  window gives different numbers — the pipeline re-runs for any
  window by changing the dates in `data/fetch.sh`.
- Nothing here is a landing recommendation. It is a comparison of
  sky geometry over one window, and the decision column says which
  site wins that comparison.

## Data sources

- JPL Horizons System — Sun and Earth ephemeris —
  https://ssd.jpl.nasa.gov/horizons/
- NASA Quickmap — LRO terrain and imagery —
  https://quickmap.lroc.asu.edu/
- NASA Moon Trek — interactive lunar mapping —
  https://trek.nasa.gov/moon/
- CSA Moon Exploration — background —
  https://www.asc-csa.gc.ca/eng/exploration/moon-exploration.asp
- NASA Artemis III landing regions, October 28, 2024 — region
  coordinates, Table 4 of the 1D-CNN preprint
  (doi:10.20944/preprints202511.1488.v1 — preprint, not peer-reviewed)
- 1D-CNN preprint, Table 5 — oxide abundances per region

## Use of AI

This entry was built by Jessica Williams' team with Muse (Meta's
personal AI agent) as the AI tool. Muse wrote the navigator program,
generated the page from the Horizons data, and narrated the demo
video with AI text-to-speech. All numeric values come from JPL
Horizons ephemeris rows, not from the AI — the program only reads,
counts, and plots them. No AI-generated images are used: every
diagram is rendered by the program from the measured data. See
AI_DISCLOSURE.md for the tool-by-tool record, and SUBMISSION.md for
the text that goes on the project page.
