# plot_flux_tau.gp - Flux vs epaisseur optique tau.
# Genere out/flux_vs_tau.png
set terminal pngcairo size 900,600 enhanced font 'sans,12'
set output 'out/flux_vs_tau.png'
set title 'Flux vs epaisseur optique tau'
set xlabel 'epaisseur optique tau'
set ylabel 'flux (fraction du faisceau incident)'
set grid
set key top right
set yrange [0:1]
set datafile commentschars "#"
plot 'out/flux_vs_tau.dat' using 1:2 with linespoints lw 2 pt 6 title 'Reflech i', \
     'out/flux_vs_tau.dat' using 1:3 with linespoints lw 2 pt 8 title 'Transmis', \
     'out/flux_vs_tau.dat' using 1:4 with linespoints lw 2 pt 4 title 'Absorbe'