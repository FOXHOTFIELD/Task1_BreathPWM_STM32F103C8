/**
 * @file    app_breath.h
 * @brief   呼吸灯状态机（评分点 3 + 4 + 发挥项 3）
 *
 * 设计要点：
 *   - 相位累加器 s_phase 在 0..2000 上做三角波扫描，一次完整呼吸 = 2000 相位步；
 *   - 10ms 控制节拍推进相位，占空比 = f(相位)，f 可选线性或 gamma 校正；
 *   - 暂停（STOP/按键）时只冻结相位、不改变 CCR，因此亮度停在当前占空比；
 *   - 继续（RUN/按键）时直接从冻结的相位接着走，不会跳变回 0。
 *
 * 为什么需要 gamma 校正（发挥项）：人眼对亮度的感知近似幂次 2.2 关系，
 * 直接按线性占空比扫描时会觉得"亮的时间长、暗得太快"。把线性相位先经
 * y = x^2.2 映射再送 PWM，视觉上才是一呼一吸均匀的呼吸效果。
 */
#ifndef APP_BREATH_H
#define APP_BREATH_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>

typedef enum
{
    BREATH_STATE_HOLD = 0,   /**< 暂停/手动保持：占空比冻结 */
    BREATH_STATE_RUN  = 1    /**< 自动呼吸 */
} breath_state_t;

typedef enum
{
    BREATH_MODE_LINEAR = 0,  /**< 线性扫描（示波器上看占空比是三角波） */
    BREATH_MODE_GAMMA  = 1   /**< gamma 2.2 视觉线性化（默认） */
} breath_mode_t;

/** @brief 初始化：进入 RUN + GAMMA，起始相位 0（最暗） */
void breath_init(void);

/** @brief 10ms 节拍：推进相位并更新占空比（中断上下文） */
void breath_tick_10ms(void);

/** @brief 按键/命令切换：RUN <-> HOLD */
void breath_toggle(void);

/** @brief 显式设置运行状态：1 = 继续呼吸（从冻结相位恢复），0 = 暂停保持 */
void breath_set_running(uint8_t run);

/** @brief 当前是否在自动呼吸 */
uint8_t breath_is_running(void);

/** @brief 设置曲线模式 */
void breath_set_mode(breath_mode_t mode);

/** @brief 取曲线模式 */
breath_mode_t breath_get_mode(void);

/** @brief 设置一次完整呼吸的周期（ms） */
void breath_set_period_ms(uint32_t period_ms);

/** @brief 取呼吸周期（ms） */
uint32_t breath_get_period_ms(void);

/** @brief 手动把占空比定在指定值并进入 HOLD（串口 D= 命令使用） */
void breath_hold_at_duty(uint16_t duty_pm);

/** @brief 当前相位 0..2000（INFO 命令用） */
uint16_t breath_get_phase(void);

/** @brief 当前占空比（千分比） */
uint16_t breath_get_duty_pm(void);

#ifdef __cplusplus
}
#endif

#endif /* APP_BREATH_H */
