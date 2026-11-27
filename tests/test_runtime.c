#include "App/pomodoro.h"
#include "App/display.h"
#include "App/digit_font.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

/* 只链接通用元件层；这里没有定义 g_oled/g_ec11/g_buzzer 或硬件寄存器。 */
static oledBuffer frames[2];
static volatile uint32_t tick;
static uint8_t buttonDown, outputVolume, transmittedContrast;
static uint32_t irqEnabled = 1U, reads, transmissions;

static uint8_t readButton(void) { ++reads; return buttonDown; }
static uint32_t lock(void)
{
    uint32_t previous = irqEnabled;
    irqEnabled = 0U;
    return previous;
}
static void unlock(uint32_t previous) { irqEnabled = previous; }
static void setVolume(uint8_t level) { outputVolume = level; }
static void stop(void) { outputVolume = 0U; }

static OLED_STATUS transmit(OLED *self, uint8_t *data, uint16_t length)
{
    ++transmissions;
    if (length == 9U && data[0] == 0U && data[1] == 0x81U)
        transmittedContrast = data[2];
    self->transmitCallBack(self, OLED_OK);
    return OLED_OK;
}

static const EC11Hardware encoderHardware = {readButton, lock, unlock};
static const BuzzerHardware buzzerHardware = {setVolume, stop};
static EC11 encoder = {.hardware = &encoderHardware};
static Buzzer buzzer = {.hardware = &buzzerHardware};
static OLED display = {
    .address = 0x3cU,
    .contrast = 0xcfU,
    .frontBuffer = &frames[0], .backBuffer = &frames[1],
    .transmit = transmit,
};
static Pomodoro app = {
    .devices = {.display = &display, .encoder = &encoder, .buzzer = &buzzer, .tick5ms = &tick},
};

static void testWordBlocks(void)
{
    static const uint32_t block[] = {0x04030201U, 0x08070605U, 0x0c0b0a09U, 0x100f0e0dU};
    memset(display.backBuffer->oledData, 0xa5, OLED_BUFFER_SIZE);
    assert(oled_setBlockWords(&display, 120U, 6U, block, 2U, 2U) == OLED_OK);
    for (unsigned i = 0U; i < OLED_BUFFER_SIZE; ++i) {
        unsigned page = i / OLED_WIDTH, x = i % OLED_WIDTH;
        uint8_t expected = 0xa5U;
        if (page >= 6U && x >= 120U) expected = (uint8_t)((page - 6U) * 8U + x - 119U);
        assert(display.backBuffer->oledData[i] == expected);
    }
    assert(oled_setBlockWords(&display, 1U, 0U, block, 1U, 1U) == OLED_INVALID_ARGUMENT);
    assert(oled_setBlockWords(&display, 124U, 0U, block, 2U, 1U) == OLED_INVALID_ARGUMENT);
    assert(oled_setBlockWords(&display, 0U, 7U, block, 1U, 2U) == OLED_INVALID_ARGUMENT);
    assert(oled_setBlockWords(&display, 0U, 0U, block, 0U, 1U) == OLED_INVALID_ARGUMENT);
    assert(oled_setBlockWords(&display, 0U, 0U, NULL, 1U, 1U) == OLED_INVALID_ARGUMENT);
    assert(oled_setBlockWords(NULL, 0U, 0U, block, 1U, 1U) == OLED_INVALID_ARGUMENT);
}

static void testEncoder(void)
{
    EC11Input input;
    encoder.forwardCount = 255U;
    encoder.reverseCount = 1U;
    encoder.pressCount = 3U;
    buttonDown = 2U; /* 元件接口归一化输入电平。 */
    EC11_Read(&encoder, &input);
    assert(input.rotation == 254 && input.buttonDown == 1U);
    assert(encoder.forwardCount == 0U && encoder.reverseCount == 0U && encoder.pressCount == 0U);
    assert(irqEnabled == 1U);
    irqEnabled = 0U;
    encoder.reverseCount = 255U;
    EC11_Read(&encoder, &input);
    assert(input.rotation == -255 && irqEnabled == 0U);
    irqEnabled = 1U;
    buttonDown = 0U;
}

static void testService(void)
{
    tick = 100U;
    Pomodoro_Init(&app);
    assert(app.service.initialized && !app.button.stable && outputVolume == 0U);
    uint32_t previousReads = reads;
    for (unsigned i = 0U; i < 12U; ++i) Pomodoro_Service(&app);
    assert(reads == previousReads && transmissions >= 3U);
    assert(transmittedContrast == 96U);

    encoder.forwardCount = 1U;
    ++tick;
    Pomodoro_Service(&app);
    assert(app.navigation.selectedPreset == 1U && encoder.forwardCount == 0U);
    press(&app, CLICK);
    press(&app, CLICK);
    app.countdown.remainingTicks = 1U;
    ++tick;
    Pomodoro_Service(&app);
    assert(app.countdown.inBreak && app.service.notice.visible);
    assert(outputVolume == 2U && buzzer.ringing);
    tick = 221U;
    Pomodoro_Service(&app);
    assert(outputVolume == 2U);
    tick = 222U;
    Pomodoro_Service(&app);
    assert(outputVolume == 0U && !buzzer.ringing);
    tick = 502U;
    Pomodoro_Service(&app);
    assert(!app.service.notice.visible);

    app.prefs.sound.muted = 1U;
    app.countdown.inBreak = 0U;
    app.countdown.remainingTicks = 1U;
    ++tick;
    Pomodoro_Service(&app);
    assert(app.service.notice.visible && !buzzer.ringing && outputVolume == 0U);
    app.navigation.page = EDIT_SETTING;
    app.navigation.selectedSetting = BRIGHTNESS;
    app.navigation.editor.value = 5U;
    encoder.forwardCount = 1U;
    ++tick;
    Pomodoro_Service(&app);
    assert(display.contrast == 255U && app.prefs.display.brightness == 3U);

    tick = UINT32_MAX - 10U;
    buttonDown = 1U;
    Pomodoro_Init(&app);
    assert(app.button.consumed && app.button.stable);
    app.navigation.page = TIMER;
    app.countdown.remainingTicks = 100U;
    tick = 5U;
    Pomodoro_Service(&app);
    assert(app.countdown.remainingTicks == 84U);
    Buzzer_Play(&buzzer, 9U, UINT32_MAX - 10U, 20U);
    assert(outputVolume == 5U);
    Buzzer_Service(&buzzer, 8U);
    assert(buzzer.ringing);
    Buzzer_Service(&buzzer, 9U);
    assert(!buzzer.ringing && outputVolume == 0U);
    buttonDown = 0U;
}

static void assertGlyph(unsigned x, unsigned page, unsigned digit)
{
    unsigned width = countdownDigits.widthWords * 4U;
    const uint8_t *glyph = (const uint8_t *)countdownDigits.glyphs +
                           digit * width * countdownDigits.pageCount;
    for (unsigned row = 0U; row < countdownDigits.pageCount; ++row) {
        assert(memcmp(&display.backBuffer->oledData[(page + row) * OLED_WIDTH + x],
                      glyph + row * width, width) == 0);
    }
}

static void dumpFrame(const char *directory, const char *name)
{
    if (directory == NULL) return;
    char path[512];
    assert(snprintf(path, sizeof(path), "%s/%s.bin", directory, name) > 0);
    FILE *file = fopen(path, "wb");
    assert(file != NULL);
    assert(fwrite(display.backBuffer->oledData, 1U, OLED_BUFFER_SIZE, file) == OLED_BUFFER_SIZE);
    assert(fclose(file) == 0);
}

static void testDisplay(const char *directory)
{
    Pomodoro_Init(&app);
    drawPomodoro(&app);
    dumpFrame(directory, "home");
    app.navigation.page = TIMER;
    app.countdown.remainingTicks = 25U * POMODORO_TICKS_PER_MINUTE;
    drawPomodoro(&app);
    assertGlyph(12U, 2U, 2U);
    assertGlyph(36U, 2U, 5U);
    assertGlyph(68U, 2U, 0U);
    assertGlyph(92U, 2U, 0U);
    dumpFrame(directory, "timer-25-00");
    app.countdown.remainingTicks = 599U * POMODORO_TICKS_PER_SECOND;
    app.countdown.paused = 1U;
    drawPomodoro(&app);
    assertGlyph(12U, 2U, 0U);
    assertGlyph(36U, 2U, 9U);
    assertGlyph(68U, 2U, 5U);
    assertGlyph(92U, 2U, 9U);
    dumpFrame(directory, "timer-09-59-paused");
    app.navigation.page = SETTINGS;
    drawPomodoro(&app);
    dumpFrame(directory, "settings");
    app.navigation.page = EDIT_SETTING;
    app.navigation.selectedSetting = VOLUME;
    app.navigation.editor.value = 5U;
    drawPomodoro(&app);
    dumpFrame(directory, "volume");
    app.navigation.selectedSetting = PRESET_COUNT;
    app.navigation.editor.value = 3U;
    drawPomodoro(&app);
    dumpFrame(directory, "preset-count");
    app.prefs.presets.count = 3U;
    app.navigation.page = HOME;
    app.navigation.selectedPreset = 3U;
    drawPomodoro(&app);
    dumpFrame(directory, "settings-card");
}

int main(int argc, char **argv)
{
    testWordBlocks();
    testEncoder();
    assert(oled_Init(&display) == OLED_OK);
    testService();
    testDisplay(argc > 1 ? argv[1] : NULL);
    puts("Passed word blits, encoder snapshots, service timing and display tests.");
    return 0;
}
