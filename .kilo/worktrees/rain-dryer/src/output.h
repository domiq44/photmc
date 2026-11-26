/*
 * output.h - Écriture des fichiers de données exploitables par gnuplot.
 */
#ifndef OUTPUT_H
#define OUTPUT_H

#include "mc.h"

/* Écrit l'histogramme angulaire pondéré dans un fichier .dat.
 * Colonnes : angle_deg  poids_moyen  densite_par_radian  nombre_photons
 */
int write_angular(const char* path, const MCResult* res);

/* Écrit une ligne de flux (theta, R, T, A) dans un fichier de scan.
 * Si le fichier n'existe pas, écrit un en-tête de commentaire. */
int write_flux_line(const char* path, double x, const MCResult* res);

/* Écrit l'histogramme du nombre de collisions. */
int write_collisions(const char* path, const MCResult* res);
/* Écrit l'histogramme de profondeur de sortie. */
int write_depth(const char* path, const MCResult* res);

#endif /* OUTPUT_H */
