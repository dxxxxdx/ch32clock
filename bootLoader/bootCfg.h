//
// Created by dxxdx on 2026/11/23.
//

#ifndef CH32CLOCK_BOOTCFG_H
#define CH32CLOCK_BOOTCFG_H
#include <stdint.h>

/* 上电时 EC11 按下且 USB 主机发来总线复位后置 1，供后续入口选择使用。 */
extern uint8_t goBootloader;

void checkBootloader(void);
/* 显示 bootloadermode；内部真正的 ROM 跳转入口目前留空。 */
void enterBootloader(void);
#endif //CH32CLOCK_BOOTCFG_H
