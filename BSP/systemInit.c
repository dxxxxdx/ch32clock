//
// Created by dxxdx on 2026/11/23.
//

#include "systemInit.h"
#include <stdint.h>
#include "system_ch32v20x.h"
#include "GPIO.h"
#include "I2C.h"
#include "IRQ.h"
#include "clocktick.h"
#include "TIM.h"

/* 板级初始化入口：系统时钟 → GPIO/TIM 时钟 → 5 ms 时基 → I2C DMA → 外设中断。
 * 启动汇编直接进入 main，故 SystemInit 在这里调用一次即可。 */
void BSP_SystemInit(void)
{
    SystemInit();          /* HSE 8 MHz → 系统时钟 144 MHz */
    BSP_GPIO_Init();       /* PA5~7 旋钮输入、TIM2/3 时钟，APB1 分频到 36 MHz */
    TIM_Init();            /* TIM3_CH4 就绪，PB1 默认低电平。 */
    clockTick5ms_Init();   /* SysTick 每 5 ms 中断一次 */

    BSP_DMA_I2C_Init();    /* I2C1 400 kHz、DMA1 Channel6 */
    /* 上电先探测 OLED：7 位地址 0x3C，线上写地址 0x78；结果不阻断启动。 */
    (void)BSP_I2C_IsReady(0x78U);
    BSP_IRQ_Init();        /* DMA、I2C 和 PA5/6/7 的中断 */
}




