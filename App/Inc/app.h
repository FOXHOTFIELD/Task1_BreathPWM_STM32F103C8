/**
 * @file    app.h
 * @brief   业务层总入口（与 CubeMX 生成的 Core 层解耦）
 */
#ifndef APP_H
#define APP_H

#ifdef __cplusplus
extern "C" {
#endif

/** @brief 业务层初始化：状态灯、PWM 初值、呼吸状态机、按键、命令行、启动信息 */
void app_init(void);

/** @brief 10ms 控制节拍（由 TIM4 更新中断调用），运行在中断上下文 */
void app_tick_10ms(void);

/** @brief 主循环任务：命令行解析 + 1Hz 状态上报 */
void app_loop(void);

#ifdef __cplusplus
}
#endif

#endif /* APP_H */
