# Résultats attendus et validation

## Graphiques générés par `make plots` (PNG gnuplot dans `out/`)

### 1. Flux vs angle d'incidence θ (`flux_vs_theta.png`)
- **Axes** : θ ∈ [0°, 80°] / Flux ∈ [0, 1]
- **Courbes** : Réfléchi (R), Transmis (T), Absorbé (A)
- **Physique** : Pour un cube (forme par défaut), R augmente avec θ (plus de chemin optique dans le nuage), T diminue. Pour une sphère, symétrie de rotation → flux indépendants de θ.
- **Vérification** : R+T+A = 1 à tout θ.

### 2. Flux vs épaisseur optique τ (`flux_vs_tau.png`)
- **Axes** : τ ∈ [0.1, 20] / Flux ∈ [0, 1]
- **Courbes** : R, T, A
- **Physique** :
  - τ → 0 : T → 1, R → 0, A → 0 (transparent)
  - τ modéré (1-5) : régime diffusif, R et T comparables
  - τ → ∞ : R → albedo (≈0.9 si absorb=0.1), T → 0, A → 1-R
- **Vérification** : R+T+A = 1 sur toute la plage.

### 3. Distribution angulaire de sortie (`angular_distribution.png`)
- **Axes** : Angle de diffusion global ∈ [0°, 180°] / Densité par radian
- **Physique** : Pic vers l'avant (forward peak) si g1>0, arrière si g2<0, isotrope si g=0.
- **Colonnes .dat** : `angle_deg  poids_moyen  densite_par_radian  nb_photons`

### 4. Fonction de phase double HG analytique (`phase_function.png`)
- **Axes** : cosθ ∈ [-1, 1] / p(cosθ)
- **Courbes** : p_HG(g1), p_HG(g2), mélange f·p1+(1-f)·p2
- **Usage** : Comparer la distribution simulée (graphique 3) à la théorie.

### 5. Distribution angulaire en échelle log (`angular_log.png`)
- **Axes** : Angle (deg) / Densité (log Y)
- **Révèle** : Forward peak (ordres de grandeur > queue arrière), bruit statistique aux grands angles.

### 6. Densité par stéradian (`angular_sr.png`)
- **Axes** : Angle (deg) / dP/dΩ
- **Formule** : dP/dΩ = (dP/dθ) / (2π sinθ)
- **Intérêt** : Représentation physique standard (indépendante du maillage angulaire).

### 7. Densité en cosθ (`angular_costheta.png`)
- **Axes** : cosθ ∈ [-1, 1] / p(cosθ)
- **Formule** : p(cosθ) = (dP/dθ) / sinθ
- **Intérêt** : Comparaison directe avec fonction de phase HG.

### 8. Bilan R+T+A vs θ (`bilan_theta.png`) et vs τ (`bilan_tau.png`)
- **Type** : Aire empilée (stacked area)
- **Vérification visuelle** : Somme = 1 sur tout l'intervalle (ligne horizontale à 1).

### 9. Rapports R/T et T/R vs τ (`rapport_rt.png`)
- **Axes** : τ / Rapport (log Y)
- **Courbes** : R/T (rouge), T/R (vert)
- **Physique** : Transition transparent (T/R→∞) → diffusif (R/T≈1) → opaque (R/T→∞).

### 10. Comparaison des formes (`comparaison_formes_RTA.png`, `comparaison_formes_bilan.png`)
- **Scans** : 3 runs séparés (`out/sphere/`, `out/cube/`, `out/cylinder/`) avec mêmes paramètres
- **Superposition** : R, T, A vs τ pour chaque forme
- **Physique** : Géométrie influence chemin optique moyen → flux différents.

### 11. Histogramme collisions (`hist_collisions.png`)
- **Axes** : Nombre de collisions / Nombre de photons
- **Données** : `out/hist_collisions.dat` (`n_collisions  nb_photons`)
- **Moyenne** : `mean_collisions` dans l'en-tête
- **Physique** : Distribution géométrique modifiée par géométrie et absorption.

### 12. Profondeur de sortie (`profondeur.png`)
- **Axes** : Profondeur (proj. axe faisceau) / Poids moyen
- **Données** : `out/profondeur.dat` (`depth  poids`)
- **Bornes** : ± rayon englobant (adaptatif par géométrie)
- **Moyenne** : `mean_depth` dans l'en-tête
- **Physique** : Pénétration moyenne, dépend τ, absorption, géométrie.

## Fichiers de données (`out/`)

| Fichier | Colonnes | Description |
|---------|----------|-------------|
| `angular_distribution.dat` | `angle_deg  poids_moyen  densite_par_radian  nb_photons` | Sortie principale |
| `flux_vs_theta.dat` | `theta  R  T  A` | Scan θ |
| `flux_vs_tau.dat` | `tau  R  T  A` | Scan τ |
| `hist_collisions.dat` | `n_collisions  nb_photons` | Instrumentation |
| `profondeur.dat` | `depth  poids` | Instrumentation |

## Vérifications de cohérence (checklist)

Pour **tout** jeu de paramètres physiquement valides :

- [ ] `R + T + A = 1.000000 ± 1e-6` (bilan exact par construction méthode des poids)
- [ ] `R ≥ 0, T ≥ 0, A ≥ 0` (pas de flux négatifs)
- [ ] `reflected + transmitted = out_weight` (cohérence interne)
- [ ] `photons_in ≤ nphotons` (photons manquants = rate le nuage)
- [ ] `maxsteps_reached = 0` pour τ ≤ 20, absorb > 0 (sinon augmenter `--maxsteps`)
- [ ] Distribution angulaire : ∫ p(θ) dΩ ≈ 1 (vérification par somme pondérée)
- [ ] `mean_collisions` cohérent avec τ et géométrie (τ=1 → ~1-2 collisions)

## Cas de référence analytiques connus

| Cas | Paramètres | Résultat attendu | Tolérance |
|-----|------------|------------------|-----------|
| Sphère, τ→0, isotrope, sans absorption | `--tau 0.001 --g1 0 --g2 0 --absorb 0` | T≈1, R≈0, A≈0 | < 1% |
| Slab infini, diffusion isotrope, albedo=1 | Approximé par cube τ≫1 | R=0.75, T=0.25 (Chandrasekhar) | ~5% (effets bords) |
| Sphère, τ=1, isotrope, albedo=1 | `--tau 1 --absorb 0 --g1 0 --g2 0` | R≈0.38, T≈0.62 | < 3% |
| Conservation énergie forte absorption | `--absorb 0.9 --tau 5` | A ≈ 0.9, R+T ≈ 0.1 | < 2% |

> Voir `make test` pour validation automatisée (reproductibilité, référence, convergence, formes).