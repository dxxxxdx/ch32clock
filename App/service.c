#include "pomodoro.h"
#include "display.h"

/* 所有时长均以 5 ms 节拍计数。 */
enum { ALERT_DURATION_TICKS = 120, NOTICE_DURATION_TICKS = 400 };
static const uint8_t contrastLevels[] = {0, 16, 48, 96, 160, 255};

static uint32_t secondsRemaining(const Pomodoro *const app)
{
    return (app->countdown.remainingTicks + POMODORO_TICKS_PER_SECOND - 1U) /
           POMODORO_TICKS_PER_SECOND;
}

static void refreshDisplay(Pomodoro *const app)
{
    uint8_t brightness = app->navigation.page == EDIT_SETTING &&
                         app->navigation.selectedSetting == BRIGHTNESS ?
                         app->navigation.editor.value : app->prefs.display.brightness;
    oled_setContrast(app->devices.display, contrastLevels[brightness]);
    drawPomodoro(app);
}

void Pomodoro_Init(Pomodoro *const app)
{
    app->service.initialized = 0U;
    app->prefs = defaultPreferences;
    app->navigation = (NavigationState){.page = HOME};
    app->countdown = (CountdownState){0};
    app->service.notice = (NoticeState){0};
    Buzzer_Stop(app->devices.buzzer);
    EC11Input input;
    EC11_Read(app->devices.encoder, &input); /* 丢弃初始化期间的转动。 */
    app->button = (Button){
        .raw = input.buttonDown,
        .stable = input.buttonDown,
        .consumed = input.buttonDown,
    };
    app->service.lastTick = *app->devices.tick5ms;
    app->service.initialized = 1U;
    refreshDisplay(app);
}

void Pomodoro_Service(Pomodoro *const app)
{

    OLED_Service(app->devices.display); /* DMA 状态推进不受 5 ms 节拍限制。 */
    uint32_t now = *app->devices.tick5ms;
    uint32_t elapsed = now - app->service.lastTick;
    if (elapsed == 0U) return;
    app->service.lastTick = now;

    Buzzer_Service(app->devices.buzzer, now);
    uint32_t oldSeconds = secondsRemaining(app);
    uint8_t expired = advanceTimer(app, elapsed);
    if (expired) {
        app->service.notice.startedAt = now;
        app->service.notice.visible = 1U;
        if (!app->prefs.sound.muted)
            Buzzer_Play(app->devices.buzzer, app->prefs.sound.volume, now, ALERT_DURATION_TICKS);
    }
    uint8_t clearNotice = app->service.notice.visible &&
                         now - app->service.notice.startedAt >= NOTICE_DURATION_TICKS;
    if (clearNotice) app->service.notice.visible = 0U;

    EC11Input input;
    EC11_Read(app->devices.encoder, &input);
    Gesture gesture = sampleButton(&app->button, input.buttonDown, now);
    if (input.rotation != 0) turn(app, input.rotation);
    if (gesture != NO_CLICK) {
        app->service.notice.visible = 0U;
        press(app, gesture);
    }
    if (input.rotation != 0 || gesture != NO_CLICK || expired || clearNotice ||
        secondsRemaining(app) != oldSeconds) refreshDisplay(app);
}
