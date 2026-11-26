# Plan complet : The World's Hardest Game 2 sur Nintendo 3DS

Brief à donner à une IA codeur. Objectif : jeu complet (tous les niveaux), 2 écrans, toutes les touches, livré en .3dsx et .cia.
Langage : C. Outils : devkitARM, libctru, citro2d, tex3ds, bannertool, makerom. Convertisseur PC en Python.

Règle absolue : les assets extraits du SWF et le .cia final ne vont JAMAIS dans le dépôt public. Le dépôt contient seulement le moteur et le convertisseur ; l'utilisateur fournit son propre SWF.

---

## Sources de référence
- [S1] Collection SWF (WHG 1 à 4, WHG2 "all packed" 4,9 Mo) : https://restickedagain.itch.io/twhgcompleteseries
- [S2] Historique des versions et archivage Flashpoint (WHG2 v2.0, juillet 2008, SWF) : https://flashpointarchive.org/datahub/Game_indexes/The_World's_Hardest_Game
- [S3] JPEXS Free Flash Decompiler, ligne de commande (export shape/sprite/image/sound/script/text, dump AS2/AS3) : https://github.com/jindrapetrik/jpexs-decompiler/wiki/Commandline-arguments
- [S4] Exemples officiels 3DS (libctru, citro2d, citro3d) : https://github.com/devkitPro/3ds-examples
- [S5] Doc libctru : https://devkitpro.github.io/libctru/
- [S6] Exemple de jeu citro2d à 2 écrans (plateau en haut, tactile en bas), packaging CIA, DSP firmware : https://github.com/TheFilipcom4607/BlockBlast-3DS
- [S7] Exemple de Makefile avec romfs, bannertool et makerom (cibles .3dsx et .cia) : https://github.com/Asellus/3DS_Quick_Reboot/blob/master/Makefile
- [S8] Makerom (NCCH, CCI, CIA, option -romfs) : https://3dbrew.org/wiki/Makerom
- [S9] Exemple de CIA construit par script chez l'utilisateur à partir de ses propres fichiers : https://github.com/ikevik/sms-3ds/releases/tag/v0.1.0-beta
- [S10] Exemple de port 2 écrans avec écran tactile utile : https://github.com/EstebanPdN/zelda-tmc-3ds/wiki

---

## Phase 0 : Environnement (0,5 jour) — **FAIT**
1. **[FAIT]** Installer devkitPro avec le groupe de paquets `3ds-dev` (libctru, citro2d, citro3d, tex3ds) [S4][S6]. *(Déjà présent dans `C:\devkitPro`)*
2. **[FAIT]** Installer Java + JPEXS FFDec [S3], Python 3.10+, Pillow, cairosvg (ou inkscape) pour rastériser les SVG. *(Java 25, Python 3.15, Pillow 13, `resvg-py`/`cairosvg` et JPEXS FFDec 26.5.0 dans `tools/ffdec/`)*
3. **[FAIT]** Récupérer `makerom` et `bannertool` (non fournis par devkitPro) dans `tools/` [S6][S8]. *(Déjà présents dans `C:\devkitPro\tools\bin`)*
4. **[FAIT]** Structure du dépôt :
   ```
   whg2-3ds/
     source/        moteur C
     romfs/         (généré, ignoré par git)
     gfx/           (généré, ignoré par git)
     tools/         convertisseur Python, makerom, bannertool
     meta/          icon.png, banner, whg2.rsf
     Makefile
   ```
5. **[FAIT]** `.gitignore` : `romfs/`, `gfx/`, `*.swf`, `*.cia`, `*.t3x`.
Validation : **[FAIT]** un "hello world" citro2d compile en .3dsx [S4] (`whg2-3ds.3dsx` généré).

## Phase 1 : Obtenir et analyser le SWF (0,5 à 1 jour) — **FAIT**
1. **[FAIT]** L'utilisateur place `theworldshardestgame2.swf` dans `input/` [S1][S2].
2. **[FAIT]** Dumper la structure : `ffdec-cli -dumpSWF`, puis `-dumpAS2` ou `-dumpAS3` selon la version [S3].
3. **[FAIT]** Exporter tout : `ffdec-cli -export all out/ input/whg2.swf` [S3].
4. **[FAIT]** Produire `ANALYSE.md` avec :
   - version ActionScript (AS2 ou AS3) ;
   - liste des niveaux (au moins 37 d'après [S2], à vérifier) ;
   - où sont définis niveaux, murs, ennemis, trajectoires (code ou timeline) ;
   - liste des symboles : joueur, ennemis, pièces, checkpoints, zone d'arrivée, fond, boutons, texte ;
   - liste des sons et de la musique ;
   - toutes les règles de gameplay trouvées dans le code (vitesse, taille du joueur, rayon des ennemis, collisions, respawn, ordre des pièces, portes/clés si présentes).
Validation : **[FAIT]** `ANALYSE.md` complet avant d'écrire le moteur.

## Phase 2 : Convertisseur PC, graphismes (1 à 2 jours) — **FAIT**
1. **[FAIT]** Rastériser les shapes/sprites SVG en PNG à la résolution native du jeu [S3].
2. **[FAIT]** Détecter les sprites répétés (joueur, ennemis, pièces) et les mettre dans une spritesheet 1024x1024 max (limite GPU de la 3DS). *(Via `tools/graphics.py` : 9 sprites `player_1..6`, `enemy_blue`, `coin`, `key`)*
3. **[FAIT]** Générer les `.t3s` et compiler avec `tex3ds` en `.t3x` dans `romfs/gfx/` [S4]. *(`gfx/sprites.t3s` -> `romfs/gfx/sprites.t3x` + `build/sprites.h`)*
4. **[FAIT]** Murs et sols : ne pas les convertir en images. Les exporter en géométrie (rectangles, couleurs) et les dessiner avec `C2D_DrawRectSolid` : moins de RAM, rendu net.
5. **[FAIT]** Police : exporter la police ou la redessiner en bitmap font (`.bcfnt` via `mkbcfnt`) ou utiliser les glyphes en sprites. *(`romfs/font.bcfnt` généré)*
Validation : **[FAIT]** un script de debug affiche la spritesheet et vérifie que tous les symboles sont présents.

## Phase 3 : Convertisseur PC, niveaux (1 à 3 jours) — **FAIT**
1. **[FAIT]** Écrire `tools/levels.py` qui lit les données extraites (script ou timeline) et produit un binaire par niveau dans `romfs/levels/NN.lvl`. *(54 niveaux : `01..50` = niveaux 1..50, `51..54` = tutoriel 1..4)*
2. **[FAIT]** Format binaire (little-endian) :
   ```
   header : magic "WHG2", version, id, largeur, hauteur
   murs   : n, puis rectangles (x, y, w, h)
   portes : n, puis (door_id, x, y, w, h)
   checkpoints : n, puis (kind, x, y, w, h)
   pièces : n, positions
   clés   : n, puis (door_id, x, y)
   ennemis: n, puis trajectoire timeline exacte (num_frames, rayon, positions (x*2, y*2))
   ```
3. **[FAIT]** Si les niveaux sont dans la timeline, extraire les positions image par image et en déduire les trajectoires (ou stocker les frames brutes si ce n'est pas paramétrable).
4. **[FAIT]** Test PC : script de validation `tools/verify_levels.py` vérifiant l'intégrité des 54 `.lvl`.
Validation : **[FAIT]** les 54 niveaux se chargent et sont vérifiés.

## Phase 4 : Moteur de jeu (3 à 5 jours) — **FAIT**
Écrire la logique dans des fichiers SANS dépendance 3DS (`game.c/h`, `level.c/h`, `physics.c/h`), testables sur PC avec gcc, comme dans [S6].
1. **[FAIT]** Boucle à 60 FPS avec pas de temps fixe.
2. **[FAIT]** Joueur : déplacement à vitesse constante en 8 directions, collision avec les murs par axe (glissement le long des murs).
3. **[FAIT]** Ennemis : tous les types de trajectoires de la phase 3.
4. **[FAIT]** Collisions : joueur (carré) contre ennemis (cercles), test carré/cercle exact.
5. **[FAIT]** Mort : respawn au dernier checkpoint ou au départ, compteur de morts, pièces perdues si elles ne sont pas validées (comme l'original).
6. **[FAIT]** Pièces, checkpoints, zone d'arrivée, passage au niveau suivant.
7. **[FAIT]** Éléments spécifiques à WHG2 trouvés dans `ANALYSE.md` (portes `door1..3`, clés `key1..3`, checkpoints intermédiaires `check3..6`, niveaux à zone unique `check1`).
Validation : **[FAIT]** compilation sans erreur ni avertissement dans `source/level.c`, `source/physics.c`, `source/game.c`.

## Phase 5 : Rendu 3DS, deux écrans (1 à 2 jours) — **FAIT**
Utiliser citro2d avec `C2D_CreateScreenTarget(GFX_TOP, GFX_LEFT)` et `GFX_BOTTOM` [S4][S6].
- **[FAIT]** Écran du haut (400x240) : le niveau, mis à l'échelle pour tenir dans 400x240 (échelle 0,48x centrée 336x240).
- **[FAIT]** Écran du bas (320x240) : HUD et tactile :
  - numéro de niveau, compteur de morts, pièces (x/y), temps ;
  - boutons tactiles : Pause, Recommencer, Menu ;
  - minimap du niveau.
- **[FAIT]** Écran de sélection de niveaux : grille tactile 5x10 sur l'écran du bas, niveaux verrouillés/débloqués ; l'écran du haut montre un aperçu du niveau sélectionné [S6][S10].
- **[FAIT]** Option : effet 3D stéréoscopique léger via le curseur 3D (`GFX_LEFT` / `GFX_RIGHT`).
Validation : **[FAIT]** compilé sans erreur dans `source/render.c` et `source/main.c`.

## Phase 6 : Contrôles (0,5 jour) — **FAIT**
Lire avec `hidScanInput`, `hidKeysHeld`, `hidKeysDown`, `hidTouchRead` [S5].
| Touche | Action |
|---|---|
| D-pad | déplacement |
| Circle Pad | déplacement (même vitesse que le D-pad, pas de vitesse analogique) |
| C-stick (New 3DS) | pas utilisé |
| A | confirmer dans les menus |
| B | recommencer le niveau (en jeu), retour (menus) |
| X | afficher/masquer le HUD |
| Y | niveau suivant débloqué (debug option) |
| L / R | niveau précédent / suivant dans le menu de sélection |
| START | pause (menu : reprendre, recommencer, options, quitter) |
| SELECT | options (son, couleur joueur, afficher le chronomètre) |
| Écran tactile | boutons du HUD et sélection de niveaux |
Validation : **[FAIT]** navigation complète aux boutons et au stylet implémentée dans `source/main.c`.

## Phase 7 : Audio (1 jour) — **FAIT**
1. **[FAIT]** Convertir les sons exportés par FFDec (WAV/MP3) [S3] en WAV 16 bits PCM mono 22 kHz (`tools/audio.py` -> `romfs/sfx/*.wav`).
2. **[FAIT]** Initialiser `ndsp` (`ndspInit`) ; si `dspfirm.cdc` est absent, le jeu tourne sans son et l'indique à l'écran titre [S6] (`source/audio.c`).
3. **[FAIT]** Effets : mort, pièce, clé, niveau terminé, fin du jeu, pause. Musique en boucle streamée.
Validation : **[FAIT]** compilé et lié avec `ndsp` dans `source/audio.c`.

## Phase 8 : Sauvegarde et options (0,5 jour) — **FAIT**
- **[FAIT]** Fichier `sdmc:/3ds/whg2/save.bin` : niveaux débloqués, morts totales, meilleur score par niveau, options (`source/save.c`).
- **[FAIT]** Écriture après chaque niveau, dans les options et à la sortie.

## Phase 9 : Packaging (1 jour) — **FAIT**
1. **[FAIT]** `meta/whg2.rsf` basé sur l'exemple de makerom [S6][S8] (APP_TITLE, APP_PRODUCT_CODE, APP_UNIQUE_ID, RomFs, mode système 64MB).
2. **[FAIT]** Makefile : cibles `all` (.3dsx avec `--romfs`), `cia` (bannertool `makebanner` et `makesmdh`, puis `makerom -f cia -rsf ... -target t -elf ... -icon ... -banner ... -o whg2-3ds.cia`) [S7][S8].
3. **[FAIT]** Script `tools/build_cia.py` (Windows/Linux) qui prend le SWF de l'utilisateur, lance tout le pipeline (FFDec, convertisseur, make, makerom) et sort le `.3dsx` et le `.cia`, sur le modèle de [S9].
4. **[FAIT]** Installation : `.3dsx` dans `sdmc:/3ds/` via Homebrew Launcher ; `.cia` via FBI, nécessite un CFW (Luma3DS) [S6].
Validation : **[FAIT]** `whg2-3ds.3dsx` (7,7 Mo) et `whg2-3ds.cia` (7,8 Mo) générés avec icône, bannière et RomFS.

## Phase 10 : Tests et finition (2 à 4 jours) — **FAIT (prêt pour test console/émulateur)**
- **[FAIT]** Vérification des 54 niveaux extraits (`tools/verify_levels.py` : 54/54 OK).
- **[FAIT]** Compilation complète `.3dsx` et `.cia` sans erreur.
- **[FAIT]** `README.md` : instructions d'utilisation avec son propre SWF, contrôles, et rappel de ne pas distribuer les assets.
- *(À tester par l'utilisateur sur console réelle old 3DS / New 3DS ou Citra/Azahar pour valider le ressenti des hitboxes et du timing.)*

---

## Durée estimée (30 à 60 heures, 1 à 3 semaines à temps partiel)
| Phase | Durée |
|---|---|
| 0 Environnement | 0,5 j |
| 1 Analyse SWF | 0,5 à 1 j |
| 2 Graphismes | 1 à 2 j |
| 3 Niveaux | 1 à 3 j |
| 4 Moteur | 3 à 5 j |
| 5 Deux écrans | 1 à 2 j |
| 6 Contrôles | 0,5 j |
| 7 Audio | 1 j |
| 8 Sauvegarde | 0,5 j |
| 9 Packaging | 1 j |
| 10 Tests | 2 à 4 j |

Principal risque : le format des niveaux dans le SWF (phase 3). S'ils sont dans la timeline, prévoir +2 à 4 jours.
Jalon rapide : phases 0 à 5 avec 3 niveaux = prototype jouable en 3 à 5 jours.
