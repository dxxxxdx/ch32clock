/* 双缓冲 OLED：绘图、交换、提交 DMA 在主循环；中断只公布传输结果。 */
#include "oled.h"
#include "oledConst.h"
#include <stddef.h>
#include <string.h>

typedef uint32_t oledWord __attribute__((may_alias));
_Static_assert(_Alignof(oledBuffer) >= 4U, "OLED buffer must be word aligned");
_Static_assert(offsetof(oledBuffer, oledData) % 4U == 0U, "OLED pixels must be word aligned");
_Static_assert(offsetof(oledBuffer, oledData) == offsetof(oledBuffer, commandByte) + 1U,
               "OLED control byte and pixels must be contiguous for DMA");
_Static_assert(OLED_WIDTH % sizeof(oledWord) == 0U, "OLED page must contain whole words");

enum {
    OLED_OFF,
    OLED_INIT_SEND,
    OLED_INIT_WAIT,
    OLED_IDLE,
    OLED_WINDOW_SEND,
    OLED_WINDOW_WAIT,
    OLED_PIXELS_SEND,
    OLED_PIXELS_WAIT,
    OLED_INIT_ERROR,
};

/* 初始化和命令流共用静态缓冲，同一时刻只绑定一个 OLED 实例。 */
static OLED *activeOled;
static uint8_t initStream[sizeof(oledInitBitstream)];
void oled_setContrast(OLED *const self, uint8_t value)
{
    self->contrast = value;
}

static uint8_t windowStream[] = {0x00U, 0x81U, 0xcfU, 0x21U, 0U, 127U, 0x22U, 0U, 7U};

/* DMA/I2C IRQ 中只改状态；下一段命令和缓冲交换都留给 Service。 */
static void oled_i2c_done(OLED *self, OLED_STATUS result)
{
    self->status = (uint8_t)result;
}

/* STOP 尚未结束时底层返回 BUSY，下一次 Service 再提交。 */
static OLED_STATUS oled_start_dma(OLED *self, uint8_t *data, uint16_t len)
{
    self->status = OLED_BUSY;
    OLED_STATUS result = self->transmit(self, data, len);
    if (result != OLED_OK) self->status = (uint8_t)result;
    return result;
}

/* 上一帧 DMA 已停止；交换后按 32 位字复制新前台，供后续局部绘图。 */
static void oled_exchange(OLED *self)
{
    oledBuffer *oldFront = self->frontBuffer;
    self->frontBuffer = self->backBuffer;
    self->backBuffer = oldFront;

    oledWord *dst = (oledWord *)self->backBuffer->oledData;
    const oledWord *src = (const oledWord *)self->frontBuffer->oledData;
    oledWord *const end = dst + OLED_BUFFER_SIZE / sizeof(oledWord);
    while (dst != end) {
        *dst++ = *src++; /* 对齐的 32 位读写，编译为 lw / sw。 */
    }
    self->exchangeBuffer = 0U;
}

OLED_STATUS oled_Init(OLED *self)
{
    if (self == NULL || self->address > 0x7FU || self->frontBuffer == NULL ||
        self->backBuffer == NULL || self->frontBuffer == self->backBuffer ||
        self->transmit == NULL) {
        return OLED_INVALID_ARGUMENT;
    }
    if (activeOled != NULL && activeOled != self) return OLED_BUSY;

    activeOled = self;
    self->transmitCallBack = oled_i2c_done;
    memcpy(initStream, oledInitBitstream, sizeof(initStream));
    self->frontBuffer->commandByte = 0x40U;
    self->backBuffer->commandByte = 0x40U;
    self->status = OLED_OK;
    self->exchangeBuffer = 1U; /* 初始化后至少显示一次当前后台画面。 */
    self->phase = OLED_INIT_SEND;
    return OLED_OK;
}

OLED_STATUS oled_setString(OLED *self, uint8_t page, const char *str, uint8_t charNum)
{
    if (self == NULL || self->backBuffer == NULL || str == NULL || page >= OLED_PAGES) {
        return OLED_INVALID_ARGUMENT;
    }
    uint8_t *pixels = &self->backBuffer->oledData[page * OLED_WIDTH];
    uint16_t column = 0U;
    for (uint8_t i = 0U; i < charNum && column + OLED_FONT_WIDTH <= OLED_WIDTH; ++i) {
        uint8_t ch = (uint8_t)str[i];
        if (ch == 0U) break;
        if (ch < ' ' || (uint16_t)(ch - ' ') >= sizeof(asc2_8x6) / sizeof(asc2_8x6[0])) {
            ch = '?';
        }
        memcpy(&pixels[column], asc2_8x6[ch - ' '], OLED_FONT_WIDTH);
        column += OLED_FONT_WIDTH;
    }
    return OLED_OK;
}

OLED_STATUS oled_clearBuffer(OLED *self)
{
    if (self == NULL || self->backBuffer == NULL) return OLED_INVALID_ARGUMENT;
    oledWord *pixels = (oledWord *)self->backBuffer->oledData;
    for (uint16_t i = 0U; i < OLED_BUFFER_SIZE / sizeof(oledWord); ++i) pixels[i] = 0U;
    return OLED_OK;
}

OLED_STATUS oled_setPages(OLED *self, uint8_t firstPage, const uint8_t *data, uint8_t pageCount)
{
    if (self == NULL || self->backBuffer == NULL || firstPage >= OLED_PAGES ||
        pageCount > OLED_PAGES - firstPage || (data == NULL && pageCount != 0U)) {
        return OLED_INVALID_ARGUMENT;
    }
    if (pageCount != 0U) {
        uint8_t *target = &self->backBuffer->oledData[firstPage * OLED_WIDTH];
        uint16_t length = (uint16_t)(pageCount * OLED_WIDTH);
        if (((uintptr_t)data & 3U) == 0U) {
            oledWord *dst = (oledWord *)target;
            const oledWord *src = (const oledWord *)data;
            for (uint16_t i = 0U; i < length / sizeof(oledWord); ++i) dst[i] = src[i];
        } else {
            memcpy(target, data, length);
        }
    }
    return OLED_OK;
}

OLED_STATUS oled_setBlockWords(OLED *const self, uint8_t x, uint8_t firstPage,
                               const uint32_t *data, uint8_t widthWords, uint8_t pageCount)
{
    if (self == NULL || self->backBuffer == NULL || data == NULL ||
        (x & 3U) != 0U || ((uintptr_t)data & 3U) != 0U || widthWords == 0U ||
        pageCount == 0U || (unsigned)x + (unsigned)widthWords * 4U > OLED_WIDTH ||
        firstPage >= OLED_PAGES || pageCount > OLED_PAGES - firstPage) {
        return OLED_INVALID_ARGUMENT;
    }
    for (uint8_t page = 0U; page < pageCount; ++page) {
        oledWord *dst = (oledWord *)&self->backBuffer->oledData[
            (firstPage + page) * OLED_WIDTH + x];
        for (uint8_t word = 0U; word < widthWords; ++word) *dst++ = *data++;
    }
    return OLED_OK;
}

OLED_STATUS oled_Flush(OLED *self)
{
    if (self == NULL || self != activeOled || self->phase == OLED_OFF) {
        return OLED_INVALID_ARGUMENT;
    }
    if (self->phase == OLED_INIT_ERROR) return (OLED_STATUS)self->status;
    self->exchangeBuffer = 1U;
    if (self->phase == OLED_IDLE) self->status = OLED_OK; /* 允许重试上次帧错误。 */
    return OLED_OK;
}

void OLED_Service(OLED *self)
{
    if (self == NULL || self != activeOled) return;

    OLED_STATUS result;
    switch (self->phase) {
        case OLED_INIT_SEND:
            result = oled_start_dma(self, initStream, sizeof(initStream));
            if (result == OLED_OK) self->phase = OLED_INIT_WAIT;
            else if (result != OLED_BUSY) self->phase = OLED_INIT_ERROR;
            break;
        case OLED_INIT_WAIT:
            if (self->status == OLED_BUSY) break;
            self->phase = self->status == OLED_OK ? OLED_IDLE : OLED_INIT_ERROR;
            break;
        case OLED_IDLE:
            if (self->status != OLED_OK ||
                (self->exchangeBuffer == 0U && self->autoFlush == 0U)) break;
            if (self->exchangeBuffer != 0U) oled_exchange(self);
            self->phase = OLED_WINDOW_SEND;
            break;
        case OLED_WINDOW_SEND:
            windowStream[2] = self->contrast; /* 上一段命令 DMA 已释放。 */
            result = oled_start_dma(self, windowStream, sizeof(windowStream));
            if (result == OLED_OK) self->phase = OLED_WINDOW_WAIT;
            else if (result != OLED_BUSY) self->phase = OLED_IDLE;
            break;
        case OLED_WINDOW_WAIT:
            if (self->status == OLED_BUSY) break;
            self->phase = self->status == OLED_OK ? OLED_PIXELS_SEND : OLED_IDLE;
            break;
        case OLED_PIXELS_SEND:
            result = oled_start_dma(self, &self->frontBuffer->commandByte,
                                    OLED_BUFFER_SIZE + 1U);
            if (result == OLED_OK) self->phase = OLED_PIXELS_WAIT;
            else if (result != OLED_BUSY) self->phase = OLED_IDLE;
            break;
        case OLED_PIXELS_WAIT:
            if (self->status != OLED_BUSY) self->phase = OLED_IDLE;
            break;
        default:
            break;
    }
}
