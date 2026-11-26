/*
 * geometry.c - Implémentation des intersections rayon/forme.
 */
#include "geometry.h"
#include <math.h>

double geo_bound_radius(const Geometry* g)
{
    switch (g->type)
    {
    case SHAPE_SPHERE:
        return g->size;
    case SHAPE_CUBE:
        /* distance du centre à un sommet */
        return g->size * 1.7320508075688772; /* sqrt(3) */
    case SHAPE_CYLINDER:
        return sqrt(g->size * g->size + (0.5 * g->height) * (0.5 * g->height));
    default:
        return 0.0;
    }
}

int geo_contains(const Geometry* g, double x, double y, double z, double eps)
{
    switch (g->type)
    {
    case SHAPE_SPHERE:
        return (x * x + y * y + z * z) < (g->size - eps) * (g->size - eps);
    case SHAPE_CUBE:
        return (fabs(x) < g->size - eps) && (fabs(y) < g->size - eps) && (fabs(z) < g->size - eps);
    case SHAPE_CYLINDER:
    {
        double hz = 0.5 * g->height;
        double r2 = x * x + y * y;
        return (r2 < (g->size - eps) * (g->size - eps)) && (fabs(z) < hz - eps);
    }
    default:
        return 0;
    }
}

/*
 * Sphère centrée en O de rayon R :
 *   |p + t d|^2 = R^2  ->  t^2 + 2(p.d)t + (|p|^2 - R^2) = 0
 */
static int intersect_sphere(const Geometry* g, double px, double py, double pz, double dx,
                            double dy, double dz, double* t_enter, double* t_exit)
{
    double R = g->size;
    double b = px * dx + py * dy + pz * dz;         /* p.d (car d normé) */
    double c = px * px + py * py + pz * pz - R * R; /* |p|^2 - R^2 */
    double disc = b * b - c;
    double sq;

    if (disc < 0.0)
    {
        return 0;
    }
    sq = sqrt(disc);
    *t_enter = -b - sq;
    *t_exit = -b + sq;
    return 1;
}

/*
 * Cube = boîte AABB [-L, L]^3 centrée en O : méthode des slabs.
 */
static int intersect_box(const Geometry* g, double px, double py, double pz, double dx, double dy,
                         double dz, double* t_enter, double* t_exit)
{
    const double L = g->size;
    double tmin = -INFINITY, tmax = INFINITY;
    double origin[3] = {px, py, pz};
    double dir[3] = {dx, dy, dz};
    int i;

    for (i = 0; i < 3; i++)
    {
        if (fabs(dir[i]) < 1e-15)
        {
            /* rayon parallèle au plan : doit déjà être dans le slab */
            if (origin[i] < -L || origin[i] > L)
            {
                return 0;
            }
        }
        else
        {
            double inv = 1.0 / dir[i];
            double t1 = (-L - origin[i]) * inv;
            double t2 = (L - origin[i]) * inv;
            if (t1 > t2)
            {
                double tmp = t1;
                t1 = t2;
                t2 = tmp;
            }
            if (t1 > tmin)
                tmin = t1;
            if (t2 < tmax)
                tmax = t2;
            if (tmin > tmax)
            {
                return 0;
            }
        }
    }
    *t_enter = tmin;
    *t_exit = tmax;
    return 1;
}

/*
 * Cylindre d'axe z, rayon R, |z| <= H/2.
 * Latéral : (px + t dx)^2 + (py + t dy)^2 = R^2.
 * Capsules : z = ±H/2.
 * On combine racines latérales et des capsules pour obtenir l'intervalle.
 */
static int intersect_cylinder(const Geometry* g, double px, double py, double pz, double dx,
                              double dy, double dz, double* t_enter, double* t_exit)
{
    const double R = g->size;
    const double hz = 0.5 * g->height;
    double A, B, C, disc;
    double tlat1 = 0.0, tlat2 = 0.0;
    int has_lat;
    double caps[2];
    int nc = 0;
    double best_enter = INFINITY, best_exit = -INFINITY;
    int k;

    /* --- Racines latérales --- */
    A = dx * dx + dy * dy;
    B = 2.0 * (px * dx + py * dy);
    C = px * px + py * py - R * R;
    has_lat = 0;
    if (A > 1e-15)
    {
        disc = B * B - 4.0 * A * C;
        if (disc >= 0.0)
        {
            double sq = sqrt(disc);
            tlat1 = (-B - sq) / (2.0 * A);
            tlat2 = (-B + sq) / (2.0 * A);
            has_lat = 1;
        }
    }
    else
    {
        /* rayon parallèle à l'axe : intersection latérale vide */
        has_lat = 0;
    }

    /* --- Capsules z = ±hz --- */
    if (fabs(dz) > 1e-15)
    {
        caps[nc++] = (-hz - pz) / dz;
        caps[nc++] = (hz - pz) / dz;
    }

    /* On évalue tous les candidats t valides (point sur la surface). */
    for (k = 0; k < nc; k++)
    {
        double t = caps[k];
        double x = px + t * dx, y = py + t * dy;
        if (x * x + y * y <= R * R + 1e-12)
        {
            if (t < best_enter)
                best_enter = t;
            if (t > best_exit)
                best_exit = t;
        }
    }
    if (has_lat)
    {
        double t;
        for (k = 0; k < 2; k++)
        {
            t = (k == 0) ? tlat1 : tlat2;
            double z = pz + t * dz;
            if (fabs(z) <= hz + 1e-12)
            {
                if (t < best_enter)
                    best_enter = t;
                if (t > best_exit)
                    best_exit = t;
            }
        }
    }

    if (best_enter == (double)INFINITY || best_exit == -(double)INFINITY || best_exit < best_enter)
    {
        return 0;
    }
    *t_enter = best_enter;
    *t_exit = best_exit;
    return 1;
}

int geo_ray_intersect(const Geometry* g, double px, double py, double pz, double dx, double dy,
                      double dz, double* t_enter, double* t_exit)
{
    switch (g->type)
    {
    case SHAPE_SPHERE:
        return intersect_sphere(g, px, py, pz, dx, dy, dz, t_enter, t_exit);
    case SHAPE_CUBE:
        return intersect_box(g, px, py, pz, dx, dy, dz, t_enter, t_exit);
    case SHAPE_CYLINDER:
        return intersect_cylinder(g, px, py, pz, dx, dy, dz, t_enter, t_exit);
    default:
        return 0;
    }
}