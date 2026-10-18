/**
 * @file    main.c
 * @brief   25级软件第一期考核任务：STM32 基础 + 串口控制 PWM 呼吸灯
 * @target  STM32F103C8T6 @ 72MHz（HSE 8MHz，PLL x9）
 *
 * 启动流程：
 *   HAL_Init -> SystemClock_Config(72MHz) -> GPIO/UART1/TIM3/TIM4 初始化
 *   -> app_init()（业务层）-> 启动 PWM 输出与 10ms 控制节拍 -> 主循环
 */
#include "main.h"
#include "peripherals.h"
#include "board_config.h"
#include "app.h"

static void SystemClock_Config(void);

int main(void)
{
    /* 复位所有外设、初始化 Flash 接口与 SysTick（1ms 时基） */
    HAL_Init();

    /* HSE 8MHz -> PLL x9 -> SYSCLK 72MHz，APB1 = 36MHz（定时器时钟 x2 = 72MHz） */
    SystemClock_Config();

    MX_GPIO_Init();
    MX_USART1_UART_Init();
    MX_TIM3_Init();     /* 呼吸灯 PWM 通道，PA6 */
    MX_TIM4_Init();     /* 10ms 控制节拍 */

    /* 业务层初始化：状态灯、PWM 初值、呼吸状态机、按键、命令行、日志 */
    app_init();

    /* 启动 PWM 输出（占空比由上一步写入的 CCR 决定） */
#if (BREATH_LED_MODE == 0)
    HAL_TIM_PWM_Start(BREATH_TIM, BREATH_TIM_CH);
#else
    /* 板载 LED 方案：TIM3 的更新中断已在 app_pwm_init() 里直接启动，
       这里不能再调 HAL_TIM_PWM_Start（没有配置 PWM 通道） */
#endif

    /* 启动 10ms 控制节拍：呼吸步进、按键消抖、1Hz 状态灯与状态上报 */
    HAL_TIM_Base_Start_IT(TICK_TIM);

    while (1)
    {
        app_loop();
    }
}

/**
 * @brief 系统时钟配置：HSE 8MHz x PLL9 = 72MHz
 * @note  APB1 二分频 = 36MHz，但 TIM2/TIM3/TIM4 的计数时钟仍为 72MHz，
 *        因此 PWM 频率计算中定时器时钟按 72MHz 代入。
 */
static void SystemClock_Config(void)
{
    RCC_OscInitTypeDef RCC_OscInitStruct = {0};
    RCC_ClkInitTypeDef RCC_ClkInitStruct = {0};

    RCC_OscInitStruct.OscillatorType      = RCC_OSCILLATORTYPE_HSE;
    RCC_OscInitStruct.HSEState            = RCC_HSE_ON;
    RCC_OscInitStruct.HSEPredivValue      = RCC_HSE_PREDIV_DIV1;
    RCC_OscInitStruct.HSIState            = RCC_HSI_ON;
    RCC_OscInitStruct.PLL.PLLState        = RCC_PLL_ON;
    RCC_OscInitStruct.PLL.PLLSource       = RCC_PLLSOURCE_HSE;
    RCC_OscInitStruct.PLL.PLLMUL          = RCC_PLL_MUL9;
    if (HAL_RCC_OscConfig(&RCC_OscInitStruct) != HAL_OK)
    {
        Error_Handler();
    }

    RCC_ClkInitStruct.ClockType      = RCC_CLOCKTYPE_HCLK | RCC_CLOCKTYPE_SYSCLK
                                     | RCC_CLOCKTYPE_PCLK1 | RCC_CLOCKTYPE_PCLK2;
    RCC_ClkInitStruct.SYSCLKSource   = RCC_SYSCLKSOURCE_PLLCLK;
    RCC_ClkInitStruct.AHBCLKDivider  = RCC_SYSCLK_DIV1;
    RCC_ClkInitStruct.APB1CLKDivider = RCC_HCLK_DIV2;
    RCC_ClkInitStruct.APB2CLKDivider = RCC_HCLK_DIV1;

    if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_2) != HAL_OK)
    {
        Error_Handler();
    }
}

/**
 * @brief 致命错误：关闭中断并原地等待（调试器可在此处停下查看调用栈）
 */
void Error_Handler(void)
{
    __disable_irq();
    while (1)
    {
    }
}

#ifdef  USE_FULL_ASSERT
void assert_failed(uint8_t *file, uint32_t line)
{
    (void)file;
    (void)line;
    Error_Handler();
}
#endif
