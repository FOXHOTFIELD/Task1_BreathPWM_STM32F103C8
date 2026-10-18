/**
 * @file    app_led.c
 * @brief   1Hz 状态灯，输出脚由 board_config.h 的 BREATH_LED_MODE 决定：
 *            mode 0 → PC13（板载，低电平点亮）
 *            mode 1 → PA6（GPIO 推挽，外接 LED：正极经 220Ω 到 3V3，负极接 PA6）
 *            mode 2 → 不输出（板载灯已被软件 PWM 呼吸占用，且没有第二颗灯）
 */
#include "main.h"
#include "board_config.h"
#include "app_led.h"

#define LED_HALF_TICKS   (500U / TICK_PERIOD_MS)   /* 500ms / 10ms = 50 拍 */

#if (BREATH_LED_MODE == 0)
#define BLINK_PORT       LED1_PORT
#define BLINK_PIN        LED1_PIN
#define BLINK_ON         LED1_ON_LEVEL
#define BLINK_OFF        LED1_OFF_LEVEL
#elif (BREATH_LED_MODE == 1)
#define BLINK_PORT       GPIOA
#define BLINK_PIN        GPIO_PIN_6
#define BLINK_ON         GPIO_PIN_RESET
#define BLINK_OFF        GPIO_PIN_SET
#endif

#if (BREATH_LED_MODE != 2)
static uint16_t s_ticks;
static uint8_t  s_on;
#endif

void led_init(void)
{
#if (BREATH_LED_MODE != 2)
    s_ticks = 0U;
    s_on    = 0U;
    HAL_GPIO_WritePin(BLINK_PORT, BLINK_PIN, BLINK_OFF);
#endif
}

void led_tick_10ms(void)
{
#if (BREATH_LED_MODE != 2)
    if (++s_ticks < LED_HALF_TICKS)
    {
        return;
    }
    s_ticks = 0U;
    s_on    = (uint8_t)(!s_on);
    HAL_GPIO_WritePin(BLINK_PORT, BLINK_PIN, s_on ? BLINK_ON : BLINK_OFF);
#endif
}

void led_toggle(void)
{
#if (BREATH_LED_MODE != 2)
    s_on = (uint8_t)(!s_on);
    HAL_GPIO_WritePin(BLINK_PORT, BLINK_PIN, s_on ? BLINK_ON : BLINK_OFF);
#endif
}
