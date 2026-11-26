# Tests de validation

## Exécution

```bash
make test
# ou directement :
bash tests/test_photonmc.sh ./photonmc
bash tests/test_reference.sh ./photonmc 0.01
bash tests/test_convergence.sh ./photonmc 0.05
bash tests/test_shapes.sh ./photonmc
```

Chaque script retourne **code de sortie 0 si PASS, 1 si FAIL** (intégrable en CI).

---

## 1. `test_photonmc.sh` — Reproductibilité & Cohérence physique

**Objectif** : Valider que le simulateur est déterministe et conserve l'énergie.

| Test | Méthode | Critère de réussite |
|------|---------|---------------------|
| Reproductibilité | 2 runs identiques (même graine `seed=42`) | \|R₁-R₂\|, \|T₁-T₂\|, \|A₁-A₂\| < 1e-3 |
| Bilan énergie | Extraction R,T,A depuis `angular_distribution.dat` | \|R+T+A - 1\| < 5e-3 |

**Paramètres** : Sphère, τ=2.0, θ=0, absorb=0.1, g1=0.8, g2=-0.3, f=0.7, N=200k, bins=180

**Sortie attendue** :
```
=== Tests de photonmc ===
Binaire : ./photonmc

[1] Reproductibilite (mem graine -> meme sortie)
   Run1 : R=0.326780 T=0.448815 A=0.224405
   Run2 : R=0.326780 T=0.448815 A=0.224405
   PASS

[2] Coherence physique (R + T + A ≈ 1)
   R=0.326780 T=0.448815 A=0.224405  ->  somme=1.000000
   PASS

=== Resultat : 2 OK, 0 FAIL ===
```

---

## 2. `test_reference.sh` — Cas analytique de référence

**Objectif** : Valider contre un cas où la solution exacte est connue.

**Cas** : Sphère, diffusion isotrope (g1=g2=0), sans absorption, τ très faible (0.001).

**Théorie** : Limite optiquement mince → photons traversent sans collision.
- Transmission T → 1
- Réflexion R → 0
- Absorption A → 0

**Paramètres** : `--tau 0.001 --absorb 0.0 --g1 0 --g2 0 --f 0.5 --nphotons 200000`

**Critères** (tolérance configurable, défaut 1%) :
- \|1 - T\| < tol
- \|R\| < tol
- \|A\| < tol
- \|R+T+A - 1\| < 5e-3

**Sortie attendue** :
```
=== Test de reference analytique (tau faible, isotrope, sans absorption) ===
Binaire : ./photonmc

   R=0.000700 T=0.999300 A=0.000000
   PASS

=== Resultat : 1 OK, 0 FAIL ===
```

---

## 3. `test_convergence.sh` — Convergence Monte Carlo

**Objectif** : Vérifier que les estimateurs convergent vers la valeur vraie quand N augmente.

**Méthode** : 3 simulations avec N croissant, comparaison N2 vs N3 (variation relative).

| N | Description |
|---|-------------|
| 20 000 | Run rapide, bruit important |
| 80 000 | Run intermédiaire |
| 320 000 | Run référence (bruit ~1/√N) |

**Critère** : Variation relative \|v₃ - v₂\| / v₂ < 5% (défaut) pour R, T, A.

**Paramètres** : Identiques à test_photonmc.sh (τ=2, absorb=0.1, etc.)

**Sortie attendue** :
```
=== Test de convergence (tolerance relative = 0.05) ===
Binaire : ./photonmc

   N=20000 : R=0.326786 T=0.449920 A=0.223294
   N=80000 : R=0.328088 T=0.448294 A=0.223617
   N=320000 : R=0.326687 T=0.449044 A=0.224269

   PASS (les valeurs convergent entre N=80000 et N=320000)

=== Resultat : 1 OK, 0 FAIL ===
```

> **Note** : La tolérance 5% est conservatrice. Théoriquement, erreur MC ~ 1/√N → variation N=80k→320k attendue ~ √(1/80k - 1/320k) / (1/√80k) ≈ 13% en écart-type, mais corrélation entre runs (même graine) réduit la variance.

---

## 4. `test_shapes.sh` — Validation géométries

**Objectif** : Vérifier que le bilan énergétique tient pour toutes les formes.

**Formes testées** : `cube`, `cylinder` (sphère déjà couverte par autres tests)

**Paramètres** : τ=2.0, θ=30°, absorb=0.1, g1=0.8, g2=-0.3, f=0.7, N=100k

**Critère** : \|R+T+A - 1\| < 5e-3 pour chaque forme.

**Sortie attendue** :
```
[cube] PASS R=0.384242 T=0.386719 A=0.229039 sum=1.000000
[cylinder] PASS R=0.385747 T=0.399859 A=0.214395 sum=1.000001
Resultat : 2 OK, 0 FAIL
```

---

## Couverture actuelle vs idéale

| Catégorie | Couvert | Manquant |
|-----------|---------|----------|
| Reproductibilité | ✅ | - |
| Cas analytique (τ→0) | ✅ | Cas slab infini, sphère τ=1 albedo=1 |
| Convergence N | ✅ | Test variance théorique vs empirique |
| Géométries | ✅ (cube, cylinder) | - |
| **Absorption forte** | ❌ | `absorb=0.9`, `absorb=0.99` |
| **Asymétrie extrême** | ❌ | `g=±0.99`, `g=±0.999` |
| **maxsteps atteint** | ❌ | τ grand, absorb=0 → photons piégés |
| **Graines multiples** | ❌ | Variance inter-graines (Pas seulement intra-graine) |
| **Non-régression .dat** | ❌ | Checksums fichiers de sortie |
| **Valgrind/ASan** | ❌ | Pas de test mémoire automatique |

---

## Ajout de nouveaux tests

### Template script
```bash
#!/usr/bin/env bash
# tests/test_nouveau.sh
set -uo pipefail
BIN="${1:-./photonmc}"
TMPDIR="$(mktemp -d)"
trap 'rm -rf "$TMPDIR"' EXIT
# ... logique test ...
exit $FAIL
```

### Intégration Makefile
```makefile
test: $(BIN)
	@bash tests/test_photonmc.sh ./photonmc
	@bash tests/test_reference.sh ./photonmc 0.01
	@bash tests/test_convergence.sh ./photonmc 0.05
	@bash tests/test_shapes.sh ./photonmc
	@bash tests/test_nouveau.sh ./photonmc   # <-- ajouter
```

### Bonnes pratiques
- **set -uo pipefail** : détection variables non définies + échec pipeline
- **mktemp -d** : isolation runs, nettoyage auto (trap EXIT)
- **Sortie structurée** : `PASS`/`FAIL` + compteurs + exit code
- **Tolérance paramétrable** : `$2` avec défaut raisonnable
- **Silencieux par défaut** : rediriger stdout/stderr binaire vers `/dev/null`, n'afficher que résumé

---

## Intégration CI (GitHub Actions)

```yaml
# .github/workflows/ci.yml (extrait)
- name: Run tests
  run: make test
```

Les scripts retournent **exit code ≠ 0 en cas d'échec** → job CI échoue automatiquement.

---

## Tests manuels recommandés (non automatisés)

```bash
# Absorption forte
./photonmc --absorb 0.9 --tau 5 --nphotons 100000

# Diffusion très avant (g→1)
./photonmc --g1 0.99 --g2 0.99 --f 1.0 --tau 2 --nphotons 100000

# Diffusion très arrière (g→-1)
./photonmc --g1 -0.99 --g2 -0.99 --f 1.0 --tau 2 --nphotons 100000

# Tau très grand sans absorption (test maxsteps)
./photonmc --tau 100 --absorb 0 --nphotons 10000 --maxsteps 1000000

# Vérification fichiers .dat (format)
head -20 out/angular_distribution.dat
```

---

## Métriques de qualité (dashboard)

| Métrique | Valeur actuelle | Cible |
|----------|-----------------|-------|
| Tests unitaires | 4 suites | 8+ suites |
| Couverture cas limites | ~40% | >90% |
| Temps `make test` | ~8 s | < 15 s |
| Faux positifs | 0 | 0 |
| Faux négatifs | Inconnu | 0 |