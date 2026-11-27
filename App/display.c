#include "display.h"
#include "digit_font.h"
#include <string.h>

static void line(const Pomodoro *const app, uint8_t page, const char *text)
{
    oled_setString(app->devices.display, page, text, (uint8_t)strlen(text));
}

static char *blankLine(Pomodoro *const app)
{
    memset(app->display.line, ' ', sizeof(app->display.line) - 1U);
    app->display.line[sizeof(app->display.line) - 1U] = '\0';
    return app->display.line;
}

/* 字模在 Flash 中已经排成页和 word；此路径只调用 32 位字块搬运。 */
static void digit(const Pomodoro *const app, const DigitFont *const font,
                  uint8_t x, uint8_t firstPage, unsigned value)
{
    if (value > 9U) return;
    unsigned glyphWords = font->widthWords * font->pageCount;
    oled_setBlockWords(app->devices.display, x, firstPage,
                       font->glyphs + value * glyphWords, font->widthWords, font->pageCount);
}

static void countdown(const Pomodoro *const app)
{
    unsigned seconds = (app->countdown.remainingTicks + POMODORO_TICKS_PER_SECOND - 1U) /
                       POMODORO_TICKS_PER_SECOND;
    unsigned minutes = seconds / 60U;
    unsigned width = countdownDigits.widthWords * 4U;
    unsigned left = ((OLED_WIDTH - (4U * width + 20U)) / 2U) & ~3U;
    digit(app, &countdownDigits, left, 2U, minutes / 10U);
    digit(app, &countdownDigits, left + width + 4U, 2U, minutes % 10U);
    oled_setBlockWords(app->devices.display, left + 2U * width + 8U, 2U,
                       countdownColon, 1U, countdownDigits.pageCount);
    digit(app, &countdownDigits, left + 2U * width + 16U, 2U, seconds % 60U / 10U);
    digit(app, &countdownDigits, left + 3U * width + 20U, 2U, seconds % 10U);
}

static void durationRow(Pomodoro *const app, uint8_t page, const char *name,
                        unsigned minutes, uint8_t selected)
{
    char *row = blankLine(app);
    row[0] = selected ? '>' : ' ';
    memcpy(row + 2, name, strlen(name));
    row[12] = '0' + minutes / 10U;
    row[13] = '0' + minutes % 10U;
    memcpy(row + 15, "min", 3U);
    line(app, page, row);
}

static const char *const settingNames[SETTING_COUNT] = {
    [BRIGHTNESS] = "Brightness", [VOLUME] = "Volume",
    [MUTE] = "Mute", [PRESET_COUNT] = "Preset count",
};

static void drawSettings(Pomodoro *const app, uint8_t editingCount)
{
    const NavigationState *const nav = &app->navigation;
    line(app, 0U, "Settings");
    for (unsigned i = 0; i < SETTING_COUNT; ++i) {
        char *row = blankLine(app);
        row[0] = i == nav->selectedSetting ? '>' : ' ';
        memcpy(row + 2, settingNames[i], strlen(settingNames[i]));
        if (i == PRESET_COUNT && editingCount) {
            row[16] = '[';
            row[17] = '0' + nav->editor.value;
            row[18] = ']';
        } else if (i == MUTE) {
            memcpy(row + 17, app->prefs.sound.muted ? "On" : "Off",
                   app->prefs.sound.muted ? 2U : 3U);
        } else row[17] = '0' + Pomodoro_GetSetting(&app->prefs, (Setting)i);
        line(app, 2U + i, row);
    }
    const char *hint = "Tap open / Hold back";
    if (nav->selectedSetting == PRESET_COUNT) hint = "Tap edit / Hold back";
    if (editingCount) hint = "Tap OK / Hold undo";
    line(app, 7U, hint);
}

void drawPomodoro(Pomodoro *const app)
{
    const NavigationState *const nav = &app->navigation;
    uint8_t notice = app->service.notice.visible;
    oled_clearBuffer(app->devices.display);
    switch (nav->page) {
    case HOME:
    case PREVIEW:
    case EDIT_PRESET:
        if (nav->page == HOME && nav->selectedPreset == app->prefs.presets.count) {
            line(app, 0U, "Settings");
            line(app, 7U, "Tap to open");
        } else {
            char *title = blankLine(app);
            memcpy(title, "Edit Preset 1", 13U);
            title[12] = '1' + nav->selectedPreset;
            line(app, 0U, nav->page == EDIT_PRESET ? title : title + 5);
            const Preset *const preset = &app->prefs.presets.items[nav->selectedPreset];
            durationRow(app, 2U, "Focus", preset->focusMinutes,
                        nav->page == EDIT_PRESET && !nav->editor.editingBreak);
            durationRow(app, 4U, "Break", preset->breakMinutes,
                        nav->page == EDIT_PRESET && nav->editor.editingBreak);
            const char *hint = "Tap open / Hold edit";
            if (nav->page == EDIT_PRESET) hint = "Tap next / Hold done";
            if (nav->page == PREVIEW) hint = notice ? "Break complete" : "Tap start / Hold back";
            line(app, 7U, hint);
        }
        if (nav->page == HOME) {
            static const uint32_t selected = 0x3c3c3c3cU, idle = 0x08080808U;
            for (unsigned i = 0; i <= app->prefs.presets.count; ++i)
                oled_setBlockWords(app->devices.display, 100U + 8U * i, 0U,
                                   i == nav->selectedPreset ? &selected : &idle, 1U, 1U);
        }
        break;
    case TIMER: {
        const char *heading = app->countdown.inBreak ? "Break" : "Focus";
        if (app->countdown.paused)
            heading = app->countdown.inBreak ? "Break / Paused" : "Focus / Paused";
        else if (notice && app->countdown.inBreak) heading = "Break / Focus done";
        line(app, 0U, heading);
        countdown(app);
        line(app, 7U, app->countdown.paused ? "Tap play / Hold exit" : "Tap pause / Hold exit");
        break;
    }
    case SETTINGS:
        drawSettings(app, 0U);
        break;
    case EDIT_SETTING:
        if (nav->selectedSetting == PRESET_COUNT) {
            drawSettings(app, 1U);
            break;
        }
        line(app, 0U, settingNames[nav->selectedSetting]);
        if (nav->selectedSetting == MUTE) line(app, 3U, nav->editor.value ? "         On" : "         Off");
        else {
            unsigned left = ((OLED_WIDTH - settingDigits.widthWords * 4U) / 2U) & ~3U;
            digit(app, &settingDigits, left, 2U, nav->editor.value);
        }
        line(app, 7U, "Tap OK / Hold back");
        break;
    }
    oled_Flush(app->devices.display);
}
