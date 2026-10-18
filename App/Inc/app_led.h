/**
 * @file    app_led.h
 * @brief   状态灯：PC13，每 500ms 翻转一次（1Hz 闪烁）
 */
#ifndef APP_LED_H
#define APP_LED_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>

void led_init(void);
void led_tick_10ms(void);   /**< 由 10ms 节拍调用 */
void led_toggle(void);

#ifdef __cplusplus
}
#endif

#endif /* APP_LED_H */
