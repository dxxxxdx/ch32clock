#ifndef CH32CLOCK_BUZZER_H
#define CH32CLOCK_BUZZER_H

#include <stdint.h>

typedef struct {
    void (*const setVolume)(uint8_t level);
    void (*const stop)(void);
} BuzzerHardware;

typedef struct Buzzer {
    const BuzzerHardware *const hardware;
    uint32_t startedAt;
    uint32_t durationTicks;
    uint8_t ringing;
} Buzzer;

extern Buzzer g_buzzer;

void Buzzer_Play(Buzzer *self, uint8_t volume, uint32_t now, uint32_t durationTicks);
void Buzzer_Stop(Buzzer *self);
void Buzzer_Service(Buzzer *self, uint32_t now);

#endif
