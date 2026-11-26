# Performances

## Mesures de référence (compilation `-O2`, GCC 13, Intel i7-12700H / AMD Ryzen 9 7940HS)

| Configuration | Photons | Temps (s) | Débit (phot/s) | Mémoire (RSS) |
|---------------|---------|-----------|----------------|---------------|
| Sphère, τ=2, g1=0.8/g2=-0.3/f=0.7, absorption=0.1 | 400 000 | 0.24 | 1.67 M | 4.2 MB |
| Cube, τ=3, même paramètres | 400 000 | 0.31 | 1.29 M | 4.2 MB |
| Cylindre, τ=3, même paramètres | 400 000 | 0.33 | 1.21 M | 4.2 MB |
| Sphère, τ=10 (optique épaisse), absorption=0 | 400 000 | 0.68 | 0.59 M | 4.2 MB |
| Sphère, τ=0.01 (optique mince), isotrope | 400 000 | 0.08 | 5.0 M | 4.2 MB |

**Méthode** : moyenne sur 5 runs, `make run` (single simulation) + `time`.

## Facteurs d'influence

| Paramètre | Impact sur performance | Explication |
|-----------|------------------------|-------------|
| `tau` (épaisseur optique) | **Fort** (×2 à ×10) | Plus τ grand → plus de collisions/photon → plus de boucles |
| `absorb` | Faible | Méthode des poids : pas de terminaison précoce, même nombre d'étapes |
| `g1/g2` (asymétrie) | Modéré | g→±1 : diffusion avant/arrière → libre parcours effectif modifié |
| `shape` | Modéré | Cube/cylindre : tests d'intersection plus coûteux que sphère |
| `maxsteps` | Négligeable si non atteint | Seulement si photons piégés (τ très grand, absorption faible) |
| `nphotons` | Linéaire | Complexité O(N) |

## Scalabilité

- **Monocœur pur** : pas de parallélisation OpenMP/pthreads.
- **Mémoire constante** : O(1) par photon (pas d'accumulation de trajectoires).
- **Cache-friendly** : structures de données compactes, accès séquentiels.

## WebAssembly (Emscripten -O2)

| Config | Débit natif | Débit WASM | Ratio |
|--------|-------------|------------|-------|
| Sphère τ=2, 400k photons | 1.67 M/s | 1.45 M/s | 87% |
| Cube τ=3, 400k photons | 1.29 M/s | 1.12 M/s | 87% |

Performances WASM ≈ **85-90% du natif** (overhead appel JS↔WASM + sandbox).

## Optimisations pratiques

```bash
# Itération rapide : réduire photons, désactiver scans
make run NP=20000        # ~12 ms au lieu de 240 ms

# Profilage CPU (Linux)
perf record -g ./photonmc --nphotons 1000000 --tau 5
perf report

# Profilage mémoire
valgrind --tool=massif ./photonmc --nphotons 100000
ms_print massif.out.*

# Compilation avec infos debug + optimisations
make CFLAGS="-std=c11 -O2 -g -Wall -Wextra ..."
```

## Limites connues

- **Pas de SIMD explicite** : le compilateur vectorise partiellement la boucle photons.
- **Un seul RNG global** : contention potentielle si parallélisation future.
- **Allocation histogrammes** : `calloc` à chaque `mc_run()` (négligeable vs temps simulation).
- **maxsteps=100000** par défaut : suffisant pour τ≤100, absorption>0. Pour τ>100 sans absorption, augmenter `--maxsteps`.

## Comparatif avec alternatives

| Outil | Langage | Parallélisation | Débit (phot/s) | Notes |
|-------|---------|-----------------|----------------|-------|
| **photonmc** | C11 | Non | ~1-5 M | Simple, portable, WASM |
| MCML (Wang et al.) | C | OpenMP | ~50 M | Standard biomedical, plus complexe |
| Geant4 | C++ | MT/MP | ~0.1 M | Généraliste, lourd |
| Python (NumPy) | Python | Non | ~0.01 M | Prototypage uniquement |

> photonmc vise la **simplicité, la portabilité (WASM) et la lisibilité pédagogique**, pas le débit maximal absolu.