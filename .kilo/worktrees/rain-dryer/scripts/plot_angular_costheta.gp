set terminal pngcairo size 900,600 enhanced
set output 'out/angular_costheta.png'
set title 'Densite en cos(theta)'
set xlabel 'cos(theta)'
set ylabel 'p(cos theta)'
set grid
plot 'out/angular_distribution.dat' using (cos($1*pi/180)):($3/(2*pi)) with points title 'p(cos)'
