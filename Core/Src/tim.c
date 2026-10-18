/**
 * @file    tim.c
 * @brief   TIM3 = 呼吸灯 PWM（CH1 -> PA6，频率/占空比运行中可调）
 *          TIM4 = 10ms 控制节拍（呼吸步进 / 按键消抖 / 1Hz 状态）
 *
 * 定时器时钟说明：APB1 预分频为 2，但 TIM2/TIM3/TIM4 的计数时钟是 APB1 的 2 倍，
 * 即 72MHz。所以 PSC=71 时计数频率 1MHz，PWM 频率 = 1MHz / (ARR+1)。
 */
#include "main.h"
#include "peripherals.h"
#include "board_config.h"

TIM_HandleTypeDef htim3;
TIM_HandleTypeDef htim4;

/**
 * @brief  TIM3：
 *           BREATH_LED_MODE=0 时做硬件 PWM（CH1 → PA6）；
 *           其它模式用板载 PC13，它没有定时器通道，于是把 TIM3 改成软件 PWM
 *           的时基（25.6kHz 更新中断，128 级分辨率 → 默认 200Hz 载波）。
 */
void MX_TIM3_Init(void)
{
#if (BREATH_LED_MODE == 0)
    TIM_OC_InitTypeDef sConfigOC = {0};

    htim3.Instance               = TIM3;
    htim3.Init.Prescaler         = 71;
    htim3.Init.CounterMode       = TIM_COUNTERMODE_UP;
    htim3.Init.Period            = 999;
    htim3.Init.ClockDivision     = TIM_CLOCKDIVISION_DIV1;
    htim3.Init.AutoReloadPreload = TIM_AUTORELOAD_PRELOAD_ENABLE;

    if (HAL_TIM_PWM_Init(&htim3) != HAL_OK)
    {
        Error_Handler();
    }

    sConfigOC.OCMode     = TIM_OCMODE_PWM1;
    sConfigOC.Pulse      = 500;
    sConfigOC.OCPolarity = TIM_OCPOLARITY_HIGH;
    sConfigOC.OCFastMode = TIM_OCFAST_DISABLE;

    if (HAL_TIM_PWM_ConfigChannel(&htim3, &sConfigOC, BREATH_TIM_CH) != HAL_OK)
    {
        Error_Handler();
    }
#else
    /* 软件 PWM 时基：72MHz / (0+1) / (2812+1) = 25.6kHz，除 128 级 = 200Hz 载波 */
    htim3.Instance               = TIM3;
    htim3.Init.Prescaler         = 0;
    htim3.Init.CounterMode       = TIM_COUNTERMODE_UP;
    htim3.Init.Period            = 2812;
    htim3.Init.ClockDivision     = TIM_CLOCKDIVISION_DIV1;
    htim3.Init.AutoReloadPreload = TIM_AUTORELOAD_PRELOAD_ENABLE;

    if (HAL_TIM_Base_Init(&htim3) != HAL_OK)   /* 这里只借它的时钟与更新中断 */
    {
        Error_Handler();
    }
#endif
}

/**
 * @brief  TIM4 产生 10ms 更新中断（72MHz/72 = 1MHz，10000 计数 = 10ms）
 */
void MX_TIM4_Init(void)
{
    htim4.Instance               = TIM4;
    htim4.Init.Prescaler         = 71;
    htim4.Init.CounterMode       = TIM_COUNTERMODE_UP;
    htim4.Init.Period            = 9999;
    htim4.Init.ClockDivision     = TIM_CLOCKDIVISION_DIV1;
    htim4.Init.AutoReloadPreload = TIM_AUTORELOAD_PRELOAD_ENABLE;

    if (HAL_TIM_Base_Init(&htim4) != HAL_OK)
    {
        Error_Handler();
    }
}
