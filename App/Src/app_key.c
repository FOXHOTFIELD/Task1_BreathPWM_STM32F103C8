/**
 * @file    app_key.c
 * @brief   按键：EXTI 下降沿中断登记 + 10ms 节拍消抖（发挥项 1）
 *
 * 中断里只置标志，消抖和业务动作放在 10ms 节拍里完成；
 * 串口提示再由主循环打印，绝不在中断里做阻塞发送。
 */
#include "main.h"
#include "board_config.h"
#include "app_key.h"
#include "app_breath.h"

static volatile uint8_t  s_pending;     /* 中断登记：有下降沿 */

static volatile uint32_t s_count;       /* 有效按下次数 */
static volatile uint8_t  s_event;       /* 供主循环打印提示 */
static uint32_t          s_last_ms;     /* 上次有效按下的时间戳 */

void key_init(void)
{
    s_pending = 0U;
    s_event   = 0U;
    s_count   = 0U;
    s_last_ms = 0U;
}

/**
 * @brief HAL 的 EXTI 回调（由 EXTI0_IRQHandler -> HAL_GPIO_EXTI_IRQHandler 触发）
 */
void HAL_GPIO_EXTI_Callback(uint16_t GPIO_Pin)
{
    if (GPIO_Pin == KEY_PIN)
    {
        s_pending = 1U;
    }
}

void key_tick_10ms(void)
{
    uint32_t now;

    if (s_pending == 0U)
    {
        return;
    }
    s_pending = 0U;

    now = HAL_GetTick();
    /* 机械按键抖动通常 < 10ms，这里再做一次 30ms 的软件消抖 */
    if ((now - s_last_ms) < KEY_DEBOUNCE_MS)
    {
        return;
    }
    s_last_ms = now;
    s_count++;
    s_event = 1U;

    /* 一次按下 = 暂停/继续 切换 */
    breath_toggle();
}

uint32_t key_get_press_count(void)
{
    return s_count;
}

uint8_t key_take_event(void)
{
    uint8_t ev = s_event;
    s_event = 0U;
    return ev;
}
