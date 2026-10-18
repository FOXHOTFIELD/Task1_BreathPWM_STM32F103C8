/**
 * @file    app_pwm.h
 * @brief   TIM3_CH1 PWM 的封装：频率/占空比可在运行中修改，且改频率时保持占空比
 *
 * 频率计算公式（TIM 计数时钟 72MHz）：
 *   计数分频  psc     = ceil(72MHz / (freq * 65536)) - 1      // 保证 ARR 不溢出 16 位
 *   计数时钟  tick_hz = 72MHz / (psc + 1)
 *   计数周期  arr     = round(tick_hz / freq) - 1
 *   比较值    ccr     = round((arr + 1) * duty_pm / 1000)
 */
#ifndef APP_PWM_H
#define APP_PWM_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>
#include "stm32f1xx_hal.h"

typedef struct
{
    uint32_t freq_hz;    /**< 设定频率 */
    uint16_t duty_pm;    /**< 设定占空比，千分比（500 = 50.0%） */
    uint32_t psc;        /**< 实际生效的分频寄存器值 */
    uint32_t arr;        /**< 实际生效的自动重装值 */
    uint32_t ccr;        /**< 实际生效的比较值 */
    uint32_t actual_hz;  /**< 由 psc/arr 反算的真实频率（有取整误差） */
} pwm_state_t;

/** @brief 初始化：按给定时频率/占空比配置并保存状态 */
HAL_StatusTypeDef app_pwm_init(uint32_t freq_hz, uint16_t duty_pm);

/** @brief 修改频率（保持当前占空比不变），越界返回 HAL_ERROR */
HAL_StatusTypeDef app_pwm_set_frequency(uint32_t freq_hz);

/** @brief 修改占空比（千分比 0..1000） */
HAL_StatusTypeDef app_pwm_set_duty_pm(uint16_t duty_pm);

/** @brief 取当前状态副本 */
void app_pwm_get_state(pwm_state_t *st);

/**
 * @brief 软件 PWM 的一个节拍（仅 BREATH_LED_MODE != 0 时有效）
 * @note  由 TIM3 更新中断调用；在中断里翻转 PC13，配合 128 级计数形成 PWM。
 *        默认方案（mode 0，硬件 PWM）下本函数不存在，也不需要调用。
 */
void app_pwm_sw_tick(void);

#ifdef __cplusplus
}
#endif

#endif /* APP_PWM_H */
