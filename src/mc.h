/*
 * mc.h - Moteur Monte Carlo de transport de photons (méthode des poids).
 */
#ifndef MC_H
#define MC_H

#include "geometry.h"
#include "phase.h"
#include "rng.h"

typedef struct
{
    Geometry geom;       /* forme du nuage */
    double tau;          /* épaisseur optique (le long de L_char) */
    double theta_deg;    /* angle d'incidence du faisceau (deg, / normale) */
    double absorb;       /* probabilité d'absorption par collision, [0,1] */
    PhaseFunction phase; /* fonction de phase double HG */
    long nphotons;       /* nombre de photons incidents */
    int nbins;           /* nombre de bins angulaires de sortie */
    uint32_t seed;       /* graine aléatoire */
    int maxsteps;        /* nombre max d'étapes par photon */
} MCConfig;

typedef struct
{
    double reflected;      /* fraction de poids ressortie côté entrée */
    double transmitted;    /* fraction de poids ressortie côté sortie */
    double absorbed;       /* 1 - reflected - transmitted */
    double out_weight;     /* poids total sortant (somme) / N */
    double* hist_weight;   /* somme des poids par bin angulaire */
    double* hist_count;    /* nombre de photons par bin angulaire */
    double bin_width_rad;  /* largeur d'un bin en radians */
    int nbins;             /* nombre de bins angulaires */
    long photons_in;       /* photons effectivement injectés (ayant touché) */
    long maxsteps_reached; /* photons qui ont atteint maxsteps sans sortir */
    /* instrumentation */
    long* hist_collisions;       /* nb de photons par nombre de collisions */
    int max_collisions;          /* taille de hist_collisions */
    double mean_collisions;      /* nombre moyen de collisions par photon */
    double* hist_depth;          /* poids par bin de profondeur (projection) */
    int ndepth;                  /* nombre de bins de profondeur */
    double depth_min, depth_max; /* bornes de l'histogramme de profondeur */
    double mean_depth;           /* profondeur moyenne de sortie (proj. axe) */
} MCResult;

/* Alloue (si besoin) et exécute la simulation. hist_* sont alloués par la fonction. */
int mc_run(const MCConfig* cfg, MCResult* res);

/* Libère les tableaux d'histogramme d'un résultat. */
void mc_result_free(MCResult* res);

#endif /* MC_H */