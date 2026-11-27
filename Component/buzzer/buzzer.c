#include "buzzer.h"

void Buzzer_Stop(Buzzer *const self)
{
    self->hardware->stop();
    self->ringing = 0U;
}

void Buzzer_Play(Buzzer *const self, uint8_t volume, uint32_t now, uint32_t durationTicks)
{
    if (volume == 0U || durationTicks == 0U) {
        Buzzer_Stop(self);
        return;
    }
    if (volume > 5U) volume = 5U;
    self->startedAt = now;
    self->durationTicks = durationTicks;
    self->ringing = 1U;
    self->hardware->setVolume(volume);
}

void Buzzer_Service(Buzzer *const self, uint32_t now)
{
    if (self->ringing && now - self->startedAt >= self->durationTicks)
        Buzzer_Stop(self);
}
