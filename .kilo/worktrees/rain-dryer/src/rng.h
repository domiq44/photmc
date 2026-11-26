/*
 * rng.h - Générateur de nombres aléatoires de qualité (Mersenne Twister MT19937)
 *
 * Implémentation autonome (sans dépendance externe) du MT19937 de Matsumoto &
 * Nishimura, avec conversion en flottant double dans [0, 1).
 */
#ifndef RNG_H
#define RNG_H

#include <stdint.h>

/* Contexte du générateur : état interne du MT19937. */
typedef struct
{
    uint32_t state[624]; /* état (624 mots de 32 bits) */
    int index;           /* index courant dans le tableau */
    int seeded;          /* 1 si l'état a été initialisé */
} RNG;

/* Initialise le générateur avec une graine (seed) 32 bits. */
void rng_init(RNG* rng, uint32_t seed);

/* Retourne un entier non signé 32 bits uniforme dans [0, 2^32). */
uint32_t rng_u32(RNG* rng);

/* Retourne un flottant double uniforme dans [0, 1). */
double rng_double(RNG* rng);

#endif /* RNG_H */