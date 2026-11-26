set terminal pngcairo size 900,600 enhanced
set output 'out/rapport_rt.png'
set title 'Rapport R/T et T/R vs tau'
set xlabel 'tau'
set ylabel 'rapport'
set logscale y
set grid
plot 'out/flux_vs_tau.dat' using 1:($2/$3) title 'R/T', '' using 1:($3/$2) title 'T/R'
