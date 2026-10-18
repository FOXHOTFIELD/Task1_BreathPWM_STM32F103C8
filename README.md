# 25 级软件第一期考核任务：STM32 PWM 呼吸灯 + 串口命令控制

基于 **STM32F103C8T6 + CubeMX/HAL** 的呼吸灯工程：定时器硬件 PWM 驱动 LED 做呼吸效果，按键（外部中断）暂停/继续，串口命令行实时改频率、占空比与曲线。

- 版本：**v1.0.0**
- 主控：STM32F103C8T6，HSE 8MHz × PLL9 = **72MHz**
- 工具链：Keil MDK 5.43a（ARMCC V5.06 + MicroLIB，-O2）
- 构建状态：**0 Error / 0 Warning**；Code 15482 B / RO 1066 B / RW 64 B / ZI 1616 B；产物 `MDK-ARM/Objects/Task1_BreathPWM.hex`（46782 B）

## 硬件与接线

| 功能 | 引脚 | 外设 | 接线 |
|---|---|---|---|
| 状态灯 | PC13 | GPIO 推挽 | 板载 LED，低电平点亮，1Hz 翻转 |
| 呼吸灯 | PA6 | TIM3_CH1（复用推挽） | LED 正极 → PA6，负极 → 220Ω → GND |
| 按键 | PA0 | EXTI0 下降沿 | 按键一端 PA0，另一端 GND（内部上拉） |
| 串口 | PA9/PA10 | USART1 115200 8N1 | 接 CH340：PA9→RX，PA10→TX，必须共地 |

> PA6 的 PWM 是**推挽输出，不需要上拉电阻**。若 LED 是蓝/白色（压降≈3V），220Ω 下电流过小，建议换红色 LED 或改用 100Ω。

### LED 分配方案（`App/Inc/board_config.h` 的 `BREATH_LED_MODE`）

| 值 | 呼吸灯 | 1Hz 状态灯 | 说明 |
|---|---|---|---|
| **0（默认）** | PA6（TIM3_CH1 硬件 PWM） | PC13 板载 | 需要外接一颗 LED 到 PA6 |
| 1 | PC13 板载（软件 PWM，128 级） | PA6（外接） | 只有一颗板载灯时用，CPU 多耗 3%~8% |
| 2 | PC13 板载（软件 PWM，128 级） | 无 | 完全不外接 LED |

改这个宏后需重新编译；方案 1/2 的呼吸灯是"定时器中断 + 软件翻转 GPIO"，严格说不属于"定时器硬件输出 PWM"，验收时应主动说明。

## 串口命令

| 命令 | 说明 |
|---|---|
| `F=<1..10000>` | 设 PWM 频率（Hz），**保持当前占空比** |
| `D=<0..100>` | 设占空比（%），可带 1 位小数，进入 HOLD |
| `RUN` / `STOP` | 继续呼吸（从冻结相位）/ 暂停（保持当前占空比） |
| `MODE=LINEAR\|GAMMA` | 线性扫描 / gamma 2.2 视觉线性化 |
| `PERIOD=<200..10000>` | 一次完整呼吸的周期（ms，默认 2400） |
| `STAT=ON\|OFF` | 每秒状态行开关 |
| `INFO` | 打印参数、寄存器值、按键次数、运行时间 |
| `RESET` | 恢复默认（500Hz / GAMMA / 2400ms / RUN） |
| `HELP` | 命令帮助 |

非法命令统一返回 `ERR:<原因>`（`UNKNOWN_CMD` / `SYNTAX` / `RANGE` / `LINE_TOO_LONG`）。

## 编译与下载

```
# Keil GUI：打开 MDK-ARM/Task1_BreathPWM.uvprojx，F7 编译，F8 下载
# 命令行：
UV4.exe -b "MDK-ARM/Task1_BreathPWM.uvprojx" -j0 -o "MDK-ARM/build.log"
echo %ERRORLEVEL%      :: 0 = 无错误
```
器件包：`Keil.STM32F1xx_DFP.2.2.0`；工程已配 MicroLIB + C99 + -O2 + 生成 HEX。
中文注释若显示乱码：Keil → Edit → Configuration → Encoding 选 UTF-8。

## 目录结构

```
Core/           CubeMX 层：时钟、GPIO、USART1、TIM3/TIM4、MSP、中断入口
App/            业务层：呼吸状态机、PWM 封装、按键、命令行、日志
Drivers/        STM32F1xx HAL v1.1.10 + CMSIS（随仓库提供，clone 即可编译）
MDK-ARM/        Keil 工程、启动文件、DebugConfig
docs/           实现说明 docx（评分点逐条对照、原理推导、验收流程）
```

## 验证状态

- Keil 全量重编译：0 Error / 0 Warning（`-r` 重建，日志见构建输出）
- 主机端单元测试：命令解析 / 定点格式化 / 帧格式共 **38 项断言全部通过**（脚本在 `../_tools/host_test`）
- **未验证**：真实板上的光强曲线、PA6 波形（本仓库作者没有该硬件，测试时请按 docs 里的验收流程逐条确认）

## 已知取舍与注意事项

- 呼吸相位量程 0..20000，周期量化误差 < 0.5%；
- 改频率时 PSC/ARR/CCR 三寄存器同写并置 `EGR.UG`，避免出现一帧错误占空比；
- 暂停只冻结相位、不写 CCR，所以亮度"定住"；继续从冻结相位接着走；
- F1 HAL **没有** `HAL_TIM_PWM_MspPostInit` 钩子，PA6 的复用配置在 `HAL_TIM_PWM_MspInit` 里（漏了就没波形）。
