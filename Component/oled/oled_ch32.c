#include "oled.h"
#include "I2C.h"

/* CH32V203 适配层：此工程只有一个 g_oled，使用 I2C1 的 DMA 发送通道。 */
static oledBuffer oledFrames[2];

static OLED_STATUS oled_ch32_result(I2C_HW_RESULT result)
{
    switch (result) {
        case I2C_OK: return OLED_OK;
        case I2C_NACK: return OLED_NACK;
        case I2C_BUSY: return OLED_BUSY;
        case I2C_TIMEOUT: return OLED_TIMEOUT;
        case I2C_INVALID_ARGUMENT: return OLED_INVALID_ARGUMENT;
        case I2C_DMA_ERROR: return OLED_DMA_ERROR;
        default: return OLED_BUS_ERROR;
    }
}

/* BSP 在 DMA/I2C 完成或出错时进入这里，直接通知 g_oled。 */
static void oled_ch32_done(I2C_HW_RESULT result)
{
    if (g_oled.transmitCallBack != 0) {
        g_oled.transmitCallBack(&g_oled, oled_ch32_result(result));
    }
}

/* data[0] 已包含 OLED 控制字；BSP 负责地址阶段和整包 DMA。 */
static OLED_STATUS oled_ch32_transmit(OLED *self, uint8_t *data, uint16_t length)
{
    if (self != &g_oled) return OLED_INVALID_ARGUMENT;

    BSP_I2C_DMA_OnComplete = oled_ch32_done;
    return oled_ch32_result(BSP_I2C_DMA_Transmit(data, (uint8_t)(self->address << 1),
                                                length));
}

OLED g_oled = {
    .address = 0x3CU,
    .contrast = 0xcfU,
    .frontBuffer = &oledFrames[0],
    .backBuffer = &oledFrames[1],
    .transmit = oled_ch32_transmit,
};
