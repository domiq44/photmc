/*
 * main.c - Photopmg : simulateur Monte Carlo de transport de photons.
 *
 * Utilisation :
 *   photonmc [options]
 *
 * Options principales :
 *   --shape  {sphere|cube|cylinder}   forme du nuage (défaut: sphere)
 *   --size   L        dimension caractéristique L_char (défaut: 1.0)
 *   --height H        hauteur totale du cylindre (défaut: 2.0)
 *   --tau    tau      épaisseur optique (défaut: 1.0)
 *   --theta  deg      angle d'incidence en degrés (défaut: 0)
 *   --absorb a        probabilité d'absorption par collision (défaut: 0)
 *   --g1     g1       asymétrie composante 1 (défaut: 0.0)
 *   --g2     g2       asymétrie composante 2 (défaut: 0.0)
 *   --f      f        mélange (poids de g1) (défaut: 0.5)
 *   --nphotons N      nombre de photons incidents (défaut: 100000)
 *   --bins   B        nombre de bins angulaires (défaut: 180)
 *   --seed   S        graine aléatoire (défaut: 12345)
 *   --maxsteps M      étapes max par photon (défaut: 100000)
 *   --outdir DIR      dossier de sortie (défaut: ./out)
 *   --scan   theta|tau  active un balayage et écrit flux_vs_<scan>.dat
 *   --from A --to B --scan-steps K   bornes et pas du balayage
 */
#include <errno.h>
#include <getopt.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>

#include "mc.h"
#include "output.h"

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

static void usage(const char* prog)
{
    fprintf(stderr,
            "Usage: %s [options]\n"
            "  --shape {sphere|cube|cylinder}  forme (defaut: sphere)\n"
            "  --size L        dimension caract. L_char (defaut: 1.0)\n"
            "  --height H      hauteur cylindre (defaut: 2.0)\n"
            "  --tau TAU       epaisseur optique (defaut: 1.0)\n"
            "  --theta DEG     angle d'incidence deg (defaut: 0)\n"
            "  --absorb A      absorption par collision (defaut: 0)\n"
            "  --g1 G1 --g2 G2 --f F   double Henyey-Greenstein\n"
            "  --nphotons N    nombre de photons (defaut: 100000)\n"
            "  --bins B        bins angulaires (defaut: 180)\n"
            "  --seed S        graine (defaut: 12345)\n"
            "  --maxsteps M    etapes max (defaut: 100000)\n"
            "  --outdir DIR    dossier de sortie (defaut: ./out)\n"
            "  --scan theta|tau  balayage + ecriture flux_vs_<scan>.dat\n"
            "  --from A --to B --scan-steps K   bornes/pas du balayage\n",
            prog);
}

/* Crée le dossier de sortie s'il n'existe pas. */
static int ensure_dir(const char* path)
{
    struct stat st;
    if (stat(path, &st) == 0)
    {
        return S_ISDIR(st.st_mode);
    }
    if (mkdir(path, 0755) == 0)
    {
        return 1;
    }
    return (errno == EEXIST);
}

/* Copie la config en modifiant theta ou tau pour les scans. */
static void run_single(const MCConfig* base, const char* outdir)
{
    MCResult res;
    char path[1024];

    if (!mc_run(base, &res))
    {
        fprintf(stderr, "Erreur d'allocation mémoire.\n");
        exit(EXIT_FAILURE);
    }

    printf("# resultats (photons_in=%ld)\n", res.photons_in);
    printf("reflected   = %.6f\n", res.reflected);
    printf("transmitted = %.6f\n", res.transmitted);
    printf("absorbed    = %.6f\n", res.absorbed);

    snprintf(path, sizeof(path), "%s/angular_distribution.dat", outdir);
    if (write_angular(path, &res))
        printf("Ecrit: %s\n", path);
    snprintf(path, sizeof(path), "%s/hist_collisions.dat", outdir);
    if (write_collisions(path, &res))
        printf("Ecrit: %s\n", path);
    snprintf(path, sizeof(path), "%s/profondeur.dat", outdir);
    if (write_depth(path, &res))
        printf("Ecrit: %s\n", path);
    mc_result_free(&res);
}

static void run_scan(const MCConfig* base, const char* outdir, const char* scan, double from,
                     double to, int steps)
{
    MCConfig cfg = *base;
    char path[1024];
    int i, n;

    if (steps < 1)
        steps = 1;
    n = steps;
    snprintf(path, sizeof(path), "%s/flux_vs_%s.dat", outdir, scan);
    /* supprime un éventuel ancien fichier pour repartir proprement */
    remove(path);

    for (i = 0; i < n; i++)
    {
        double x = (n == 1) ? from : from + (to - from) * (double)i / (double)(n - 1);
        MCResult res;
        cfg = *base;
        if (strcmp(scan, "theta") == 0)
        {
            cfg.theta_deg = x;
        }
        else
        { /* tau */
            cfg.tau = x;
        }
        if (!mc_run(&cfg, &res))
        {
            fprintf(stderr, "Erreur d'allocation mémoire.\n");
            exit(EXIT_FAILURE);
        }
        write_flux_line(path, x, &res);
        mc_result_free(&res);
        printf("  %s=%g  R=%.4f T=%.4f A=%.4f\n", scan, x, res.reflected, res.transmitted,
               res.absorbed);
    }
    printf("Ecrit: %s\n", path);
}

int main(int argc, char** argv)
{
    MCConfig cfg;
    const char* outdir = "out";
    const char* scan = NULL;
    double scan_from = 0.0, scan_to = 89.0;
    int scan_steps = 45;
    int opt;

    /* valeurs par défaut */
    cfg.geom.type = SHAPE_SPHERE;
    cfg.geom.size = 1.0;
    cfg.geom.height = 2.0;
    cfg.tau = 1.0;
    cfg.theta_deg = 0.0;
    cfg.absorb = 0.0;
    cfg.phase.g1 = 0.0;
    cfg.phase.g2 = 0.0;
    cfg.phase.f = 0.5;
    cfg.nphotons = 100000L;
    cfg.nbins = 180;
    cfg.seed = 12345;
    cfg.maxsteps = 100000;

    static struct option long_opts[] = {
        {"shape", required_argument, 0, 's'},    {"size", required_argument, 0, 'z'},
        {"height", required_argument, 0, 'H'},   {"tau", required_argument, 0, 't'},
        {"theta", required_argument, 0, 'i'},    {"absorb", required_argument, 0, 'a'},
        {"g1", required_argument, 0, '1'},       {"g2", required_argument, 0, '2'},
        {"f", required_argument, 0, 'f'},        {"nphotons", required_argument, 0, 'n'},
        {"bins", required_argument, 0, 'b'},     {"seed", required_argument, 0, 'S'},
        {"maxsteps", required_argument, 0, 'm'}, {"outdir", required_argument, 0, 'o'},
        {"scan", required_argument, 0, 'c'},     {"from", required_argument, 0, 'F'},
        {"to", required_argument, 0, 'T'},       {"scan-steps", required_argument, 0, 'K'},
        {"help", no_argument, 0, 'h'},           {0, 0, 0, 0}};

    while ((opt = getopt_long(argc, argv, "h", long_opts, NULL)) != -1)
    {
        switch (opt)
        {
        case 's':
            if (strcmp(optarg, "sphere") == 0)
                cfg.geom.type = SHAPE_SPHERE;
            else if (strcmp(optarg, "cube") == 0)
                cfg.geom.type = SHAPE_CUBE;
            else if (strcmp(optarg, "cylinder") == 0)
                cfg.geom.type = SHAPE_CYLINDER;
            else
            {
                fprintf(stderr, "Forme inconnue: %s\n", optarg);
                return EXIT_FAILURE;
            }
            break;
        case 'z':
            cfg.geom.size = atof(optarg);
            break;
        case 'H':
            cfg.geom.height = atof(optarg);
            break;
        case 't':
            cfg.tau = atof(optarg);
            break;
        case 'i':
            cfg.theta_deg = atof(optarg);
            break;
        case 'a':
            cfg.absorb = atof(optarg);
            break;
        case '1':
            cfg.phase.g1 = atof(optarg);
            break;
        case '2':
            cfg.phase.g2 = atof(optarg);
            break;
        case 'f':
            cfg.phase.f = atof(optarg);
            break;
        case 'n':
            cfg.nphotons = atol(optarg);
            break;
        case 'b':
            cfg.nbins = atoi(optarg);
            break;
        case 'S':
            cfg.seed = (uint32_t)strtoul(optarg, NULL, 0);
            break;
        case 'm':
            cfg.maxsteps = atoi(optarg);
            break;
        case 'o':
            outdir = optarg;
            break;
        case 'c':
            scan = optarg;
            break;
        case 'F':
            scan_from = atof(optarg);
            break;
        case 'T':
            scan_to = atof(optarg);
            break;
        case 'K':
            scan_steps = atoi(optarg);
            break;
        case 'h':
            usage(argv[0]);
            return EXIT_SUCCESS;
        default:
            usage(argv[0]);
            return EXIT_FAILURE;
        }
    }

    /* validations */
    if (cfg.absorb < 0.0)
        cfg.absorb = 0.0;
    if (cfg.absorb > 1.0)
        cfg.absorb = 1.0;
    if (cfg.phase.f < 0.0)
        cfg.phase.f = 0.0;
    if (cfg.phase.f > 1.0)
        cfg.phase.f = 1.0;
    if (cfg.phase.g1 < -1.0 || cfg.phase.g1 > 1.0)
    {
        fprintf(stderr, "g1 doit etre dans [-1, 1]\n");
        return EXIT_FAILURE;
    }
    if (cfg.phase.g2 < -1.0 || cfg.phase.g2 > 1.0)
    {
        fprintf(stderr, "g2 doit etre dans [-1, 1]\n");
        return EXIT_FAILURE;
    }
    if (cfg.geom.size <= 0.0)
    {
        fprintf(stderr, "size (L_char) doit etre > 0\n");
        return EXIT_FAILURE;
    }
    if (cfg.geom.height <= 0.0)
    {
        fprintf(stderr, "height (cylindre) doit etre > 0\n");
        return EXIT_FAILURE;
    }
    if (cfg.theta_deg < 0.0 || cfg.theta_deg > 90.0)
    {
        fprintf(stderr, "theta doit etre dans [0, 90] deg\n");
        return EXIT_FAILURE;
    }
    if (cfg.tau <= 0.0)
    {
        fprintf(stderr, "tau doit etre > 0\n");
        return EXIT_FAILURE;
    }
    if (cfg.nphotons <= 0)
        cfg.nphotons = 1;
    if (cfg.nphotons > 1000000000L)
        cfg.nphotons = 1000000000L;
    if (cfg.nbins < 1)
        cfg.nbins = 1;
    if (cfg.nbins > 3600)
        cfg.nbins = 3600;
    if (cfg.maxsteps < 1)
        cfg.maxsteps = 1;
    if (cfg.maxsteps > 1000000)
        cfg.maxsteps = 1000000;

    if (scan != NULL)
    {
        if (strcmp(scan, "theta") != 0 && strcmp(scan, "tau") != 0)
        {
            fprintf(stderr, "--scan doit valoir 'theta' ou 'tau'\n");
            return EXIT_FAILURE;
        }
        if (scan_from > scan_to)
        {
            fprintf(stderr, "--from doit etre <= --to\n");
            return EXIT_FAILURE;
        }
        if (scan_steps < 1)
            scan_steps = 1;
    }

    if (!ensure_dir(outdir))
    {
        fprintf(stderr, "Impossible de creer le dossier: %s\n", outdir);
        return EXIT_FAILURE;
    }

    if (scan != NULL)
    {
        printf("=== Balayage %s [%g .. %g] (%d points) ===\n", scan, scan_from, scan_to,
               scan_steps);
        run_scan(&cfg, outdir, scan, scan_from, scan_to, scan_steps);
    }
    else
    {
        run_single(&cfg, outdir);
    }
    return EXIT_SUCCESS;
}