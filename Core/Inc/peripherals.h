/**
 * @file    peripherals.h
 * @brief   外设初始化函数声明（与 CubeMX 生成的 MX_xxx_Init 命名保持一致）
 */
#ifndef __PERIPHERALS_H
#define __PERIPHERALS_H

#ifdef __cplusplus
extern "C" {
#endif

void MX_GPIO_Init(void);
void MX_USART1_UART_Init(void);
void MX_TIM3_Init(void);   /* 呼吸灯 PWM：TIM3_CH1 = PA6 */
void MX_TIM4_Init(void);   /* 控制节拍：10ms 定时中断 */

#ifdef __cplusplus
}
#endif

#endif /* __PERIPHERALS_H */
