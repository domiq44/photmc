set terminal pngcairo size 900,600 enhanced
set output 'out/profondeur.png'
set title 'Profondeur de sortie'
set xlabel 'profondeur'
set ylabel 'poids'
set grid
plot 'out/profondeur.dat' using 1:2 with boxes title 'profondeur'
