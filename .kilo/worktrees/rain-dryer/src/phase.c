/*
 * phase.c - Fonction de phase double Henyey-Greenstein.
 */
#include "phase.h"
#include <math.h>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

/* Clampe g strictement dans (-1, 1) pour eviter toute division par zero
 * dans l'inverse de la CDF de HG (cas g = +/- 1 avec u aux bornes). */
static double clamp_g(double g)
{
    const double eps = 1.0 - 1e-12;
    if (g > eps)
        return eps;
    if (g < -eps)
        return -eps;
    return g;
}

/* p_HG(mu; g) par stéré-radian. */
static double hg_value(double mu, double g)
{
    double g2 = g * g;
    double denom = 1.0 + g2 - 2.0 * g * mu;
    /* denom >= (1-|g|)^2 > 0 tant que |g| < 1 */
    return (1.0 - g2) / (4.0 * M_PI * denom * sqrt(denom));
}

double phase_eval(const PhaseFunction* ph, double cos_theta)
{
    return ph->f * hg_value(cos_theta, clamp_g(ph->g1)) +
           (1.0 - ph->f) * hg_value(cos_theta, clamp_g(ph->g2));
}

/*
 * Inverse de la CDF de la HG.
 *   mu = 1/(2g) * [ 1 + g^2 - ( (1 - g^2) / (1 - g + 2 g u) )^2 ]
 * Cas limite g -> 0 : mu = 2u - 1 (diffusion isotrope).
 */
static double hg_sample_cos(double g, double u)
{
    double g2 = g * g;
    if (fabs(g) < 1e-8)
    {
        return 2.0 * u - 1.0;
    }
    {
        double t = (1.0 - g2) / (1.0 - g + 2.0 * g * u);
        return (1.0 + g2 - t * t) / (2.0 * g);
    }
}

double phase_sample_cos(const PhaseFunction* ph, double u)
{
    /* Choix de la composante : g1 avec probabilite f. */
    if (u < ph->f)
    {
        /* u sur [0,f) : renormaliser sur [0,1) pour inverser la HG(g1). */
        double uu = (ph->f > 0.0) ? u / ph->f : 0.0;
        return hg_sample_cos(clamp_g(ph->g1), uu);
    }
    else
    {
        double uu = (ph->f < 1.0) ? (u - ph->f) / (1.0 - ph->f) : 0.0;
        return hg_sample_cos(clamp_g(ph->g2), uu);
    }
}