/**
 * @file    app_breath.c
 * @brief   呼吸灯状态机实现（三角波相位 + gamma 校正查表）
 */
#include "main.h"
#include "board_config.h"
#include "app_breath.h"
#include "app_pwm.h"

/* gamma 2.2 校正表：y = 1000 * (x/32)^2.2，x = 0..32 */
#define GAMMA_LUT_N   33U
static const uint16_t k_gamma_lut[GAMMA_LUT_N] =
{
    0,    0,    2,    5,   10,   17,   25,   35,
   47,   61,   77,   95,  116,  138,  162,  189,
  218,  249,  282,  318,  356,  396,  439,  484,
  531,  581,  633,  688,  745,  805,  868,  933,
 1000
};

/* 相位单位取 1/10 千分比（0..20000），比 0..2000 少 10 倍量化误差：
   例如 2400ms 周期时每拍步进 83（=20000*10/2400 取整），实际周期 2409ms，
   误差 0.4%；若用 0..2000 则步进 8，实际 2500ms，误差 4%。 */
#define PHASE_FULL      20000U   /* 0->10000 渐亮，10000->20000 渐暗 */

static breath_state_t s_state      = BREATH_STATE_RUN;
static breath_mode_t  s_mode       = BREATH_MODE_GAMMA;
static uint32_t       s_period_ms  = BREATH_PERIOD_DEFAULT_MS;
static uint16_t       s_phase      = 0U;
static uint16_t       s_duty_pm    = 0U;

/**
 * @brief 线性相位(0..1000) -> gamma 校正后的占空比(千分比)
 * @note  33 点查表 + 线性插值，误差 < 1‰，不依赖浮点数学库
 */
static uint16_t breath_gamma_map(uint16_t linear)
{
    uint32_t scaled;
    uint32_t idx;
    uint32_t frac;
    uint32_t a;
    uint32_t b;

    if (linear > 1000U)
    {
        linear = 1000U;
    }
    scaled = (uint32_t)linear * 32U;          /* 0..32000 */
    idx    = scaled / 1000U;                  /* 0..32 */
    frac   = scaled % 1000U;                  /* 0..999 */
    if (idx >= (GAMMA_LUT_N - 1U))
    {
        return 1000U;
    }
    a = k_gamma_lut[idx];
    b = k_gamma_lut[idx + 1U];
    return (uint16_t)(a + (((b - a) * frac) / 1000U));
}

void breath_init(void)
{
    s_state     = BREATH_STATE_RUN;
    s_mode      = BREATH_MODE_GAMMA;
    s_period_ms = BREATH_PERIOD_DEFAULT_MS;
    s_phase     = 0U;
    s_duty_pm   = 0U;
    (void)app_pwm_set_duty_pm(0U);
}

void breath_tick_10ms(void)
{
    uint32_t step;
    uint16_t tri;
    uint16_t linear;
    uint16_t duty;

    if (s_state != BREATH_STATE_RUN)
    {
        return;   /* 暂停：相位与占空比都冻结 */
    }

    /* 相位步进：一次完整呼吸 20000 相位，按周期折算到每个 10ms 节拍 */
    step = ((uint32_t)PHASE_FULL * TICK_PERIOD_MS) / s_period_ms;
    if (step == 0U)
    {
        step = 1U;
    }
    s_phase = (uint16_t)(s_phase + step);
    if (s_phase >= PHASE_FULL)
    {
        s_phase = (uint16_t)(s_phase - PHASE_FULL);
    }

    /* 三角波：0..10000 渐亮，10000..20000 渐暗，再折算成千分比 */
    tri = (s_phase <= (PHASE_FULL / 2U)) ? s_phase : (uint16_t)(PHASE_FULL - s_phase);
    linear = (uint16_t)(tri / 10U);            /* 0..1000 千分比 */

    duty = (s_mode == BREATH_MODE_GAMMA) ? breath_gamma_map(linear) : linear;

    if (duty != s_duty_pm)
    {
        s_duty_pm = duty;
        (void)app_pwm_set_duty_pm(duty);
    }
}

void breath_toggle(void)
{
    s_state = (s_state == BREATH_STATE_RUN) ? BREATH_STATE_HOLD : BREATH_STATE_RUN;
}

void breath_set_running(uint8_t run)
{
    s_state = (run != 0U) ? BREATH_STATE_RUN : BREATH_STATE_HOLD;
}

uint8_t breath_is_running(void)
{
    return (uint8_t)(s_state == BREATH_STATE_RUN);
}

void breath_set_mode(breath_mode_t mode)
{
    s_mode = mode;
    /* 切换曲线后下一次节拍就会按新曲线重新计算占空比 */
}

breath_mode_t breath_get_mode(void)
{
    return s_mode;
}

void breath_set_period_ms(uint32_t period_ms)
{
    if (period_ms < BREATH_PERIOD_MIN_MS)
    {
        period_ms = BREATH_PERIOD_MIN_MS;
    }
    if (period_ms > BREATH_PERIOD_MAX_MS)
    {
        period_ms = BREATH_PERIOD_MAX_MS;
    }
    s_period_ms = period_ms;
}

uint32_t breath_get_period_ms(void)
{
    return s_period_ms;
}

void breath_hold_at_duty(uint16_t duty_pm)
{
    if (duty_pm > 1000U)
    {
        duty_pm = 1000U;
    }
    s_state   = BREATH_STATE_HOLD;
    s_duty_pm = duty_pm;
    (void)app_pwm_set_duty_pm(duty_pm);
}

uint16_t breath_get_phase(void)
{
    return s_phase;
}

uint16_t breath_get_duty_pm(void)
{
    return s_duty_pm;
}
