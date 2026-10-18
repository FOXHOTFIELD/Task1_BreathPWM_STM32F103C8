/**
 * @file    stm32f1xx_it.h
 * @brief   中断服务函数声明
 */
#ifndef __STM32F1xx_IT_H
#define __STM32F1xx_IT_H

#ifdef __cplusplus
extern "C" {
#endif

void NMI_Handler(void);
void HardFault_Handler(void);
void MemManage_Handler(void);
void BusFault_Handler(void);
void UsageFault_Handler(void);
void SVC_Handler(void);
void DebugMon_Handler(void);
void PendSV_Handler(void);
void SysTick_Handler(void);
void EXTI0_IRQHandler(void);    /* 按键 PA0 外部中断 */
void TIM4_IRQHandler(void);     /* 10ms 控制节拍 */
void USART1_IRQHandler(void);   /* 串口收发中断 */

#ifdef __cplusplus
}
#endif

#endif /* __STM32F1xx_IT_H */
