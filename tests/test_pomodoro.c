#include "App/pomodoro.h"
#include <assert.h>
#include <stdio.h>

static void testPresetCount(void)
{
    Pomodoro app = {.prefs = defaultPreferences};
    turn(&app, -1); /* settings card */
    press(&app, CLICK);
    turn(&app, -1);
    for (int count = 1; count <= 3; ++count) {
        assert(app.navigation.selectedSetting == PRESET_COUNT);
        press(&app, CLICK);
        turn(&app, count - app.navigation.editor.value);
        press(&app, CLICK);
        press(&app, BACK);
        assert(app.navigation.selectedPreset == count && app.prefs.presets.count == count);
        turn(&app, 1);
        assert(app.navigation.selectedPreset == 0);
        turn(&app, count);
        press(&app, CLICK);
        turn(&app, -1);
    }
    assert(app.prefs.presets.items[1].focusMinutes == 50 && app.prefs.presets.items[2].focusMinutes == 15);
}

static void testTimer(void)
{
    Pomodoro app = {.prefs = defaultPreferences};
    press(&app, BACK); /* edit first preset */
    turn(&app, 1);
    press(&app, CLICK);
    turn(&app, -1);
    press(&app, BACK);
    assert(app.prefs.presets.items[0].focusMinutes == 26 && app.prefs.presets.items[0].breakMinutes == 4);

    press(&app, CLICK);
    press(&app, CLICK);
    assert(app.countdown.remainingTicks == 312000);
    advanceTimer(&app, 1);
    press(&app, CLICK);
    advanceTimer(&app, 5000);
    assert(app.countdown.remainingTicks == 311999);
    press(&app, CLICK);
    assert(advanceTimer(&app, 312001) && app.countdown.inBreak && app.countdown.remainingTicks == 47998);
    assert(advanceTimer(&app, 47998) && app.navigation.page == PREVIEW);
    press(&app, CLICK);
    press(&app, BACK);
    assert(app.navigation.page == PREVIEW);
}

static void testSettings(void)
{
    Pomodoro app = {.prefs = defaultPreferences};
    turn(&app, -1);
    press(&app, CLICK);
    for (int setting = BRIGHTNESS; setting <= MUTE; ++setting) {
        uint8_t original = Pomodoro_GetSetting(&app.prefs, (Setting)setting);
        press(&app, CLICK);
        turn(&app, 1);
        press(&app, BACK);
        assert(Pomodoro_GetSetting(&app.prefs, (Setting)setting) == original);

        press(&app, CLICK);
        turn(&app, 1);
        press(&app, CLICK);
        assert(Pomodoro_GetSetting(&app.prefs, (Setting)setting) == original + 1);
        turn(&app, 1);
    }
}

static void startTap(Button *button, uint32_t tick)
{
    assert(sampleButton(button, 1, tick) == NO_CLICK);
    assert(sampleButton(button, 1, tick + 4) == NO_CLICK);
    assert(sampleButton(button, 0, tick + 10) == NO_CLICK);
}

static void testGestures(void)
{
    Button button = {0};
    startTap(&button, 10);
    assert(sampleButton(&button, 0, 24) == NO_CLICK);
    assert(sampleButton(&button, 0, 84) == CLICK);

    startTap(&button, 100);
    assert(sampleButton(&button, 0, 114) == NO_CLICK);
    startTap(&button, 130);
    assert(sampleButton(&button, 0, 144) == BACK);
    assert(sampleButton(&button, 0, 220) == NO_CLICK);

    sampleButton(&button, 1, 0xfffffff0u);
    sampleButton(&button, 1, 0xfffffff4u);
    assert(sampleButton(&button, 1, 128) == BACK);
    sampleButton(&button, 0, 132);
    sampleButton(&button, 0, 136);
    assert(sampleButton(&button, 0, 222) == NO_CLICK);
}

int main(void)
{
    testPresetCount();
    testTimer();
    testSettings();
    testGestures();
    puts("Passed preset navigation, timer transitions, settings and gesture tests.");
}
