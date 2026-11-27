#ifndef CH32CLOCK_I2C_H
#define CH32CLOCK_I2C_H

#include <stdint.h>

typedef enum {
    I2C_OK = 0,
    I2C_NACK,
    I2C_BUS_ERROR,
    I2C_BUSY,
    I2C_TIMEOUT,
    I2C_INVALID_ARGUMENT,
    I2C_DMA_ERROR,
} I2C_HW_RESULT;

void BSP_DMA_I2C_Init(void);

/* address8b 是含读写位的 8 位写地址（bit0=0），如 OLED 0x3C 使用 0x78。
 * source[0] 是地址之后的首字节，OLED 的 0x00/0x40 控制字也须放在流中。
 * 返回 I2C_OK 只表示已提交；完成回调前不得修改或复用 source。
 * 完成回调发生在 BTF（成功时）和 STOP 请求之后；此时总线 BUSY 位可能尚未清零。
 * 下一包在主循环中另行提交，若返回 I2C_BUSY 则稍后重试。 */
I2C_HW_RESULT BSP_I2C_DMA_Transmit(uint8_t *source, uint8_t address8b,
                                    uint16_t lenth);

/* 同步探测设备是否应答：START + 8 位写地址 + STOP，不发送数据。
 * 地址格式与 DMA_Transmit 相同，如 OLED 0x3C 传 0x78。
 * 仅在初始化完成后的主循环上下文调用；DMA/总线忙时返回 I2C_BUSY。
 * 有限轮询等待；返回 I2C_OK、I2C_NACK、I2C_TIMEOUT 或相应错误。
 * 不改 DMA 传输结果，也不触发 DMA 完成回调。 */
I2C_HW_RESULT BSP_I2C_IsReady(uint8_t address8b);

extern volatile uint8_t BSP_I2C_DMA_Busy;
/* 传输中为 I2C_BUSY；完成后为最终结果。 */
extern volatile I2C_HW_RESULT BSP_I2C_DMA_Result;

/* 在中断上下文调用；DMA 已释放 source，STOP 已请求但可能尚未完成。 */
extern void (*BSP_I2C_DMA_OnComplete)(I2C_HW_RESULT result);

#endif
