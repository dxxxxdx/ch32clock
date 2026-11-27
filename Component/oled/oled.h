#ifndef CH32CLOCK_OLED_H
#define CH32CLOCK_OLED_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define OLED_WIDTH 128U
#define OLED_PAGES 8U
#define OLED_BUFFER_SIZE (OLED_WIDTH * OLED_PAGES)
#define OLED_FONT_WIDTH 6U

typedef enum {
    OLED_OK = 0,
    OLED_NACK,
    OLED_TIMEOUT,
    OLED_BUS_ERROR,
    OLED_ARBITRATION_LOST,
    OLED_INVALID_ARGUMENT,
    OLED_BUSY,
    OLED_DMA_ERROR,
} OLED_STATUS;

typedef struct oledBuffer {
    uint8_t padding[3];
    uint8_t commandByte;
    /* 控制字和显存连续，DMA 从 commandByte 发送 1025 字节；像素按 4 字节对齐。 */
    uint8_t oledData[OLED_BUFFER_SIZE] __attribute__((aligned(4)));
} oledBuffer;

typedef struct OLED {
    const uint8_t address;       /* 7 位 I2C 地址。 */
    volatile uint8_t status;     /* DMA 回调写入 OLED_STATUS；发送期间为 OLED_BUSY。 */
    uint8_t autoFlush;           /* 1：每帧完成后继续发送整屏。 */
    uint8_t exchangeBuffer;      /* 绘图端提交的新画面，Service 消费后清零。 */
    uint8_t phase;               /* 驱动内部状态，仅 Service 改写。 */
    uint8_t contrast;            /* 此实例下一帧采用的对比度。 */
    oledBuffer *frontBuffer;     /* DMA 正在发送或待发送的完整画面；绘图端不得写。 */
    oledBuffer *backBuffer;      /* 绘图端写入的画面；只在主循环中访问。 */
    /* 平台层负责提交整个数据流；OLED_OK 只表示 DMA 已启动。 */
    OLED_STATUS (*transmit)(struct OLED *self, uint8_t *data, uint16_t length);
    /* 平台层在 DMA/I2C 完成后调用；驱动只在这里记录最终状态。 */
    void (*transmitCallBack)(struct OLED *self, OLED_STATUS result);

} OLED;

extern OLED g_oled;

/* 设置完成回调并安排初始化命令；此后须反复调用 OLED_Service。 */
OLED_STATUS oled_Init(OLED *self);

/* 以下绘图函数只写 backBuffer，不立即启动 DMA。 */
OLED_STATUS oled_setString(OLED *self, uint8_t page, const char *str, uint8_t charNum);
OLED_STATUS oled_clearBuffer(OLED *self);
/* data 至少含 pageCount * 128 字节，且不与目标页重叠。 */
OLED_STATUS oled_setPages(OLED *self, uint8_t firstPage, const uint8_t *data, uint8_t pageCount);
/* 页对齐字块：x 是 4 的倍数，每页连续复制 widthWords 个 32 位字。
 * data 按页紧密排列、4 字节对齐，且不与目标块重叠。 */
OLED_STATUS oled_setBlockWords(OLED *self, uint8_t x, uint8_t firstPage,
                               const uint32_t *data, uint8_t widthWords, uint8_t pageCount);

/* 提交后台画面；真正交换、复制和发送都由 OLED_Service 在主循环完成。 */
OLED_STATUS oled_Flush(OLED *self);
/* 主循环周期调用；每次最多推进一个 DMA 阶段，不等待 I2C 总线。 */
void OLED_Service(OLED *self);
/* Applied on the next flushed frame. */
void oled_setContrast(OLED *self, uint8_t value);

/* 绘图与 Service 必须在同一执行上下文，批量绘图完成后再调用 oled_Flush。
 * 不要跨 Service 缓存 backBuffer 指针；DMA 完成回调只发布结果，不交换缓冲。 */

#ifdef __cplusplus
}
#endif

#endif
