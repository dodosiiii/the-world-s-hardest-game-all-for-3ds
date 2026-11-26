#include "language.h"

#include <stdbool.h>
#include <string.h>

typedef struct {
    const char* french;
    const char* english;
    const char* italian;
    const char* spanish;
} Translation;

static const Translation s_translations[] = {
    { "Rapide  x1.25", "Fast  x1.25", "Veloce  x1.25", "Rapido  x1.25" },
    { "Turbo  x1.5", "Turbo  x1.5", "Turbo  x1.5", "Turbo  x1.5" },
    { "Normale  x1.0", "Normal  x1.0", "Normale  x1.0", "Normal  x1.0" },
    { "Suivi  x1.5", "Follow  x1.5", "Segui  x1.5", "Seguimiento  x1.5" },
    { "Proche  x2.0", "Close  x2.0", "Vicino  x2.0", "Cerca  x2.0" },
    { "Global  x1.0", "Global  x1.0", "Globale  x1.0", "Global  x1.0" },
    { "Niveau %u / %u", "Level %u / %u", "Livello %u / %u", "Nivel %u / %u" },
    { "Niveau %u", "Level %u", "Livello %u", "Nivel %u" },
    { "Succes %u%%", "Achievements %u%%", "Obiettivi %u%%", "Logros %u%%" },
    { "Bientot disponible", "Coming soon", "Prossimamente", "Proximamente" },
    { "PROGRESSION", "PROGRESS", "PROGRESSIONE", "PROGRESO" },
    { "MORTS", "DEATHS", "MORTI", "MUERTES" },
    { "Son indisponible : dspfirm.cdc manquant", "Sound unavailable: dspfirm.cdc missing", "Audio non disponibile: dspfirm.cdc mancante", "Audio no disponible: falta dspfirm.cdc" },
    { "SUCCES DEBLOQUE", "ACHIEVEMENT UNLOCKED", "OBIETTIVO SBLOCCATO", "LOGRO DESBLOQUEADO" },
    { "Monde", "World", "Mondo", "Mundo" },
    { "Niveau", "Level", "Livello", "Nivel" },
    { "%s %d / %u", "%s %d / %u", "%s %d / %u", "%s %d / %u" },
    { "Record : %d morts", "Record: %d deaths", "Record: %d morti", "Record: %d muertes" },
    { "%u pieces", "%u coins", "%u monete", "%u monedas" },
    { "Tutoriel %d / %u", "Tutorial %d / %u", "Tutorial %d / %u", "Tutorial %d / %u" },
    { "TERMINE", "COMPLETED", "COMPLETATO", "COMPLETADO" },
    { "Felicitations", "Congratulations", "Congratulazioni", "Felicidades" },
    { "Vous avez termine %s.", "You completed %s.", "Hai completato %s.", "Has completado %s." },
    { "MORTS TOTALES", "TOTAL DEATHS", "MORTI TOTALI", "MUERTES TOTALES" },
    { "HUD masque", "HUD hidden", "HUD nascosto", "HUD oculto" },
    { "Appuyez sur X pour afficher", "Press X to show", "Premi X per mostrare", "Pulsa X para mostrar" },
    { "Monde %u / 5  -  Salle %c%u", "World %u / 5  -  Room %c%u", "Mondo %u / 5  -  Stanza %c%u", "Mundo %u / 5  -  Sala %c%u" },
    { "Niveau %u / %u", "Level %u / %u", "Livello %u / %u", "Nivel %u / %u" },
    { "Tutoriel %u / %u", "Tutorial %u / %u", "Tutorial %u / %u", "Tutorial %u / %u" },
    { "MORTS", "DEATHS", "MORTI", "MUERTES" },
    { "ARGENT", "CASH", "DENARO", "DINERO" },
    { "PIECES", "COINS", "MONETE", "MONEDAS" },
    { "TEMPS", "TIME", "TEMPO", "TIEMPO" },
    { "ZOOM", "ZOOM", "ZOOM", "ZOOM" },
    { "Zoom", "Zoom", "Zoom", "Zoom" },
    { "Recommencer", "Restart", "Ricomincia", "Reiniciar" },
    { "Masquer HUD", "Hide HUD", "Nascondi HUD", "Ocultar HUD" },
    { "Pause", "Pause", "Pausa", "Pausa" },
    { "Menu", "Menu", "Menu", "Menu" },
    { "Choisir un jeu", "Choose a game", "Scegli un gioco", "Elige un juego" },
    { "Options", "Options", "Opzioni", "Opciones" },
    { "Ouvrir", "Open", "Apri", "Abrir" },
    { "SELECT", "SELECT", "SELECT", "SELECT" },
    { "Hub", "Hub", "Hub", "Hub" },
    { "Continuer", "Continue", "Continua", "Continuar" },
    { "Jouer", "Play", "Gioca", "Jugar" },
    { "Niveaux", "Levels", "Livelli", "Niveles" },
    { "%u niveaux", "%u levels", "%u livelli", "%u niveles" },
    { "Tutoriel", "Tutorial", "Tutorial", "Tutorial" },
    { "Succes", "Achievements", "Obiettivi", "Logros" },
    { "Valider", "Confirm", "Conferma", "Confirmar" },
    { "B", "B", "B", "B" },
    { "Choisir une salle", "Choose a room", "Scegli una stanza", "Elige una sala" },
    { "Retour", "Back", "Indietro", "Atras" },
    { "Monde %d", "World %d", "Mondo %d", "Mundo %d" },
    { "Jouer", "Play", "Gioca", "Jugar" },
    { "Monde", "World", "Mondo", "Mundo" },
    { "Niveaux", "Levels", "Livelli", "Niveles" },
    { "Jeu", "Game", "Gioco", "Juego" },
    { "OBTENU", "UNLOCKED", "SBLOCCATO", "DESBLOQUEADO" },
    { "Defiler", "Scroll", "Scorri", "Desplazar" },
    { "1.  Bases", "1.  Basics", "1.  Basi", "1.  Basicos" },
    { "2.  Ennemis", "2.  Enemies", "2.  Nemici", "2.  Enemigos" },
    { "3.  Cles", "3.  Keys", "3.  Chiavi", "3.  Llaves" },
    { "4.  Pieces", "4.  Coins", "4.  Monete", "4.  Monedas" },
    { "Reprendre", "Resume", "Riprendi", "Continuar" },
    { "Quitter au menu", "Quit to menu", "Esci al menu", "Salir al menu" },
    { "Son", "Sound", "Audio", "Sonido" },
    { "Couleur", "Color", "Colore", "Color" },
    { "Chronometre", "Timer", "Cronometro", "Cronometro" },
    { "Vitesse", "Speed", "Velocita", "Velocidad" },
    { "Zoom camera", "Camera zoom", "Zoom telecamera", "Zoom de camara" },
    { "Modifier", "Change", "Modifica", "Cambiar" },
    { "Touchez l'ecran ou appuyez sur A", "Touch the screen or press A", "Tocca lo schermo o premi A", "Toca la pantalla o pulsa A" },
    { "Choisir la langue", "Choose a language", "Scegli la lingua", "Elige el idioma" },
    { "Langue", "Language", "Lingua", "Idioma" },
    { "OK", "OK", "OK", "OK" },
    { "D-PAD", "D-PAD", "D-PAD", "D-PAD" },
    { "D-PAD: selectionner", "D-PAD: select", "CROCE: seleziona", "CRUCETA: elegir" },
    { "A: confirmer", "A: confirm", "A: conferma", "A: confirmar" },
    { "New 3DS  -  804 MHz", "New 3DS  -  804 MHz", "New 3DS  -  804 MHz", "New 3DS  -  804 MHz" },
    { "Old 3DS  -  268 MHz", "Old 3DS  -  268 MHz", "Old 3DS  -  268 MHz", "Old 3DS  -  268 MHz" },
    { "30 Niveaux classiques", "30 Classic Levels", "30 Livelli classici", "30 Niveles clasicos" },
    { "50 Niveaux + 4 Tutoriels", "50 Levels + 4 Tutorials", "50 Livelli + 4 Tutorial", "50 Niveles + 4 Tutoriales" },
    { "5 Mondes (48 Salles)", "5 Worlds (48 Rooms)", "5 Mondi (48 Stanze)", "5 Mundos (48 Salas)" },
    { "5 Mondes (60 Salles)", "5 Worlds (60 Rooms)", "5 Mondi (60 Stanze)", "5 Mundos (60 Salas)" },
    { "Premier pas", "First Steps", "Primi passi", "Primeros pasos" },
    { "Termine un niveau.", "Complete a level.", "Completa un livello.", "Completa un nivel." },
    { "En route", "Getting There", "Si parte", "En marcha" },
    { "Termine 5 niveaux.", "Complete 5 levels.", "Completa 5 livelli.", "Completa 5 niveles." },
    { "A mi-chemin", "Halfway There", "A meta strada", "A medio camino" },
    { "Termine 15 niveaux.", "Complete 15 levels.", "Completa 15 livelli.", "Completa 15 niveles." },
    { "Jeu termine", "Game Complete", "Gioco completato", "Juego completado" },
    { "Termine les 30 niveaux.", "Complete all 30 levels.", "Completa tutti i 30 livelli.", "Completa los 30 niveles." },
    { "Sans faute", "Flawless", "Senza errori", "Sin fallos" },
    { "Termine un niveau sans mourir.", "Complete a level without dying.", "Completa un livello senza morire.", "Completa un nivel sin morir." },
    { "As du carre", "Square Ace", "Asso del quadrato", "As del cuadrado" },
    { "Termine 10 niveaux sans mourir.", "Complete 10 levels without dying.", "Completa 10 livelli senza morire.", "Completa 10 niveles sin morir." },
    { "Ramasseur", "Collector", "Collezionista", "Recolector" },
    { "Ramasse 25 pieces.", "Collect 25 coins.", "Raccogli 25 monete.", "Recoge 25 monedas." },
    { "Collectionneur", "Hoarder", "Accumula tesori", "Acumulador" },
    { "Ramasse 100 pieces.", "Collect 100 coins.", "Raccogli 100 monete.", "Recoge 100 monedas." },
    { "Tresor", "Treasure Hunter", "Cacciatore di tesori", "Cazatesoros" },
    { "Ramasse 500 pieces.", "Collect 500 coins.", "Raccogli 500 monete.", "Recoge 500 monedas." },
    { "Premiere cle", "First Key", "Prima chiave", "Primera llave" },
    { "Ramasse une cle.", "Collect a key.", "Raccogli una chiave.", "Recoge una llave." },
    { "Porte-cle", "Key Collector", "Portachiavi", "Llavero" },
    { "Ramasse 10 cles.", "Collect 10 keys.", "Raccogli 10 chiavi.", "Recoge 10 llaves." },
    { "Point de repere", "Checkpoint", "Punto di controllo", "Punto de control" },
    { "Atteins 10 checkpoints.", "Reach 10 checkpoints.", "Raggiungi 10 checkpoint.", "Llega a 10 puntos de control." },
    { "Termine 10 niveaux.", "Complete 10 levels.", "Completa 10 livelli.", "Completa 10 niveles." },
    { "Marathon", "Marathon", "Maratona", "Maraton" },
    { "Termine 25 niveaux.", "Complete 25 levels.", "Completa 25 livelli.", "Completa 25 niveles." },
    { "Termine les 50 niveaux.", "Complete all 50 levels.", "Completa tutti i 50 livelli.", "Completa los 50 niveles." },
    { "Ramasse 50 pieces.", "Collect 50 coins.", "Raccogli 50 monete.", "Recoge 50 monedas." },
    { "Ramasse 200 pieces.", "Collect 200 coins.", "Raccogli 200 monete.", "Recoge 200 monedas." },
    { "Ramasse 1000 pieces.", "Collect 1000 coins.", "Raccogli 1000 monete.", "Recoge 1000 monedas." },
    { "Ramasse 15 cles.", "Collect 15 keys.", "Raccogli 15 chiavi.", "Recoge 15 llaves." },
    { "Atteins 25 checkpoints.", "Reach 25 checkpoints.", "Raggiungi 25 checkpoint.", "Llega a 25 puntos de control." },
    { "Premier monde", "First World", "Primo mondo", "Primer mundo" },
    { "Termine un monde.", "Complete a world.", "Completa un mondo.", "Completa un mundo." },
    { "Explorateur", "Explorer", "Esploratore", "Explorador" },
    { "Termine 3 mondes.", "Complete 3 worlds.", "Completa 3 mondi.", "Completa 3 mundos." },
    { "Les 5 mondes", "All Five Worlds", "Tutti e 5 i mondi", "Los 5 mundos" },
    { "Termine les 5 mondes.", "Complete all 5 worlds.", "Completa tutti i 5 mondi.", "Completa los 5 mundos." },
    { "Termine un monde sans mourir.", "Complete a world without dying.", "Completa un mondo senza morire.", "Completa un mundo sin morir." },
    { "Termine 3 mondes sans mourir.", "Complete 3 worlds without dying.", "Completa 3 mondi senza morire.", "Completa 3 mundos sin morir." },
    { "Ramasse 250 pieces.", "Collect 250 coins.", "Raccogli 250 monete.", "Recoge 250 monedas." },
    { "Atteins 50 checkpoints.", "Reach 50 checkpoints.", "Raggiungi 50 checkpoint.", "Llega a 50 puntos de control." },
    { "Boussole", "Compass", "Bussola", "Brujula" },
    { "Randonneur", "Hiker", "Escursionista", "Senderista" },
    { "Petit chrono", "Quick Timer", "Cronometro lampo", "Crono veloz" },
    { "Termine 3 niveaux en moins d'une minute.", "Complete 3 levels in under one minute each.", "Completa 3 livelli in meno di un minuto ciascuno.", "Completa 3 niveles en menos de un minuto cada uno." },
    { "Fonceur", "Speedster", "Velocista", "Velocista" },
    { "Termine 10 niveaux en moins d'une minute.", "Complete 10 levels in under one minute each.", "Completa 10 livelli in meno di un minuto ciascuno.", "Completa 10 niveles en menos de un minuto cada uno." },
    { "Termine 1 monde en moins de 10 minutes.", "Complete 1 world in under 10 minutes.", "Completa 1 mondo in meno di 10 minuti.", "Completa 1 mundo en menos de 10 minutos." },
    { "Termine 3 mondes en moins de 10 minutes.", "Complete 3 worlds in under 10 minutes each.", "Completa 3 mondi in meno di 10 minuti ciascuno.", "Completa 3 mundos en menos de 10 minutos cada uno." },
    { "Premier diamant", "First Diamond", "Primo diamante", "Primer diamante" },
    { "Ramasse un diamant.", "Collect a diamond.", "Raccogli un diamante.", "Recoge un diamante." },
    { "Diamantaire", "Diamond Collector", "Collezionista di diamanti", "Coleccionista de diamantes" },
    { "Ramasse 10 diamants.", "Collect 10 diamonds.", "Raccogli 10 diamanti.", "Recoge 10 diamantes." },
    { "Mode sans mort", "No-death mode", "Modalita senza morte", "Modo sin muertes" },
    { "Invincibilite activee", "Invincibility on", "Invincibilita attiva", "Invulnerabilidad activa" },
    { "Invincibilite desactivee", "Invincibility off", "Invincibilita disattiva", "Invulnerabilidad desactivada" },
    { "Tout reinitialiser", "Reset all progress", "Azzera tutti i progressi", "Reiniciar todo el progreso" },
    { "Reinitialiser", "Reset", "Azzera", "Reiniciar" },
    { "Annuler", "Cancel", "Annulla", "Cancelar" },
    { "Reinitialiser la progression des 4 jeux ?", "Reset progress for all 4 games?", "Azzerare i progressi di tutti e 4 i giochi?", "Reiniciar el progreso de los 4 juegos?" },
    { "Niveaux, records et succes seront effaces.", "Levels, records, and achievements will be erased.", "Livelli, record e obiettivi verranno cancellati.", "Se borraran niveles, records y logros." },
    { "Les options et la langue seront conservees.", "Options and language will be kept.", "Opzioni e lingua verranno mantenute.", "Se conservaran las opciones y el idioma." },
    { "Echec de sauvegarde. Reessayez.", "Save failed. Please try again.", "Salvataggio non riuscito. Riprova.", "Error al guardar. Intentalo de nuevo." },
    { "Selectionner", "Select", "Seleziona", "Elegir" },
    { "Effets", "Effects", "Effetti", "Efectos" },
    { "Verrouille", "Locked", "Bloccato", "Bloqueado" },
    { "Record : %d morts  -  %s", "Record: %d deaths  -  %s", "Record: %d morti  -  %s", "Record: %d muertes  -  %s" },
    { "4 jeux  -  1 hub", "4 games  -  1 hub", "4 giochi  -  1 hub", "4 juegos  -  1 hub" },
    { "D-Pad + stick : bouger", "D-pad + stick: move", "Croce + stick: muovi", "Cruceta + stick: mover" },
    { "A : choisir   B : retour", "A: choose   B: back", "A: scegli   B: indietro", "A: elegir   B: volver" },
    { "SELECT : options", "SELECT: options", "SELECT: opzioni", "SELECT: opciones" },
    { "A / tactile : passer", "A / touch: skip", "A / tocco: salta", "A / tactil: saltar" }
};

/* Table de hachage construite au premier appel : language_text() est appelee des dizaines de
 * fois par image, et parcourait avant toute la table (~170 comparaisons) a chaque appel. */
#define LT_SLOTS 512
static int16_t s_lt_index[LT_SLOTS];
static bool s_lt_ready = false;

static uint32_t lt_hash(const char* s) {
    uint32_t h = 2166136261u;
    while (*s) { h ^= (uint8_t)*s++; h *= 16777619u; }
    return h;
}

static void lt_build(void) {
    memset(s_lt_index, -1, sizeof(s_lt_index));
    unsigned n = sizeof(s_translations) / sizeof(s_translations[0]);
    for (unsigned i = 0; i < n && i < LT_SLOTS / 2; i++) {
        uint32_t h = lt_hash(s_translations[i].french);
        for (unsigned p = 0; p < LT_SLOTS; p++) {
            unsigned slot = (h + p) & (LT_SLOTS - 1);
            int16_t cur = s_lt_index[slot];
            if (cur < 0) { s_lt_index[slot] = (int16_t)i; break; }
            if (strcmp(s_translations[cur].french, s_translations[i].french) == 0) break;  /* doublon : le premier gagne */
        }
    }
    s_lt_ready = true;
}

const char* language_text(uint8_t language, const char* text) {
    if (!text || language >= LANG_COUNT || language == LANG_FRENCH) return text;
    if (!s_lt_ready) lt_build();
    uint32_t h = lt_hash(text);
    for (unsigned p = 0; p < LT_SLOTS; p++) {
        int16_t idx = s_lt_index[(h + p) & (LT_SLOTS - 1)];
        if (idx < 0) break;
        const Translation* t = &s_translations[idx];
        if (strcmp(text, t->french) == 0) {
            if (language == LANG_ENGLISH) return t->english;
            if (language == LANG_ITALIAN) return t->italian;
            return t->spanish;
        }
    }
    return text;
}

const char* language_name(uint8_t language) {
    static const char* const names[LANG_COUNT] = {
        "English", "Francais", "Italiano", "Espanol"
    };
    return language < LANG_COUNT ? names[language] : names[LANG_FRENCH];
}
