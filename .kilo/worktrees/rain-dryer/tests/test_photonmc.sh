#!/usr/bin/env bash
# tests/test_photonmc.sh - Tests de validation du simulateur photonmc.
#
# Tests :
#   1. Reproductibilite : meme graine -> meme sortie (R, T, A a 1e-3 pres).
#   2. Coherence physique : R + T + A ≈ 1 (a 1e-3 pres).
#
# Usage : ./tests/test_photonmc.sh [chemin/vers/photonmc]
# Defaut : photonmc dans le repertoire courant.

set -uo pipefail

BIN="${1:-./photonmc}"
TMPDIR="$(mktemp -d)"
PASS=0
FAIL=0

cleanup() { rm -rf "$TMPDIR"; }
trap cleanup EXIT

run_sim() {
    "$BIN" --shape sphere --tau 2.0 --theta 0 --absorb 0.1 \
        --g1 0.8 --g2 -0.3 --f 0.7 \
        --nphotons 200000 --bins 180 --seed 42 \
        --outdir "$1" > /dev/null 2>&1
}

# Extrait R, T, A depuis l'en-tete "# R=x T=y A=z" du fichier angular_distribution.dat
extract_flux() {
    local line
    line=$(grep '^# R=' "$1" | head -1)
    line="${line#*R=}"
    R="${line%% *}"
    line="${line#*T=}"
    T="${line%% *}"
    line="${line#*A=}"
    A="${line%% *}"
}

echo "=== Tests de photonmc ==="
echo "Binaire : $BIN"
echo

# --- Test 1 : reproductibilite ---
echo "[1] Reproductibilite (mem graine -> meme sortie)"
run_sim "$TMPDIR/run1"
run_sim "$TMPDIR/run2"

extract_flux "$TMPDIR/run1/angular_distribution.dat"
R1=$R; T1=$T; A1=$A
extract_flux "$TMPDIR/run2/angular_distribution.dat"
R2=$R; T2=$T; A2=$A

echo "   Run1 : R=$R1 T=$T1 A=$A1"
echo "   Run2 : R=$R2 T=$T2 A=$A2"

ok=1
for pair in "R1:R2" "T1:T2" "A1:A2"; do
    v1="${pair%%:*}"
    v2="${pair##*:}"
    val1="${!v1}"
    val2="${!v2}"
    diff=$(awk "BEGIN {d=$val1-$val2; if (d<0) d=-d; printf \"%.6f\", d}")
    if awk "BEGIN {exit !($diff < 0.001)}"; then
        :
    else
        ok=0
        echo "   Ecart trop grand pour $v1 : $diff"
    fi
done

if [ $ok -eq 1 ]; then
    echo "   PASS"
    PASS=$((PASS + 1))
else
    echo "   FAIL"
    FAIL=$((FAIL + 1))
fi
echo

# --- Test 2 : coherence physique ---
echo "[2] Coherence physique (R + T + A ≈ 1)"
extract_flux "$TMPDIR/run1/angular_distribution.dat"
sum=$(awk "BEGIN {printf \"%.6f\", $R + $T + $A}")
echo "   R=$R T=$T A=$A  ->  somme=$sum"
if awk "BEGIN {d=$sum-1; if (d<0) d=-d; exit !(d < 0.005)}"; then
    echo "   PASS"
    PASS=$((PASS + 1))
else
    err=$(awk "BEGIN {d=$sum-1; if (d<0) d=-d; printf \"%.6f\", d}")
    echo "   FAIL (|R+T+A-1| = $err > 0.005)"
    FAIL=$((FAIL + 1))
fi
echo

echo "=== Resultat : $PASS OK, $FAIL FAIL ==="
exit $FAIL