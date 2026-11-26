#include "audio.h"
#include <3ds.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define MUSIC_BUF_BYTES 16384
#define MUSIC_BUFS 3
#define MUSIC_CH 0

typedef struct {
    u8* data;
    u32 size;
    u32 sample_rate;
    ndspWaveBuf wbuf;
} SfxClip;

static bool s_dsp_ok = false;
static bool s_muted = false;
static SfxClip s_sfx[SFX_COUNT];

static FILE* s_music_file = NULL;
static u32 s_music_data_start = 44;
static u32 s_music_rate = 22050;
static u8* s_music_buf[MUSIC_BUFS] = { NULL, NULL, NULL };
static ndspWaveBuf s_music_wbuf[MUSIC_BUFS];

static const char* s_sfx_paths[SFX_COUNT] = {
    "romfs:/sfx/die.wav",
    "romfs:/sfx/coin.wav",
    "romfs:/sfx/key.wav",
    "romfs:/sfx/win.wav",
    "romfs:/sfx/win_game.wav",
    "romfs:/sfx/pause.wav",
    "romfs:/sfx/diamond.wav",
    "romfs:/sfx/check.wav",
    "romfs:/sfx/die_water.wav",
    "romfs:/sfx/die_pit.wav",
};

static bool parse_wav_header(FILE* f, u32* out_rate, u32* out_data_offset, u32* out_data_size) {
    u8 hdr[44];
    if (fread(hdr, 1, 44, f) != 44) return false;
    if (memcmp(hdr, "RIFF", 4) != 0 || memcmp(hdr + 8, "WAVE", 4) != 0) return false;
    *out_rate = (u32)(hdr[24] | (hdr[25] << 8) | (hdr[26] << 16) | (hdr[27] << 24));
    fseek(f, 12, SEEK_SET);
    while (!feof(f)) {
        u8 ch[8];
        if (fread(ch, 1, 8, f) != 8) return false;
        u32 csize = (u32)(ch[4] | (ch[5] << 8) | (ch[6] << 16) | (ch[7] << 24));
        if (memcmp(ch, "data", 4) == 0) {
            *out_data_offset = (u32)ftell(f);
            *out_data_size = csize;
            return true;
        }
        fseek(f, csize, SEEK_CUR);
    }
    return false;
}

static void fill_music_buf(int idx) {
    if (!s_music_file || !s_music_buf[idx]) return;
    size_t got = fread(s_music_buf[idx], 1, MUSIC_BUF_BYTES, s_music_file);
    if (got < MUSIC_BUF_BYTES) {
        fseek(s_music_file, (long)s_music_data_start, SEEK_SET);
        size_t rest = fread(s_music_buf[idx] + got, 1, MUSIC_BUF_BYTES - got, s_music_file);
        got += rest;
    }
    if (got == 0) return;
    memset(&s_music_wbuf[idx], 0, sizeof(ndspWaveBuf));
    s_music_wbuf[idx].data_vaddr = s_music_buf[idx];
    s_music_wbuf[idx].nsamples = (u32)(got / 2);
    DSP_FlushDataCache(s_music_buf[idx], (u32)got);
    ndspChnWaveBufAdd(MUSIC_CH, &s_music_wbuf[idx]);
}

bool audio_init(void) {
    memset(s_sfx, 0, sizeof(s_sfx));
    memset(s_music_wbuf, 0, sizeof(s_music_wbuf));
    if (R_FAILED(ndspInit())) {
        s_dsp_ok = false;
        return false;
    }
    s_dsp_ok = true;
    ndspSetOutputMode(NDSP_OUTPUT_STEREO);

    for (int i = 0; i < SFX_COUNT; i++) {
        FILE* f = fopen(s_sfx_paths[i], "rb");
        if (!f) continue;
        u32 rate = 22050, off = 44, sz = 0;
        if (parse_wav_header(f, &rate, &off, &sz) && sz > 0) {
            u8* buf = (u8*)linearAlloc(sz);
            if (buf) {
                fseek(f, (long)off, SEEK_SET);
                fread(buf, 1, sz, f);
                DSP_FlushDataCache(buf, sz);
                s_sfx[i].data = buf;
                s_sfx[i].size = sz;
                s_sfx[i].sample_rate = rate;

                int ch = 1 + i;
                ndspChnReset(ch);
                ndspChnSetInterp(ch, NDSP_INTERP_LINEAR);
                ndspChnSetRate(ch, (float)rate);
                ndspChnSetFormat(ch, NDSP_FORMAT_MONO_PCM16);
                float mix[12] = { 0.85f, 0.85f, 0 };
                ndspChnSetMix(ch, mix);
            }
        }
        fclose(f);
    }

    s_music_file = fopen("romfs:/sfx/music.wav", "rb");
    if (s_music_file) {
        u32 sz = 0;
        if (parse_wav_header(s_music_file, &s_music_rate, &s_music_data_start, &sz)) {
            fseek(s_music_file, (long)s_music_data_start, SEEK_SET);
            for (int i = 0; i < MUSIC_BUFS; i++) {
                s_music_buf[i] = (u8*)linearAlloc(MUSIC_BUF_BYTES);
            }
            ndspChnReset(MUSIC_CH);
            ndspChnSetInterp(MUSIC_CH, NDSP_INTERP_LINEAR);
            ndspChnSetRate(MUSIC_CH, (float)s_music_rate);
            ndspChnSetFormat(MUSIC_CH, NDSP_FORMAT_MONO_PCM16);
            float mix[12] = { 0.40f, 0.40f, 0 };
            ndspChnSetMix(MUSIC_CH, mix);
            for (int i = 0; i < MUSIC_BUFS; i++) {
                fill_music_buf(i);
            }
        } else {
            fclose(s_music_file);
            s_music_file = NULL;
        }
    }
    return true;
}

void audio_exit(void) {
    if (!s_dsp_ok) return;
    for (int ch = 0; ch < 16; ch++) ndspChnWaveBufClear(ch);
    if (s_music_file) { fclose(s_music_file); s_music_file = NULL; }
    for (int i = 0; i < MUSIC_BUFS; i++) {
        if (s_music_buf[i]) { linearFree(s_music_buf[i]); s_music_buf[i] = NULL; }
    }
    for (int i = 0; i < SFX_COUNT; i++) {
        if (s_sfx[i].data) { linearFree(s_sfx[i].data); s_sfx[i].data = NULL; }
    }
    ndspExit();
    s_dsp_ok = false;
}

bool audio_is_dsp_available(void) { return s_dsp_ok; }

void audio_set_muted(bool muted) {
    s_muted = muted;
    if (!s_dsp_ok) return;
    float mix[12] = { muted ? 0.0f : 0.40f, muted ? 0.0f : 0.40f, 0 };
    ndspChnSetMix(MUSIC_CH, mix);
}

void audio_play_sfx(SfxId id) {
    if (!s_dsp_ok || s_muted || id < 0 || id >= SFX_COUNT) return;
    SfxClip* c = &s_sfx[id];
    if (!c->data || c->size == 0) return;
    int ch = 1 + id;
    ndspChnWaveBufClear(ch);
    memset(&c->wbuf, 0, sizeof(ndspWaveBuf));
    c->wbuf.data_vaddr = c->data;
    c->wbuf.nsamples = c->size / 2;
    ndspChnWaveBufAdd(ch, &c->wbuf);
}

void audio_update(void) {
    if (!s_dsp_ok || !s_music_file) return;
    for (int i = 0; i < MUSIC_BUFS; i++) {
        if (s_music_wbuf[i].status == NDSP_WBUF_DONE) {
            fill_music_buf(i);
        }
    }
}
