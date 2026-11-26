# PhotonMC WASM API Reference

Interface WebAssembly pour le simulateur Monte Carlo de transport de photons.

## Chargement du module

```javascript
// Via script tag (auto-détecté par app.js)
<script src="photonmc.js"></script>

// Ou manuellement
const Module = await PhotonMCModule();
const em_run_simulation = Module.cwrap('em_run_simulation', 'number', [
  'number', 'number', 'number', 'number', 'number',
  'number', 'number', 'number', 'number',
  'number', 'number', 'number', 'number', 'number'
]);
const em_free = Module.cwrap('em_free', null, ['number']);
```

## Fonctions exportées

### `em_run_simulation`

Exécute une simulation Monte Carlo unique.

```c
double* em_run_simulation(
  int shape,           // 0=SPHERE, 1=CUBE, 2=CYLINDER
  double size,         // L_char (rayon ou demi-arête)
  double height,       // Hauteur cylindre (ignoré sphere/cube)
  double tau,          // Épaisseur optique
  double theta,        // Angle incidence degrés [0,90]
  double absorb,       // Absorption par collision [0,1]
  double g1,           // Asymétrie composante 1 [-1,1]
  double g2,           // Asymétrie composante 2 [-1,1]
  double f,            // Mélange [0,1] (poids de g1)
  long nphotons,       // Nombre de photons
  int bins,            // Bins angulaires
  unsigned int seed,   // Graine RNG
  int maxsteps,        // Étapes max/photon
  int* out_size        // OUT: taille buffer octets
);
```

**Retourne** : Pointeur vers buffer `double*` dans heap WASM, ou `NULL` si échec.

**Layout du buffer** (tout en `double` 8 octets, index = `ptr/8 + i` via `HEAPF64`) :

| Index | Champ | Description |
|-------|-------|-------------|
| 0 | `reflected` | Fraction poids côté entrée |
| 1 | `transmitted` | Fraction poids côté sortie |
| 2 | `absorbed` | 1 - reflected - transmitted |
| 3 | `out_weight` | reflected + transmitted |
| 4 | `photons_in` | Photons injectés (touchent le nuage) |
| 5 | `mean_collisions` | Moyenne collisions/photon |
| 6 | `mean_depth` | Profondeur moyenne sortie (proj. axe) |
| 7 | `nbins` | Nombre de bins angulaires |
| 8 | `max_collisions` | Taille histogramme collisions |
| 9 | `ndepth` | Nombre de bins profondeur |
| 10 .. 10+2×nbins-1 | `hist_weight[i]`, `hist_count[i]` | Poids + compte par bin angulaire |
| ... + max_collisions | `hist_collisions[i]` | Photons par nombre de collisions |
| ... + ndepth | `hist_depth[i]` | Poids par bin profondeur |

### `em_free`

```c
void em_free(double* ptr);
```

Libère le buffer alloué par `em_run_simulation`. **Appeler obligatoirement après lecture.**

## Exemple complet JavaScript

```javascript
async function runSimulation(params) {
  const Module = await PhotonMCModule();
  
  const em_run_simulation = Module.cwrap('em_run_simulation', 'number', [
    'number', 'number', 'number', 'number', 'number',
    'number', 'number', 'number', 'number',
    'number', 'number', 'number', 'number', 'number'
  ]);
  const em_free = Module.cwrap('em_free', null, ['number']);
  
  // Alloue out_size dans heap
  const outSizePtr = Module._malloc(4);
  
  try {
    const ptr = em_run_simulation(
      params.shape, params.size, params.height, params.tau, params.theta,
      params.absorb, params.g1, params.g2, params.f,
      params.nphotons, params.bins, params.seed, params.maxsteps,
      outSizePtr
    );
    
    if (!ptr) throw new Error('Simulation failed');
    
    const outSize = Module.getValue(outSizePtr, 'i32');
    const result = readSimResult(Module, ptr, outSize);
    
    em_free(ptr);
    Module._free(outSizePtr);
    
    return result;
  } catch (e) {
    Module._free(outSizePtr);
    throw e;
  }
}

function readSimResult(Module, ptr, outSize) {
  // Use Module.getValue for reliable access (heap views may not be initialized yet)
  
  const reflected = Module.getValue(ptr + 0 * 8, 'double');
  const transmitted = Module.getValue(ptr + 1 * 8, 'double');
  const absorbed = Module.getValue(ptr + 2 * 8, 'double');
  const out_weight = Module.getValue(ptr + 3 * 8, 'double');
  const photons_in = Module.getValue(ptr + 4 * 8, 'double');
  const mean_collisions = Module.getValue(ptr + 5 * 8, 'double');
  const mean_depth = Module.getValue(ptr + 6 * 8, 'double');
  const nbins = Module.getValue(ptr + 7 * 8, 'double');
  const max_collisions = Module.getValue(ptr + 8 * 8, 'double');
  const ndepth = Module.getValue(ptr + 9 * 8, 'double');
  
  let off = 10;
  const hist_weight = [], hist_count = [];
  for (let i = 0; i < nbins; i++) {
    hist_weight.push(Module.getValue(ptr + off * 8, 'double')); off++;
    hist_count.push(Module.getValue(ptr + off * 8, 'double')); off++;
  }
  const hist_collisions = [];
  for (let i = 0; i < max_collisions; i++) {
    hist_collisions.push(Module.getValue(ptr + off * 8, 'double')); off++;
  }
  const hist_depth = [];
  for (let i = 0; i < ndepth; i++) {
    hist_depth.push(Module.getValue(ptr + off * 8, 'double')); off++;
  }
  
  return {
    reflected, transmitted, absorbed, out_weight,
    photons_in, mean_collisions, mean_depth,
    nbins, max_collisions, ndepth,
    hist_weight, hist_count, hist_collisions, hist_depth
  };
}

// Usage
const result = await runSimulation({
  shape: 0,        // SPHERE
  size: 1.0,
  height: 2.0,
  tau: 2.0,
  theta: 0,
  absorb: 0.1,
  g1: 0.8,
  g2: -0.3,
  f: 0.7,
  nphotons: 100000,
  bins: 180,
  seed: 12345,
  maxsteps: 100000
});

console.log(`R=${result.reflected.toFixed(4)} T=${result.transmitted.toFixed(4)} A=${result.absorbed.toFixed(4)}`);
```

## Validation des paramètres

| Paramètre | Contraintes |
|-----------|-------------|
| `shape` | 0, 1, ou 2 |
| `size` | > 0 |
| `height` | > 0 (cylinder) |
| `tau` | > 0 |
| `theta` | [0, 90] |
| `absorb` | [0, 1] |
| `g1`, `g2` | [-1, 1] |
| `f` | [0, 1] |
| `nphotons` | ≥ 1 |
| `bins` | ≥ 1 |
| `maxsteps` | ≥ 1 |

## Gestion d'erreurs

- `em_run_simulation` retourne `NULL` (0) si `mc_run` échoue (allocation)
- Vérifier `ptr === 0` avant lecture
- Toujours appeler `em_free(ptr)` après lecture
- Le heap WASM gère la mémoire automatiquement (`ALLOW_MEMORY_GROWTH=1`)

## Compilation

```bash
emcc -O2 -s WASM=1 -s ALLOW_MEMORY_GROWTH=1 \
  -s EXPORTED_RUNTIME_METHODS='["ccall","cwrap","getValue","_malloc","_free"]' \
  -s EXPORTED_FUNCTIONS='["_em_run_simulation","_em_free"]' \
  -s MODULARIZE=1 -s EXPORT_NAME="PhotonMCModule" \
  -I src src/mc_wasm.c src/mc.c src/geometry.c src/phase.c src/rng.c src/output.c \
  -o web/photonmc.js
```

## Console de test interactive

Voir [`api-test.html`](api-test.html) — interface Swagger-like pour tester l'API WASM dans le navigateur.

### Utilisation

```bash
# Depuis la racine du projet
cd web && python3 -m http.server 8000
# Ouvre http://localhost:8000/api-test.html
```

Ou sur GitHub Pages : https://domiq44.github.io/photmc/api-test.html

### Fonctionnalités

- **Charge automatiquement** le module WASM (`PhotonMCModule`)
- **Formulaire complet** avec tous les paramètres (`SimInput`) et validation
- **Appel `em_run_simulation`** via `Module.cwrap` avec gestion mémoire (`_malloc`/`_free`)
- **Lecture buffer** via `HEAPF64` selon le layout documenté
- **Affichage JSON** coloré (clés, strings, nombres, booléens, null)
- **Statuts** : chargement, succès (R/T/A), erreur
- **Bouton Reset** pour valeurs par défaut
- **Liens** vers JSON Schema, TypeScript defs, API.md

### Capture d'écran

```
┌─────────────────────────────────────┐
│ Paramètres d'entrée (SimInput)      │
│ Shape: [CYLINDER ▼]  Size: [1.0]    │
│ Height: [2.0]        Tau: [3.0]     │
│ Theta: [0]           Absorb: [0.05] │
│ g1: [0.8]            g2: [-0.3]     │
│ f: [0.7]             N photons: [100000] │
│ Bins: [180]          Seed: [12345]  │
│ Max steps: [100000]                │
│ [Exécuter] [Réinitialiser]          │
│ Status: Succès: R=0.1234 T=0.5678...│
└─────────────────────────────────────┘
┌─────────────────────────────────────┐
│ Résultat (SimOutput)                │
│ {                                   │
│   "reflected": 0.1234,             │
│   "transmitted": 0.5678,           │
│   "absorbed": 0.3088,              │
│   "out_weight": 0.6912,            │
│   "photons_in": 99847,             │
│   "mean_collisions": 4.23,         │
│   ...                              │
│   "hist_weight": [...],            │
│   "hist_count": [...],             │
│   "hist_collisions": [...],        │
│   "hist_depth": [...]              │
│ }                                   │
└─────────────────────────────────────┘
```

## Types TypeScript

Voir [`photonmc.d.ts`](photonmc.d.ts) pour les définitions de types complètes.