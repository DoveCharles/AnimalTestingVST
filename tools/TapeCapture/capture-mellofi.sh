#!/bin/sh
# Takes the full set of measurements off Tape MELLO-FI into renders-mellofi/ (one text file per
# test). Build first with -DAT_BUILD_TOOLS=ON. Each file starts with the settings it ran with.
set -e
cd "$(dirname "$0")/../.."
T=build/tools/TapeCapture/TapeCapture_artefacts/Release/TapeCapture
OUT=renders-mellofi
mkdir -p "$OUT"
run() { name=$1; shift; echo "$name"; "$T" "$@" > "$OUT/$name.txt"; }

$T selftest > "$OUT/00-selftest.txt"
run 01-params params

# Saturation: tone filter out of the way, so this is the drive stage alone.
for d in 0 0.25 0.5 0.75 1; do run "levels-drive-$d" levels --"Tone Enable"=0 --Drive=$d; done
for d in 0 0.5 1; do run "levels-drive-$d-boost" levels --"Tone Enable"=0 --Drive=$d --Boost=1; done
for f in 100 5000; do run "levels-drive-0.5-${f}Hz" levels --"Tone Enable"=0 --Drive=0.5 --freq=$f; done
run levels-default levels

# Frequency response of each tone control.
run response-tone-off response --"Tone Enable"=0
for t in 0 0.25 0.5 0.75 1; do run "response-tone-$t" response --Tone=$t; done
for f in 0 0.25 0.75 1; do run "response-filter-$f" response --"Tone Enable"=0 --Filter=$f; done
for w in 0.5 1; do run "response-wear-$w" response --"Tone Enable"=0 --Wear=$w; done
run response-drive-1 response --"Tone Enable"=0 --Drive=1 --level=-30

# Wow, flutter and wear on pitch.
for w in 0.25 0.5 1; do run "wow-wow-$w" wow --Wow=$w; done
for f in 0.25 0.5 1; do run "wow-flutter-$f" wow --Flutter=$f; done
for w in 0.5 1; do run "wow-wear-$w" wow --Wear=$w; done
run wow-wow-1-flutter-1 wow --Wow=1 --Flutter=1 --csv=$OUT/trace-wow-1-flutter-1.csv
run wow-wow-1 wow --Wow=1 --csv=$OUT/trace-wow-1.csv
run wow-flutter-1 wow --Flutter=1 --csv=$OUT/trace-flutter-1.csv

# Noise.
for n in 0.5 1; do run "noise-noise-$n" noise --Noise=$n; done
for n in 0.5 1; do run "noise-mech-$n" noise --"Mechanical Noise"=$n; done
run noise-wear-1 noise --Wear=1

# Stereo.
run stereo-off stereo
run stereo-width stereo --"Stereo Width"=1
run stereo-width-wow stereo --"Stereo Width"=1 --Wow=1 --Flutter=1

# One row per step for each continuous control.
for p in Drive Wear Wow Flutter Tone Filter Noise Output; do run "sweep-$p" sweep "$p" --steps=11; done

echo "Done: $OUT/"
