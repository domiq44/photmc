/*
 * geometry.h - Abstraction de la forme du nuage.
 *
 * Trois formes supportées : sphère, cube, cylindre. Pour toutes, la dimension
 * caractéristique L_char (rayon R pour la sphère, demi-arête L pour le cube,
 * rayon R pour le cylindre) est l'unité de longueur. L'épaisseur optique tau
 * est définie le long de cette dimension, d'où mu = tau / L_char = tau.
 */
#ifndef GEOMETRY_H
#define GEOMETRY_H

typedef enum
{
    SHAPE_SPHERE = 0,
    SHAPE_CUBE = 1,
    SHAPE_CYLINDER = 2
} ShapeType;

/*
 * Description d'une forme convexe centrée en (0,0,0).
 *  - size   : rayon R (sphère/cylindre) ou demi-arête L (cube).
 *  - height : hauteur totale H du cylindre (ignorée pour sphère/cube).
 * Le cylindre a son axe selon z, rayon `size`, demi-hauteur height/2.
 */
typedef struct
{
    ShapeType type;
    double size;
    double height;
} Geometry;

/* Rayon de la sphère englobante centrée en O (pour l'échantillonnage du faisceau). */
double geo_bound_radius(const Geometry* g);

/* 1 si le point est strictement à l'intérieur (avec marge eps), 0 sinon. */
int geo_contains(const Geometry* g, double x, double y, double z, double eps);

/*
 * Intersection d'un rayon p + t d (d supposé normé) avec la surface.
 * Retourne les deux paramètres d'entrée/sortie (t_enter <= t_exit).
 * Renvoie 1 si le rayon coupe la surface (deux racines réelles), 0 sinon.
 * Convention : si le point p est à l'intérieur, t_enter < 0 < t_exit.
 */
int geo_ray_intersect(const Geometry* g, double px, double py, double pz, double dx, double dy,
                      double dz, double* t_enter, double* t_exit);

#endif /* GEOMETRY_H */