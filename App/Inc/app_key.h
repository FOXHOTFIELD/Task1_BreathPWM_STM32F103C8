/**
 * @file    app_key.h
 * @brief   按键处理（PA0 / EXTI0，发挥项 1：外部中断方式）
 *
 * 结构：中断里只登记"有下降沿发生"，消抖与业务处理放到 10ms 节拍里做，
 *       这样中断服务程序足够短，也避免了在中断里直接操作业务状态。
 */
#ifndef APP_KEY_H
#define APP_KEY_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>
#include "stm32f1xx_hal.h"

void key_init(void);

/** @brief 10ms 节拍：消抖 + 触发一次"暂停/继续"切换 */
void key_tick_10ms(void);

/** @brief 累计有效按键次数（INFO 显示，同时验证中断确实在工作） */
uint32_t key_get_press_count(void);

/** @brief 取并清除"刚刚发生了一次有效按键"的标志（主循环负责打印提示，避免在中断里发串口） */
uint8_t key_take_event(void);

/** @brief HAL 的 EXTI 回调（由 HAL_GPIO_EXTI_IRQHandler 调用） */
void HAL_GPIO_EXTI_Callback(uint16_t GPIO_Pin);

#ifdef __cplusplus
}
#endif

#endif /* APP_KEY_H */
