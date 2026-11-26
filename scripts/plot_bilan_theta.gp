set terminal pngcairo size 900,600 enhanced
set output 'out/bilan_theta.png'
set title 'Bilan R+T+A vs theta'
set xlabel 'theta'
set ylabel 'fraction'
set yrange [0:1]
set grid
plot 'out/flux_vs_theta.dat' using 1:2 with filledcurves y1=0 title 'Reflechi', \
     '' using 1:($2+$3):2 with filledcurves title 'Transmis', \
     '' using 1:($2+$3+$4):($2+$3) with filledcurves title 'Absorbe'
