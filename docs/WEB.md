# Interface Web

## Deux modes d'exécution (basculement automatique)

### Mode 1 : Serveur Python local (recommandé pour développement)
```bash
make serve
# ou
python3 web/server.py --bin ./photonmc --outdir out --port 8000
```
- Lance un serveur HTTP `ThreadingHTTPServer` sur `http://localhost:8000/`
- Exécute le **binaire C natif** `./photonmc` via `subprocess.run()`
- API `POST /run` reçoit JSON, lance simulation, régénère `.dat`, retourne liste fichiers
- **Avantages** : performances natives, pas de compilation WASM requise, débogage facile
- **Dépendances** : Python 3.7+ (stdlib uniquement), binaire `photonmc` compilé

### Mode 2 : WebAssembly dans le navigateur (GitHub Pages, déploiement statique)
- Si `web/photonmc.js` et `web/photonmc.wasm` présents → chargement automatique
- Exécute le **même code C** compilé via Emscripten directement dans le navigateur
- **Aucun serveur requis** : fichiers statiques servis par GitHub Pages / tout serveur HTTP
- **Fallback automatique** : si WASM absent/échoue → bascule sur mode serveur (si dispo) ou affiche message

```bash
# Compilation WASM (requiert Emscripten)
make wasm
# Génère web/photonmc.js + web/photonmc.wasm
```

## Architecture de l'interface

```
┌─────────────────────────────────────────────────────────────┐
│                    web/index.html                            │
│  ├── Formulaire paramètres (HTML5)                          │
│  ├── 12 <canvas> pour graphiques                            │
│  └── Scripts : chart.js + app.js (+ photonmc.js si WASM)    │
└─────────────────────────────────────────────────────────────┘
                            │
              ┌─────────────┴─────────────┐
              ▼                           ▼
    ┌─────────────────────┐     ┌─────────────────────┐
    │   Mode Serveur      │     │     Mode WASM       │
    │   (server.py)       │     │   (photonmc.js/.wasm)│
    │                     │     │                     │
    │ POST /run → JSON    │     │ em_run_simulation() │
    │ subprocess → photonmc│     │ → pointeur WASM heap│
    │ lit .dat → JSON     │     │ readSimResult()     │
    └─────────────────────┘     └─────────────────────┘
              │                           │
              └─────────────┬─────────────┘
                            ▼
              ┌─────────────────────────────┐
              │     chart.js (canvas)       │
              │  - LineChart class          │
              │  - Zoom rectangulaire       │
              │  - Log scale Y              │
              │  - Bar/line modes           │
              └─────────────────────────────┘
```

## Formulaire de paramètres

| Champ | Type | Défaut | Description |
|-------|------|--------|-------------|
| `shape` | select | `cube` | `sphere` / `cube` / `cylinder` |
| `size` | number | `1.0` | Dimension caractéristique L_char (rayon/demi-arête) |
| `height` | number | `2.0` | Hauteur cylindre (ignoré sphère/cube) |
| `tau` | number | `3.0` | Épaisseur optique le long de L_char |
| `theta` | number | `0` | Angle d'incidence (deg) [0, 90] |
| `absorb` | number | `0.05` | Probabilité absorption par collision [0, 1] |
| `g1` | number | `0.8` | Asymétrie composante 1 [-1, 1] |
| `g2` | number | `-0.3` | Asymétrie composante 2 [-1, 1] |
| `f` | number | `0.7` | Mélange (poids g1) [0, 1] |
| `nphotons` | number | `300000` | Photons incidents par simulation |
| `bins` | number | `180` | Bins angulaires (résolution) |
| `seed` | number | `12345` | Graine RNG (reproductibilité) |
| `scan_steps` | number | `30` | Points pour balayages θ/τ |
| `theta_from/to` | number | `0/80` | Bornes balayage θ |
| `tau_from/to` | number | `0.1/20` | Bornes balayage τ |
| `run_scans` | checkbox | ✓ | Exécuter balayages (désactiver = simulation simple) |
| `exclude_ballistic` | checkbox | ✓ | Exclure le 1er bin (photons balistiques, angle ≈ 0°) |

## Presets (`config/presets.json`)

```json
{
  "presets": [
    {
      "name": "Sphere diffusante, sans absorption",
      "shape": "sphere", "size": 1.0, "height": 2.0,
      "tau": 2.0, "theta": 0, "absorb": 0.0,
      "g1": 0.0, "g2": 0.0, "f": 0.5,
      "nphotons": 400000, "bins": 180, "seed": 12345
    },
    {
      "name": "Cube, diffusion vers l'avant",
      "shape": "cube", "size": 1.0, "height": 2.0,
      "tau": 3.0, "theta": 0, "absorb": 0.05,
      "g1": 0.8, "g2": -0.3, "f": 0.7,
      "nphotons": 400000, "bins": 180, "seed": 12345
    },
    {
      "name": "Cylindre, incidence oblique, absorption",
      "shape": "cylinder", "size": 1.0, "height": 2.0,
      "tau": 5.0, "theta": 30, "absorb": 0.1,
      "g1": 0.9, "g2": -0.5, "f": 0.6,
      "nphotons": 400000, "bins": 180, "seed": 42
    },
    {
      "name": "Reference: isotrope, sans absorption, tau faible",
      "shape": "sphere", "size": 1.0, "height": 2.0,
      "tau": 0.001, "theta": 0, "absorb": 0.0,
      "g1": 0.0, "g2": 0.0, "f": 0.5,
      "nphotons": 200000, "bins": 180, "seed": 12345
    }
  ]
}
```
Chargés au démarrage, sélectionnables via dropdown "Configuration prédéfinie".

## Graphiques (12 canvas interactifs)

| # | Titre | Type | Données | Interactions |
|---|-------|------|---------|--------------|
| 1 | Flux vs θ | Ligne | Scan θ (R,T,A) | Zoom, pan, reset |
| 2 | Flux vs τ | Ligne | Scan τ (R,T,A) | Zoom, pan, reset |
| 3 | Distribution angulaire | Barres | `angular_distribution.dat` | Zoom, log Y (graph 5) |
| 4 | Phase HG analytique | Ligne | Calcul JS (g1,g2,f) | Zoom |
| 5 | Distribution log Y | Barres | Même que #3 | Log Y auto |
| 6 | Densité / sr | Barres | Conversion JS | Zoom |
| 7 | Densité en cosθ | Ligne | Conversion JS | Zoom |
| 8 | Bilan R+T+A vs θ | Aire empilée | Scan θ | Zoom |
| 9 | Bilan R+T+A vs τ | Aire empilée | Scan τ | Zoom |
| 10 | R/T, T/R vs τ | Ligne (log Y) | Scan τ | Zoom |
| 11 | Histogramme collisions | Barres | `hist_collisions.dat` | Zoom |
| 12 | Profondeur sortie | Barres | `profondeur.dat` | Zoom |

**Zoom** : Clic-glissé pour sélectionner région → zoom. Boutons `+` (zoom avant), `-` (zoom arrière), `↺` (reset).

## Repliement/Dépliement des graphiques (collapse/expand)

Chaque graphique possède un bouton **▶/▼** dans son en-tête (à côté des contrôles de zoom) :

- **Clique** : replie/déploie le corps du graphique (animation fluide `max-height` + `opacity`)
- **État** : persiste visuellement (flèche tournée à −90° quand replié)
- **Indépendant** : chaque graphique se gère seul
- **Utile** : sur mobile pour éviter le scrolling infini, ou sur desktop pour comparer seulement quelques graphiques

## Annotation et exclusion des photons balistiques

Sur les graphiques de distribution angulaire (#3, #5, #6, #7) :

- **Annotation** : une ligne jaune sous le titre indique *"Photons balistiques (non diffusés) : X%"* — calculée comme `hist_count[0] / photons_in × 100`
- **Checkbox "Exclure balistiques (1er bin)"** : cochée par défaut, retire le premier bin (angle ≈ 0°) des séries tracées
- **Pourquoi** : avec diffusion avant forte (`g ≈ 1`), le pic balistique écrase l'échelle et masque la forme de diffusion réelle (queue, pics secondaires)
- **Effet** : en excluant le 1er bin, l'axe Y s'adapte aux photons réellement diffusés, révélant la distribution physique

## Responsive mobile

L'interface est **mobile-first** (breakpoints 480px / 768px) :

- Marges/paddings fluides, polices adaptées
- Boutons tactiles (min-height 44px, `touch-action: manipulation`)
- Inputs plus grands (`-webkit-appearance: none`, padding 10px)
- Grille fluide `minmax(140px, 1fr)` sur mobile
- Canvas `max-height: 50vh` sur mobile, auto-resize via `devicePixelRatio`
- Select "Configuration prédéfinie" : empilé verticalement sur mobile, horizontal sur desktop

## API Serveur (`server.py`)

### `GET /` → `index.html`
### `GET /style.css`, `/app.js`, `/chart.js` → assets statiques
### `GET /config` → `config/presets.json`
### `GET /out/<fichier>` → fichiers `.dat` générés
### `POST /run` → Exécution simulation

**Request** (`Content-Type: application/json`) :
```json
{
  "shape": "cube",
  "size": 1.0,
  "height": 2.0,
  "tau": 3.0,
  "theta": 0,
  "absorb": 0.05,
  "g1": 0.8,
  "g2": -0.3,
  "f": 0.7,
  "nphotons": 200000,
  "bins": 180,
  "seed": 12345,
  "maxsteps": 100000,
  "run_scans": true,
  "scan_shape": "cube",
  "theta_from": 0,
  "theta_to": 80,
  "tau_from": 0.1,
  "tau_to": 20,
  "scan_steps": 30
}
```

**Response** (success) :
```json
{
  "ok": true,
  "files": [
    "out/flux_vs_theta.dat",
    "out/flux_vs_tau.dat",
    "out/angular_distribution.dat",
    "out/hist_collisions.dat",
    "out/profondeur.dat"
  ]
}
```

**Response** (error) :
```json
{ "ok": false, "error": "Simulation trop longue (timeout)." }
```

**Timeout** : 600s par défaut (configurable `--timeout` dans `server.py`).

## Compilation WebAssembly

```bash
# Installation Emscripten (une fois)
git clone https://github.com/emscripten-core/emsdk.git
cd emsdk && ./emsdk install latest && ./emsdk activate latest
source ./emsdk_env.sh

# Compilation
make wasm
# Sortie : web/photonmc.js (1.2 MB) + web/photonmc.wasm (450 KB)
# Copie aussi config/presets.json → web/config/presets.json (pour GitHub Pages)
```

**Flags Emscripten utilisés** :
```bash
emcc -O2 -s WASM=1 -s ALLOW_MEMORY_GROWTH=1 \
  -s EXPORTED_RUNTIME_METHODS='["ccall","cwrap","getValue"]' \
  -s EXPORTED_FUNCTIONS='["_em_run_simulation","_em_free","_malloc","_free"]' \
  -s MODULARIZE=1 -s EXPORT_NAME="PhotonMCModule" \
  -I src src/mc_wasm.c src/mc.c src/geometry.c src/phase.c src/rng.c src/output.c \
  -o web/photonmc.js
```

- `MODULARIZE=1` : module ES6, chargement via `PhotonMCModule().then(Module => ...)`
- `ALLOW_MEMORY_GROWTH=1` : heap dynamique (simulations grosses)
- `EXPORTED_FUNCTIONS` : seules fonctions C exposées au JS

## Déploiement GitHub Pages

Le workflow `.github/workflows/ci.yml` déploie automatiquement :

1. Push sur `main` → compile WASM (`make wasm`)
2. Déploie dossier `web/` sur branche `gh-pages`
3. Disponible à `https://<user>.github.io/photmc/`

**Configuration manuelle** :
1. Settings → Pages → Source : "Deploy from a branch"
2. Branch : `gh-pages`, folder : `/ (root)`
4. Save

## Dépannage

| Problème | Cause | Solution |
|----------|-------|----------|
| "Binaire introuvable" | `make` non lancé | `make` avant `make serve` |
| WASM ne charge pas | Navigateur bloque `file://` | Utiliser serveur HTTP (`python3 -m http.server`) |
| Timeout simulation | `nphotons` trop grand / `tau` trop grand | Réduire photons, cocher "Exécuter les balayages" = OFF |
| Graphiques vides | `.dat` non générés | Vérifier console navigateur (F12) → erreurs JS/réseau |
| `Module.getValue` erreur | WASM heap corrompu | Recharger page, vérifier version `photonmc.js`/.wasm match |

## Développement local (itération rapide)

```bash
# Terminal 1 : serveur API
make serve

# Terminal 2 : édition JS/CSS/HTML (rechargement auto navigateur)
# Pas de rebuild WASM nécessaire en mode serveur

# Pour tester WASM localement :
make wasm
python3 -m http.server 8000 --directory web/
# Ouvrir http://localhost:8000/ (mode WASM si fichiers présents)

# Console de test API WASM interactive (style Swagger UI)
# Ouvrir http://localhost:8000/api-test.html
```