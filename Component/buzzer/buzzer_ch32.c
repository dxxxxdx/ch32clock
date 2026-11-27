#include "buzzer.h"
#include "TIM.h"

static const BuzzerHardware hardware = {
    .setVolume = TIM_SetVolume,
    .stop = TIM_DisableOutput,
};

Buzzer g_buzzer = {.hardware = &hardware};
