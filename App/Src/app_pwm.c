/**
 * @file    app_pwm.c
 * @brief   TIM3_CH1 PWM 频率/占空比控制（评分点 3、5）
 *
 * 核心是「改频率时不能丢占空比」：
 *   1. 按目标频率重新算 PSC/ARR；
 *   2. 用同一个 duty_pm 重新算 CCR；
 *   3. 三个寄存器一起写，并置 EGR.UG 让影子寄存器立刻生效，
 *      否则新 PSC/ARR 要等到下一次更新事件才生效，中间会出现一帧错误占空比。
 */
#include "main.h"
#include "board_config.h"
#include "app_pwm.h"

#include "stm32f1xx_hal.h"

#if (BREATH_LED_MODE == 0)

static pwm_state_t s_pwm;

/**
 * @brief 由频率反算 PSC/ARR，并给出真实频率
 * @note  PSC 取使 ARR 不超过 16 位的最小分频，这样占空比分辨率最高
 */
static HAL_StatusTypeDef pwm_calc(uint32_t freq_hz, uint32_t *psc, uint32_t *arr, uint32_t *actual_hz)
{
    uint64_t den;
    uint64_t need;
    uint32_t tick_hz;
    uint32_t period;

    if (freq_hz < PWM_FREQ_MIN_HZ || freq_hz > PWM_FREQ_MAX_HZ)
    {
        return HAL_ERROR;
    }

    /* psc = ceil(72MHz / (freq * 65536)) - 1，保证计数周期不超过 65536 */
    den  = (uint64_t)freq_hz * 65536ULL;
    need = ((uint64_t)BOARD_TIM_CLK_HZ + den - 1ULL) / den;
    if (need == 0ULL)
    {
        need = 1ULL;
    }
    if (need > 65536ULL)
    {
        return HAL_ERROR;
    }
    *psc = (uint32_t)(need - 1ULL);

    tick_hz = BOARD_TIM_CLK_HZ / (*psc + 1U);
    period  = (tick_hz + freq_hz / 2U) / freq_hz;      /* 四舍五入 */
    if (period < 2U)
    {
        period = 2U;
    }
    if (period > 65536U)
    {
        period = 65536U;
    }
    *arr = period - 1U;

    /* 真实频率 = tick_hz / period，取整误差最大 1/period */
    *actual_hz = tick_hz / period;
    return HAL_OK;
}

/**
 * @brief 计算 CCR：ccr = (arr+1) * duty_pm / 1000
 */
static uint32_t pwm_calc_ccr(uint32_t arr, uint16_t duty_pm)
{
    uint32_t period = arr + 1U;
    uint32_t ccr;

    if (duty_pm > 1000U)
    {
        duty_pm = 1000U;
    }
    ccr = (uint32_t)(((uint64_t)period * (uint64_t)duty_pm) / 1000ULL);
    if (ccr > 0xFFFFU)
    {
        ccr = 0xFFFFU;
    }
    return ccr;
}

/**
 * @brief 把 PSC/ARR/CCR 一次性写下去并立即生效
 */
static void pwm_write(uint32_t psc, uint32_t arr, uint32_t ccr)
{
    __HAL_TIM_SET_PRESCALER(BREATH_TIM, psc);
    __HAL_TIM_SET_AUTORELOAD(BREATH_TIM, arr);
    __HAL_TIM_SET_COMPARE(BREATH_TIM, BREATH_TIM_CH, ccr);

    /* 立即把影子寄存器内容装进工作寄存器，避免出现一帧错误波形 */
    BREATH_TIM->Instance->EGR = TIM_EGR_UG;
}

HAL_StatusTypeDef app_pwm_init(uint32_t freq_hz, uint16_t duty_pm)
{
    HAL_StatusTypeDef st;
    uint32_t psc = 0;
    uint32_t arr = 0;
    uint32_t actual = 0;

    st = pwm_calc(freq_hz, &psc, &arr, &actual);
    if (st != HAL_OK)
    {
        return st;
    }

    s_pwm.freq_hz   = freq_hz;
    s_pwm.duty_pm   = (duty_pm > 1000U) ? 1000U : duty_pm;
    s_pwm.psc       = psc;
    s_pwm.arr       = arr;
    s_pwm.actual_hz = actual;
    s_pwm.ccr       = pwm_calc_ccr(arr, s_pwm.duty_pm);

    pwm_write(s_pwm.psc, s_pwm.arr, s_pwm.ccr);
    return HAL_OK;
}

HAL_StatusTypeDef app_pwm_set_frequency(uint32_t freq_hz)
{
    HAL_StatusTypeDef st;
    uint32_t psc = 0;
    uint32_t arr = 0;
    uint32_t actual = 0;
    uint32_t ccr;

    st = pwm_calc(freq_hz, &psc, &arr, &actual);
    if (st != HAL_OK)
    {
        return st;
    }

    /* 关键：占空比沿用旧的 duty_pm，不因为改频率而变化 */
    ccr = pwm_calc_ccr(arr, s_pwm.duty_pm);

    s_pwm.freq_hz   = freq_hz;
    s_pwm.psc       = psc;
    s_pwm.arr       = arr;
    s_pwm.actual_hz = actual;
    s_pwm.ccr       = ccr;

    pwm_write(psc, arr, ccr);
    return HAL_OK;
}

HAL_StatusTypeDef app_pwm_set_duty_pm(uint16_t duty_pm)
{
    if (duty_pm > 1000U)
    {
        duty_pm = 1000U;
    }
    s_pwm.duty_pm = duty_pm;
    s_pwm.ccr     = pwm_calc_ccr(s_pwm.arr, duty_pm);
    __HAL_TIM_SET_COMPARE(BREATH_TIM, BREATH_TIM_CH, s_pwm.ccr);
    return HAL_OK;
}

void app_pwm_get_state(pwm_state_t *st)
{
    if (st != 0)
    {
        *st = s_pwm;
    }
}

#else   /* ===== BREATH_LED_MODE != 0：板载 PC13 的软件 PWM ===== */

/* 分辨率 128 级：取 2 的幂，取模可以用位与，省指令周期 */
#define SWPWM_STEPS      128U
#define SWPWM_MASK       (SWPWM_STEPS - 1U)

static pwm_state_t       s_pwm;
static volatile uint16_t s_phase;         /* 0..127 */
static volatile uint16_t s_duty_steps;    /* 0..128，0 = 全灭，128 = 全亮 */

/**
 * @brief 由载波频率反算 ARR：载波 = 72MHz / (ARR+1) / 128
 */
static uint32_t swpwm_calc_arr(uint32_t freq_hz)
{
    uint64_t tick = (uint64_t)freq_hz * SWPWM_STEPS;
    uint32_t arr;

    if (tick == 0ULL)
    {
        tick = 1ULL;
    }
    arr = (uint32_t)(((uint64_t)BOARD_TIM_CLK_HZ + tick / 2ULL) / tick);
    if (arr < 2U)
    {
        arr = 2U;
    }
    if (arr > 65536U)
    {
        arr = 65536U;
    }
    return arr - 1U;
}

/**
 * @brief 设定载波频率（占空比保持不变）
 */
static HAL_StatusTypeDef swpwm_apply(uint32_t freq_hz)
{
    uint32_t arr;

    if (freq_hz < PWM_FREQ_MIN_HZ || freq_hz > PWM_FREQ_MAX_HZ)
    {
        return HAL_ERROR;
    }
    arr = swpwm_calc_arr(freq_hz);

    s_pwm.freq_hz   = freq_hz;
    s_pwm.psc       = 0U;
    s_pwm.arr       = arr;
    s_pwm.actual_hz = BOARD_TIM_CLK_HZ / ((arr + 1U) * SWPWM_STEPS);

    TIM3->ARR = (uint16_t)arr;
    TIM3->EGR = TIM_EGR_UG;      /* 立即装载，改载波时不会卡一帧错波形 */
    return HAL_OK;
}

HAL_StatusTypeDef app_pwm_init(uint32_t freq_hz, uint16_t duty_pm)
{
    s_phase       = 0U;
    s_pwm.duty_pm = (duty_pm > 1000U) ? 1000U : duty_pm;
    s_duty_steps  = (uint16_t)(((uint32_t)s_pwm.duty_pm * SWPWM_STEPS + 500U) / 1000U);
    s_pwm.ccr     = s_duty_steps;

    if (swpwm_apply(freq_hz) != HAL_OK)
    {
        return HAL_ERROR;
    }

    /* 启动更新中断。这里故意直接写寄存器：若走 HAL_TIM_Base_Start_IT +
       HAL_TIM_IRQHandler，每个中断还要做一串状态判断，25kHz 下 CPU 占用
       会从 3% 涨到 10% 以上。 */
    TIM3->CR1  = 0U;
    TIM3->PSC  = 0U;
    TIM3->EGR  = TIM_EGR_UG;
    TIM3->SR   = 0U;
    TIM3->DIER = TIM_DIER_UIE;
    TIM3->CR1  = TIM_CR1_ARPE | TIM_CR1_CEN;
    return HAL_OK;
}

HAL_StatusTypeDef app_pwm_set_frequency(uint32_t freq_hz)
{
    return swpwm_apply(freq_hz);   /* 只动 ARR，占空比步数不变 */
}

HAL_StatusTypeDef app_pwm_set_duty_pm(uint16_t duty_pm)
{
    if (duty_pm > 1000U)
    {
        duty_pm = 1000U;
    }
    s_pwm.duty_pm = duty_pm;
    s_duty_steps  = (uint16_t)(((uint32_t)duty_pm * SWPWM_STEPS + 500U) / 1000U);
    s_pwm.ccr     = s_duty_steps;
    return HAL_OK;
}

void app_pwm_get_state(pwm_state_t *st)
{
    if (st != 0)
    {
        *st = s_pwm;
    }
}

/**
 * @brief 软件 PWM 的一个节拍（TIM3 更新中断里调用）
 * @note  直接写 BSRR，省掉 HAL_GPIO_WritePin 的函数调用开销。
 *        PC13 低电平点亮：BSRR 高 16 位 = 置低；低 16 位 = 置高。
 */
void app_pwm_sw_tick(void)
{
    uint16_t p = (uint16_t)((s_phase + 1U) & SWPWM_MASK);

    s_phase = p;
    if (p < s_duty_steps)
    {
        GPIOC->BSRR = (uint32_t)GPIO_PIN_13 << 16;
    }
    else
    {
        GPIOC->BSRR = (uint32_t)GPIO_PIN_13;
    }
}

#endif /* BREATH_LED_MODE */
