# CI / Déploiement Continu

## Workflow GitHub Actions (`.github/workflows/ci.yml`)

### Architecture à deux jobs

```yaml
jobs:
  build-and-test:      # Exécuté sur push/PR vers main
    runs-on: ubuntu-latest
    steps:
      - checkout
      - make              # Compilation C native
      - make test         # 4 suites de tests
      - setup-emsdk       # Installation Emscripten
      - make wasm         # Compilation WebAssembly
      - test -f web/photonmc.js && test -f web/photonmc.wasm

  deploy-pages:         # Exécuté SEULEMENT sur push vers main (pas PR)
    needs: build-and-test
    runs-on: ubuntu-latest
    if: github.ref == 'refs/heads/main'
    steps:
      - checkout
      - setup-emsdk
      - make wasm
      - deploy to gh-pages (peaceiris/actions-gh-pages@v4)
```

### Déclencheurs

| Événement | Jobs exécutés |
|-----------|---------------|
| Push vers `main` | `build-and-test` → `deploy-pages` |
| Pull Request vers `main` | `build-and-test` seulement |
| Push vers autre branche | `build-and-test` seulement |

### Artefacts vérifiés

- **Binaire C** : `./photonmc` (ELF 64-bit, compilé sans warning)
- **Tests** : 4/4 PASS (reproductibilité, référence, convergence, formes)
- **WASM** : `web/photonmc.js` + `web/photonmc.wasm` (présents et valides)

### Temps d'exécution CI (indicatif)

| Job | Durée typique |
|-----|---------------|
| `build-and-test` | ~3-4 min (dont ~2 min installation emsdk) |
| `deploy-pages` | ~2 min |
| **Total** | **~5-6 min** |

---

## Configuration GitHub Pages

### Activation (une fois)

1. Repository **Settings** → **Pages** (menu latéral)
2. **Build and deployment** → **Source** : "Deploy from a branch"
3. **Branch** : `gh-pages` / **Folder** : `/ (root)`
4. **Save**

### URL déployée

```
https://<USERNAME>.github.io/photmc/
```

> Remplacez `<USERNAME>` par votre nom d'utilisateur/organisation GitHub.

### Mise à jour manuelle (si Actions échoue)

```bash
# En local
make wasm
git checkout gh-pages || git checkout --orphan gh-pages
git rm -rf .
cp -r web/* .
git add -A
git commit -m "Deploy WASM $(date)"
git push origin gh-pages --force
```

---

## Variables d'environnement / Secrets

Aucun secret requis. Le déploiement utilise `GITHUB_TOKEN` fourni automatiquement.

---

## Personnalisation

### Changer la branche de déploiement

```yaml
# .github/workflows/ci.yml
deploy-pages:
  if: github.ref == 'refs/heads/votre-branche'
```

### Ajouter des étapes (ex: analyse statique, benchmark)

```yaml
- name: Static analysis (cppcheck)
  run: |
    sudo apt-get update && sudo apt-get install -y cppcheck
    cppcheck --enable=all --std=c11 -I src src/

- name: Benchmark perf
  run: |
    make compile
    ./photonmc --nphotons 1000000 --tau 5 > /dev/null
    # ou hyperfine
```

### Matrice de tests (multi-compilateurs)

```yaml
strategy:
  matrix:
    compiler: [gcc-11, gcc-12, gcc-13, clang-15, clang-16]
    include:
      - compiler: gcc-13
        cc: gcc-13
      - compiler: clang-16
        cc: clang-16
steps:
  - name: Compile with ${{ matrix.compiler }}
    run: make CC=${{ matrix.cc }} clean compile test
```

---

## Dépannage CI

| Problème | Cause | Solution |
|----------|-------|----------|
| `emcc: command not found` | emsdk non activé | `setup-emsdk` action gère l'activation |
| `make test` échoue | Flottement statistique rare | Relancer workflow (bouton "Re-run jobs") |
| `deploy-pages` skipped | Push sur branche ≠ main | Vérifier `if: github.ref == 'refs/heads/main'` |
| 404 sur GitHub Pages | Branch `gh-pages` absente | Vérifier Settings > Pages > Source = gh-pages |
| WASM trop gros | `-O2` pas appliqué | Vérifier flags `emcc -O2` dans Makefile |

---

## Métriques de santé CI

| Métrique | Cible | Alerte si |
|----------|-------|-----------|
| Durée `build-and-test` | < 5 min | > 10 min |
| Taux de réussite tests | 100% | < 100% |
| Taille `photonmc.wasm` | < 500 KB | > 1 MB |
| Déploiement réussi | 100% | Échec |