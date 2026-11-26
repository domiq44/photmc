#!/usr/bin/env bash
# tests/test_convergence.sh - Test de convergence du simulateur photonmc.
#
# Vérifie que les resultats (R, T, A) convergent vers une valeur stable
# lorsque le nombre de photons augmente. Le seuil de convergence est
# defini par la tolerance relative (par defaut 5%).
#
# Usage : ./tests/test_convergence.sh [chemin/vers/photonmc] [tolerance]
# Defaut : photonmc dans le repertoire courant, tolerance = 0.05.

set -uo pipefail

BIN="${1:-./photonmc}"
TOL="${2:-0.05}"
TMPDIR="$(mktemp -d)"
PASS=0
FAIL=0

cleanup() { rm -rf "$TMPDIR"; }
trap cleanup EXIT

run_sim() {
    "$BIN" --shape sphere --tau 2.0 --theta 0 --absorb 0.1 \
        --g1 0.8 --g2 -0.3 --f 0.7 \
        --nphotons "$1" --bins 180 --seed 42 \
        --outdir "$2" > /dev/null 2>&1
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

echo "=== Test de convergence (tolerance relative = $TOL) ==="
echo "Binaire : $BIN"
echo

# On lance 3 simulations avec des effectifs croissants.
N1=20000
N2=80000
N3=320000

run_sim "$N1" "$TMPDIR/n1"
run_sim "$N2" "$TMPDIR/n2"
run_sim "$N3" "$TMPDIR/n3"

extract_flux "$TMPDIR/n1/angular_distribution.dat"
R1=$R; T1=$T; A1=$A
extract_flux "$TMPDIR/n2/angular_distribution.dat"
R2=$R; T2=$T; A2=$A
extract_flux "$TMPDIR/n3/angular_distribution.dat"
R3=$R; T3=$T; A3=$A

echo "   N=$N1 : R=$R1 T=$T1 A=$A1"
echo "   N=$N2 : R=$R2 T=$T2 A=$A2"
echo "   N=$N3 : R=$R3 T=$T3 A=$A3"
echo

ok=1
for pair in "R2:R3" "T2:T3" "A2:A3"; do
    v1="${pair%%:*}"
    v2="${pair##*:}"
    val1="${!v1}"
    val2="${!v2}"
    # Variation relative : |v2 - v1| / v1 (ou 1 si v1 = 0)
    rel=$(awk "BEGIN {d=$val2-$val1; if (d<0) d=-d;
                          base=($val1>0)?$val1:1.0;
                          printf \"%.6f\", d/base}")
    if awk "BEGIN {exit !($rel < $TOL)}"; then
        :
    else
        ok=0
        echo "   Convergence insuffisante pour $v1 : variation relative $rel > $TOL"
    fi
done

if [ $ok -eq 1 ]; then
    echo "   PASS (les valeurs convergent entre N=$N2 et N=$N3)"
    PASS=$((PASS + 1))
else
    echo "   FAIL"
    FAIL=$((FAIL + 1))
fi
echo

echo "=== Resultat : $PASS OK, $FAIL FAIL ==="
exit $FAIL