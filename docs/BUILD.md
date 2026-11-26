# Compilation et Build

## Prérequis

| Outil | Version minimale | Installation |
|-------|------------------|--------------|
| GCC / Clang | GCC 10 / Clang 12 | `apt install build-essential` / `brew install llvm` |
| GNU Make | 4.0 | Inclus dans build-essential |
| Python 3 | 3.7 | `apt install python3` / `brew install python` |
| Gnuplot | 5.4 | `apt install gnuplot` / `brew install gnuplot` |
| Emscripten | 3.1.50 | Voir section WebAssembly |

---

## Compilation native (C11)

```bash
# Compilation simple (cible par défaut)
make
# ou
make compile
# Produit : ./photonmc (binaire ELF 64-bit, ~30 KB)

# Compilation avec flags personnalisés
make CFLAGS="-std=c11 -O3 -march=native -Wall -Wextra ..."
# ou export CFLAGS="..." && make

# Nettoyage
make clean          # objets + binaire + artefacts WASM
make distclean      # + dossier out/ (graphiques, .dat)
```

### Flags de compilation (Makefile)

```makefile
CC      := gcc
CFLAGS  := -std=c11 -O2 -Wall -Wextra -Wpedantic -Wshadow -Wconversion \
           -Wdouble-promotion -I src
LDFLAGS := -lm
```

| Flag | Rôle |
|------|------|
| `-std=c11` | Standard C11 (alignas, _Generic, anonymous structs) |
| `-O2` | Optimisation équilibrée vitesse/taille |
| `-Wall -Wextra` | Tous avertissements standards + extras |
| `-Wpedantic` | Conformité stricte ISO C |
| `-Wshadow` | Détection variables masquées |
| `-Wconversion` | Conversions implicites potentiellement perdantes |
| `-Wdouble-promotion` | float→double implicite (coût perf) |
| `-I src` | Inclusion headers locaux |

**Résultat** : **0 avertissement, 0 erreur** sur GCC 10+/Clang 12+.

---

## Cibles Makefile principales

| Cible | Description | Dépendances |
|-------|-------------|-------------|
| `compile` / (défaut) | Binaire `./photonmc` seulement | `src/*.c` |
| `run` | Exécution simple (sphère, τ=1, θ=0) | `compile` |
| `scans` | Balayages θ (cube) + τ (sphère) + distribution | `compile` |
| `plots` | 12 PNG gnuplot dans `out/` | `scans` |
| `scans_formes` | 3 scans τ (sphere/cube/cylinder) dans `out/{sphere,cube,cylinder}/` | `compile` |
| `plots_formes` | Comparaison formes (2 PNG) | `scans_formes` |
| `all` | `compile + scans + plots + scans_formes + plots_formes` | Tout |
| `serve` | Serveur web local `http://localhost:8000/` | `compile` |
| `wasm` | Module WebAssembly `web/photonmc.js/.wasm` | Emscripten |
| `test` | Suite complète 4 tests | `compile` |
| `clean` | Nettoyage build | - |
| `distclean` | Nettoyage complet | `clean` |

### Variables surchargeables

```bash
make NP=1000000      # nphotons par défaut (défaut: 400000)
make BINS=360        # bins angulaires (défaut: 180)
make SEED=42         # graine RNG (défaut: 12345)
make OUTDIR=results  # dossier sortie (défaut: out)
```

---

## Exécution CLI

```bash
# Aide
./photonmc --help

# Simulation simple
./photonmc --shape sphere --tau 2.0 --theta 30 --absorb 0.1 \
    --g1 0.8 --g2 -0.3 --f 0.7 --nphotons 200000 --bins 180 --seed 12345

# Balayage θ (cube, τ=3)
./photonmc --scan theta --from 0 --to 80 --scan-steps 30 \
    --shape cube --tau 3.0 --absorb 0.05 --g1 0.8 --g2 -0.3 --f 0.7

# Balayage τ (sphère, θ=30)
./photonmc --scan tau --from 0.1 --to 20 --scan-steps 40 \
    --shape sphere --theta 30 --absorb 0.1 --g1 0.8 --g2 -0.3 --f 0.7

# Sortie dans dossier custom
./photonmc --outdir ./mes_resultats ...
```

### Paramètres disponibles

| Option | Type | Défaut | Description |
|--------|------|--------|-------------|
| `--shape` | sphere\|cube\|cylinder | sphere | Forme du nuage |
| `--size` | float | 1.0 | L_char (rayon/demi-arête) |
| `--height` | float | 2.0 | Hauteur cylindre |
| `--tau` | float | 1.0 | Épaisseur optique |
| `--theta` | float | 0.0 | Angle incidence (deg) [0,90] |
| `--absorb` | float | 0.0 | Absorption par collision [0,1] |
| `--g1` | float | 0.0 | Asymétrie composante 1 [-1,1] |
| `--g2` | float | 0.0 | Asymétrie composante 2 [-1,1] |
| `--f` | float | 0.5 | Mélange (poids g1) [0,1] |
| `--nphotons` | long | 100000 | Photons incidents |
| `--bins` | int | 180 | Bins angulaires |
| `--seed` | uint32 | 12345 | Graine RNG |
| `--maxsteps` | int | 100000 | Étapes max/photon |
| `--outdir` | string | ./out | Dossier sortie |
| `--scan` | theta\|tau | - | Activer balayage |
| `--from` | float | - | Borne inf balayage |
| `--to` | float | - | Borne sup balayage |
| `--scan-steps` | int | - | Nombre points balayage |

---

## WebAssembly (Emscripten)

### Installation Emscripten (une fois)

```bash
# Via emsdk (recommandé)
git clone https://github.com/emscripten-core/emsdk.git
cd emsdk
./emsdk install latest
./emsdk activate latest
source ./emsdk_env.sh

# Vérification
emcc --version
# emcc (Emscripten gcc/clang-like replacement) 3.1.50 ...
```

### Compilation

```bash
make wasm
# Produit : web/photonmc.js (loader ES6) + web/photonmc.wasm (binaire WASM)
```

### Flags Emscripten détaillés

```bash
emcc -O2 -s WASM=1 -s ALLOW_MEMORY_GROWTH=1 \
  -s EXPORTED_RUNTIME_METHODS='["ccall","cwrap","getValue"]' \
  -s EXPORTED_FUNCTIONS='["_em_run_simulation","_em_free","_malloc","_free"]' \
  -s MODULARIZE=1 -s EXPORT_NAME="PhotonMCModule" \
  -I src src/mc_wasm.c src/mc.c src/geometry.c src/phase.c src/rng.c src/output.c \
  -o web/photonmc.js
```

| Flag | Rôle |
|------|------|
| `-O2` | Optimisation WASM (équivalent -O2 natif) |
| `-s WASM=1` | Sortie WebAssembly (pas asm.js) |
| `-s ALLOW_MEMORY_GROWTH=1` | Heap dynamique (simulations grosses) |
| `-s MODULARIZE=1` | Module ES6 (`PhotonMCModule().then(Module => ...)`) |
| `-s EXPORT_NAME="PhotonMCModule"` | Nom global du module |
| `EXPORTED_FUNCTIONS` | Symboles C accessibles depuis JS |
| `EXPORTED_RUNTIME_METHODS` | Méthodes runtime utilitaires |

### Taille artefacts

| Fichier | Taille | Compression gzip |
|---------|--------|------------------|
| `photonmc.js` | ~1.2 MB | ~280 KB |
| `photonmc.wasm` | ~450 KB | ~140 KB |

---

## Interface Web

### Mode serveur local (développement)

```bash
make serve
# Lance python3 web/server.py --bin ./photonmc --outdir out --port 8000
# Ouvrir http://localhost:8000/
```

**Options serveur** :
```bash
python3 web/server.py --bin ./photonmc --port 8000 --outdir out --timeout 600
```

### Mode statique (GitHub Pages, déploiement)

```bash
# 1. Compiler WASM
make wasm

# 2. Servir dossier web/ (n'importe quel serveur HTTP statique)
python3 -m http.server 8000 --directory web/
# ou
npx serve web/
# ou déployer web/ sur GitHub Pages / Netlify / Vercel

# 3. Ouvrir http://localhost:8000/ → mode WASM automatique
```

> **Note navigateur** : Chargement `file://` bloque souvent WASM (CORS). Toujours utiliser un serveur HTTP local pour tester.

---

## Déploiement GitHub Pages (automatique)

Le workflow `.github/workflows/ci.yml` :

1. **Push sur `main`** → déclenche CI
2. **Job `build-and-test`** : compile C + `make test` + compile WASM
3. **Job `deploy-pages`** (si `main`) : compile WASM + déploie `web/` sur branche `gh-pages`
4. **Site live** : `https://<user>.github.io/photmc/`

### Configuration manuelle (première fois)

1. Settings → Pages → **Source** : "Deploy from a branch"
2. **Branch** : `gh-pages` / **Folder** : `/ (root)`
3. **Save** → premier déploiement via Actions

---

## Dépannage compilation

| Erreur | Cause | Solution |
|--------|-------|----------|
| `gcc: command not found` | Compilateur absent | `apt install build-essential` / `xcode-select --install` |
| `gnuplot: not found` | Gnuplot absent | `apt install gnuplot` / `brew install gnuplot` |
| `emcc: command not found` | Emscripten non dans PATH | `source emsdk/emsdk_env.sh` |
| `fatal error: emscripten.h` | Headers EMSDK absents | `emsdk install latest && emsdk activate latest` |
| `undefined reference to 'sqrt'` | `-lm` manquant | Vérifier `LDFLAGS := -lm` dans Makefile |
| `warning: conversion from 'double' to 'float'` | `-Wconversion` | Cast explicite ou utiliser `double` partout |

---

## Build reproductible / CI

```bash
# Version exacte compilateur
gcc --version
# gcc (Ubuntu 13.2.0-23ubuntu4) 13.2.0

# Build propre
make distclean && make

# Vérification binaire
file photonmc
# photonmc: ELF 64-bit LSB executable, x86-64, version 1 (SYSV), dynamically linked, ...

# Tests
make test
# Tous PASS → build valide
```

---

## Variables d'environnement utiles

```bash
export CC=clang           # Utiliser Clang au lieu de GCC
export CFLAGS="-O3 -march=native"  # Optimisations agressives
export MAKEFLAGS="-j$(nproc)"      # Parallélisation make (si cibles indépendantes)
```