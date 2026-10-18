/**
 * @file    app_cli.c
 * @brief   串口命令行（评分点 5 + 发挥项 2/3）
 *
 * 数据流：
 *   USART1 接收中断（1 字节） -> cli_rx_byte() 压入环形缓冲
 *   -> 主循环 cli_process() 取出字节拼行 -> 遇到 \r 或 \n 交给 handle_line()
 *   -> 解析成功/失败都给出明确回显，非法命令返回 ERR:<原因>
 *
 * 设计取舍：
 *   - 中断只做"入队"，绝不在中断里解析或打印；
 *   - 环形缓冲让串口在短时间内连续收数据（如粘贴一整条命令）也不会丢字节；
 *   - 输出统一用 ASCII，避免不同串口助手的编码设置（GBK/UTF-8）导致乱码。
 */
#include "main.h"
#include "board_config.h"
#include "app_cli.h"
#include "app_log.h"
#include "app_util.h"
#include "app_breath.h"
#include "app_pwm.h"
#include "app_key.h"

#include <string.h>

#define CLI_LINE_MAX      96U
#define CLI_RX_RING_SIZE  256U
#define U32(v)            ((unsigned long)(v))

/* ---------------- 接收环形缓冲（中断写 head，主循环读 tail） ---------------- */
static volatile uint8_t  s_ring[CLI_RX_RING_SIZE];
static volatile uint16_t s_ring_head;
static volatile uint16_t s_ring_tail;

static uint8_t  s_rx_byte;          /* HAL_UART_Receive_IT 的单字节目标 */
static char     s_line[CLI_LINE_MAX];
static uint8_t  s_len;
static uint32_t s_lines;
static uint8_t  s_stat_enabled = 1U;

/* ---------------- 内部函数声明 ---------------- */
static void cli_do_info(void);
static void cli_do_help(void);
static void cli_hint(void);
static void handle_line(char *line);
static void copy_str(char *dst, uint8_t cap, const char *src);

/* ==================================================================
 * 初始化与接收
 * ================================================================== */
void cli_init(void)
{
    s_ring_head    = 0U;
    s_ring_tail    = 0U;
    s_len          = 0U;
    s_lines        = 0U;
    s_stat_enabled = 1U;

    /* 启动 1 字节中断接收；之后每收完 1 字节在回调里重新挂上 */
    (void)HAL_UART_Receive_IT(&huart1, &s_rx_byte, 1);

    cli_banner();
}

void cli_rx_byte(uint8_t b)
{
    uint16_t next = (uint16_t)((s_ring_head + 1U) % CLI_RX_RING_SIZE);

    if (next != s_ring_tail)     /* 满了就丢弃新字节，不覆盖未处理数据 */
    {
        s_ring[s_ring_head] = b;
        s_ring_head = next;
    }
}

/**
 * @brief HAL 的串口接收完成回调：把字节交给命令行，并重新开启接收
 */
void HAL_UART_RxCpltCallback(UART_HandleTypeDef *huart)
{
    if (huart->Instance == USART1)
    {
        cli_rx_byte(s_rx_byte);
        (void)HAL_UART_Receive_IT(&huart1, &s_rx_byte, 1);
    }
}

/**
 * @brief 串口错误（溢出/帧错/噪声）后重新挂接收，避免串口"卡死"
 */
void HAL_UART_ErrorCallback(UART_HandleTypeDef *huart)
{
    if (huart->Instance == USART1)
    {
        (void)HAL_UART_Receive_IT(&huart1, &s_rx_byte, 1);
    }
}

void cli_process(void)
{
    uint8_t b;

    while (s_ring_tail != s_ring_head)
    {
        b = s_ring[s_ring_tail];
        s_ring_tail = (uint16_t)((s_ring_tail + 1U) % CLI_RX_RING_SIZE);

        if (b == '\r' || b == '\n')
        {
            if (s_len > 0U)
            {
                s_line[s_len] = '\0';
                handle_line(s_line);
                s_len = 0U;
            }
            continue;
        }
        if (b == 0x08U || b == 0x7FU)       /* 退格 */
        {
            if (s_len > 0U)
            {
                s_len--;
            }
            continue;
        }
        if (b < 0x20U || b > 0x7EU)         /* 只收可打印 ASCII */
        {
            continue;
        }
        if (s_len < (CLI_LINE_MAX - 1U))
        {
            s_line[s_len++] = (char)b;
        }
        else
        {
            logln("ERR:LINE_TOO_LONG (max %lu chars)", U32(CLI_LINE_MAX - 2U));
            s_len = 0U;
        }
    }
}

/* ==================================================================
 * 命令实现
 * ================================================================== */

static void copy_str(char *dst, uint8_t cap, const char *src)
{
    uint8_t i = 0U;

    if (cap == 0U)
    {
        return;
    }
    while (src[i] != '\0' && i < (uint8_t)(cap - 1U))
    {
        dst[i] = src[i];
        i++;
    }
    dst[i] = '\0';
}

static void cli_hint(void)
{
    logln("HINT: F=<hz> | D=<pct> | RUN | STOP | MODE=LINEAR|GAMMA | PERIOD=<ms> | STAT=ON|OFF | INFO | HELP | RESET");
}

static void handle_line(char *line)
{
    char *eq;
    char  key[16];
    char  val[24];
    char  dstr[16];
    uint32_t u32;
    uint32_t x10;
    pwm_state_t st;

    line = util_trim(line);
    if (*line == '\0')
    {
        return;
    }
    s_lines++;

    eq = strchr(line, '=');
    if (eq == 0)
    {
        if (util_str_eq_ci(line, "RUN"))
        {
            breath_set_running(1U);
            logln("OK RUN (resume breathing from phase %lu/20000)", U32(breath_get_phase()));
            return;
        }
        if (util_str_eq_ci(line, "STOP"))
        {
            breath_set_running(0U);
            util_format_fixed(dstr, breath_get_duty_pm(), 1U);
            logln("OK STOP (duty held at %s%%)", dstr);
            return;
        }
        if (util_str_eq_ci(line, "INFO"))
        {
            cli_do_info();
            return;
        }
        if (util_str_eq_ci(line, "HELP") || util_str_eq_ci(line, "?"))
        {
            cli_do_help();
            return;
        }
        if (util_str_eq_ci(line, "RESET"))
        {
            breath_init();
            (void)app_pwm_set_frequency(PWM_FREQ_DEFAULT_HZ);
            logln("OK RESET (F=%luHz, duty follows breathing, mode=GAMMA, period=%lums, RUN)",
                  U32(PWM_FREQ_DEFAULT_HZ), U32(BREATH_PERIOD_DEFAULT_MS));
            return;
        }
        logln("ERR:UNKNOWN_CMD \"%s\"", line);
        cli_hint();
        return;
    }

    /* 拆出 key / val 并各自 trim（val 可能带前后空格） */
    *eq = '\0';
    copy_str(key, (uint8_t)sizeof(key), util_trim(line));
    copy_str(val, (uint8_t)sizeof(val), eq + 1);
    copy_str(val, (uint8_t)sizeof(val), util_trim(val));

    /* ---------------- F=<频率> ---------------- */
    if (util_str_eq_ci(key, "F"))
    {
        if (util_parse_u32(val, &u32) == 0U)
        {
            logln("ERR:SYNTAX F=<hz>  (example: F=500)");
            return;
        }
        if (u32 < PWM_FREQ_MIN_HZ || u32 > PWM_FREQ_MAX_HZ)
        {
            logln("ERR:RANGE F must be %lu..%lu Hz", U32(PWM_FREQ_MIN_HZ), U32(PWM_FREQ_MAX_HZ));
            return;
        }
        if (app_pwm_set_frequency(u32) != HAL_OK)
        {
            logln("ERR:SET_FAILED F");
            return;
        }
        app_pwm_get_state(&st);
        util_format_fixed(dstr, st.duty_pm, 1U);
        logln("OK F=%luHz real=%luHz PSC=%lu ARR=%lu CCR=%lu duty=%s%% (duty kept)",
              U32(st.freq_hz), U32(st.actual_hz), U32(st.psc), U32(st.arr), U32(st.ccr), dstr);
        return;
    }

    /* ---------------- D=<占空比> ---------------- */
    if (util_str_eq_ci(key, "D"))
    {
        if (util_parse_fixed(val, &x10, 1U) == 0U)
        {
            logln("ERR:SYNTAX D=<pct>  (example: D=37.5)");
            return;
        }
        if (x10 > 1000U)
        {
            logln("ERR:RANGE D must be 0..100 %%");
            return;
        }
        breath_hold_at_duty((uint16_t)x10);
        util_format_fixed(dstr, x10, 1U);
        logln("OK D=%s%% (state=HOLD, send RUN to resume breathing)", dstr);
        return;
    }

    /* ---------------- MODE=LINEAR|GAMMA ---------------- */
    if (util_str_eq_ci(key, "MODE"))
    {
        if (util_str_eq_ci(val, "GAMMA"))
        {
            breath_set_mode(BREATH_MODE_GAMMA);
            logln("OK MODE=GAMMA (y=x^2.2 perceptual linearization)");
            return;
        }
        if (util_str_eq_ci(val, "LINEAR"))
        {
            breath_set_mode(BREATH_MODE_LINEAR);
            logln("OK MODE=LINEAR (duty ramps linearly)");
            return;
        }
        logln("ERR:RANGE MODE must be LINEAR or GAMMA");
        return;
    }

    /* ---------------- PERIOD=<ms> ---------------- */
    if (util_str_eq_ci(key, "PERIOD"))
    {
        if (util_parse_u32(val, &u32) == 0U)
        {
            logln("ERR:SYNTAX PERIOD=<ms>  (example: PERIOD=2400)");
            return;
        }
        if (u32 < BREATH_PERIOD_MIN_MS || u32 > BREATH_PERIOD_MAX_MS)
        {
            logln("ERR:RANGE PERIOD must be %lu..%lu ms",
                  U32(BREATH_PERIOD_MIN_MS), U32(BREATH_PERIOD_MAX_MS));
            return;
        }
        breath_set_period_ms(u32);
        logln("OK PERIOD=%lums (one full dark->bright->dark cycle)", U32(breath_get_period_ms()));
        return;
    }

    /* ---------------- STAT=ON|OFF ---------------- */
    if (util_str_eq_ci(key, "STAT"))
    {
        if (util_str_eq_ci(val, "ON"))
        {
            s_stat_enabled = 1U;
            logln("OK STAT=ON (one status line per second)");
            return;
        }
        if (util_str_eq_ci(val, "OFF"))
        {
            s_stat_enabled = 0U;
            logln("OK STAT=OFF");
            return;
        }
        logln("ERR:RANGE STAT must be ON or OFF");
        return;
    }

    logln("ERR:UNKNOWN_CMD \"%s=%s\"", key, val);
    cli_hint();
}

/* ==================================================================
 * INFO / HELP / 状态行 / 横幅
 * ================================================================== */
static void cli_do_info(void)
{
    pwm_state_t st;
    char        dstr[16];
    char        pstr[16];
    uint32_t    ms;

    app_pwm_get_state(&st);
    util_format_fixed(dstr, st.duty_pm, 1U);
    util_format_fixed(pstr, breath_get_duty_pm(), 1U);
    ms = HAL_GetTick();

    logln("INFO fw=Task1-v1.0 target=STM32F103C8T6 sysclk=%luMHz",
          U32(BOARD_SYSCLK_HZ / 1000000UL));
#if (BREATH_LED_MODE == 0)
    logln("INFO pins LED1=PC13 BREATH=PA6(TIM3_CH1 HW-PWM) KEY=PA0(EXTI0) UART=PA9/PA10");
#elif (BREATH_LED_MODE == 1)
    logln("INFO pins BLINK=PA6 BREATH=PC13(SW-PWM 128 steps) KEY=PA0(EXTI0) UART=PA9/PA10");
#else
    logln("INFO pins BREATH=PC13(SW-PWM 128 steps) BLINK=disabled KEY=PA0(EXTI0) UART=PA9/PA10");
#endif
    logln("INFO pwm F=%luHz real=%luHz PSC=%lu ARR=%lu CCR=%lu duty=%s%%",
          U32(st.freq_hz), U32(st.actual_hz), U32(st.psc), U32(st.arr), U32(st.ccr), dstr);
    logln("INFO breath state=%s mode=%s period=%lums phase=%lu/20000 duty=%s%%",
          breath_is_running() ? "RUN" : "HOLD",
          (breath_get_mode() == BREATH_MODE_GAMMA) ? "GAMMA" : "LINEAR",
          U32(breath_get_period_ms()), U32(breath_get_phase()), pstr);
    logln("INFO key presses=%lu (EXTI0, debounce %lums)",
          U32(key_get_press_count()), U32(KEY_DEBOUNCE_MS));
    logln("INFO uptime=%lu.%03lus cmds=%lu stat=%s",
          U32(ms / 1000UL), U32(ms % 1000UL), U32(s_lines),
          (s_stat_enabled != 0U) ? "ON" : "OFF");
}

static void cli_do_help(void)
{
    logln("---------------- COMMANDS (case insensitive) ----------------");
    logln(" F=<1..10000>        set PWM frequency in Hz, keep duty   e.g. F=500");
    logln(" D=<0..100>          set duty in percent (1 decimal)      e.g. D=37.5");
    logln(" RUN                 resume breathing from current phase");
    logln(" STOP                pause, keep the current duty");
    logln(" MODE=LINEAR|GAMMA   breathing curve (gamma 2.2 = default)");
    logln(" PERIOD=<200..10000> full breathe cycle in ms            e.g. PERIOD=2400");
    logln(" STAT=ON|OFF         one status line per second");
    logln(" INFO                dump parameters and register values");
    logln(" RESET               restore defaults (500Hz/GAMMA/2400ms/RUN)");
    logln(" HELP                this help");
    logln("-------------------------------------------------------------");
}

uint8_t cli_status_enabled(void)
{
    return s_stat_enabled;
}

void cli_print_status(void)
{
    pwm_state_t st;
    char        dstr[16];
    uint32_t    ms;

    app_pwm_get_state(&st);
    util_format_fixed(dstr, st.duty_pm, 1U);
    ms = HAL_GetTick();

    logln("STAT t=%lu.%03lus F=%luHz D=%s%% state=%s mode=%s",
          U32(ms / 1000UL), U32(ms % 1000UL), U32(st.freq_hz), dstr,
          breath_is_running() ? "RUN" : "HOLD",
          (breath_get_mode() == BREATH_MODE_GAMMA) ? "GAMMA" : "LINEAR");
}

void cli_banner(void)
{
    logln("");
    logln("================================================================");
    logln(" Task1: STM32 PWM breathing LED + UART command console");
    logln(" MCU   : STM32F103C8T6 @ 72MHz (HSE 8MHz x9), HAL driver");
#if (BREATH_LED_MODE == 0)
    logln(" Wiring: BLINK=PC13(built-in) | BREATH=PA6(TIM3_CH1, HW PWM)");
    logln("         LED anode -> PA6, LED cathode -> 220ohm -> GND");
#elif (BREATH_LED_MODE == 1)
    logln(" Wiring: BREATH=PC13(built-in, SW PWM 128 steps) | BLINK=PA6");
    logln("         external LED: anode -> 220ohm -> 3V3, cathode -> PA6");
#else
    logln(" Wiring: BREATH=PC13(built-in, SW PWM 128 steps) | BLINK=disabled");
#endif
    logln("         KEY=PA0 -> GND (internal pull-up, EXTI0 falling)");
    logln(" UART  : USART1 115200 8N1, PA9=TX PA10=RX, common GND");
    logln(" Type HELP to list commands. Send F=500 / D=37.5 / RUN / STOP.");
    logln("================================================================");
    logln("");
}
