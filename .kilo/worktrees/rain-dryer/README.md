# Simulateur Monte Carlo de transport de photons

Simulateur Monte Carlo en C (sans C++) du transport de photons dans un nuage
diffusant avec diffusion multiple et absorption, via la méthode des poids
(survival biasing). Interface web locale (HTML5 canvas / WebAssembly) et PNG
gnuplot.

## Démarrage rapide

```bash
make            # compile ./photonmc
make test       # tests de validation
make serve      # interface web sur http://localhost:8000/
```

## Documentation

- [Physique du modèle](docs/PHYSICS.md)
- [Compilation](docs/BUILD.md)
- [Utilisation CLI](docs/USAGE.md)
- [Interface web](docs/WEB.md)
- [Structure du projet](docs/STRUCTURE.md)
- [Résultats attendus](docs/RESULTS.md)
- [Tests de validation](docs/TESTING.md)
- [Workflow de développement](docs/WORKFLOW.md)
- [Performances](docs/PERFORMANCE.md)
- [CI / Déploiement](docs/CI.md)

## Résumé

| Composant | Statut |
|-----------|--------|
| Simulation C (Monte Carlo) | Fonctionnel, 0 avertissement |
| Interface web (WebAssembly) | Déployée : https://domiq44.github.io/photmc/ |
| Tests (`make test`) | 4/4 passent |
| CI GitHub Actions | Configuré |

Voir [docs/WORKFLOW.md](docs/WORKFLOW.md) pour le déploiement.
