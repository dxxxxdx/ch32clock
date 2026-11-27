//
// Created by dxxdx on 2026/11/23.
//

#include "bootCfg.h"
#include "ch32v20x.h"
#include "BSP/clocktick.h"
#include "Component/EC11/EC11.h"
#include "Component/oled/oled.h"

/* G6U6 使用 USBD（0x40005C00），不是 USBFS（0x50000000）。 */
#define BOOT_USB_CNTR  (*(volatile uint32_t *)(uintptr_t)0x40005C40u)
#define BOOT_USB_ISTR  (*(volatile uint32_t *)(uintptr_t)0x40005C44u)
#define BOOT_USB_DADDR (*(volatile uint32_t *)(uintptr_t)0x40005C4Cu)
#define BOOT_USB_RESET_EVENT 0x0400u
#define BOOT_USB_DETACH_TICKS 4u /* 20 ms，保证主机看见断开。 */
#define BOOT_USB_PROBE_TICKS 200u /* 5 ms × 200 = 1 s。 */

uint8_t goBootloader = 0u;

/* 设备端接入 USB 后，主机在开始枚举时会发总线复位；只轮询这个事件。
 * 无论是否收到复位，退出前都断开 D+ 上拉并关闭 USBD。 */
static uint8_t boot_usb_host_started(void)
{
    uint8_t hostStarted = 0u;
    uint32_t startTick;

    EXTEN->EXTEN_CTR &= ~EXTEN_USBD_PU_EN;
    RCC->CFGR0 = (RCC->CFGR0 & ~0x00C00000u) | 0x00800000u; /* 144 MHz / 3 = 48 MHz。 */
    RCC->APB1PCENR |= RCC_USBEN;

    BOOT_USB_CNTR = 0x0001u; /* FRES：复位 USBD 控制器。 */
    BOOT_USB_CNTR = 0u;      /* 退出复位并使能 USB 模拟部分，不开 USB 中断。 */
    BOOT_USB_ISTR = 0u;      /* 清掉本地复位期间留下的状态。 */
    BOOT_USB_DADDR = 0x0080u; /* EF：启用地址 0 的设备功能。 */
    GPIOA->CFGHR = (GPIOA->CFGHR & ~0x000FF000u) | 0x00044000u; /* PA11/12 浮空输入。 */
    startTick = clockTick5ms;
    while ((uint32_t)(clockTick5ms - startTick) < BOOT_USB_DETACH_TICKS) { }
    EXTEN->EXTEN_CTR |= EXTEN_USBD_PU_EN; /* D+ 内部上拉，通知主机设备接入。 */

    startTick = clockTick5ms;
    do {
        if ((BOOT_USB_ISTR & BOOT_USB_RESET_EVENT) != 0u) {
            hostStarted = 1u;
            break;
        }
    } while ((uint32_t)(clockTick5ms - startTick) < BOOT_USB_PROBE_TICKS);

    EXTEN->EXTEN_CTR &= ~EXTEN_USBD_PU_EN; /* 先让主机看到设备断开。 */
    BOOT_USB_DADDR = 0u;
    BOOT_USB_CNTR = 0x0002u; /* PDWN：关闭 USBD 模拟部分。 */
    RCC->APB1PCENR &= ~RCC_USBEN;
    return hostStarted;
}

/* PA5 上拉，EC11 按下为低；仅当 USB 主机开始枚举才请求进 bootloader。 */
void checkBootloader(void)
{
    goBootloader = 0u;
    if ((GPIOA->INDR & 0x00000020u) != 0u) return; /* 未按键，不接入 USB。 */
    goBootloader = boot_usb_host_started() != 0u;
}

/* 预留真正切换到 WCH 系统 BootLoader 的实现。 */
static void jumpToWchBootloader(void)
{



    //网上找不到原厂直接能够兼容iap的
    //实在不想分区 只能以后再搓了
    while (1);






}

/* 显示启动模式并观察 1 秒；期间有新的 EC11 事件则返回正常程序。 */
void enterBootloader(void)
{
    oled_clearBuffer(&g_oled);
    oled_setString(&g_oled, 0u, "bootloader mode", 15u);
    oled_Flush(&g_oled);

    const uint8_t forwardAtStart = g_ec11.forwardCount;
    const uint8_t reverseAtStart = g_ec11.reverseCount;
    const uint8_t pressAtStart = g_ec11.pressCount;
    uint32_t startTick = clockTick5ms;
    while ((uint32_t)(clockTick5ms - startTick) < 200u) {
        OLED_Service(&g_oled);
        if (g_ec11.forwardCount != forwardAtStart ||
            g_ec11.reverseCount != reverseAtStart ||
            g_ec11.pressCount != pressAtStart) {
            goBootloader = 0u;
            return;
        }
    }

    jumpToWchBootloader();
    for (;;) { OLED_Service(&g_oled); }
}
