#include "pomodoro.h"

const Preferences defaultPreferences = {
    .display = {.brightness = 3},
    .sound = {.volume = 2, .muted = 0},
    .presets = {
        .items = {{25, 5}, {50, 10}, {15, 3}},
        .count = 2,
    },
};

static const struct {
    uint8_t minimum, maximum;
} settingLimits[SETTING_COUNT] = {
    [BRIGHTNESS] = {1, 5},
    [VOLUME] = {1, 5},
    [MUTE] = {0, 1},
    [PRESET_COUNT] = {1, POMODORO_PRESET_CAPACITY},
};

uint8_t Pomodoro_GetSetting(const Preferences *const prefs, Setting setting)
{
    switch (setting) {
    case BRIGHTNESS: return prefs->display.brightness;
    case VOLUME: return prefs->sound.volume;
    case MUTE: return prefs->sound.muted;
    case PRESET_COUNT: return prefs->presets.count;
    default: return 0;
    }
}

static void setSetting(Preferences *const prefs, Setting setting, uint8_t value)
{
    switch (setting) {
    case BRIGHTNESS: prefs->display.brightness = value; break;
    case VOLUME: prefs->sound.volume = value; break;
    case MUTE: prefs->sound.muted = value; break;
    case PRESET_COUNT: prefs->presets.count = value; break;
    default: break;
    }
}

static uint8_t clamp(int value, int low, int high)
{
    if (value < low) return low;
    if (value > high) return high;
    return value;
}

static uint8_t cycle(int index, int steps, int count)
{
    return ((index + steps) % count + count) % count;
}

void turn(Pomodoro *const app, int steps)
{
    NavigationState *const nav = &app->navigation;
    switch (nav->page) {
    case HOME:
        nav->selectedPreset = cycle(nav->selectedPreset, steps, app->prefs.presets.count + 1);
        break;
    case EDIT_PRESET: {
        Preset *p = &app->prefs.presets.items[nav->selectedPreset];
        uint8_t *minutes = nav->editor.editingBreak ? &p->breakMinutes : &p->focusMinutes;
        *minutes = clamp(*minutes + steps, 1, 99);
        break;
    }
    case SETTINGS:
        nav->selectedSetting = cycle(nav->selectedSetting, steps, SETTING_COUNT);
        break;
    case EDIT_SETTING:
        nav->editor.value = clamp(nav->editor.value + steps,
                                  settingLimits[nav->selectedSetting].minimum,
                                  settingLimits[nav->selectedSetting].maximum);
        break;
    default: break;
    }
}

void press(Pomodoro *const app, Gesture gesture)


//懒得做函数指针表了，直接case展开面向过程


{
    NavigationState *const nav = &app->navigation;
    CountdownState *const timer = &app->countdown;
    if (gesture == NO_CLICK) return;
    uint8_t back = gesture == BACK;
    switch (nav->page) {
    case HOME:
        if (nav->selectedPreset == app->prefs.presets.count) {
            if (!back) {
                nav->page = SETTINGS;
                nav->selectedSetting = BRIGHTNESS;
            }
        } else {
            nav->editor.editingBreak = 0;
            nav->page = back ? EDIT_PRESET : PREVIEW;
        }
        break;
    case PREVIEW:
        if (back) nav->page = HOME;
        else {
            nav->page = TIMER;
            timer->inBreak = timer->paused = 0;
            timer->remainingTicks = app->prefs.presets.items[nav->selectedPreset].focusMinutes *
                                    POMODORO_TICKS_PER_MINUTE;
        }
        break;
    case TIMER:
        if (back) nav->page = PREVIEW;
        else timer->paused = !timer->paused;
        break;
    case EDIT_PRESET:
        if (back) {
            nav->page = HOME;
            break;
        }
        nav->editor.editingBreak = !nav->editor.editingBreak;
        break;
    case SETTINGS:
        if (back) nav->page = HOME;
        else {
            nav->editor.value = Pomodoro_GetSetting(&app->prefs, nav->selectedSetting);
            nav->page = EDIT_SETTING;
        }
        break;
    case EDIT_SETTING:
        nav->page = SETTINGS;
        if (!back) {
            setSetting(&app->prefs, nav->selectedSetting, nav->editor.value);
            nav->selectedPreset = app->prefs.presets.count; /* Keep settings card selected. */
        }
        break;
    }
}

uint8_t advanceTimer(Pomodoro *const app, uint32_t elapsed)
{
    CountdownState *const timer = &app->countdown;
    if (app->navigation.page != TIMER || timer->paused) return 0;
    if (elapsed < timer->remainingTicks) {
        timer->remainingTicks -= elapsed;
        return 0;
    }
    elapsed -= timer->remainingTicks;
    uint32_t breakTicks = app->prefs.presets.items[app->navigation.selectedPreset].breakMinutes *
                          POMODORO_TICKS_PER_MINUTE;
    if (!timer->inBreak && elapsed < breakTicks) {
        timer->inBreak = 1;
        timer->remainingTicks = breakTicks - elapsed;
    } else {
        app->navigation.page = PREVIEW;
        timer->remainingTicks = 0;
    }
    return 1;
}

Gesture sampleButton(Button *b, uint8_t down, uint32_t now)
{
    down = down != 0U;
    enum { DEBOUNCE = 4, DOUBLE = 60, HOLD = 140 }; /* 20, 300 and 700 ms */
    if (down != b->raw) {
        b->raw = down;
        b->changedAt = now;
    }
    if (b->raw != b->stable && now - b->changedAt >= DEBOUNCE) {
        b->stable = down;
        if (down) {
            b->pressedAt = now;
            b->consumed = 0;
        } else if (!b->consumed) {
            if (b->pending) {
                b->pending = 0;
                return BACK;
            }
            b->pending = 1;
            b->releasedAt = now;
        }
    }
    if (b->stable && !b->consumed && now - b->pressedAt >= HOLD) {
        b->consumed = 1;
        b->pending = 0;
        return BACK;
    }
    if (b->pending && !b->stable && !b->raw && now - b->releasedAt >= DOUBLE) {
        b->pending = 0;
        return CLICK;
    }
    return NO_CLICK;
}
