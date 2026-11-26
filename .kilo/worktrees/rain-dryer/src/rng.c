/*
 * rng.c - Implémentation du Mersenne Twister MT19937.
 *
 * Référence : M. Matsumoto, T. Nishimura,
 *   "Mersenne Twister: A 623-dimensionally equidistributed uniform
 *    pseudo-random number generator", ACM TOMACS 8(1), 1998.
 */
#include "rng.h"

#define MT_N 624
#define MT_M 397
#define MT_MATRIX_A 0x9908b0dfUL   /* constante de multiplication */
#define MT_UPPER_MASK 0x80000000UL /* bit de poids fort */
#define MT_LOWER_MASK 0x7fffffffUL /* bits de poids faible */

/* Génère MT_N nouveaux mots d'état à partir de l'état courant. */
static void rng_generate(RNG* rng)
{
    uint32_t y;
    static const uint32_t mag01[2] = {0x0UL, MT_MATRIX_A};
    int kk;

    for (kk = 0; kk < MT_N - MT_M; kk++)
    {
        y = (rng->state[kk] & MT_UPPER_MASK) | (rng->state[kk + 1] & MT_LOWER_MASK);
        rng->state[kk] = rng->state[kk + MT_M] ^ (y >> 1) ^ mag01[y & 0x1UL];
    }
    for (; kk < MT_N - 1; kk++)
    {
        y = (rng->state[kk] & MT_UPPER_MASK) | (rng->state[kk + 1] & MT_LOWER_MASK);
        rng->state[kk] = rng->state[kk + (MT_M - MT_N)] ^ (y >> 1) ^ mag01[y & 0x1UL];
    }
    y = (rng->state[MT_N - 1] & MT_UPPER_MASK) | (rng->state[0] & MT_LOWER_MASK);
    rng->state[MT_N - 1] = rng->state[MT_M - 1] ^ (y >> 1) ^ mag01[y & 0x1UL];

    rng->index = 0;
}

void rng_init(RNG* rng, uint32_t seed)
{
    rng->state[0] = seed;
    for (rng->index = 1; rng->index < MT_N; rng->index++)
    {
        /* Initialisation par la relation de récurrence standard. */
        rng->state[rng->index] = (uint32_t)(1812433253UL * (rng->state[rng->index - 1] ^
                                                            (rng->state[rng->index - 1] >> 30)) +
                                            (uint32_t)rng->index);
    }
    rng->index = MT_N; /* force une génération dès le premier appel */
    rng->seeded = 1;
}

uint32_t rng_u32(RNG* rng)
{
    uint32_t y;

    if (rng->index >= MT_N)
    {
        rng_generate(rng);
    }
    y = rng->state[rng->index++];

    /* "Temper" : améliore les propriétés de distribution. */
    y ^= (y >> 11);
    y ^= (y << 7) & 0x9d2c5680UL;
    y ^= (y << 15) & 0xefc60000UL;
    y ^= (y >> 18);

    return y;
}

double rng_double(RNG* rng)
{
    /* On utilise 53 bits de hasard : division par 2^53.
     * On évite soigneusement la valeur 1.0 en utilisant 2^53 - 1 au dénominateur. */
    uint32_t a = rng_u32(rng) >> 5;                   /* 27 bits */
    uint32_t b = rng_u32(rng) >> 6;                   /* 26 bits */
    return (a * 67108864.0 + b) / 9007199254740992.0; /* / 2^53 */
}