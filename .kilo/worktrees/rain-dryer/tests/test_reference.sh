#!/usr/bin/env bash
# tests/test_reference.sh - Test de cas de reference analytique.
#
# Cas : sphère, diffusion isotrope (g1=g2=0), sans absorption,
# epaisseur optique tres faible (tau = 0.001).
#
# Resultat attendu (limite optiquement mince) :
#   - la quasi-totalite des photons transmet sans collision : T ≈ 1
#   - reflechi ≈ 0, absorbe ≈ 0
#   - bilan R + T + A ≈ 1
#
# Usage : ./tests/test_reference.sh [chemin/vers/photonmc] [tolerance]
# Defaut : photonmc dans le repertoire courant, tolerance = 0.01.

set -uo pipefail

BIN="${1:-./photonmc}"
TOL="${2:-0.01}"
TMPDIR="$(mktemp -d)"
PASS=0
FAIL=0

cleanup() { rm -rf "$TMPDIR"; }
trap cleanup EXIT

run_sim() {
    "$BIN" --shape sphere --tau 0.001 --theta 0 --absorb 0.0 \
        --g1 0.0 --g2 0.0 --f 0.5 \
        --nphotons 200000 --bins 180 --seed 12345 \
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

echo "=== Test de reference analytique (tau faible, isotrope, sans absorption) ==="
echo "Binaire : $BIN"
echo

run_sim "$TMPDIR/run"
extract_flux "$TMPDIR/run/angular_distribution.dat"
echo "   R=$R T=$T A=$A"

ok=1

# T doit etre proche de 1 (transmission quasi totale)
if awk "BEGIN {d=1-$T; if (d<0) d=-d; exit !(d < $TOL)}"; then
    :
else
    ok=0
    echo "   Ecart T trop grand par rapport a 1 : |1-T| = $(awk "BEGIN {printf \"%.6f\", 1-$T}")"
fi

# R et A doivent etre proches de 0
for v in R A; do
    val="${!v}"
    if awk "BEGIN {d=$val; if (d<0) d=-d; exit !(d < $TOL)}"; then
        :
    else
        ok=0
        echo "   $v trop loin de 0 : $val"
    fi
done

# Bilan R + T + A ≈ 1
sum=$(awk "BEGIN {printf \"%.6f\", $R + $T + $A}")
if awk "BEGIN {d=$sum-1; if (d<0) d=-d; exit !(d < 0.005)}"; then
    :
else
    ok=0
    echo "   Bilan incoherent : R+T+A = $sum"
fi

if [ $ok -eq 1 ]; then
    echo "   PASS"
    PASS=$((PASS + 1))
else
    echo "   FAIL"
    FAIL=$((FAIL + 1))
fi

echo
echo "=== Resultat : $PASS OK, $FAIL FAIL ==="
exit $FAIL
