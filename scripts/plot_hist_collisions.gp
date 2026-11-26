set terminal pngcairo size 900,600 enhanced
set output 'out/hist_collisions.png'
set title 'Histogramme collisions'
set xlabel 'nb collisions'
set ylabel 'nb photons'
set grid
plot 'out/hist_collisions.dat' using 1:2 with boxes title 'collisions'
