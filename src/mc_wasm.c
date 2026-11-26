/*
 * mc_wasm.c - Interface Emscripten/WebAssembly pour le moteur Monte Carlo.
 *
 * Expose une fonction C callable depuis JavaScript qui exécute une simulation
 * unique et retourne les résultats dans un tampon mémoire WASM alloué par malloc.
 *
 * Layout du tampon retourné (tout en doubles de 8 octets, index i = HEAPF64[ptr/8 + i]) :
 *   [0]  reflected
 *   [1]  transmitted
 *   [2]  absorbed
 *   [3]  out_weight
 *   [4]  photons_in
 *   [5]  mean_collisions
 *   [6]  mean_depth
 *   [7]  nbins
 *   [8]  max_collisions
 *   [9]  ndepth
 *   [10 .. 10+2*nbins-1]   hist_weight[i], hist_count[i]
 *   [10+2*nbins .. +max_collisions-1]  hist_collisions[i]
 *   [10+2*nbins+max_collisions .. +ndepth-1]  hist_depth[i]
 *
 * Compilation :
 *   emcc -O2 -s WASM=1 -s ALLOW_MEMORY_GROWTH=1 \
 *       -s EXPORTED_RUNTIME_METHODS='["ccall","cwrap"]' \
 *       -s MODULARIZE=1 -s EXPORT_NAME="PhotonMCModule" \
 *       -I src src/mc_wasm.c src/mc.c src/geometry.c src/phase.c \
 *       src/rng.c src/output.c -o web/photonmc.js
 */

#include "mc.h"
#include <emscripten.h>
#include <stdlib.h>
#include <string.h>

/*
 * Exécute une simulation unique et retourne un pointeur vers un tampon
 * alloué par malloc (WASM heap). Retourne NULL en cas d'échec.
 * Si out_size n'est pas NULL, y écrit la taille totale du tampon en octets.
 */
EMSCRIPTEN_KEEPALIVE
double* em_run_simulation(int shape, double size, double height, double tau, double theta,
                          double absorb, double g1, double g2, double f, long nphotons, int bins,
                          unsigned int seed, int maxsteps, int* out_size)
{
    MCConfig cfg;
    memset(&cfg, 0, sizeof(cfg));
    cfg.geom.type = (ShapeType)shape;
    cfg.geom.size = size;
    cfg.geom.height = height;
    cfg.tau = tau;
    cfg.theta_deg = theta;
    cfg.absorb = absorb;
    cfg.phase.g1 = g1;
    cfg.phase.g2 = g2;
    cfg.phase.f = f;
    cfg.nphotons = nphotons;
    cfg.nbins = bins;
    cfg.seed = seed;
    cfg.maxsteps = maxsteps;

    MCResult res;
    if (!mc_run(&cfg, &res))
    {
        if (out_size)
            *out_size = 0;
        return NULL;
    }

    size_t total_doubles =
        10 + (size_t)res.nbins * 2 + (size_t)res.max_collisions + (size_t)res.ndepth;
    double* buf = (double*)malloc(total_doubles * sizeof(double));
    if (!buf)
    {
        mc_result_free(&res);
        if (out_size)
            *out_size = 0;
        return NULL;
    }

    size_t off = 0;
    buf[off++] = res.reflected;
    buf[off++] = res.transmitted;
    buf[off++] = res.absorbed;
    buf[off++] = res.out_weight;
    buf[off++] = (double)res.photons_in;
    buf[off++] = res.mean_collisions;
    buf[off++] = res.mean_depth;
    buf[off++] = (double)res.nbins;
    buf[off++] = (double)res.max_collisions;
    buf[off++] = (double)res.ndepth;

    for (int i = 0; i < res.nbins; i++)
    {
        buf[off++] = res.hist_weight[i];
        buf[off++] = res.hist_count[i];
    }
    for (int i = 0; i < res.max_collisions; i++)
    {
        buf[off++] = (double)res.hist_collisions[i];
    }
    for (int i = 0; i < res.ndepth; i++)
    {
        buf[off++] = res.hist_depth[i];
    }

    mc_result_free(&res);
    if (out_size)
        *out_size = (int)(total_doubles * sizeof(double));
    return buf;
}

/* Libère un tampon alloué par em_run_simulation. */
EMSCRIPTEN_KEEPALIVE
void em_free(double* ptr) { free(ptr); }
