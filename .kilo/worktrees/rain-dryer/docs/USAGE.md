# Utilisation en ligne de commande

## Synopsis

```bash
./photonmc [options]
```

## Options de référence

| Option | Type | Défaut | Description |
|--------|------|--------|-------------|
| `--shape` | `sphere\|cube\|cylinder` | `sphere` | Forme du nuage diffusant |
| `--size` | `float > 0` | `1.0` | Dimension caractéristique `L_char` (rayon sphère/cylindre, demi-arête cube) |
| `--height` | `float > 0` | `2.0` | Hauteur totale du cylindre (ignoré pour sphère/cube) |
| `--tau` | `float > 0` | `1.0` | Épaisseur optique le long de `L_char` |
| `--theta` | `float [0, 90]` | `0.0` | Angle d'incidence du faisceau (degrés, par rapport à la normale) |
| `--absorb` | `float [0, 1]` | `0.0` | Probabilité d'absorption par collision |
| `--g1` | `float [-1, 1]` | `0.0` | Asymétrie composante 1 (Henye-Greenstein) |
| `--g2` | `float [-1, 1]` | `0.0` | Asymétrie composante 2 |
| `--f` | `float [0, 1]` | `0.5` | Coefficient de mélange (poids de g₁) |
| `--nphotons` | `long ≥ 1` | `100000` | Nombre de photons incidents simulés |
| `--bins` | `int ≥ 1` | `180` | Nombre de bins angulaires pour distribution sortie |
| `--seed` | `uint32` | `12345` | Graine du générateur aléatoire (reproductibilité) |
| `--maxsteps` | `int ≥ 1` | `100000` | Nombre maximum d'étapes (collisions+libre parcours) par photon |
| `--outdir` | `string` | `out` | Dossier de sortie pour fichiers `.dat` |
| `--scan` | `theta\|tau` | — | Active un balayage paramétrique (écrit `flux_vs_<scan>.dat`) |
| `--from` | `float` | — | Borne inférieure du balayage |
| `--to` | `float` | — | Borne supérieure du balayage |
| `--scan-steps` | `int ≥ 1` | — | Nombre de points du balayage |

## Exemples courants

### Simulation simple (sortie console + fichiers .dat)

```bash
# Sphère, τ=2, θ=0, absorption 10%, double HG g1=0.8/g2=-0.3/f=0.7
./photonmc --shape sphere --tau 2 --absorb 0.1 --g1 0.8 --g2 -0.3 --f 0.7 \
           --nphotons 400000 --bins 180 --seed 12345

# Cube, τ=3, θ=30°, sans absorption, isotrope
./photonmc --shape cube --tau 3 --theta 30 --absorb 0 --g1 0 --g2 0 --f 0.5 \
           --nphotons 200000 --outdir resultats_cube

# Cylindre, τ=5, θ=45°, absorption forte
./photonmc --shape cylinder --size 1 --height 3 --tau 5 --theta 45 \
           --absorb 0.3 --g1 0.5 --g2 -0.5 --f 0.6 \
           --nphotons 100000 --maxsteps 200000
```

### Balayages paramétriques (génération `flux_vs_*.dat`)

```bash
# Balayage angle d'incidence θ ∈ [0°, 80°] (30 points) pour un cube
./photonmc --scan theta --from 0 --to 80 --scan-steps 30 \
           --shape cube --size 1 --tau 3 --absorb 0.05 \
           --g1 0.8 --g2 -0.3 --f 0.7 \
           --nphotons 200000 --bins 180 --seed 12345
# → out/flux_vs_theta.dat (colonnes: theta  R  T  A)

# Balayage épaisseur optique τ ∈ [0.1, 20] (40 points) pour une sphère
./photonmc --scan tau --from 0.1 --to 20 --scan-steps 40 \
           --shape sphere --theta 30 --absorb 0.1 \
           --g1 0.8 --g2 -0.3 --f 0.7 \
           --nphotons 200000 --bins 180 --seed 12345
# → out/flux_vs_tau.dat (colonnes: tau  R  T  A)
```

### Via Makefile (paramètres surchargeables)

```bash
# Compiler seulement
make

# Run simple (sphère τ=1 θ=0)
make run

# Balayages standards + distribution angulaire
make scans
# Variables: NP=400000 BINS=180 SEED=12345

# Graphiques gnuplot (nécessite gnuplot installé)
make plots

# Tout : compile + scans + plots + comparaison formes
make all

# Comparaison 3 formes (sphère/cube/cylindre) vs τ
make scans_formes
make plots_formes

# Interface web locale
make serve
```

## Sorties

### Console (stdout)

```
# resultats (photons_in=399842)
reflected   = 0.326780
transmitted = 0.448815
absorbed    = 0.224405
Ecrit: out/angular_distribution.dat
Ecrit: out/hist_collisions.dat
Ecrit: out/profondeur.dat
```

### Fichiers de données (`out/`)

| Fichier | Colonnes | Description |
|---------|----------|-------------|
| `angular_distribution.dat` | `angle_deg  poids_moyen  densite_par_radian  nb_photons` | Distribution angulaire pondérée |
| `flux_vs_theta.dat` | `theta  R  T  A` | Scan θ (si `--scan theta`) |
| `flux_vs_tau.dat` | `tau  R  T  A` | Scan τ (si `--scan tau`) |
| `hist_collisions.dat` | `n_collisions  nb_photons` | Histogramme collisions |
| `profondeur.dat` | `depth  poids` | Profondeur sortie (proj. axe faisceau) |

**En-têtes** : Chaque fichier commence par des lignes `#` contenant métadonnées (paramètres, sommes, moyennes).

#### Format `angular_distribution.dat` détaillé

```
# Distribution angulaire de sortie (fichier gnuplot)
# angle_deg   poids_moyen   densite_par_radian   nb_photons
# R=0.326780 T=0.448815 A=0.224405  (out_weight=0.775595)
# photons_in=399842  nbins=180
0.500000  0.001234  0.070921  1234
1.500000  0.002345  0.134782  2345
...
179.500000  0.000001  0.000057  1
```

- `poids_moyen` = somme des poids dans le bin / `N_in`
- `densite_par_radian` = `poids_moyen` / `Δθ` (Δθ = π/nbins radians)
- **Intégrale** de la densité sur [0, π] = `out_weight = R + T` (≠ 1 si absorption)

#### Format `hist_collisions.dat`

```
# Histogramme du nombre de collisions par photon
# n_collisions  nb_photons
# mean_collisions=3.245
0  12345
1  45678
2  67890
...
1000  12
```

#### Format `profondeur.dat`

```
# Profondeur de sortie (projection le long de l'axe)
# depth  poids
# mean_depth=0.456
-0.990000  0.001234
-0.970000  0.002345
...
0.990000  0.003456
```

- Bornes : ± rayon englobant (adaptatif selon géométrie)
- 100 bins uniformes

## Codes de retour

| Code | Signification |
|------|---------------|
| `0` | Succès |
| `1` | Erreur arguments / allocation / exécution |

## Variables d'environnement (Makefile)

```bash
make NP=1000000      # nphotons (défaut 400000)
make BINS=360        # bins angulaires (défaut 180)
make SEED=42         # graine (défaut 12345)
make OUTDIR=results  # dossier sortie (défaut out)
```

## Dépannage CLI

| Symptôme | Cause | Solution |
|----------|-------|----------|
| `Erreur d'allocation mémoire` | `nphotons` trop grand ou `bins` trop grand | Réduire `--nphotons` (max 1e9), `--bins` (max 3600) |
| `maxsteps atteint` affiché | Photons piégés (τ grand, absorption=0) | Augmenter `--maxsteps` (max 1e6) |
| `theta doit etre dans [0, 90] deg` | Angle invalide | Vérifier `--theta` |
| `tau doit etre > 0` | τ ≤ 0 | `--tau` strictement positif |
| `g1 doit etre dans [-1, 1]` | Asymétrie hors bornes | Clamper g1/g2 dans [-1, 1] |
| Fichiers `.dat` vides | Simulation n'a produit aucun photon incident | Vérifier géométrie/tau, augmenter `--nphotons` |