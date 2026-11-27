#include "App/pomodoro.h"
#include "systemInit.h"
#include "clocktick.h"
#include "bootLoader/bootCfg.h"

static Pomodoro app = {
    .devices = {
        .display = &g_oled,
        .encoder = &g_ec11,
        .buzzer = &g_buzzer,
        .tick5ms = &clockTick5ms,
    },
};

int main(void)
{
    BSP_SystemInit();
    if (oled_Init(&g_oled) != OLED_OK) for (;;) { }
    checkBootloader();
    if (goBootloader != 0U) enterBootloader();
    Pomodoro_Init(&app);
    for (;;) Pomodoro_Service(&app);
}
