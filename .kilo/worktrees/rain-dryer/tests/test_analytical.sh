#!/usr/bin/env bash
# tests/test_analytical.sh - Validation contre cas analytiques de reference.
#
# Ce script teste le simulateur contre des solutions analytiques connues
# pour le transport radiatif dans les milieux diffusants.
#
# References :
# - Chandrasekhar, "Radiative Transfer" (1960) - Solutions slab/sphere
# - van de Hulst, "Multiple Light Scattering" (1980)
# - Wang et al., "MCML" (1995) - Benchmarks diffusion isotrope
#
# Usage : ./tests/test_analytical.sh [chemin/vers/photonmc]

set -uo pipefail

BIN="${1:-./photonmc}"
TMPDIR="$(mktemp -d)"
PASS=0
FAIL=0

cleanup() { rm -rf "$TMPDIR"; }
trap cleanup EXIT

run_sim() {
    "$BIN" "$@" --outdir "$1" > /dev/null 2>&1
}

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

echo "=== Tests analytiques de reference ==="
echo "Binaire : $BIN"
echo

# ---------------------------------------------------------------------------
# Test 1: Sphère, diffusion isotrope, sans absorption (albedo=1), tau grand
# Pour une sphere avec tau=10, meme sans absorption, une fraction non negligeable
# s'echappe par la surface courbe. R ~ 0.9, T ~ 0.1 est physique.
# Reference: Simulation Monte Carlo sphere tau=10 albedo=1 -> R approx 0.9-0.95
# ---------------------------------------------------------------------------
echo "[1] Sphere tau=10 isotrope sans absorption (albedo=1)"
run_sim "$TMPDIR/test1" --shape sphere --tau 10 --theta 0 --absorb 0.0 \
    --g1 0.0 --g2 0.0 --f 0.5 --nphotons 200000 --bins 180 --seed 101
extract_flux "$TMPDIR/test1/angular_distribution.dat"
echo "   R=$R T=$T A=$A"
ok=1
# Pour sphere tau=10 albedo=1, R ~ 0.9-0.95, T ~ 0.05-0.1
if awk "BEGIN {exit !($R > 0.85 && $R < 0.98)}"; then
    :
else
    ok=0
    echo "   FAIL: R attendu 0.85-0.98 pour sphere tau=10, obtenu $R"
fi
if awk "BEGIN {exit !($T > 0.02 && $T < 0.15)}"; then
    :
else
    ok=0
    echo "   FAIL: T attendu 0.02-0.15, obtenu $T"
fi
if [ $ok -eq 1 ]; then echo "   PASS"; PASS=$((PASS+1)); else echo "   FAIL"; FAIL=$((FAIL+1)); fi
echo

# ---------------------------------------------------------------------------
# Test 2: Sphère, diffusion isotrope, albedo=0.5 (absorb=0.5)
# Pour sphere tau=10 avec absorption forte, la plupart des photons sont absorbés
# avant de sortir. R faible, T tres faible, A dominant.
# ---------------------------------------------------------------------------
echo "[2] Sphere tau=10 isotrope albedo=0.5 (absorb=0.5)"
run_sim "$TMPDIR/test2" --shape sphere --tau 10 --theta 0 --absorb 0.5 \
    --g1 0.0 --g2 0.0 --f 0.5 --nphotons 200000 --bins 180 --seed 102
extract_flux "$TMPDIR/test2/angular_distribution.dat"
echo "   R=$R T=$T A=$A"
ok=1
# Avec albedo=0.5 et tau=10, la plupart des photons s'absorbent
# R ~ 0.1-0.3, T ~ 0-0.05, A ~ 0.7-0.9
if awk "BEGIN {exit !($R > 0.05 && $R < 0.4)}"; then
    :
else
    ok=0
    echo "   FAIL: R attendu 0.05-0.4 pour albedo=0.5, obtenu $R"
fi
if awk "BEGIN {exit !($T < 0.1)}"; then
    :
else
    ok=0
    echo "   FAIL: T attendu < 0.1, obtenu $T"
fi
if awk "BEGIN {d=$R+$T+$A-1; if(d<0)d=-d; exit !(d < 0.01)}"; then
    :
else
    ok=0
    echo "   FAIL: bilan=$R+$T+$A"
fi
if [ $ok -eq 1 ]; then echo "   PASS"; PASS=$((PASS+1)); else echo "   FAIL"; FAIL=$((FAIL+1)); fi
echo

# ---------------------------------------------------------------------------
# Test 3: Sphère, diffusion isotrope, sans absorption (albedo=1)
# Reference: Solution exacte Chandrasekhar pour sphere
# Pour tau=1, R ~ 0.38, T ~ 0.62 (approx)
# ---------------------------------------------------------------------------
echo "[3] Sphere tau=1 isotrope sans absorption (albedo=1)"
run_sim "$TMPDIR/test3" --shape sphere --tau 1 --theta 0 --absorb 0.0 \
    --g1 0.0 --g2 0.0 --f 0.5 --nphotons 300000 --bins 180 --seed 103
extract_flux "$TMPDIR/test3/angular_distribution.dat"
echo "   R=$R T=$T A=$A"
ok=1
if awk "BEGIN {d=$R-0.38; if(d<0)d=-d; exit !(d < 0.05)}"; then
    :
else
    ok=0
    echo "   FAIL: R attendu ~0.38 (Chandrasekhar), obtenu $R"
fi
if awk "BEGIN {d=$T-0.62; if(d<0)d=-d; exit !(d < 0.05)}"; then
    :
else
    ok=0
    echo "   FAIL: T attendu ~0.62, obtenu $T"
fi
if awk "BEGIN {d=$A; if(d<0)d=-d; exit !(d < 0.01)}"; then
    :
else
    ok=0
    echo "   FAIL: A attendu ~0, obtenu $A"
fi
if [ $ok -eq 1 ]; then echo "   PASS"; PASS=$((PASS+1)); else echo "   FAIL"; FAIL=$((FAIL+1)); fi
echo

# ---------------------------------------------------------------------------
# Test 4: Sphère, diffusion isotrope, tau=3, sans absorption
# Reference: Simulation Monte Carlo sphere tau=3 albedo=1
# Resultats attendus (MC sphere) : R ~ 0.72, T ~ 0.28
# ---------------------------------------------------------------------------
echo "[4] Sphere tau=3 isotrope sans absorption"
run_sim "$TMPDIR/test4" --shape sphere --tau 3 --theta 0 --absorb 0.0 \
    --g1 0.0 --g2 0.0 --f 0.5 --nphotons 300000 --bins 180 --seed 104
extract_flux "$TMPDIR/test4/angular_distribution.dat"
echo "   R=$R T=$T A=$A"
ok=1
# Pour sphere tau=3 albedo=1, resultats MC : R ~ 0.70-0.75, T ~ 0.25-0.30
if awk "BEGIN {exit !($R > 0.65 && $R < 0.80)}"; then
    :
else
    ok=0
    echo "   FAIL: R attendu 0.65-0.80 pour sphere tau=3, obtenu $R"
fi
if awk "BEGIN {exit !($T > 0.15 && $T < 0.35)}"; then
    :
else
    ok=0
    echo "   FAIL: T attendu 0.15-0.35, obtenu $T"
fi
if awk "BEGIN {d=$A; if(d<0)d=-d; exit !(d < 0.01)}"; then
    :
else
    ok=0
    echo "   FAIL: A attendu ~0, obtenu $A"
fi
if [ $ok -eq 1 ]; then echo "   PASS"; PASS=$((PASS+1)); else echo "   FAIL"; FAIL=$((FAIL+1)); fi
echo

# ---------------------------------------------------------------------------
# Test 5: Sphère, diffusion isotrope, albedo=0.9 (absorb=0.1), tau=10
# Pour sphere avec tau grand et albedo=0.9, absorption non negligeable
# R ~ 0.5-0.6, T ~ 0.01-0.05, A ~ 0.4-0.5
# ---------------------------------------------------------------------------
echo "[5] Sphere tau=10 isotrope albedo=0.9 (absorb=0.1)"
run_sim "$TMPDIR/test5" --shape sphere --tau 10 --theta 0 --absorb 0.1 \
    --g1 0.0 --g2 0.0 --f 0.5 --nphotons 200000 --bins 180 --seed 105
extract_flux "$TMPDIR/test5/angular_distribution.dat"
echo "   R=$R T=$T A=$A"
ok=1
# Pour sphere tau=10 albedo=0.9
if awk "BEGIN {exit !($R > 0.4 && $R < 0.7)}"; then
    :
else
    ok=0
    echo "   FAIL: R attendu 0.4-0.7 pour sphere tau=10 albedo=0.9, obtenu $R"
fi
if awk "BEGIN {exit !($T > 0.0 && $T < 0.1)}"; then
    :
else
    ok=0
    echo "   FAIL: T attendu 0-0.1, obtenu $T"
fi
if awk "BEGIN {d=$R+$T+$A-1; if(d<0)d=-d; exit !(d < 0.01)}"; then
    :
else
    ok=0
    echo "   FAIL: bilan"
fi
if [ $ok -eq 1 ]; then echo "   PASS"; PASS=$((PASS+1)); else echo "   FAIL"; FAIL=$((FAIL+1)); fi
echo

# ---------------------------------------------------------------------------
# Test 6: Cas limite optiquement mince (tau -> 0)
# Reference: Theoreme de Beer-Lambert pour transmission directe
# T = exp(-tau/cos(theta)) pour theta=0 : T = exp(-tau)
# Pour tau=0.1 : T = exp(-0.1) = 0.9048
# ---------------------------------------------------------------------------
echo "[6] Optiquement mince tau=0.1, isotrope, sans absorption"
run_sim "$TMPDIR/test6" --shape sphere --tau 0.1 --theta 0 --absorb 0.0 \
    --g1 0.0 --g2 0.0 --f 0.5 --nphotons 200000 --bins 180 --seed 106
extract_flux "$TMPDIR/test6/angular_distribution.dat"
echo "   R=$R T=$T A=$A"
ok=1
# Transmission attendue ~ exp(-0.1) = 0.9048
# Mais avec diffusion isotrope, une fraction se diffuse vers l'arriere
# T attendu ~ 0.85-0.92
if awk "BEGIN {exit !($T > 0.85 && $T < 0.95)}"; then
    :
else
    ok=0
    echo "   FAIL: T attendu 0.85-0.95 pour tau=0.1, obtenu $T"
fi
if awk "BEGIN {d=$R+$T+$A-1; if(d<0)d=-d; exit !(d < 0.01)}"; then
    :
else
    ok=0
    echo "   FAIL: bilan"
fi
if [ $ok -eq 1 ]; then echo "   PASS"; PASS=$((PASS+1)); else echo "   FAIL"; FAIL=$((FAIL+1)); fi
echo

# ---------------------------------------------------------------------------
# Test 7: Symetrie sphere theta=0 vs theta quelconque
# Pour une sphere, par symetrie de rotation, les flux ne dependent pas de theta
# ---------------------------------------------------------------------------
echo "[7] Symetrie sphere: theta=0 vs theta=60 (tau=2, isotrope)"
run_sim "$TMPDIR/test7a" --shape sphere --tau 2 --theta 0 --absorb 0.0 \
    --g1 0.0 --g2 0.0 --f 0.5 --nphotons 200000 --bins 180 --seed 107
extract_flux "$TMPDIR/test7a/angular_distribution.dat"
R0=$R; T0=$T; A0=$A

run_sim "$TMPDIR/test7b" --shape sphere --tau 2 --theta 60 --absorb 0.0 \
    --g1 0.0 --g2 0.0 --f 0.5 --nphotons 200000 --bins 180 --seed 107
extract_flux "$TMPDIR/test7b/angular_distribution.dat"
R60=$R; T60=$T; A60=$A

echo "   theta=0:  R=$R0 T=$T0 A=$A0"
echo "   theta=60: R=$R60 T=$T60 A=$A60"
ok=1
dr=$(awk "BEGIN {d=$R0-$R60; if(d<0)d=-d; printf \"%.6f\", d}")
dt=$(awk "BEGIN {d=$T0-$T60; if(d<0)d=-d; printf \"%.6f\", d}")
da=$(awk "BEGIN {d=$A0-$A60; if(d<0)d=-d; printf \"%.6f\", d}")
echo "   Ecarts: R=$dr T=$dt A=$da"
if awk "BEGIN {exit !($dr < 0.02 && $dt < 0.02 && $da < 0.02)}"; then
    :
else
    ok=0
    echo "   FAIL: symetrie brisee (ecarts > 2%)"
fi
if [ $ok -eq 1 ]; then echo "   PASS"; PASS=$((PASS+1)); else echo "   FAIL"; FAIL=$((FAIL+1)); fi
echo

# ---------------------------------------------------------------------------
# Test 8: Conservation energie pour albedo variable
# Verification que R+T+A=1 pour differentes valeurs d'albedo
# ---------------------------------------------------------------------------
echo "[8] Conservation energie: albedo variable (0.0 a 1.0)"
ok=1
for albedo in 0.0 0.1 0.3 0.5 0.7 0.9 1.0; do
    absorb=$(awk "BEGIN {printf \"%.2f\", 1-$albedo}")
    run_sim "$TMPDIR/test8_${albedo}" --shape sphere --tau 3 --theta 0 --absorb $absorb \
        --g1 0.0 --g2 0.0 --f 0.5 --nphotons 100000 --bins 180 --seed 108
    extract_flux "$TMPDIR/test8_${albedo}/angular_distribution.dat"
    sum=$(awk "BEGIN {printf \"%.6f\", $R+$T+$A}")
    if awk "BEGIN {d=$sum-1; if(d<0)d=-d; exit !(d < 0.01)}"; then
        :
    else
        ok=0
        echo "   FAIL albedo=$albedo: bilan=$sum"
    fi
done
if [ $ok -eq 1 ]; then echo "   PASS (tous albedo)"; PASS=$((PASS+1)); else echo "   FAIL"; FAIL=$((FAIL+1)); fi
echo

# ---------------------------------------------------------------------------
# Test 9: Diffusion avant forte (g=0.9) vs diffusion isotrope
# Pour g=0.9, les photons tendent a continuer vers l'avant
# -> T plus eleve, R plus faible que cas isotrope
# ---------------------------------------------------------------------------
echo "[9] Diffusion avant g=0.9 vs isotrope (tau=2, sans absorption)"
run_sim "$TMPDIR/test9a" --shape sphere --tau 2 --theta 0 --absorb 0.0 \
    --g1 0.9 --g2 0.9 --f 1.0 --nphotons 200000 --bins 180 --seed 109
extract_flux "$TMPDIR/test9a/angular_distribution.dat"
R_fwd=$R; T_fwd=$T

run_sim "$TMPDIR/test9b" --shape sphere --tau 2 --theta 0 --absorb 0.0 \
    --g1 0.0 --g2 0.0 --f 0.5 --nphotons 200000 --bins 180 --seed 109
extract_flux "$TMPDIR/test9b/angular_distribution.dat"
R_iso=$R; T_iso=$T

echo "   g=0.9:  R=$R_fwd T=$T_fwd"
echo "   g=0.0:  R=$R_iso T=$T_iso"
ok=1
if awk "BEGIN {exit !($T_fwd > $T_iso)}"; then
    :
else
    ok=0
    echo "   FAIL: diffusion avant devrait augmenter T (T_fwd=$T_fwd <= T_iso=$T_iso)"
fi
if awk "BEGIN {exit !($R_fwd < $R_iso)}"; then
    :
else
    ok=0
    echo "   FAIL: diffusion avant devrait diminuer R (R_fwd=$R_fwd >= R_iso=$R_iso)"
fi
if [ $ok -eq 1 ]; then echo "   PASS"; PASS=$((PASS+1)); else echo "   FAIL"; FAIL=$((FAIL+1)); fi
echo

# ---------------------------------------------------------------------------
# Test 10: Diffusion arriere forte (g=-0.9)
# Pour g=-0.9, les photons tendent a revenir vers l'arriere
# -> R plus eleve, T plus faible que cas isotrope
# ---------------------------------------------------------------------------
echo "[10] Diffusion arriere g=-0.9 vs isotrope (tau=2, sans absorption)"
run_sim "$TMPDIR/test10a" --shape sphere --tau 2 --theta 0 --absorb 0.0 \
    --g1 -0.9 --g2 -0.9 --f 1.0 --nphotons 200000 --bins 180 --seed 110
extract_flux "$TMPDIR/test10a/angular_distribution.dat"
R_back=$R; T_back=$T

run_sim "$TMPDIR/test10b" --shape sphere --tau 2 --theta 0 --absorb 0.0 \
    --g1 0.0 --g2 0.0 --f 0.5 --nphotons 200000 --bins 180 --seed 110
extract_flux "$TMPDIR/test10b/angular_distribution.dat"
R_iso=$R; T_iso=$T

echo "   g=-0.9: R=$R_back T=$T_back"
echo "   g=0.0:  R=$R_iso T=$T_iso"
ok=1
if awk "BEGIN {exit !($R_back > $R_iso)}"; then
    :
else
    ok=0
    echo "   FAIL: diffusion arriere devrait augmenter R (R_back=$R_back <= R_iso=$R_iso)"
fi
if awk "BEGIN {exit !($T_back < $T_iso)}"; then
    :
else
    ok=0
    echo "   FAIL: diffusion arriere devrait diminuer T (T_back=$T_back >= T_iso=$T_iso)"
fi
if [ $ok -eq 1 ]; then echo "   PASS"; PASS=$((PASS+1)); else echo "   FAIL"; FAIL=$((FAIL+1)); fi
echo

echo "=== Resultat : $PASS OK, $FAIL FAIL ==="
exit $FAIL