/*
 * output.c - Écriture des fichiers .dat pour gnuplot.
 */
#include "output.h"
#include <stdio.h>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

int write_angular(const char* path, const MCResult* res)
{
    FILE* fp = fopen(path, "w");
    int b;

    if (!fp)
    {
        return 0;
    }
    fprintf(fp, "# Distribution angulaire de sortie (fichier gnuplot)\n");
    fprintf(fp, "# angle_deg   poids_moyen   densite_par_radian   nb_photons\n");
    fprintf(fp, "# R=%.6f T=%.6f A=%.6f  (out_weight=%.6f)\n", res->reflected, res->transmitted,
            res->absorbed, res->out_weight);
    fprintf(fp, "# photons_in=%ld  nbins=%d\n", res->photons_in, res->nbins);

    for (b = 0; b < res->nbins; b++)
    {
        double angle_deg = ((double)b + 0.5) * res->bin_width_rad * 180.0 / M_PI;
        double dens = res->hist_weight[b] / res->bin_width_rad;
        fprintf(fp, "%.6f  %.6e  %.6e  %.0f\n", angle_deg, res->hist_weight[b], dens,
                res->hist_count[b]);
    }
    fclose(fp);
    return 1;
}

int write_flux_line(const char* path, double x, const MCResult* res)
{
    /* Vérifie l'existence pour écrire l'en-tête une seule fois. */
    FILE* test = fopen(path, "r");
    int exists = (test != NULL);
    FILE* fp;

    if (test)
    {
        fclose(test);
    }
    fp = fopen(path, "a");
    if (!fp)
    {
        return 0;
    }
    if (!exists)
    {
        fprintf(fp, "# Scan de flux vs parametre (fichier gnuplot)\n");
        fprintf(fp, "# x   reflect   transmis   absorbe\n");
    }
    fprintf(fp, "%.6f  %.6f  %.6f  %.6f\n", x, res->reflected, res->transmitted, res->absorbed);
    fclose(fp);
    return 1;
}

int write_collisions(const char* path, const MCResult* res)
{
    FILE* fp = fopen(path, "w");
    int i;
    if (!fp)
        return 0;
    fprintf(fp, "# Histogramme du nombre de collisions par photon\n");
    fprintf(fp, "# n_collisions  nb_photons\n");
    fprintf(fp, "# mean_collisions=%.6f\n", res->mean_collisions);
    for (i = 0; i < res->max_collisions; i++)
    {
        if (res->hist_collisions[i] > 0)
            fprintf(fp, "%d  %ld\n", i, res->hist_collisions[i]);
    }
    fclose(fp);
    return 1;
}

int write_depth(const char* path, const MCResult* res)
{
    FILE* fp = fopen(path, "w");
    int i;
    if (!fp)
        return 0;
    fprintf(fp, "# Profondeur de sortie (projection le long de l'axe)\n");
    fprintf(fp, "# depth  poids\n");
    fprintf(fp, "# mean_depth=%.6f\n", res->mean_depth);
    for (i = 0; i < res->ndepth; i++)
    {
        double d =
            res->depth_min + (i + 0.5) * (res->depth_max - res->depth_min) / (double)res->ndepth;
        fprintf(fp, "%.6f  %.6e\n", d, res->hist_depth[i]);
    }
    fclose(fp);
    return 1;
}
