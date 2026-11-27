#ifndef POMODORO_H
#define POMODORO_H

#include <stdint.h>
#include "Component/EC11/EC11.h"
#include "Component/buzzer/buzzer.h"
#include "Component/oled/oled.h"

enum {
    POMODORO_PRESET_CAPACITY = 3,
    POMODORO_TICKS_PER_SECOND = 200,
    POMODORO_TICKS_PER_MINUTE = 60 * POMODORO_TICKS_PER_SECOND,
};

typedef enum { HOME, PREVIEW, EDIT_PRESET, TIMER, SETTINGS, EDIT_SETTING } Page;
typedef enum { BRIGHTNESS, VOLUME, MUTE, PRESET_COUNT, SETTING_COUNT } Setting;
typedef enum { NO_CLICK, CLICK, BACK } Gesture;

typedef struct {
    uint8_t focusMinutes;
    uint8_t breakMinutes;
} Preset;

typedef struct {
    uint8_t brightness;
} DisplayConfig;

typedef struct {
    uint8_t volume;
    uint8_t muted;
} SoundConfig;

typedef struct {
    Preset items[POMODORO_PRESET_CAPACITY];
    uint8_t count;
} PresetConfig;

typedef struct {
    DisplayConfig display;
    SoundConfig sound;
    PresetConfig presets;
} Preferences;

typedef struct {
    uint8_t value;
    uint8_t editingBreak;
} EditorState;

typedef struct {
    Page page;
    uint8_t selectedPreset;
    Setting selectedSetting;
    EditorState editor;
} NavigationState;

typedef struct {
    uint32_t remainingTicks;
    uint8_t inBreak;
    uint8_t paused;
} CountdownState;

typedef struct {
    uint8_t raw, stable, pending, consumed;
    uint32_t changedAt, pressedAt, releasedAt;
} Button;

/* 句柄在静态初始化时绑定，此后只修改元件状态，不替换元件。 */
typedef struct {
    OLED *const display;
    EC11 *const encoder;
    Buzzer *const buzzer;
    const volatile uint32_t *const tick5ms;
} PomodoroDevices;

typedef struct {
    uint32_t startedAt;
    uint8_t visible;
} NoticeState;

typedef struct {
    uint32_t lastTick;
    NoticeState notice;
    uint8_t initialized;
} ServiceState;

typedef struct {
    /* 单实例绘图暂存区，随应用静态分配。 */
    char line[22];
} DisplayState;

typedef struct {
    PomodoroDevices devices;
    Preferences prefs;
    NavigationState navigation;
    CountdownState countdown;
    Button button;
    ServiceState service;
    DisplayState display;
} Pomodoro;

extern const Preferences defaultPreferences;
void turn(Pomodoro *app, int steps);
void press(Pomodoro *app, Gesture gesture);
/* Returns 1 when a phase ends and the buzzer should ring, otherwise 0. */
uint8_t advanceTimer(Pomodoro *app, uint32_t elapsed);
Gesture sampleButton(Button *button, uint8_t down, uint32_t now);
uint8_t Pomodoro_GetSetting(const Preferences *prefs, Setting setting);

/* 外设初始化完成后调用一次；运行状态保存在应用实例中。 */
void Pomodoro_Init(Pomodoro *app);
void Pomodoro_Service(Pomodoro *app);

#endif
