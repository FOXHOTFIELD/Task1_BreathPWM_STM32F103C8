/**
 * @file    stm32f1xx_it.c
 * @brief   中断服务函数：只做最轻的处理，把实际工作留给回调/主循环
 */
#include "main.h"
#include "stm32f1xx_it.h"
#include "board_config.h"
#include "app_pwm.h"

/* 各外设句柄定义在 gpio.c/usart.c/tim.c 中 */
extern UART_HandleTypeDef huart1;
extern TIM_HandleTypeDef  htim4;

/* ---------------- Cortex-M3 内核异常 ---------------- */
void NMI_Handler(void)
{
    while (1)
    {
    }
}

void HardFault_Handler(void)
{
    while (1)
    {
    }
}

void MemManage_Handler(void)
{
    while (1)
    {
    }
}

void BusFault_Handler(void)
{
    while (1)
    {
    }
}

void UsageFault_Handler(void)
{
    while (1)
    {
    }
}

void SVC_Handler(void)
{
}

void DebugMon_Handler(void)
{
}

void PendSV_Handler(void)
{
}

/**
 * @brief SysTick 1ms 中断：给 HAL_Delay/HAL_GetTick 提供时基
 */
void SysTick_Handler(void)
{
    HAL_IncTick();
}

/**
 * @brief 按键外部中断（PA0）：HAL 会转到 HAL_GPIO_EXTI_Callback()
 */
void EXTI0_IRQHandler(void)
{
    HAL_GPIO_EXTI_IRQHandler(KEY_PIN);
}

/**
 * @brief 10ms 控制节拍中断：HAL 会转到 HAL_TIM_PeriodElapsedCallback()
 */
void TIM4_IRQHandler(void)
{
    HAL_TIM_IRQHandler(TICK_TIM);
}

/**
 * @brief 串口中断：接收按 1 字节中断方式工作，回调在 app_cli.c
 */
void USART1_IRQHandler(void)
{
    HAL_UART_IRQHandler(&huart1);
}

#if (BREATH_LED_MODE != 0)
/**
 * @brief 软件 PWM 的更新中断（板载 PC13 方案）
 * @note  25kHz 级别的中断，故意不走 HAL_TIM_IRQHandler：
 *        HAL 在中断里要做一串状态机判断，开销太大；这里只清标志 + 翻转一个 GPIO。
 */
void TIM3_IRQHandler(void)
{
    if ((TIM3->SR & TIM_SR_UIF) != 0U)
    {
        TIM3->SR = (uint16_t)~TIM_SR_UIF;
        app_pwm_sw_tick();
    }
}
#endif
