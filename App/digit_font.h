#ifndef POMODORO_DIGIT_FONT_H
#define POMODORO_DIGIT_FONT_H

#include <stdint.h>

/* 字模按 SSD1306 页格式存储；每页宽度为完整的 32 位字。 */
typedef struct {
    uint8_t widthWords;
    uint8_t pageCount;
    const uint32_t *const glyphs;
} DigitFont;

extern const DigitFont countdownDigits;
extern const DigitFont settingDigits;
extern const uint32_t countdownColon[];

#endif
