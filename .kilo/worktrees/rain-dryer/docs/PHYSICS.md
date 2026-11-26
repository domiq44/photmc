# Physique du modèle

## 1. Géométrie du nuage

Le nuage est une forme **convexe centrée en (0,0,0)**. Trois formes supportées :

| Forme | Paramètre `--size` | Paramètre `--height` | L_char | Description |
|-------|-------------------|---------------------|--------|-------------|
| `sphere` | Rayon `R` | Ignoré | `R` | Sphère `x²+y²+z² ≤ R²` |
| `cube` | Demi-arête `L` | Ignoré | `L` | Cube `[-L,L]³` (AABB) |
| `cylinder` | Rayon `R` | Hauteur `H` | `R` | Cylindre axe z, `|z| ≤ H/2`, `x²+y² ≤ R²` |

**Unités** : Toutes les longueurs sont exprimées **en unités de L_char** (donc L_char = 1).
L'épaisseur optique `tau` est définie le long de cette dimension caractéristique :

```
μ = τ / L_char = τ          (coefficient d'atténuation)
```

Le libre parcours exponentiel a donc pour paramètre `μ = τ`.

---

## 2. Faisceau incident

Un faisceau **parallèle, monocromatique** arrive avec un angle d'incidence `θ` (degrés) par rapport à la normale (axe +z). La direction de propagation est :

```
d₀ = (sin θ, 0, cos θ)     (θ ∈ [0°, 90°])
```

### Injection (échantillonnage du disque projeté)

1. **Disque projeté** : rayon = rayon de la sphère englobante `R_bound = geo_bound_radius(geom)`
   - Sphère : `R`
   - Cube : `L√3` (distance centre-sommet)
   - Cylindre : `√(R² + (H/2)²)`

2. **Point uniforme sur le disque** (densité surfacique uniforme) :
   ```
   r = R_bound × √u₁,    φ = 2π u₂     (u₁,u₂ ~ U[0,1))
   (x,y) = (r cos φ, r sin φ) dans base (u,v) ⟂ d₀
   ```

3. **Position de départ** : reculé en amont d'une marge `margin = 1 + R_bound`
   ```
   S = centre + x·u + y·v - d₀ × margin
   ```

4. **Intersection rayon/surface** : premier point d'entrée `t_enter > 0`.
   - Si `t_enter < 0` : point de départ déjà dans le volume → photon rejeté (injection invalide)
   - Photon injecté en `P = S + d₀ × t_enter`, direction `d₀`

**Photons manquants** : Ceux qui ne coupent pas la surface ne sont **pas comptés** comme incidents (`photons_in ≤ nphotons`).

---

## 3. Libre parcours et diffusion

### Libre parcours
À chaque étape, la distance au prochain événement (collision ou sortie) est tirée selon la loi exponentielle de paramètre `μ = τ` :

```
s = -ln(1 - u) / μ,    u ~ U[0,1)
```

Si `s < L` (distance restante jusqu'à la surface) → **collision dans le volume**.
Sinon → **sortie du nuage**.

### Diffusion (fonction de phase double Henyey-Greenstein)

La fonction de phase (densité de probabilité par stéradian) est un mélange de deux HG :

```
p(cos Θ) = f · p_HG(cos Θ; g₁) + (1-f) · p_HG(cos Θ; g₂)
```

où la fonction de phase HG simple est :

```
p_HG(μ; g) = (1 - g²) / [4π (1 + g² - 2g μ)^(3/2)],    μ = cos Θ ∈ [-1, 1]
```

**Paramètres** :
- `g₁, g₂ ∈ [-1, 1]` : asymétrie (g>0 → diffusion avant, g<0 → arrière, g=0 → isotrope)
- `f ∈ [0, 1]` : poids de la composante 1

### Échantillonnage de la double HG

1. **Choix de la composante** : tirage `u₃ ~ U[0,1)`
   - si `u₃ < f` → composante 1 (g₁), renormaliser `u₃' = u₃ / f`
   - sinon → composante 2 (g₂), renormaliser `u₃' = (u₃ - f) / (1-f)`

2. **Inversion CDF de HG** pour `g ≠ 0` :
   ```
   μ = [1 + g² - ((1 - g²) / (1 - g + 2g u))²] / (2g)
   ```
   Pour `g → 0` (isotrope) : `μ = 2u - 1`

3. **Angle azimutal** : `φ = 2π u₄,  u₄ ~ U[0,1)`

4. **Rotation du vecteur direction** : construction base orthonormée locale `(u, v, d)` puis rotation.

---

## 4. Méthode des poids (Survival Biasing / Absorption)

### Problème
Absorption physique : à chaque collision, probabilité `a` que le photon soit absorbé (disparaît).
Simulation naïve : tirage `u ~ U[0,1)`, si `u < a` → photon tué. **Gaspille** les photons absorbés.

### Solution : Méthode des poids
Au lieu de tuer, on **réduit le poids** du photon :

```
w ← w × (1 - a)    (a = probabilité absorption par collision)
```

Le photon **survit toujours** avec un poids diminué. L'espérance de contribution est préservée :

```
E[contribution] = (1-a) × w + a × 0 = (1-a)w
```

**Avantages** :
- Aucun photon gaspillé → variance réduite
- Bilan énergétique exact : `A = 1 - (R + T)` par construction
- Statistiques correctes même pour absorption forte (`a → 1`)

### Algorithme complet par photon

```
w = 1.0
injecter photon → (pos, dir)
boucle maxsteps:
    calculer distance à surface L
    tirer libre parcours s ~ Exp(τ)
    si s < L:           # collision
        pos += dir × s
        w *= (1 - absorb)     # méthode des poids
        tirer nouvelle direction (double HG)
        dir = nouvelle_direction
    else:               # sortie
        pos += dir × L
        projeter pos sur axe d₀ : proj = pos·d₀
        si proj < 0:  reflected   += w
        sinon:        transmitted += w
        incrémenter histogrammes (angle, collisions, profondeur)
        break
si maxsteps atteint: poids restant considéré absorbé (non ajouté aux flux)
```

---

## 5. Classification sortie & Observables

### Réfléchi vs Transmis
Projection de la position de sortie sur l'axe du faisceau incident `d₀` :

```
proj = x·d₀ₓ + y·d₀ᵧ + z·d₀_z
```

- `proj < 0` → photon ressorti **côté entrée** → **Réfléchi (R)**
- `proj ≥ 0` → photon ressorti **côté opposé** → **Transmis (T)**

> **Justification** : Pour un faisceau incident selon `+z` (θ=0), `d₀ = (0,0,1)`, `proj = z`.
> - `z < 0` : face d'entrée (réflexion)
> - `z > 0` : face opposée (transmission)
> Généralisé à tout θ par projection sur `d₀`.

### Flux normalisés
```
R = (Σ w_reflected)   / N_in
T = (Σ w_transmitted) / N_in
A = 1 - R - T
```
où `N_in = photons_in` (photons ayant touché le nuage).

### Angle de diffusion global
Angle entre direction incidente `d₀` et direction de sortie `dir_out` :

```
cos Θ_out = dir_out · d₀
Θ_out = acos(cos Θ_out) ∈ [0, π]
```
Incrémenté dans l'histogramme angulaire (bins uniformes en Θ) **pondéré par le poids `w`**.

### Instrumentation supplémentaire

| Observable | Description | Stockage |
|------------|-------------|----------|
| `hist_collisions[n]` | Nombre de photons ayant subi exactement `n` collisions | `long[]`, taille `max_collisions` (cap 1001) |
| `mean_collisions` | Moyenne collisions/photon (pondérée) | `double` |
| `hist_depth[i]` | Poids moyen par bin de profondeur (proj. sur `d₀`) | `double[]`, 100 bins, bornes ±R_bound |
| `mean_depth` | Profondeur moyenne de sortie (pondérée par poids) | `double` |
| `maxsteps_reached` | Photons ayant atteint `maxsteps` sans sortir | `long` |

---

## 6. Validation physique

### Conservation de l'énergie
Par construction (méthode des poids) :
```
R + T + A ≡ 1      (exact en arithmétique flottante, modulo ε_machine)
```
Vérifié par tous les tests (`make test`).

### Cas limites analytiques

| Cas | Paramètres | Résultat attendu |
|-----|------------|------------------|
| τ → 0, isotrope, sans absorption | `--tau 0.001 --absorb 0 --g1 0 --g2 0` | T → 1, R → 0, A → 0 |
| τ → ∞, albedo=1 (conservatif) | `--tau 100 --absorb 0 --g1 0 --g2 0` | R → 1, T → 0 (sphère) |
| Absorption forte | `--absorb 0.9 --tau 5` | A ≈ 0.9, R+T ≈ 0.1 |
| Diffusion isotrope (g=0) | `--g1 0 --g2 0` | p(cosΘ) = 1/4π (uniforme sphère) |

### Symétries
- **Sphère + θ=0** : symétrie axiale → distribution azimutale uniforme, flux indépendants de θ
- **Cube/Cylindre + θ≠0** : brisure symétrie → dépendance angulaire non triviale

---

## 7. Références

1. **Henyey & Greenstein (1941)** — "Diffuse radiation in the galaxy", ApJ 93, 70. Fonction de phase HG originale.
2. **Chandrasekhar (1960)** — *Radiative Transfer*, Dover. Solutions analytiques slab/sphère.
3. **Wang, Jacques & Zheng (1995)** — "MCML: Monte Carlo modeling of light transport in multi-layered tissues", Comp. Meth. Prog. Biomed. 47, 131. Code de référence biomedical.
4. **Cashwell & Everett (1959)** — *A Practical Manual on the Monte Carlo Method for Random Walk Problems*, Pergamon. Méthode des poids (survival biasing).
5. **Matsumoto & Nishimura (1998)** — "Mersenne Twister: A 623-dimensionally equidistributed uniform pseudo-random number generator", ACM TOMACS 8(1). RNG MT19937.

---

## 8. Limitations du modèle

| Limitation | Impact | Contournement |
|------------|--------|---------------|
| Pas de polarisation | Négligeable pour flux scalaires | Modèle scalaire uniquement |
| Milieu homogène | Pas de gradients τ(r) | Extension possible: τ(x,y,z) |
| Phase function HG seulement | Pas de Mie, Rayleigh, etc. | Extension: table de phase arbitraire |
| Géométries convexes seulement | Pas de coques, cavités | Extension: CSG / maillage surface |
| Monocœur | Temps ∝ N | Parallélisation embarassingly parallel |
| Pas de variance reduction avancée | Russian roulette, splitting | Méthode des poids = survival biasing basique |