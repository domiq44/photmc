# plot_comparaison_formes.gp - Comparaison sphere/cube/cylinder vs tau.
# Objectif : lancer 3 scans avec --outdir distincts, puis superposer R, T, A
# sur un meme graphe.
# Usage (depuis la racine du projet) :
#   gnuplot -e "outdir1='out_formes_sphere'; outdir2='out_formes_cube'; outdir3='out_formes_cyl'" scripts/plot_comparaison_formes.gp
#
# Si les variables outdir1/2/3 ne sont pas definies, on utilise par defaut
# out/sphere, out/cube, out/cylinder (crees par des runs separes).

set terminal pngcairo size 900,600 enhanced font 'sans,12'
set datafile commentschars "#"

# Chemins par defaut (surchargeables avec -e).
if (!exists("outdir1")) outdir1 = "out/sphere"
if (!exists("outdir2")) outdir2 = "out/cube"
if (!exists("outdir3")) outdir3 = "out/cylinder"

f1 = sprintf("%s/flux_vs_tau.dat", outdir1)
f2 = sprintf("%s/flux_vs_tau.dat", outdir2)
f3 = sprintf("%s/flux_vs_tau.dat", outdir3)

set key top right
set grid
set xlabel "epaisseur optique tau"
set ylabel "fraction (R + T + A)"
set yrange [0:1]

# --- Reflecte (R) ---
set output "out/comparaison_formes_R.png"
set title "Comparaison des formes - Reflechi (R) vs tau"
plot f1 using 1:2 with linespoints lw 2 pt 6 title "sphere", \
     f2 using 1:2 with linespoints lw 2 pt 8 title "cube", \
     f3 using 1:2 with linespoints lw 2 pt 4 title "cylinder"

# --- Transmis (T) ---
set output "out/comparaison_formes_T.png"
set title "Comparaison des formes - Transmis (T) vs tau"
plot f1 using 1:3 with linespoints lw 2 pt 6 title "sphere", \
     f2 using 1:3 with linespoints lw 2 pt 8 title "cube", \
     f3 using 1:3 with linespoints lw 2 pt 4 title "cylinder"

# --- Absorbe (A) ---
set output "out/comparaison_formes_A.png"
set title "Comparaison des formes - Absorbe (A) vs tau"
plot f1 using 1:4 with linespoints lw 2 pt 6 title "sphere", \
     f2 using 1:4 with linespoints lw 2 pt 8 title "cube", \
     f3 using 1:4 with linespoints lw 2 pt 4 title "cylinder"

# --- Multi-courbes R+T+A empilees (sphere, comme reference) ---
set output "out/comparaison_formes_bilan.png"
set title "Comparaison des formes - Bilan R+T+A vs tau (sphere)"
plot f1 using 1:2 with filledcurves y1=0 title "sphere R", \
     '' using 1:($2+$3):2 with filledcurves title "sphere T", \
     '' using 1:($2+$3+$4):($2+$3) with filledcurves title "sphere A"
