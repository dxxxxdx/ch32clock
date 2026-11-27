#include "I2C.h"
#include "ch32v20x.h"

/* 一包发送的硬件阶段；START/ADDR/BTF 由 I2C 事件中断推进，数据由 DMA 发送。 */
typedef enum {
    TX_IDLE,
    TX_WAIT_SB,
    TX_WAIT_ADDR,
    TX_DMA,
    TX_WAIT_BTF,
} TxPhase;

static volatile TxPhase txPhase;
static uint8_t txAddress; /* 8 位写地址，例如 OLED 的 0x3C << 1 = 0x78。 */

volatile uint8_t BSP_I2C_DMA_Busy;
volatile I2C_HW_RESULT BSP_I2C_DMA_Result = I2C_OK;
void (*BSP_I2C_DMA_OnComplete)(I2C_HW_RESULT result);

/* 初始化 I2C1 的 400 kHz 时序和 DMA1 Channel6 的固定配置。
 * 前置条件：BSP_GPIO_Init 已设置 APB1=36 MHz 并配置 PB6/PB7。
 * 这里只准备硬件；每包的地址、源指针和长度由发送函数设置。 */
void BSP_DMA_I2C_Init(void)
{
    RCC->APB1PCENR |= 0x00200000u; /* I2C1 时钟。 */
    RCC->AHBPCENR  |= 0x00000001u; /* DMA1 时钟。 */

    /* BSP_GPIO_Init 已将 PCLK1 设为 36 MHz；快速模式 400 kHz，duty=2。
     * CCR=36 MHz/(3*400 kHz)=30；RTR=11。 */
    I2C1->CTLR1 = 0x0000u;
    I2C1->CTLR2 = 0x0024u;
    I2C1->CKCFGR = 0x801Eu;  /* FS=1，CCR=30。 */
    I2C1->RTR = 0x000Bu;
    I2C1->OADDR1 = 0x4000u;  /* 7 位地址模式要求 bit14=1。 */
    I2C1->CTLR1 = 0x0001u;   /* PE=1，启用 I2C1。 */

    /* I2C1_TX 固定映射到 DMA1 Channel6；内存地址和长度每包设置。 */
    DMA1_Channel6->CFGR = 0x0000u;
    DMA1->INTFCR = 0x00F00000u;
    DMA1_Channel6->PADDR = (uint32_t)(uintptr_t)&I2C1->DATAR;
    DMA1_Channel6->CNTR = 0u;
    DMA1_Channel6->CFGR = 0x0000209Au; /* 高优先级、内存递增、内存到外设、完成/错误中断。 */

    txPhase = TX_IDLE;
    BSP_I2C_DMA_Busy = 0u;
    BSP_I2C_DMA_Result = I2C_OK;
}

/* 统一收尾一包发送，并将最终结果交给上层回调。
 * 成功时由 BTF 事件调用，源缓冲已可复用；失败时立即停止 DMA。
 * 两种结果都在仍为主机时请求 STOP，然后通知上层。 */
static void tx_finish(I2C_HW_RESULT result)
{
    if (txPhase == TX_IDLE) {
        I2C1->STAR1 = 0xB0FFu; /* 无活动事务时只清遗留错误标志。 */
        return;
    }

    I2C1->CTLR2 = 0x0024u;      /* 关事件/错误中断及 DMA 请求，保留 FREQ=36。 */
    DMA1_Channel6->CFGR &= ~0x0001u;
    DMA1->INTFCR = 0x00F00000u;
    I2C1->STAR1 = 0xB0FFu;      /* AF/BERR/ARLO/OVR/TIMEOUT 写 0 清除。 */

    /* 成功时 BTF 已确认末字节发完；错误时也要释放总线。
     * 仲裁丢失后不再是主机，不能强发 STOP。 */
    if ((I2C1->STAR2 & 0x0001u) != 0u) {
        I2C1->CTLR1 |= 0x0200u;
    }
    txPhase = TX_IDLE;

    /* 先公布最终结果和缓冲区所有权，再调用上层完成回调。 */
    BSP_I2C_DMA_Result = result;
    __asm__ volatile ("fence rw, rw" ::: "memory");
    BSP_I2C_DMA_Busy = 0u;

    if (BSP_I2C_DMA_OnComplete != 0) {
        BSP_I2C_DMA_OnComplete(result);
    }
}

/* 异步提交一包 I2C 写数据：先装 DMA，再请求 START，后续由中断推进。
 * source 包含地址后的全部数据；I2C_OK 仅代表提交成功，不代表发送完成。
 * 总线或本模块忙时返回 I2C_BUSY，提交后直到回调前不得修改 source。 */
I2C_HW_RESULT BSP_I2C_DMA_Transmit(uint8_t *source, uint8_t address8b,
                                    uint16_t lenth)
{
    if (source == 0 || lenth == 0u || (address8b & 1u) != 0u) {
        return I2C_INVALID_ARGUMENT;
    }

    /* 上一包的 STOP 尚未使 BUSY 清零时，下一包稍后再提交。 */
    if (txPhase != TX_IDLE || (I2C1->STAR2 & 0x0002u) != 0u) {
        return I2C_BUSY;
    }

    /* 先装 DMA 源地址/长度；清旧中断。ADDR 事件到来前不会启动通道。 */
    DMA1_Channel6->CFGR &= ~0x0001u;
    DMA1->INTFCR = 0x00F00000u;
    DMA1_Channel6->MADDR = (uint32_t)(uintptr_t)source;
    DMA1_Channel6->CNTR = lenth;

    txAddress = address8b;
    BSP_I2C_DMA_Result = I2C_BUSY;
    BSP_I2C_DMA_Busy = 1u;
    txPhase = TX_WAIT_SB;

    I2C1->STAR1 = 0xB0FFu;
    PFIC->IPRR[1] = 0x00018001u; /* 清 DMA1_CH6、I2C1_EV、I2C1_ER pending。 */
    __asm__ volatile ("fence rw, rw" ::: "memory");
    I2C1->CTLR2 = 0x0324u;       /* FREQ=36，打开事件/错误中断。 */
    I2C1->CTLR1 |= 0x0100u;      /* 请求 START，等待事件中断处理 SB。 */
    return I2C_OK;
}

enum { I2C_READY_POLL_LIMIT = 100000U };

/* 探测由轮询独占地址阶段；同时检查错误，避免把 NACK 等成超时。 */
static I2C_HW_RESULT i2c_ready_wait(uint16_t flag)
{
    for (uint32_t remaining = I2C_READY_POLL_LIMIT; remaining != 0U; --remaining) {
        uint16_t status = I2C1->STAR1;
        if ((status & 0x4000U) != 0U) return I2C_TIMEOUT;
        if ((status & 0x0b00U) != 0U) return I2C_BUS_ERROR; /* BERR/ARLO/OVR。 */
        if ((status & 0x0400U) != 0U) return I2C_NACK;
        if ((status & flag) != 0U) return I2C_OK;
    }
    return I2C_TIMEOUT;
}

I2C_HW_RESULT BSP_I2C_IsReady(uint8_t address8b)
{
    if ((address8b & 1U) != 0U) return I2C_INVALID_ARGUMENT;
    if (txPhase != TX_IDLE || (I2C1->STAR2 & 0x0002U) != 0U) return I2C_BUSY;
    if ((I2C1->CTLR1 & 0x0001U) == 0U) return I2C_BUS_ERROR;

    uint16_t control2 = I2C1->CTLR2;
    /* 暂停 I2C 事件/错误/缓冲中断和 DMA 请求，ADDR 由本函数清除。 */
    I2C1->CTLR2 = control2 & (uint16_t)~0x1f00U;
    I2C1->STAR1 = 0xb0ffU;
    PFIC->IPRR[1] = 0x00018000U; /* 清 I2C1_EV/ER 遗留 pending。 */

    I2C1->CTLR1 |= 0x0100U;
    I2C_HW_RESULT result = i2c_ready_wait(0x0001U); /* SB。 */
    if (result == I2C_OK) {
        I2C1->DATAR = address8b; /* 读 STAR1 后写地址，清 SB。 */
        result = i2c_ready_wait(0x0002U); /* ADDR 表示从设备已 ACK。 */
    }

    /* 顺序读 STAR1/STAR2，清除成功时的 ADDR；仍为主机才请求 STOP。
     * 仲裁丢失时不向其他主机的事务强发 STOP。 */
    (void)I2C1->STAR1;
    uint16_t status2 = I2C1->STAR2;
    I2C1->CTLR1 &= (uint16_t)~0x0100U; /* 也取消尚未完成的 START。 */
    if ((status2 & 0x0001U) != 0U) I2C1->CTLR1 |= 0x0200U;
    I2C1->STAR1 = 0xb0ffU; /* 包括 NACK 的 AF 标志，均在返回前清除。 */

    if ((status2 & 0x0001U) != 0U) {
        uint32_t remaining = I2C_READY_POLL_LIMIT;
        while ((I2C1->CTLR1 & 0x0200U) != 0U || (I2C1->STAR2 & 0x0002U) != 0U) {
            if (--remaining == 0U) {
                result = I2C_TIMEOUT;
                break;
            }
        }
    }
    PFIC->IPRR[1] = 0x00018000U;
    I2C1->CTLR2 = control2;
    return result;
}

/* 处理 I2C 事件：SB 时写地址，ADDR 时启动 DMA，BTF 时确认末字节发完。 */
void I2C1_EV_IRQHandler(void)
    __attribute__((interrupt("WCH-Interrupt-fast")));

void I2C1_EV_IRQHandler(void)
{
    uint16_t sr = I2C1->STAR1;
    if ((sr & 0x4F00u) != 0u) {
        tx_finish((sr & 0x0400u) != 0u ? I2C_NACK : I2C_BUS_ERROR);
        return;
    }

    if (txPhase == TX_WAIT_SB && (sr & 0x0001u) != 0u) {
        txPhase = TX_WAIT_ADDR;
        I2C1->DATAR = txAddress; /* 读 STAR1 再写地址，清 SB。 */
    } else if (txPhase == TX_WAIT_ADDR && (sr & 0x0002u) != 0u) {
        (void)I2C1->STAR2;      /* 读 STAR1 再读 STAR2，清 ADDR。 */
        txPhase = TX_DMA;
        DMA1_Channel6->CFGR |= 0x0001u;
        I2C1->CTLR2 = 0x0924u; /* 开 DMA/错误中断；数据期关事件中断。 */
    } else if (txPhase == TX_WAIT_BTF && (sr & 0x0004u) != 0u) {
        tx_finish(I2C_OK);
    }
}

/* 处理 DMA 完成/错误。完成只代表数据进入 DATAR，随后还要等 I2C 的 BTF。 */
void DMA1_Channel6_IRQHandler(void)
    __attribute__((interrupt("WCH-Interrupt-fast")));

void DMA1_Channel6_IRQHandler(void)
{
    uint32_t flags = DMA1->INTFR & 0x00A00000u; /* TCIF6：完成；TEIF6：错误。 */
    DMA1->INTFCR = 0x00F00000u;
    if (txPhase != TX_DMA) return;

    if ((flags & 0x00800000u) != 0u) {
        tx_finish(I2C_DMA_ERROR);
    } else if ((flags & 0x00200000u) != 0u) {
        DMA1_Channel6->CFGR &= ~0x0001u;
        txPhase = TX_WAIT_BTF;
        /* DMA 完成只说明最后一字节进入 DATAR；重新打开事件中断等 BTF。 */
        I2C1->CTLR2 = 0x0324u;

        /* 若 BTF 恰好已到，直接收尾，避免漏掉完成事件。 */
        uint16_t sr = I2C1->STAR1;
        if ((sr & 0x4F00u) != 0u) {
            tx_finish((sr & 0x0400u) != 0u ? I2C_NACK : I2C_BUS_ERROR);
        } else if ((sr & 0x0004u) != 0u) {
            tx_finish(I2C_OK);
        }
    }
}

/* 处理地址或数据阶段的 I2C 错误；NACK 和其他总线错误走同一完成回调。 */
void I2C1_ER_IRQHandler(void)
    __attribute__((interrupt("WCH-Interrupt-fast")));

void I2C1_ER_IRQHandler(void)
{
    /* 地址或数据阶段 NACK 都从这里通知上层；无需在发送函数里轮询。 */
    uint16_t sr = I2C1->STAR1;
    if ((sr & 0x4F00u) != 0u) {
        tx_finish((sr & 0x0400u) != 0u ? I2C_NACK : I2C_BUS_ERROR);
    }
}
