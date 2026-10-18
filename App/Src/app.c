/**
 * @file    app.c
 * @brief   业务总调度：初始化顺序、10ms 节拍、主循环
 */
#include "main.h"
#include "board_config.h"
#include "app.h"
#include "app_led.h"
#include "app_pwm.h"
#include "app_breath.h"
#include "app_key.h"
#include "app_cli.h"
#include "app_log.h"

#define TICKS_PER_SECOND   (1000U / TICK_PERIOD_MS)

static volatile uint8_t s_stat_due;
static uint16_t         s_ticks_1s;

void app_init(void)
{
    led_init();

    /* PWM 先按"最暗"起步，呼吸状态机第一步会立刻接管 */
    if (app_pwm_init(PWM_FREQ_DEFAULT_HZ, 0U) != HAL_OK)
    {
        Error_Handler();
    }
    breath_init();

    key_init();

    /* 日志与命令行：先横幅，再开始收命令 */
    log_init();
    cli_init();
}

/**
 * @brief TIM4 的 10ms 更新中断 -> 这里做所有周期任务（中断上下文）
 * @note  只做寄存器级操作和标志更新，不允许阻塞等待
 */
void app_tick_10ms(void)
{
    breath_tick_10ms();
    key_tick_10ms();
    led_tick_10ms();

    if (++s_ticks_1s >= TICKS_PER_SECOND)
    {
        s_ticks_1s = 0U;
        s_stat_due = 1U;
    }
}

void app_loop(void)
{
    cli_process();

    /* 按键事件提示（发挥项 1：证明按键是走外部中断的） */
    if (key_take_event() != 0U)
    {
        logln("[KEY] IRQ #%lu -> state=%s (duty frozen at %lu/1000)",
              (unsigned long)key_get_press_count(),
              breath_is_running() ? "RUN" : "HOLD",
              (unsigned long)breath_get_duty_pm());
    }

    /* 1Hz 状态上报（发挥项 2） */
    if (s_stat_due != 0U)
    {
        s_stat_due = 0U;
        if (cli_status_enabled() != 0U)
        {
            cli_print_status();
        }
    }
}

/**
 * @brief HAL 定时器周期回调：由 TIM4_IRQHandler -> HAL_TIM_IRQHandler 触发
 */
void HAL_TIM_PeriodElapsedCallback(TIM_HandleTypeDef *htim)
{
    if (htim->Instance == TIM4)
    {
        app_tick_10ms();
    }
}
