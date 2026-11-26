# The World's Hardest Game 2 sur Nintendo 3DS

Ce dossier racine contient le port autonome de *The World's Hardest Game 2*. Le projet final qui regroupe les quatre jeux est dans [`hub/`](hub/).

## Builds

Depuis la racine du projet, `make` compile le jeu autonome et produit `whg2-3ds.3dsx` ainsi que `whg2-3ds.elf`.

Pour fabriquer le CIA autonome installable depuis FBI :

```sh
make cia
```

Le CIA autonome obtenu est `whg2-3ds.cia`.

### Hub final v1.2

Le build final du hub se fait dans `hub/`, pas avec le Makefile de la racine :

```sh
cd hub
make cia
```

Il produit `hub/whg-hub-3ds.cia`. Le hub utilise le logo Nintendo standard au lancement, une bannière HOME fixe et son jingle. Les réglages de la bannière sont dans `hub/Makefile`.

## Installation

- Pour le hub, copie `hub/whg-hub-3ds.cia` sur la carte SD et installe-le avec FBI.
- Pour le jeu autonome, utilise `whg2-3ds.cia` ou place `whg2-3ds.3dsx` dans un dossier d'application sous `sdmc:/3ds/`.

## Contrôles du jeu autonome

| Touche | Action |
|---|---|
| D-Pad / Circle Pad | Déplacer le personnage |
| A | Confirmer |
| B | Recommencer le niveau ou revenir en arrière |
| X | Afficher ou masquer le HUD |
| Y | Passer au niveau suivant |
| L / R | Changer de niveau dans la sélection |
| START | Mettre en pause |
| SELECT | Ouvrir les options |
| Écran tactile | Utiliser le HUD et les menus |

## Prérequis de compilation

- devkitPro avec les outils 3DS (`libctru`, `citro2d`, `tex3ds`, `bannertool` et `makerom`)
- Un terminal MSYS2/devkitPro avec `make` configuré
- Pour régénérer les assets du hub : Python et les dépendances fournies dans `hub/tools/python-site`

Les assets et les données de jeu sont destinés à un usage personnel. Ne redistribue pas de fichiers de jeu protégés par le droit d'auteur.
