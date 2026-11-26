set terminal pngcairo size 900,600 enhanced
set output 'out/phase_function.png'
set title 'Fonction de phase double HG'
set xlabel 'cos(theta)'
set ylabel 'p(cos theta)'
set grid
set xrange [-1:1]
g1=0.8; g2=-0.3; f=0.7
hg(x,g)=(1-g*g)/(4*pi*(1+g*g-2*g*x)**1.5)
p(x)=f*hg(x,g1)+(1-f)*hg(x,g2)
plot p(x) title 'melange', f*hg(x,g1) title 'p_HG(g1)', (1-f)*hg(x,g2) title 'p_HG(g2)'
