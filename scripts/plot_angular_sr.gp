set terminal pngcairo size 900,600 enhanced
set output 'out/angular_sr.png'
set title 'Densite par steradian'
set xlabel 'angle (deg)'
set ylabel 'densite/sr'
set grid
plot 'out/angular_distribution.dat' using 1:($3/(2*pi)) with boxes title 'par sr'
