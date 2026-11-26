#include "audio.h"
#include <3ds.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/*
 * Musique en streaming : 4 tampons de 32 Ko (~0,74 s chacun a 22 kHz mono).
 * Pres de 3 secondes sont ainsi prechargees, ce qui absorbe les ralentissements
 * ponctuels de lecture depuis RomFS pendant le demarrage et les transitions.
 */
#define MUSIC_BUF_BYTES 32768
#define MUSIC_BUFS 4
#define MUSIC_CH 0
#define MUSIC_VOLUME 0.28f
#define SFX_VOLUME 0.45f

typedef struct {
    u32 rate;
    u16 channels;
    u16 bits;
    u32 data_offset;
    u32 data_size;
} WavInfo;

typedef struct {
    u8* data;
    u32 size;
    u32 sample_rate;
    u32 frame_bytes;
    u16 format;
    ndspWaveBuf wbuf;
} SfxClip;

static bool s_dsp_ok = false;
static bool s_muted = false;
static SfxClip s_sfx[SFX_COUNT];

static FILE* s_music_file = NULL;
static u32 s_music_data_start = 44;
static u32 s_music_data_size = 0;
static u32 s_music_data_read = 0;
static u32 s_music_rate = 22050;
static u32 s_music_frame_bytes = 2;
static int s_music_next_buf = 0;
static u8* s_music_buf[MUSIC_BUFS];
static ndspWaveBuf s_music_wbuf[MUSIC_BUFS];

static const char* s_sfx_names[SFX_COUNT] = {
    "die.wav",
    "coin.wav",
    "key.wav",
    "win.wav",
    "win_game.wav",
    "pause.wav",
    "diamond.wav",
    "check.wav",
    "die_water.wav",
    "die_pit.wav",
};

static u16 rd16(const u8* p) { return (u16)(p[0] | (p[1] << 8)); }
static u32 rd32(const u8* p) {
    return (u32)p[0] | ((u32)p[1] << 8) | ((u32)p[2] << 16) | ((u32)p[3] << 24);
}

/* Lecture robuste d'un WAV PCM : chunks dans n'importe quel ordre, mono/stereo, 8/16 bits. */
static bool wav_parse(FILE* f, WavInfo* w) {
    u8 hdr[12];
    memset(w, 0, sizeof(*w));
    if (fseek(f, 0, SEEK_SET) != 0) return false;
    if (fread(hdr, 1, 12, f) != 12) return false;
    if (memcmp(hdr, "RIFF", 4) != 0 || memcmp(hdr + 8, "WAVE", 4) != 0) return false;

    bool have_fmt = false;
    for (;;) {
        u8 ch[8];
        if (fread(ch, 1, 8, f) != 8) return false;
        u32 csize = rd32(ch + 4);
        if (memcmp(ch, "fmt ", 4) == 0) {
            u8 fmt[16];
            if (csize < 16 || fread(fmt, 1, 16, f) != 16) return false;
            u16 tag = rd16(fmt);
            if (tag != 1 && tag != 0xFFFE) return false; /* PCM uniquement */
            w->channels = rd16(fmt + 2);
            w->rate = rd32(fmt + 4);
            w->bits = rd16(fmt + 14);
            have_fmt = true;
            long skip = (long)csize - 16 + (long)(csize & 1);
            if (skip > 0) fseek(f, skip, SEEK_CUR);
        } else if (memcmp(ch, "data", 4) == 0) {
            if (!have_fmt) return false;
            long pos = ftell(f);
            if (pos < 0) return false;
            fseek(f, 0, SEEK_END);
            long end = ftell(f);
            fseek(f, pos, SEEK_SET);
            w->data_offset = (u32)pos;
            w->data_size = csize;
            if (end >= pos && (u64)w->data_offset + (u64)w->data_size > (u64)end)
                w->data_size = (u32)(end - pos); /* en-tete trop optimiste */
            return true;
        } else {
            fseek(f, (long)csize + (long)(csize & 1), SEEK_CUR);
        }
    }
}

static bool wav_format(const WavInfo* w, u16* out_fmt, u32* out_frame_bytes) {
    if (w->rate < 4000 || w->rate > 48000) return false;
    if (w->channels == 1 && w->bits == 16) { *out_fmt = NDSP_FORMAT_MONO_PCM16;   *out_frame_bytes = 2; return true; }
    if (w->channels == 2 && w->bits == 16) { *out_fmt = NDSP_FORMAT_STEREO_PCM16; *out_frame_bytes = 4; return true; }
    if (w->channels == 1 && w->bits == 8)  { *out_fmt = NDSP_FORMAT_MONO_PCM8;    *out_frame_bytes = 1; return true; }
    if (w->channels == 2 && w->bits == 8)  { *out_fmt = NDSP_FORMAT_STEREO_PCM8;  *out_frame_bytes = 2; return true; }
    return false;
}

static void set_channel_mix(int ch, float vol) {
    float mix[12];
    memset(mix, 0, sizeof(mix));
    mix[0] = vol;
    mix[1] = vol;
    ndspChnSetMix(ch, mix);
}

static bool fill_music_buf(int idx) {
    if (!s_music_file || !s_music_buf[idx] || s_music_data_size == 0) return false;

    size_t filled = 0;
    while (filled < MUSIC_BUF_BYTES) {
        if (s_music_data_read >= s_music_data_size) {
            if (fseek(s_music_file, (long)s_music_data_start, SEEK_SET) != 0) return false;
            s_music_data_read = 0;
        }

        u32 available = s_music_data_size - s_music_data_read;
        size_t request = MUSIC_BUF_BYTES - filled;
        if (request > available) request = available;
        size_t got = fread(s_music_buf[idx] + filled, 1, request, s_music_file);
        if (got == 0) {
            /* fichier tronque : on reboucle plutot que de couper la musique */
            if (s_music_data_read == 0) return false;
            s_music_data_read = s_music_data_size;
            continue;
        }
        filled += got;
        s_music_data_read += (u32)got;
    }

    memset(&s_music_wbuf[idx], 0, sizeof(ndspWaveBuf));
    s_music_wbuf[idx].data_vaddr = s_music_buf[idx];
    s_music_wbuf[idx].nsamples = (u32)(filled / s_music_frame_bytes);
    DSP_FlushDataCache(s_music_buf[idx], (u32)filled);
    ndspChnWaveBufAdd(MUSIC_CH, &s_music_wbuf[idx]);
    return true;
}

bool audio_init(void) {
    memset(s_sfx, 0, sizeof(s_sfx));
    memset(s_music_wbuf, 0, sizeof(s_music_wbuf));
    memset(s_music_buf, 0, sizeof(s_music_buf));
    if (R_FAILED(ndspInit())) {
        s_dsp_ok = false;
        return false;
    }
    s_dsp_ok = true;
    ndspSetOutputMode(NDSP_OUTPUT_STEREO);
    return true;
}

void audio_unload_bank(void) {
    if (!s_dsp_ok) return;
    for (int ch = 0; ch < 16; ch++) ndspChnWaveBufClear(ch);
    if (s_music_file) { fclose(s_music_file); s_music_file = NULL; }
    s_music_data_size = 0;
    s_music_data_read = 0;
    s_music_next_buf = 0;
    for (int i = 0; i < MUSIC_BUFS; i++) {
        if (s_music_buf[i]) { linearFree(s_music_buf[i]); s_music_buf[i] = NULL; }
    }
    for (int i = 0; i < SFX_COUNT; i++) {
        if (s_sfx[i].data) { linearFree(s_sfx[i].data); s_sfx[i].data = NULL; }
    }
    memset(s_sfx, 0, sizeof(s_sfx));
    memset(s_music_wbuf, 0, sizeof(s_music_wbuf));
}

bool audio_load_bank(const char* romfs_prefix) {
    if (!s_dsp_ok || !romfs_prefix) return false;
    audio_unload_bank();

    char path[128];
    for (int i = 0; i < SFX_COUNT; i++) {
        snprintf(path, sizeof(path), "%s/sfx/%s", romfs_prefix, s_sfx_names[i]);
        FILE* f = fopen(path, "rb");
        if (!f) continue;

        WavInfo w;
        u16 fmt = 0;
        u32 fb = 0;
        if (wav_parse(f, &w) && wav_format(&w, &fmt, &fb)) {
            u32 sz = w.data_size - (w.data_size % fb);
            if (sz > 0) {
                u8* buf = (u8*)linearAlloc(sz);
                if (buf) {
                    fseek(f, (long)w.data_offset, SEEK_SET);
                    size_t got = fread(buf, 1, sz, f);
                    got -= got % fb;
                    if (got > 0) {
                        DSP_FlushDataCache(buf, (u32)got);
                        s_sfx[i].data = buf;
                        s_sfx[i].size = (u32)got;
                        s_sfx[i].sample_rate = w.rate;
                        s_sfx[i].frame_bytes = fb;
                        s_sfx[i].format = fmt;

                        int ch = 1 + i;
                        ndspChnReset(ch);
                        ndspChnSetInterp(ch, NDSP_INTERP_LINEAR);
                        ndspChnSetRate(ch, (float)w.rate);
                        ndspChnSetFormat(ch, fmt);
                        set_channel_mix(ch, SFX_VOLUME);
                    } else {
                        linearFree(buf);
                    }
                }
            }
        }
        fclose(f);
    }

    snprintf(path, sizeof(path), "%s/sfx/music.wav", romfs_prefix);
    s_music_file = fopen(path, "rb");
    if (s_music_file) {
        WavInfo w;
        u16 fmt = 0;
        u32 fb = 0;
        if (wav_parse(s_music_file, &w) && wav_format(&w, &fmt, &fb) && w.data_size >= fb) {
            s_music_rate = w.rate;
            s_music_frame_bytes = fb;
            s_music_data_start = w.data_offset;
            s_music_data_size = w.data_size - (w.data_size % fb);
            s_music_data_read = 0;
            s_music_next_buf = 0;
            fseek(s_music_file, (long)s_music_data_start, SEEK_SET);
            for (int i = 0; i < MUSIC_BUFS; i++) {
                s_music_buf[i] = (u8*)linearAlloc(MUSIC_BUF_BYTES);
            }
            ndspChnReset(MUSIC_CH);
            ndspChnSetInterp(MUSIC_CH, NDSP_INTERP_LINEAR);
            ndspChnSetRate(MUSIC_CH, (float)s_music_rate);
            ndspChnSetFormat(MUSIC_CH, fmt);
            set_channel_mix(MUSIC_CH, s_muted ? 0.0f : MUSIC_VOLUME);
            for (int i = 0; i < MUSIC_BUFS; i++) fill_music_buf(i);
        } else {
            fclose(s_music_file);
            s_music_file = NULL;
        }
    }
    return true;
}

void audio_exit(void) {
    if (!s_dsp_ok) return;
    audio_unload_bank();
    ndspExit();
    s_dsp_ok = false;
}

bool audio_is_dsp_available(void) { return s_dsp_ok; }

bool audio_has_sfx(SfxId id) {
    return s_dsp_ok && id >= 0 && id < SFX_COUNT && s_sfx[id].data != NULL;
}

void audio_set_muted(bool muted) {
    s_muted = muted;
    if (!s_dsp_ok) return;
    set_channel_mix(MUSIC_CH, muted ? 0.0f : MUSIC_VOLUME);
}

void audio_play_sfx(SfxId id) {
    if (!s_dsp_ok || s_muted || id < 0 || id >= SFX_COUNT) return;
    SfxClip* c = &s_sfx[id];
    if (!c->data || c->size == 0) return;
    int ch = 1 + id;
    if (c->wbuf.status == NDSP_WBUF_QUEUED || c->wbuf.status == NDSP_WBUF_PLAYING) {
        /* Les pieces s'enchainent vite : on laisse finir le son en cours.
         * Les autres effets (mort, victoire...) sont relances immediatement. */
        if (id == SFX_COIN || id == SFX_DIAMOND) return;
        ndspChnWaveBufClear(ch);
    }
    memset(&c->wbuf, 0, sizeof(ndspWaveBuf));
    c->wbuf.data_vaddr = c->data;
    c->wbuf.nsamples = c->size / c->frame_bytes;
    ndspChnWaveBufAdd(ch, &c->wbuf);
}

void audio_update(void) {
    if (!s_dsp_ok || !s_music_file) return;
    for (int n = 0; n < MUSIC_BUFS; n++) {
        int idx = s_music_next_buf;
        u8 st = s_music_wbuf[idx].status;
        /* FREE = tampon jamais mis en file (echec de lecture precedent) : on retente */
        if (st != NDSP_WBUF_DONE && st != NDSP_WBUF_FREE) break;
        if (!fill_music_buf(idx)) break;
        s_music_next_buf = (s_music_next_buf + 1) % MUSIC_BUFS;
    }
}
