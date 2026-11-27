//
// Created by dxxdx on 2026/11/23.
//

#include "EC11.h"

void EC11_Read(EC11 *const self, EC11Input *const input)
{
    uint32_t previous = self->hardware->lock();
    input->rotation = (int16_t)self->forwardCount - self->reverseCount;
    input->buttonDown = self->hardware->readButton() != 0U;
    self->forwardCount = self->reverseCount = self->pressCount = 0U;
    self->hardware->unlock(previous);
}
