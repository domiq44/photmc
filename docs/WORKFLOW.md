# Workflow de développement

## Cycle local (modification → test → validation)

### 1. Modifier le code

```bash
# Éditer sources
$EDITOR src/mc.c src/geometry.c src/phase.c ...

# Ou interface web
$EDITOR web/app.js web/index.html web/style.css ...
```

### 2. Recompiler le binaire C

```bash
make compile          # Compilation rapide (objets + binaire)
# ou simplement
make                  # Cible par défaut = compile
```

**Vérification** : `0 warning, 0 error` avec flags stricts.

### 3. (Optionnel) Recompiler WebAssembly

```bash
# Nécessite Emscripten installé et activé
source ~/emsdk/emsdk_env.sh   # ou chemin vers votre emsdk
make wasm

# Produit : web/photonmc.js + web/photonmc.wasm
# Taille attendue : ~1.2 MB JS + ~450 KB WASM (gzippé: ~280 KB + ~140 KB)
```

> **Note** : En mode développement local, `make serve` utilise le binaire C natif → **pas besoin de recompiler WASM** à chaque modification C.

### 4. Lancer les tests de validation

```bash
make test
```

**Résultat attendu** : 4/4 suites PASS
```
=== Tests de photonmc === ... PASS
=== Test de reference analytique === ... PASS
=== Test de convergence === ... PASS
=== Test des formes === ... PASS
```

### 5. Tester l'interface web

#### Mode serveur local (recommandé dev)
```bash
make serve
# Lance python3 web/server.py --bin ./photonmc --outdir out --port 8000
# Ouvrir http://localhost:8000/
```
- Recharge auto navigateur (F5) après modif HTML/JS/CSS
- Modif C → `make compile` → test immédiat (pas de rebuild WASM)

#### Mode statique (test WASM local)
```bash
make wasm
python3 -m http.server 8000 --directory web/
# Ouvrir http://localhost:8000/ → mode WASM automatique
```

### 6. Générer graphiques de référence (optionnel)

```bash
make plots          # 12 PNG dans out/ (nécessite gnuplot)
make plots_formes   # Comparaison 3 formes
```

---

## Cycle distant (GitHub Pages)

### 1. Commit & Push

```bash
git add -A
git commit -m "feat: description courte"
git push origin main
```

### 2. Surveillance CI (onglet Actions)

Le workflow `.github/workflows/ci.yml` exécute :

| Job | Durée | Vérifications |
|-----|-------|---------------|
| `build-and-test` | ~3-4 min | Compile C, `make test`, compile WASM, vérifie artefacts |
| `deploy-pages` | ~2 min | Déploie `web/` sur branche `gh-pages` (seulement si `main`) |

**Indicateurs de succès** :
- ✅ Vert sur les deux jobs
- `make test` : 4 PASS
- `web/photonmc.js` + `web/photonmc.wasm` présents

### 3. Vérification déploiement

```bash
# Attendre ~1-2 min après fin CI
# Hard refresh navigateur (Ctrl+F5 / Cmd+Shift+R)
open https://<USERNAME>.github.io/photmc/
```

> Remplacez `<USERNAME>` par votre user/org GitHub.

### 4. Dépannage déploiement

| Problème | Solution |
|----------|----------|
| Page 404 | Settings > Pages > Source = `gh-pages` / root |
| Ancien WASM chargé | Hard refresh (Ctrl+F5) ou vider cache navigateur |
| Erreur JS console | Vérifier version `photonmc.js` match `photonmc.wasm` (même commit) |
| CI `deploy-pages` skipped | Push sur branche ≠ `main` (PR ne déploie pas) |

---

## Fichiers générés (non versionnés)

Ces fichiers sont dans `.gitignore` et **régénérés automatiquement** :

| Fichier/Dossier | Généré par | Régénération |
|-----------------|------------|--------------|
| `obj/` | `make compile` | `make` |
| `photonmc` | `make compile` | `make` |
| `out/` | `make scans` / `./photonmc` | `make scans` / `make plots` |
| `web/photonmc.js` | `make wasm` | `make wasm` (local + CI) |
| `web/photonmc.wasm` | `make wasm` | `make wasm` (local + CI) |

> **Ne pas commiter** ces fichiers. La CI régénère WASM à chaque push sur `main`.

---

## Bonnes pratiques

### Commits
```bash
# Messages conventionnels recommandés
git commit -m "feat: add cylinder geometry support"
git commit -m "fix: correct phase sampling for g≈1"
git commit -m "perf: vectorize collision loop"
git commit -m "docs: update PHYSICS.md with Chandrasekhar ref"
git commit -m "test: add absorption edge case test"
```

### Branches
- `main` : stable, déployée sur GitHub Pages
- Feature branches : `feat/nom`, `fix/nom`, `perf/nom`
- PR vers `main` → CI exécute tests complets

### Tests avant push
```bash
# Checklist pre-push
make clean && make compile    # Build propre
make test                     # Tous tests PASS
make wasm                     # WASM compile (si emsdk dispo)
# Optionnel: make plots       # Graphiques OK
```

### Modifications C → WASM
Si vous modifiez `src/*.c` ou `src/*.h` :
```bash
make wasm   # Recompile tout (mc_wasm.c + dépendances)
```
Le Makefile gère les dépendances correctement.

---

## Outils de développement recommandés

| Outil | Installation | Usage |
|-------|--------------|-------|
| `clang-format` | `apt install clang-format` | Formatage auto (`clang-format -i src/*.c src/*.h`) |
| `cppcheck` | `apt install cppcheck` | Analyse statique (`cppcheck --enable=all src/`) |
| `valgrind` | `apt install valgrind` | Détection fuites (`valgrind ./photonmc ...`) |
| `perf` | `apt install linux-tools` | Profilage CPU (`perf record -g ./photonmc ...`) |
| `hyperfine` | `cargo install hyperfine` | Benchmark CLI (`hyperfine './photonmc --nphotons 100000'`) |
| `bear` | `apt install bear` | Génère `compile_commands.json` (`bear -- make`) |

---

## Checklist release (version mineure)

- [ ] `make clean && make compile && make test` : PASS
- [ ] `make wasm` : artefacts valides
- [ ] `make serve` → test manuel interface web
- [ ] Mise à jour `README.md` (badges, version)
- [ ] Tag Git : `git tag v1.x.0 && git push --tags`
- [ ] GitHub Release : notes + artefacts WASM (optionnel)