set terminal pngcairo size 900,600 enhanced
set output 'out/angular_log.png'
set title 'Distribution angulaire (log)'
set xlabel 'angle (deg)'
set ylabel 'densite'
set logscale y
set grid
plot 'out/angular_distribution.dat' using 1:3 with boxes title 'densite'
