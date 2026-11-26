/*
 * phase.h - Fonction de phase double Henyey-Greenstein.
 *
 *   p(cos th) = f * p_HG(cos th; g1) + (1 - f) * p_HG(cos th; g2)
 *   p_HG(mu; g) = (1 - g^2) / [4 pi (1 + g^2 - 2 g mu)^(3/2)]
 *
 * L'échantillonnage se fait en tirant d'abord la composante (g1 avec proba f,
 * g2 sinon) puis en inversant la CDF de la HG correspondante.
 */
#ifndef PHASE_H
#define PHASE_H

typedef struct
{
    double g1; /* premier paramètre d'asymétrie, [-1, 1] */
    double g2; /* second paramètre d'asymétrie, [-1, 1] */
    double f;  /* coefficient de mélange, [0, 1] (poids de g1) */
} PhaseFunction;

/* Évalue la fonction de phase (par stéré radian) pour un cosinus donné. */
double phase_eval(const PhaseFunction* ph, double cos_theta);

/*
 * Échantillonne un nouveau cos(theta) selon la double HG,
 * à partir d'un flottant uniforme u dans [0, 1).
 */
double phase_sample_cos(const PhaseFunction* ph, double u);

#endif /* PHASE_H */