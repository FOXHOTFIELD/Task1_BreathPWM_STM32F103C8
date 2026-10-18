/**
 * @file    board_config.h
 * @brief   板级配置集中处：改引脚/范围只改这一个文件
 *
 * 接线表（STM32F103C8T6 最小系统板）
 * ------------------------------------------------------------------
 *  功能        引脚      方向/外设          说明
 *  状态灯      PC13      GPIO 推挽         板载 LED，低电平点亮，1Hz 翻转
 *  呼吸灯      PA6       TIM3_CH1 PWM      LED 正极 -> PA6，负极 -> 220Ω -> GND
 *  按键        PA0       EXTI0 下降沿      按键一端接 PA0，另一端接 GND，内部上拉
 *  串口 TX     PA9       USART1_TX         接 CH340/ST-Link 的 RX
 *  串口 RX     PA10      USART1_RX         接 CH340/ST-Link 的 TX
 *  GND         任意 GND  -                 与串口模块共地
 * ------------------------------------------------------------------
 */
#ifndef BOARD_CONFIG_H
#define BOARD_CONFIG_H

#ifdef __cplusplus
extern "C" {
#endif

#include "stm32f1xx_hal.h"

/* ================= 时钟 ================= */
#define BOARD_SYSCLK_HZ          72000000UL   /* HSE 8MHz x PLL9 */
#define BOARD_HSE_HZ             8000000UL
#define BOARD_TIM_CLK_HZ         72000000UL   /* TIM2/3/4 计数时钟 */

/* ================= LED 分配方案（三选一） =================
 * 背景：PC13 没有任何定时器复用通道（F103 的 TIM3 重映射只到 PC6..PC9），
 *       所以板载 LED 做不了硬件 PWM，只能用"定时器更新中断 + 软件翻转 GPIO"模拟；
 *       而且板子上只有一颗 LED，"1Hz 闪烁"和"呼吸"没法同时占同一个脚。
 *
 * 0 = 呼吸灯 = PA6（TIM3_CH1 硬件 PWM，默认方案）
 *     1Hz 状态灯 = PC13（板载）
 *     接线：LED 正极 → 220Ω → PA6，负极 → GND
 *
 * 1 = 呼吸灯 = PC13（板载，软件 PWM：TIM3 25.6kHz 中断、128 级分辨率）
 *     1Hz 状态灯 = PA6（GPIO 推挽，低电平点亮）
 *     接线：LED 正极 → 220Ω → 3V3，负极 → PA6
 *     代价：CPU 约 3%~5%；亮度只有 128 级（1/128 ≈ 0.8%）台阶；
 *           波形由 CPU 翻转产生，边沿有中断延迟抖动，严格说不属于"定时器硬件输出的 PWM"
 *
 * 2 = 呼吸灯 = PC13（软件 PWM，同上）
 *     不输出 1Hz 状态灯（串口的每秒 STAT 行仍然保留）
 *     适用于完全没有第二颗 LED：此时评分点 1 的"LED 每秒翻转"要单独向验收说明取舍
 */
#define BREATH_LED_MODE          0

/* ================= 状态灯 LED1（板载 PC13） ================= */
#define LED1_PORT                GPIOC
#define LED1_PIN                 GPIO_PIN_13
#define LED1_ON_LEVEL            GPIO_PIN_RESET   /* 低电平点亮 */
#define LED1_OFF_LEVEL           GPIO_PIN_SET

/* ================= 呼吸灯 LED2（TIM3_CH1 = PA6） ================= */
#define BREATH_PORT              GPIOA
#define BREATH_PIN               GPIO_PIN_6
#define BREATH_TIM_CH            TIM_CHANNEL_1

/* ================= 按键 KEY1（PA0 / EXTI0） ================= */
#define KEY_PORT                  GPIOA
#define KEY_PIN                   GPIO_PIN_0
#define KEY_IRQn                  EXTI0_IRQn
#define KEY_DEBOUNCE_MS           30U

/* ================= 控制节拍 TIM4 ================= */
#define TICK_PERIOD_MS            10U

/* ================= 业务参数范围 ================= */
#if (BREATH_LED_MODE == 0)
/* 硬件 PWM：1Hz ~ 10kHz，占空比分辨率由 ARR 自动保证 */
#define PWM_FREQ_MIN_HZ           1UL
#define PWM_FREQ_MAX_HZ           10000UL
#else
/* 软件 PWM：载波 = 更新中断频率 / 128，中断太密会吃掉 CPU，
   因此把可用载波限制在 20..500Hz（对应中断 2.56k..64kHz） */
#define PWM_FREQ_MIN_HZ           20UL
#define PWM_FREQ_MAX_HZ           500UL
#endif
#define PWM_FREQ_DEFAULT_HZ       500UL
#define PWM_DUTY_DEFAULT_PM       500U        /* 千分比 500 = 50.0% */

#define BREATH_PERIOD_MIN_MS      200U        /* 一次"暗->亮->暗"的最短时间 */
#define BREATH_PERIOD_MAX_MS      10000UL
#define BREATH_PERIOD_DEFAULT_MS  2400U

/* ================= 外设句柄（定义在 gpio.c/usart.c/tim.c） ================= */
extern UART_HandleTypeDef huart1;
extern TIM_HandleTypeDef  htim3;   /* 呼吸灯 PWM */
extern TIM_HandleTypeDef  htim4;   /* 10ms 控制节拍 */

#define BREATH_TIM                (&htim3)
#define TICK_TIM                  (&htim4)

#ifdef __cplusplus
}
#endif

#endif /* BOARD_CONFIG_H */
