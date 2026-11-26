# Structure du projet

```
.
├── .github/
│   └── workflows/
│       └── ci.yml              # GitHub Actions: build, test, WASM, deploy Pages
├── .clang-format               # Config formatage LLVM (indent 4, Allman, 100 cols)
├── .gitignore                  # Ignore obj/, out/, photonmc, web/photonmc.*, photonmc-asan, compile_commands.json
├── Makefile                    # Build system principal (cibles: compile, test, wasm, serve, plots, all, format, tidy, asan, compdb, bench)
├── README.md                   # Vue d'ensemble, démarrage rapide, liens docs
├── config/
│   └── presets.json            # Configurations prédéfinies pour l'interface web / CLI
├── docs/
│   ├── BUILD.md                # Compilation, flags, exécution CLI, WASM, déploiement
│   ├── CI.md                   # GitHub Actions, déploiement Pages, personnalisation
│   ├── PERFORMANCE.md          # Benchmarks, facteurs d'influence, scalabilité, comparatifs
│   ├── PHYSICS.md              # Modèle physique complet (géométrie, injection, diffusion, poids)
│   ├── RESULTS.md              # Description 12 graphiques, fichiers .dat, vérifications, cas analytiques
│   ├── STRUCTURE.md            # Ce fichier
│   ├── TESTING.md              # 6 suites tests, couverture, template nouveaux tests, métriques
│   ├── USAGE.md                # CLI référence complète, exemples, format fichiers .dat
│   ├── WEB.md                  # Interface web: 2 modes, API, graphiques, WASM, déploiement
│   └── WORKFLOW.md             # Workflow dev local + distant, fichiers générés
├── scripts/
│   ├── plot_angular.gp         # Distribution angulaire (lin)
│   ├── plot_angular_costheta.gp # Densité en cosθ
│   ├── plot_angular_log.gp     # Distribution angulaire (log Y)
│   ├── plot_angular_sr.gp      # Densité par stéradian
│   ├── plot_bilan_tau.gp       # Bilan R+T+A vs τ (aire empilée)
│   ├── plot_bilan_theta.gp     # Bilan R+T+A vs θ (aire empilée)
│   ├── plot_comparaison_formes.gp # Comparaison 3 formes vs τ
│   ├── plot_flux_tau.gp        # Flux R/T/A vs τ
│   ├── plot_flux_theta.gp      # Flux R/T/A vs θ
│   ├── plot_hist_collisions.gp # Histogramme collisions
│   ├── plot_phase.gp           # Fonction phase HG analytique
│   ├── plot_profondeur.gp      # Profondeur sortie
│   └── plot_rapport_rt.gp      # Rapports R/T, T/R vs τ (log Y)
├── src/
│   ├── geometry.c / geometry.h # Intersections rayon/surface (sphère, cube AABB, cylindre)
│   ├── main.c                  # CLI, parsing args, orchestration runs/scans, I/O
│   ├── mc.c / mc.h             # Moteur Monte Carlo principal (boucle photons, méthode des poids)
│   ├── mc_wasm.c               # Interface Emscripten: em_run_simulation(), em_free()
│   ├── output.c / output.h     # Écriture fichiers .dat (gnuplot-compatible)
│   ├── phase.c / phase.h       # Double Henyey-Greenstein: évaluation + échantillonnage inversion CDF
│   ├── rng.c / rng.h           # Mersenne Twister MT19937 autonome (uint32 + double [0,1))
├── tests/
│   ├── test_analytical.sh      # Validation Chandrasekhar (sphère τ=1,3,10 albedo 0.5/0.9/1.0, etc.)
│   ├── test_convergence.sh     # Convergence Monte Carlo (N=20k/80k/320k, variation <5%)
│   ├── test_edge_cases.sh      # Cas limites: absorption forte, g=±0.99, maxsteps, variance, grazing
│   ├── test_photonmc.sh        # Reproductibilité + bilan R+T+A=1
│   ├── test_reference.sh       # Cas analytique τ→0 isotrope sans absorption (T≈1)
│   └── test_shapes.sh          # Bilan énergie cube + cylinder
├── web/
│   ├── app.js                  # Logique UI: collecte params, appels WASM/serveur, rendu 12 graphiques
│   ├── chart.js                # Mini librairie canvas: LineChart (zoom, pan, logY, bar/line)
│   ├── index.html              # Page principale: formulaire + 12 canvas + preset dropdown
│   ├── scan-worker.js          # Web Worker pour balayages WASM parallèles (θ/τ)
│   ├── server.py               # Serveur HTTP stdlib: POST /run → subprocess photonmc → .dat
│   ├── style.css               # Styles: grid, cartes graphiques, responsive
│   ├── favicon.svg             # Favicon photon-themed (évite 404 sur GitHub Pages)
│   └── config/
│       └── presets.json        # (généré par `make wasm`) copié depuis config/presets.json
├── obj/                        # (généré) fichiers objets .o
├── out/                        # (généré) fichiers .dat + .png gnuplot
├── photonmc                    # (généré) binaire ELF natif
└── photonmc-asan               # (généré) binaire AddressSanitizer + UBSan
```

## Description des modules C (`src/`)

| Module | Rôle | API publique | Dépendances |
|--------|------|--------------|-------------|
| `rng` | Générateur aléatoire MT19937 | `rng_init()`, `rng_u32()`, `rng_double()` | - |
| `geometry` | Géométries + intersections | `geo_bound_radius()`, `geo_contains()`, `geo_ray_intersect()` | `math.h` |
| `phase` | Fonction de phase double HG | `phase_eval()`, `phase_sample_cos()` | `math.h` |
| `mc` | Moteur Monte Carlo | `mc_run()`, `mc_result_free()`, structs `MCConfig`/`MCResult` | `rng`, `geometry`, `phase` |
| `output` | Écriture fichiers données | `write_angular()`, `write_flux_line()`, `write_collisions()`, `write_depth()` | `mc.h`, `stdio.h` |
| `main` | Point d'entrée CLI | `main()` | `mc`, `output`, `getopt` |
| `mc_wasm` | Glue Emscripten | `em_run_simulation()`, `em_free()` | `mc`, `emscripten.h` |

## Flux de données

```
CLI / Web API
     │
     ▼
MCConfig (params physiques)
     │
     ▼
mc_run() ──────────────────────────────────┐
     │                                      │
     ├─ RNG (MT19937)                       │
     ├─ Geometry (intersections)            │
     ├─ Phase (double HG sampling)          │
     └─ Weight method (absorption)          │
     │                                      │
     ▼                                      │
MCResult (flux R/T/A, histogrammes)        │
     │                                      │
     ├─ write_angular() → angular_distribution.dat
     ├─ write_flux_line() → flux_vs_theta/tau.dat
     ├─ write_collisions() → hist_collisions.dat
     └─ write_depth() → profondeur.dat
     │
     ▼
Gnuplot scripts → PNG (out/)
     │
     ▼
Web UI (chart.js) → Canvas HTML5
```

## Fichiers générés (non versionnés)

| Dossier/Fichier | Généré par | Contenu |
|-----------------|------------|---------|
| `obj/*.o` | `make compile` | Objets compilation |
| `photonmc` | `make compile` | Binaire natif |
| `photonmc-asan` | `make asan` | Binaire ASan + UBSan |
| `out/*.dat` | `make scans` / `./photonmc` | Données simulation |
| `out/*.png` | `make plots` | Graphiques gnuplot |
| `web/photonmc.js` | `make wasm` | Loader WASM ES6 |
| `web/photonmc.wasm` | `make wasm` | Binaire WebAssembly |
| `web/config/presets.json` | `make wasm` | Copié depuis config/presets.json (pour GitHub Pages) |
| `web/favicon.svg` | (statique) | Favicon SVG |

> Tous ignorés par `.gitignore`. Régénérés par `make`, `make wasm`, `make scans`, `make plots`.

## Dépendances externes

| Outil | Usage | Requis pour |
|-------|-------|-------------|
| GCC/Clang | Compilation C | `make compile` |
| GNU Make | Build system | Toutes cibles `make` |
| Gnuplot | Graphiques PNG | `make plots` |
| Python 3.7+ | Serveur web local | `make serve` |
| Emscripten (emcc) | Compilation WASM | `make wasm` |
| Node.js | (optionnel) serve statique | `npx serve web/` |
| bear | compile_commands.json | `make compdb` (LSP/clangd) |
| hyperfine | Benchmark | `make bench` (optionnel) |
| clang-format | Formatage | `make format` |
| clang-tidy | Analyse statique | `make tidy` |