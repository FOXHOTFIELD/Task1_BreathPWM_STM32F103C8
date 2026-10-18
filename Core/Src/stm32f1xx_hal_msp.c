/**
 * @file    stm32f1xx_hal_msp.c
 * @brief   HAL 底层初始化回调：时钟使能、引脚复用、中断优先级
 */
#include "main.h"
#include "board_config.h"

/**
 * @brief 全局 MSP：使能 AFIO/PWR 时钟，保留 SWD 调试口（关闭 JTAG 释放 PB3/PB4/PA15）
 */
void HAL_MspInit(void)
{
    __HAL_RCC_AFIO_CLK_ENABLE();
    __HAL_RCC_PWR_CLK_ENABLE();

    /* 只用 SWD 调试：释放 PB3/PB4/PA15 作普通 IO（呼吸灯/调试时不会冲突） */
    __HAL_AFIO_REMAP_SWJ_NOJTAG();
}

/**
 * @brief USART1 底层：时钟、TX/RX 引脚、NVIC
 */
void HAL_UART_MspInit(UART_HandleTypeDef *huart)
{
    GPIO_InitTypeDef GPIO_InitStruct = {0};

    if (huart->Instance == USART1)
    {
        __HAL_RCC_USART1_CLK_ENABLE();
        __HAL_RCC_GPIOA_CLK_ENABLE();

        /* PA9 = USART1_TX，复用推挽输出 */
        GPIO_InitStruct.Pin   = GPIO_PIN_9;
        GPIO_InitStruct.Mode  = GPIO_MODE_AF_PP;
        GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_HIGH;
        HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);

        /* PA10 = USART1_RX，上拉输入（模块未接时不会悬空误触发） */
        GPIO_InitStruct.Pin  = GPIO_PIN_10;
        GPIO_InitStruct.Mode = GPIO_MODE_INPUT;
        GPIO_InitStruct.Pull = GPIO_PULLUP;
        HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);

        HAL_NVIC_SetPriority(USART1_IRQn, 3, 0);
        HAL_NVIC_EnableIRQ(USART1_IRQn);
    }
}

/**
 * @brief TIM4（10ms 控制节拍）底层：时钟 + 更新中断
 */
void HAL_TIM_Base_MspInit(TIM_HandleTypeDef *htim_base)
{
    if (htim_base->Instance == TIM4)
    {
        __HAL_RCC_TIM4_CLK_ENABLE();

        HAL_NVIC_SetPriority(TIM4_IRQn, 2, 0);
        HAL_NVIC_EnableIRQ(TIM4_IRQn);
    }
#if (BREATH_LED_MODE != 0)
    /* 板载 PC13 没有定时器通道：TIM3 改为软件 PWM 时基，需要它的更新中断 */
    else if (htim_base->Instance == TIM3)
    {
        __HAL_RCC_TIM3_CLK_ENABLE();

        HAL_NVIC_SetPriority(TIM3_IRQn, 3, 0);
        HAL_NVIC_EnableIRQ(TIM3_IRQn);
    }
#endif
}

/**
 * @brief TIM3（PWM）底层：时钟使能 + 通道 1 引脚复用
 *
 * 注意：STM32F1 HAL（v1.1.10）**没有** HAL_TIM_PWM_MspPostInit 这个钩子
 * （那是 L4/G4 等系列才有的），PWM 引脚必须在这里配置，
 * 否则 PA6 停在默认输入态，示波器上看不到任何波形。
 */
void HAL_TIM_PWM_MspInit(TIM_HandleTypeDef *htim_pwm)
{
#if (BREATH_LED_MODE == 0)
    GPIO_InitTypeDef GPIO_InitStruct = {0};

    if (htim_pwm->Instance == TIM3)
    {
        __HAL_RCC_TIM3_CLK_ENABLE();
        __HAL_RCC_GPIOA_CLK_ENABLE();

        /* PA6 = TIM3_CH1，复用推挽输出 */
        GPIO_InitStruct.Pin   = BREATH_PIN;
        GPIO_InitStruct.Mode  = GPIO_MODE_AF_PP;
        GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_HIGH;
        HAL_GPIO_Init(BREATH_PORT, &GPIO_InitStruct);
    }
#else
    /* 软件 PWM 方案不用 PWM 通道，PA6 的用途见 gpio.c */
    (void)htim_pwm;
#endif
}
