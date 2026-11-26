# plot_angular.gp - Distribution angulaire de sortie (densite ponderee).
# Genere out/angular_distribution.png
set terminal pngcairo size 900,600 enhanced font 'sans,12'
set output 'out/angular_distribution.png'
set title 'Distribution angulaire de sortie (densite ponderee)'
set xlabel 'angle de diffusion global theta_out (deg)'
set ylabel 'densite (par radian)'
set grid
set key top right
set datafile commentschars "#"
set style fill solid 0.4
set boxwidth 0.8
plot 'out/angular_distribution.dat' using 1:3 with boxes lw 1 title 'densite ponderee'