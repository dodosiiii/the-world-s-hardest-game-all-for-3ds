#ifndef LANGUAGE_H
#define LANGUAGE_H

#include <stdint.h>

typedef enum {
    LANG_ENGLISH = 0,
    LANG_FRENCH,
    LANG_ITALIAN,
    LANG_SPANISH,
    LANG_COUNT
} Language;

const char* language_text(uint8_t language, const char* text);
const char* language_name(uint8_t language);

#endif
