//
// Created by dxxdx on 2026/11/23.
//

#ifndef CH32CLOCK_EC11_H
#define CH32CLOCK_EC11_H

#include <stdint.h>

typedef struct {
    uint8_t (*const readButton)(void);
    uint32_t (*const lock)(void);
    void (*const unlock)(uint32_t previous);
} EC11Hardware;

/* 中断产生的事件计数；主循环取走后清零，重复事件不会被单个标志位合并。
 * 主循环读取并清零计数时，应暂时屏蔽 EC11 的 EXTI 中断。 */
typedef struct EC11 {
    const EC11Hardware *const hardware;
    volatile uint8_t forwardCount;   /* 正转事件数。 */
    volatile uint8_t reverseCount;   /* 反转事件数。 */
    volatile uint8_t pressCount;     /* 按下事件数，松开不计。 */
    uint8_t hasLastEventTick;        /* 首次记录前为 0，避免第 0 个节拍被误挡。 */
    uint32_t lastEventTick5ms;       /* 上一次记录事件时的 clockTick5ms 值。 */

} EC11;

/* EXTI9_5 中断直接累加事件；业务层可读取这些计数。 */
extern EC11 g_ec11;

typedef struct {
    int16_t rotation;
    uint8_t buttonDown;
} EC11Input;

/* 在同一临界区取走计数和采样按键；恢复进入前的中断状态。 */
void EC11_Read(EC11 *self, EC11Input *input);

#endif //CH32CLOCK_EC11_H
