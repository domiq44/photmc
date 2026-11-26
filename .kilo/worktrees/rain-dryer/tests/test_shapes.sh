#!/usr/bin/env bash
set -uo pipefail
BIN="${1:-./photonmc}"
TMPDIR="$(mktemp -d)"
trap 'rm -rf "$TMPDIR"' EXIT
PASS=0; FAIL=0
for shape in cube cylinder; do
  "$BIN" --shape "$shape" --tau 2.0 --theta 30 --absorb 0.1 \
      --g1 0.8 --g2 -0.3 --f 0.7 --nphotons 100000 --bins 180 --seed 7 \
      --outdir "$TMPDIR/$shape" > /dev/null 2>&1
  line=$(grep '^# R=' "$TMPDIR/$shape/angular_distribution.dat" | head -1)
  R=$(echo "$line" | sed 's/.*R=//;s/ .*//')
  T=$(echo "$line" | sed 's/.*T=//;s/ .*//')
  A=$(echo "$line" | sed 's/.*A=//;s/ .*//')
  sum=$(awk "BEGIN {printf \"%.6f\", $R+$T+$A}")
  if awk "BEGIN {d=$sum-1; if(d<0)d=-d; exit !(d<0.005)}"; then
    echo "[$shape] PASS R=$R T=$T A=$A sum=$sum"; PASS=$((PASS+1))
  else
    echo "[$shape] FAIL sum=$sum"; FAIL=$((FAIL+1))
  fi
done
echo "Resultat : $PASS OK, $FAIL FAIL"; exit $FAIL
