#ifndef AUDIO_H
#define AUDIO_H

#include <stdbool.h>

typedef enum {
    SFX_DIE = 0,
    SFX_COIN,
    SFX_KEY,
    SFX_WIN,
    SFX_WIN_GAME,
    SFX_PAUSE,
    SFX_COUNT
} SfxId;

bool audio_init(void);
void audio_exit(void);
bool audio_is_dsp_available(void);
void audio_set_muted(bool muted);
void audio_play_sfx(SfxId id);
void audio_update(void);

#endif // AUDIO_H
