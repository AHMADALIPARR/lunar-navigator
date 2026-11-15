#!/bin/sh
# fetch.sh — pull Sun + Earth apparent azimuth/elevation from JPL Horizons
# for the nine Artemis III candidate regions.
# Endpoint per Horizons API v1.3 docs (2025 June):
#   https://ssd.jpl.nasa.gov/api/horizons.api
# (the old ssd-api.jpl.nasa.gov host 404s; the docs point here.)
# Window: 2026-11-15 to 2026-11-29 (14 days), every 6 hours.
# QUANTITIES='4' = airless apparent azimuth & elevation, degrees.
set -e
cd "$(dirname "$0")"
mkdir -p raw csv
API="https://ssd.jpl.nasa.gov/api/horizons.api"
START="2026-11-15"
STOP="2026-11-29"
while IFS="$(printf '\t')" read -r slug name lon lat; do
  case "$slug" in \#*|"") continue;; esac
  for body in 10 399; do
    case $body in 10) bname=sun;; 399) bname=earth;; esac
    out="raw/${slug}-${bname}.txt"
    if [ -s "$out" ] && grep -q '\$\$EOE' "$out"; then echo "skip $out (have it)"; continue; fi
    echo "fetch $slug $bname (lon $lon lat $lat) ..."
    curl -s --max-time 120 --retry 2 "$API?format=text&COMMAND='${body}'&OBJ_DATA='NO'&MAKE_EPHEM='YES'&EPHEM_TYPE='OBSERVER'&CENTER='coord@301'&SITE_COORD='${lon},${lat},0'&START_TIME='${START}'&STOP_TIME='${STOP}'&STEP_SIZE='6%20h'&QUANTITIES='4'&CSV_FORMAT='YES'&ANG_FORMAT='DEG'" -o "$out"
    grep -q '\$\$EOE' "$out" || { echo "FAIL $out"; cat "$out"; exit 1; }
    awk '/^\$\$SOE/{f=1;next}/^\$\$EOE/{f=0}f' "$out" | sed 's/^ *//;s/,$//' > "csv/${slug}-${bname}.csv"
    echo "  $(wc -l < "csv/${slug}-${bname}.csv") rows -> csv/${slug}-${bname}.csv"
  done
done < sites.tsv
echo "fetch complete"
