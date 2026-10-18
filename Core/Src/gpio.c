/**
 * @file    gpio.c
 * @brief   GPIO 初始化：LED、按键（EXTI0）
 *
 * PC13 在所有方案下都配成输出：
 *   mode 0 → 做 1Hz 状态灯
 *   mode 1/2 → 做软件 PWM 的呼吸灯（TIM3 更新中断里翻转它）
 * PA6 只在 mode 1 下作为 1Hz 状态灯（GPIO 推挽）；mode 0 时由 PWM 的
 *   HAL_TIM_PWM_MspInit 配成复用推挽，mode 2 时不用。
 */
#include "main.h"
#include "peripherals.h"
#include "board_config.h"

void MX_GPIO_Init(void)
{
    GPIO_InitTypeDef GPIO_InitStruct = {0};

    __HAL_RCC_GPIOA_CLK_ENABLE();
    __HAL_RCC_GPIOC_CLK_ENABLE();
    __HAL_RCC_AFIO_CLK_ENABLE();

    /* ---------- PC13：板载 LED，低电平点亮 ---------- */
    HAL_GPIO_WritePin(LED1_PORT, LED1_PIN, LED1_OFF_LEVEL);
    GPIO_InitStruct.Pin   = LED1_PIN;
    GPIO_InitStruct.Mode  = GPIO_MODE_OUTPUT_PP;
    GPIO_InitStruct.Pull  = GPIO_NOPULL;
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;   /* PC13 属弱驱动引脚，不需要高速 */
    HAL_GPIO_Init(LED1_PORT, &GPIO_InitStruct);

#if (BREATH_LED_MODE == 1)
    /* ---------- PA6：mode 1 下当 1Hz 状态灯（外接 LED，低电平点亮） ---------- */
    HAL_GPIO_WritePin(GPIOA, GPIO_PIN_6, GPIO_PIN_SET);
    GPIO_InitStruct.Pin   = GPIO_PIN_6;
    GPIO_InitStruct.Mode  = GPIO_MODE_OUTPUT_PP;
    GPIO_InitStruct.Pull  = GPIO_NOPULL;
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
    HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);
#endif

    /* ---------- KEY1：PA0，下降沿触发外部中断（发挥项 1） ---------- */
    /* 内部上拉，按键另一端接 GND：松开为高，按下为低 -> 下降沿 */
    GPIO_InitStruct.Pin  = KEY_PIN;
    GPIO_InitStruct.Mode = GPIO_MODE_IT_FALLING;
    GPIO_InitStruct.Pull = GPIO_PULLUP;
    HAL_GPIO_Init(KEY_PORT, &GPIO_InitStruct);

    HAL_NVIC_SetPriority(KEY_IRQn, 3, 0);
    HAL_NVIC_EnableIRQ(KEY_IRQn);
}
