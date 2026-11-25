# The World's Hardest Game 2 — Nintendo 3DS (v1.0)

Portage complet de *The World's Hardest Game 2* sur Nintendo 3DS (**50 niveaux principaux + 4 niveaux tutoriel**), livré au format `.3dsx` (Homebrew Launcher) et `.cia` (Menu HOME via FBI).

> **Avertissement légal** : Ce dépôt ne contient que le moteur C et les outils de conversion Python. Les assets extraits du SWF (`romfs/`, `gfx/`, `out/`) ainsi que le `.cia` final ne doivent **jamais** être distribués dans un dépôt public. Vous devez fournir votre propre fichier `theworldshardestgame2.swf`.

---

## Fonctionnalités
- **54 niveaux complets** extraits fidèlement (murs, portes, clés, pièces, checkpoints intermédiaires, trajectoires d'ennemis exactes).
- **Double écran 3DS** :
  - **Écran du haut (400×240)** : terrain de jeu net (rendu vectoriel sans flou, contour sombre continu des murs).
  - **Écran du bas (320×240)** : HUD complet (niveau, morts, barre de progression des pièces, chronomètre, minimap temps réel) + boutons tactiles et grille de sélection 5×10.
- **Audio** : musique de fond streamée en boucle + 6 effets sonores (mort, pièce, clé, victoire, fin de jeu, pause) via `ndsp`. Si `dspfirm.cdc` est absent, le jeu fonctionne sans son et l'affiche sur l'écran titre.
- **Sauvegarde automatique** sur carte SD (`sdmc:/3ds/whg2/save.bin`) : progression, morts totales, records par niveau et options.

---

## Contrôles
| Touche | Action |
|---|---|
| **D-Pad / Circle Pad** | Déplacement en 8 directions (vitesse constante) |
| **A** | Confirmer dans les menus |
| **B** | Recommencer le niveau (en jeu) / Retour (menus) |
| **X** | Afficher / masquer le HUD |
| **Y** | Passer au niveau suivant (débloque le niveau) |
| **L / R** | Niveau précédent / suivant dans la sélection |
| **START** | Pause |
| **SELECT** | Options (Son, Couleur du joueur 1–6, Chronomètre) |
| **Écran tactile** | Boutons du HUD, sélection de niveaux, menus |

---

## Installation sur Nintendo 3DS
- **Via QR Code (FBI -> Remote Install -> Scan QR Code)** :

  ![QR Code FBI](meta/qr_cia.png)

- **Via CIA (recommandé, CFW Luma3DS requis)** : copiez `whg2-3ds.cia` sur votre carte SD et installez-le avec **FBI**.
- **Via Homebrew Launcher** : copiez `whg2-3ds.3dsx` dans `sdmc:/3ds/`.
- *(Optionnel pour le son)* : assurez-vous d'avoir dumpé le firmware DSP (`sdmc:/3ds/dspfirm.cdc`) avec **DSP1** ou **Rosalina** (`L+Down+Select` -> Miscellaneous options -> Dump DSP firmware).

---

## Compiler depuis les sources (avec votre propre SWF)
### Prérequis
- **devkitPro** avec le groupe `3ds-dev` (`libctru`, `citro2d`, `tex3ds`, `bannertool`, `makerom`, `mkbcfnt`).
- **Python 3.10+** avec `pillow` et `resvg-py`.
- **FFmpeg** dans le `PATH`.
- **JPEXS Free Flash Decompiler** (`ffdec-cli`) dans `tools/ffdec/` ou dans le `PATH`.

### Étapes
1. Placez votre fichier `theworldshardestgame2.swf` dans `input/`.
2. Lancez le script de build complet :
   ```bash
   python tools/build_cia.py input/theworldshardestgame2.swf
   ```
   Les fichiers `whg2-3ds.3dsx` et `whg2-3ds.cia` seront générés à la racine.
