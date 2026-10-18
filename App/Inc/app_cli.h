/**
 * @file    app_cli.h
 * @brief   串口命令行（评分点 5 + 发挥项 2/3）
 *
 * 支持的命令（大小写不敏感，整行以 \r 或 \n 结束）：
 * ------------------------------------------------------------------
 *  F=<1..10000>     设 PWM 频率（Hz）             例：F=500
 *  D=<0..100>       设占空比（%，可带 1 位小数）  例：D=37.5
 *  RUN              继续呼吸灯（从当前相位恢复）
 *  STOP             暂停，占空比保持在当前值
 *  MODE=LINEAR|GAMMA 切换曲线（发挥项：非线性呼吸）
 *  PERIOD=<200..10000> 一次完整呼吸的周期（ms）
 *  STAT=ON|OFF      开关每秒一次的状态上报
 *  INFO             打印全部运行参数与外设寄存器值
 *  HELP             打印命令帮助
 *  RESET            恢复默认参数（500Hz / 50% / 2400ms / GAMMA / RUN）
 * ------------------------------------------------------------------
 *  非法命令统一返回：ERR:<原因>，并提示 HELP
 */
#ifndef APP_CLI_H
#define APP_CLI_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>

/** @brief 初始化：启动串口 1 字节中断接收，打印启动横幅 */
void cli_init(void);

/** @brief 收到一个字节（中断上下文调用，只做缓冲） */
void cli_rx_byte(uint8_t b);

/** @brief 主循环调用：把攒够的一整行交给解析器 */
void cli_process(void);

/** @brief 是否开启 1Hz 状态上报 */
uint8_t cli_status_enabled(void);

/** @brief 打印一行状态（发挥项 2） */
void cli_print_status(void);

/** @brief 打印启动横幅与接线提示 */
void cli_banner(void);

#ifdef __cplusplus
}
#endif

#endif /* APP_CLI_H */
