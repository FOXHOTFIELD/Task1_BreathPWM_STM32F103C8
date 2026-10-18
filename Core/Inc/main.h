/**
 * @file    main.h
 * @brief   25级软件第一期考核任务 —— PWM 呼吸灯 + 串口命令控制
 * @target  STM32F103C8T6 最小系统板, HSE 8MHz, SYSCLK 72MHz
 * @note    本工程按 CubeMX + HAL 库的组织方式编写：Core/ 层只放外设初始化，
 *          全部业务逻辑放在 App/ 层，便于用 CubeMX 重新生成代码时不丢业务。
 */
#ifndef __MAIN_H
#define __MAIN_H

#ifdef __cplusplus
extern "C" {
#endif

#include "stm32f1xx_hal.h"

/**
 * @brief 致命错误处理：关中断后停在此处，便于调试器定位
 */
void Error_Handler(void);

#ifdef __cplusplus
}
#endif

#endif /* __MAIN_H */
