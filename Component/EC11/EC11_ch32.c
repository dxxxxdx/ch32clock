#include "EC11.h"
#include "ch32v20x.h"

static uint8_t readButton(void)
{
    return (GPIOA->INDR & (1U << 5)) == 0U;
}

static uint32_t lock(void)
{
    uint32_t enabled = NVIC_GetStatusIRQ(EXTI9_5_IRQn);
    NVIC_DisableIRQ(EXTI9_5_IRQn);
    return enabled;
}

static void unlock(uint32_t enabled)
{
    if (enabled != 0U) NVIC_EnableIRQ(EXTI9_5_IRQn);
}

static const EC11Hardware hardware = {
    .readButton = readButton,
    .lock = lock,
    .unlock = unlock,
};

EC11 g_ec11 = {.hardware = &hardware};
