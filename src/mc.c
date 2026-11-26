/*
 * mc.c - Moteur Monte Carlo de transport de photons.
 *
 * Principe (méthode des poids / survival biasing) :
 *  - Un photon part de la surface d'entrée avec un poids w = 1.
 *  - À chaque collision, au lieu de tuer le photon avec probabilité a,
 *    on conserve le photon mais on multiplie son poids par (1 - a) :
 *        w <- w * (1 - a)
 *    L'espérance de contribution est ainsi préservée (aucun photon gaspillé),
 *    et l'énergie absorbée est implicitement 1 - (somme des poids sortants).
 *  - Le photon est ensuite diffusé selon la double Henyey-Greenstein.
 *
 * Géométrie : toutes les longueurs sont en unités de L_char = 1,
 * donc le coefficient d'atténuation vaut mu = tau.
 */
#include "mc.h"
#include <math.h>
#include <stdlib.h>
#include <string.h>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

/* Construit une base orthonormée (u, v, d) avec d = direction normalisée. */
static void build_basis(double dx, double dy, double dz, double* ux, double* uy, double* uz,
                        double* vx, double* vy, double* vz)
{
    double ax, ay, az;
    /* choisit un vecteur auxiliaire non colinéaire */
    if (fabs(dz) < 0.9)
    {
        ax = 0.0;
        ay = 0.0;
        az = 1.0;
    }
    else
    {
        ax = 1.0;
        ay = 0.0;
        az = 0.0;
    }

    /* u = normalize(a x d) */
    *ux = ay * dz - az * dy;
    *uy = az * dx - ax * dz;
    *uz = ax * dy - ay * dx;
    {
        double n = sqrt((*ux) * (*ux) + (*uy) * (*uy) + (*uz) * (*uz));
        *ux /= n;
        *uy /= n;
        *uz /= n;
    }
    /* v = d x u */
    *vx = dy * (*uz) - dz * (*uy);
    *vy = dz * (*ux) - dx * (*uz);
    *vz = dx * (*uy) - dy * (*ux);
}

/* Tourne la direction courante (ux,uy,uz) d'un angle (cos_t, sin_t, phi). */
static void rotate_direction(double ux, double uy, double uz, double cos_t, double sin_t,
                             double phi, double* nx, double* ny, double* nz)
{
    double ax, ay, az, bx, by, bz;
    double cp = cos(phi), sp = sin(phi);

    build_basis(ux, uy, uz, &ax, &ay, &az, &bx, &by, &bz);

    *nx = cos_t * ux + sin_t * (cp * ax + sp * bx);
    *ny = cos_t * uy + sin_t * (cp * ay + sp * by);
    *nz = cos_t * uz + sin_t * (cp * az + sp * bz);
}

/*
 * Injecte un photon : point aléatoire sur le disque projeté perpendiculaire au
 * faisceau, reculé en amont, puis on cherche la première entrée dans la forme.
 * Retourne 1 si le photon a touché le nuage (pos, dir renseignés), 0 sinon.
 */
static int inject_photon(const MCConfig* cfg, RNG* rng, const double* d0, double* pos, double* dir)
{
    const Geometry* g = &cfg->geom;
    double rb = geo_bound_radius(g);
    double margin = 1.0 + rb; /* recule largement en amont */
    double ux, uy, uz, vx, vy, vz;
    double r, phi, px, py, sx, sy, sz;
    double t_enter, t_exit;

    /* base perpendiculaire au faisceau */
    build_basis(d0[0], d0[1], d0[2], &ux, &uy, &uz, &vx, &vy, &vz);

    /* point uniforme sur le disque de rayon rb (sqrt pour l'uniformité surfacique) */
    r = rb * sqrt(rng_double((RNG*)rng));
    phi = 2.0 * M_PI * rng_double((RNG*)rng);
    px = r * cos(phi);
    py = r * sin(phi);

    /* point de départ : centre + décalage transverse - d0 * margin (en amont) */
    sx = px * ux + py * vx - d0[0] * margin;
    sy = px * uy + py * vy - d0[1] * margin;
    sz = px * uz + py * vz - d0[2] * margin;

    /* cherche l'intersection avant ; le point est en amont, t_enter doit être > 0 */
    if (!geo_ray_intersect(g, sx, sy, sz, d0[0], d0[1], d0[2], &t_enter, &t_exit))
    {
        return 0; /* rate le nuage -> photon rejeté */
    }
    if (t_enter < 0.0)
    {
        /* Le point de depart est deja a l'interieur du volume : injection
         * invalide, on rejette le photon. */
        return 0;
    }
    pos[0] = sx + d0[0] * t_enter;
    pos[1] = sy + d0[1] * t_enter;
    pos[2] = sz + d0[2] * t_enter;
    dir[0] = d0[0];
    dir[1] = d0[1];
    dir[2] = d0[2];
    return 1;
}

int mc_run(const MCConfig* cfg, MCResult* res)
{
    RNG rng;
    const double mu = cfg->tau; /* L_char = 1 -> mu = tau */
    const double albedo = 1.0 - cfg->absorb;
    const int nbins = cfg->nbins;
    const double theta = cfg->theta_deg * M_PI / 180.0;
    /* direction de propagation du faisceau : angle theta / normale z */
    const double d0[3] = {sin(theta), 0.0, cos(theta)};
    double sum_w_ref = 0.0, sum_w_trans = 0.0, sum_w_out = 0.0;
    long p;

    memset(res, 0, sizeof(*res));
    res->nbins = nbins;
    res->bin_width_rad = M_PI / (double)nbins;

    res->hist_weight = (double*)calloc((size_t)nbins, sizeof(double));
    res->hist_count = (double*)calloc((size_t)nbins, sizeof(double));
/* Borner la taille de l'histogramme des collisions pour eviter une
 * allocation O(maxsteps) potentiellement enorme. Les valeurs au-dela du
 * cap sont regroupees dans le dernier bin. */
#define MC_COLLISIONS_CAP 1001
    res->max_collisions =
        (cfg->maxsteps + 1 < MC_COLLISIONS_CAP) ? (cfg->maxsteps + 1) : MC_COLLISIONS_CAP;
    if (res->max_collisions < 1)
        res->max_collisions = 1;
    res->hist_collisions = (long*)calloc((size_t)res->max_collisions, sizeof(long));
    {
        /* La profondeur de sortie (projection sur l'axe du faisceau) est bornee
         * par le rayon de la sphère englobante : |proj| <= |pos| <= bound_radius.
         * On adapte la plage de l'histogramme a la geometrie au lieu de fixer
         * [-5, 5] en dur. */
        double br = geo_bound_radius(&cfg->geom);
        res->ndepth = 100;
        res->depth_min = -br;
        res->depth_max = br;
        if (!(res->depth_max > res->depth_min))
        {
            res->depth_min = -1.0;
            res->depth_max = 1.0;
        }
    }
    res->hist_depth = (double*)calloc((size_t)res->ndepth, sizeof(double));
    if (!res->hist_weight || !res->hist_count || !res->hist_collisions || !res->hist_depth)
    {
        free(res->hist_weight);
        free(res->hist_count);
        free(res->hist_collisions);
        free(res->hist_depth);
        res->hist_weight = NULL;
        res->hist_count = NULL;
        res->hist_collisions = NULL;
        res->hist_depth = NULL;
        return 0;
    }

    rng_init(&rng, cfg->seed);

    for (p = 0; p < cfg->nphotons; p++)
    {
        double pos[3], dir[3];
        int steps;
        double w = 1.0;
        int injected;
        int ncoll = 0;

        injected = inject_photon(cfg, &rng, d0, pos, dir);
        if (!injected)
        {
            continue; /* photon manqué : non compté comme incident */
        }
        res->photons_in++;

        for (steps = 0; steps < cfg->maxsteps; steps++)
        {
            double t_enter, t_exit, L, s;

            if (!geo_ray_intersect(&cfg->geom, pos[0], pos[1], pos[2], dir[0], dir[1], dir[2],
                                   &t_enter, &t_exit))
            {
                /* ne devrait pas arriver (point intérieur) : on abandonne */
                break;
            }
            L = t_exit; /* distance restante jusqu'à la surface (point intérieur) */

            /* libre parcours exponentiel de paramètre mu */
            s = -log(1.0 - rng_double(&rng)) / mu;

            if (s < L)
            {
                /* collision dans le volume */
                double cos_t, sin_t, phi;
                pos[0] += dir[0] * s;
                pos[1] += dir[1] * s;
                pos[2] += dir[2] * s;

                /* méthode des poids : on survit avec un poids réduit */
                w *= albedo;
                ncoll++;

                /* direction diffusée */
                cos_t = phase_sample_cos(&cfg->phase, rng_double(&rng));
                if (cos_t > 1.0)
                    cos_t = 1.0;
                if (cos_t < -1.0)
                    cos_t = -1.0;
                sin_t = sqrt(fmax(0.0, 1.0 - cos_t * cos_t));
                phi = 2.0 * M_PI * rng_double(&rng);
                {
                    double nx, ny, nz;
                    rotate_direction(dir[0], dir[1], dir[2], cos_t, sin_t, phi, &nx, &ny, &nz);
                    dir[0] = nx;
                    dir[1] = ny;
                    dir[2] = nz;
                }
            }
            else
            {
                /* sortie du nuage */
                double proj, cos_out, theta_out;
                int bin;

                pos[0] += dir[0] * L;
                pos[1] += dir[1] * L;
                pos[2] += dir[2] * L;

                /* classification réfléchi / transmis via projection sur l'axe faisceau */
                proj = pos[0] * d0[0] + pos[1] * d0[1] + pos[2] * d0[2];
                if (proj < 0.0)
                {
                    sum_w_ref += w;
                }
                else
                {
                    sum_w_trans += w;
                }
                sum_w_out += w;

                /* instrumentation : collisions et profondeur */
                {
                    int ci = (ncoll < res->max_collisions) ? ncoll : res->max_collisions - 1;
                    res->hist_collisions[ci]++;
                    res->mean_collisions += (double)ncoll;
                }
                {
                    int di = (int)((proj - res->depth_min) / (res->depth_max - res->depth_min) *
                                   (double)res->ndepth);
                    if (di < 0)
                        di = 0;
                    if (di >= res->ndepth)
                        di = res->ndepth - 1;
                    res->hist_depth[di] += w;
                    res->mean_depth += w * proj;
                }

                /* angle de diffusion global entre direction incidente et sortante */
                cos_out = dir[0] * d0[0] + dir[1] * d0[1] + dir[2] * d0[2];
                if (cos_out > 1.0)
                    cos_out = 1.0;
                if (cos_out < -1.0)
                    cos_out = -1.0;
                theta_out = acos(cos_out); /* [0, pi] */
                bin = (int)(theta_out / res->bin_width_rad);
                if (bin < 0)
                    bin = 0;
                if (bin >= nbins)
                    bin = nbins - 1;
                res->hist_weight[bin] += w;
                res->hist_count[bin] += 1.0;

                break; /* photon quitté */
            }
        }
        if (steps == cfg->maxsteps)
        {
            res->maxsteps_reached++;
        }
        /* si maxsteps atteint : le poids restant est considéré comme absorbé
         * (on ne l'ajoute pas aux flux sortants). */
    }

    {
        double N = (double)res->photons_in;
        if (N > 0.0)
        {
            res->reflected = sum_w_ref / N;
            res->transmitted = sum_w_trans / N;
            res->out_weight = res->reflected + res->transmitted;
            res->absorbed = 1.0 - res->out_weight;
            if (res->photons_in > 0)
            {
                res->mean_collisions /= (double)res->photons_in;
            }
            if (sum_w_out > 0.0)
            {
                /* mean_depth accumule sum(w * proj) : la moyenne ponderee correcte
                 * est sum(w*proj) / sum(w), pas / N. */
                res->mean_depth /= sum_w_out;
            }
            /* densité par radian : (w/N) / delta_theta */
            {
                int b;
                for (b = 0; b < nbins; b++)
                {
                    res->hist_weight[b] /= N; /* poids moyen par bin */
                }
            }
        }
    }
    return 1;
}

void mc_result_free(MCResult* res)
{
    free(res->hist_weight);
    free(res->hist_count);
    free(res->hist_collisions);
    free(res->hist_depth);
    res->hist_weight = NULL;
    res->hist_count = NULL;
    res->hist_collisions = NULL;
    res->hist_depth = NULL;
}