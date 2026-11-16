/*
 * PWM 应用层：TIM4 CH3 / CH4（PB8 / PB9）两路 PWM 的占空比、频率、输出使能
 *
 * 注意：TIM4 的 OCPolarity 必须配成 HIGH。配成 LOW 会整体反相，
 *       而且 CCR=0（Out=OFF）会变成恒高电平，灯反而常亮。
 *
 * 硬件约束：TIM4 的 CH3 与 CH4 共用同一个计数器时基（PSC / ARR），
 * 所以两路的 PWM 频率必然相同，只有占空比可以各自独立。
 * 菜单里两个 "Freq" 行绑定的是同一个变量，改任意一个两行同时变化。
 * 要真正独立的双频，得把 CH4 换到另一个定时器（需要改 .ioc）。
 *
 * 时基固定为 3600 个计数一个周期：
 *   占空比分辨率 1/3600 约 0.028 %，CCR = 占空比 x 36
 *   频率靠预分频实现：72 MHz / ((PSC + 1) x 3600)
 *   频率表里的每个频点都能整除，PSC 都是整数，不需要近似
 *
 * 扫频：
 *   频率扫频不是跳档，而是在 PSC 0..199 之间逐 1 步进，
 *   100 Hz ~ 20 kHz 平滑过渡。起止频率仍用 8 档表选，
 *   换算成 PSC 后从一端走到另一端，到端点自动折返。
 *   代价是低频段密、高频段疏（PSC 1 -> 0 就是 10 kHz -> 20 kHz），
 *   这是 ARR 固定 3600 带来的硬件限制。
 *   脉宽扫频同理，在起止占空比之间逐 1 % 步进，两通道各自独立。
 *   速度统一用"单程秒数"表示：从起点走到终点用多久。
 */
#ifndef __PWM_APP_H
#define __PWM_APP_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>
#include <stdbool.h>

/* 通道序号，和菜单里的 Ch3 / Ch4 对应 */
#define PWM_CH3             0
#define PWM_CH4             1
#define PWM_CH_COUNT        2

/* 频率表条目数 */
#define PWM_FREQ_COUNT      8

/* 扫频单程时间表条目数 */
#define PWM_SWEEP_TIME_COUNT 6

/* 频率表：数值（Hz）和显示名，菜单与扫频共用同一份，避免两张表对不上 */
extern const uint16_t PwmFreqTable[PWM_FREQ_COUNT];
extern const char*    PwmFreqNames[PWM_FREQ_COUNT];

/* 扫频单程时间（秒）和显示名 */
extern const uint16_t PwmSweepTimeTable[PWM_SWEEP_TIME_COUNT];
extern const char*    PwmSweepTimeNames[PWM_SWEEP_TIME_COUNT];

/* ---------------------------------------------------------------- */
/* 下面这些变量直接挂到菜单控件上，由 OLED_UI 修改                    */
/* ---------------------------------------------------------------- */
extern int8_t g_pwmFreqIndex;                   /* 手动频率表索引，两通道共用 */
extern int8_t g_pwmDuty[PWM_CH_COUNT];          /* 手动占空比 0..100 % */
extern int8_t g_pwmOut[PWM_CH_COUNT];           /* 是否输出：0=OFF, 1=ON */

/* 扫频开关 */
extern bool   g_pwmFreqSweep;                   /* 频率扫频，两通道共用 */
extern bool   g_pwmDutySweep[PWM_CH_COUNT];     /* 脉宽扫频，每通道独立 */

/* 频率扫频的起止档位（8 档频率表索引）和单程时间 */
extern int8_t g_pwmSweepFreqFromIdx;
extern int8_t g_pwmSweepFreqToIdx;
extern int8_t g_pwmSweepFreqTimeIdx;

/* 脉宽扫频的起止占空比（0..100 %）和单程时间，两通道独立 */
extern int8_t g_pwmSweepDutyFrom[PWM_CH_COUNT];
extern int8_t g_pwmSweepDutyTo[PWM_CH_COUNT];
extern int8_t g_pwmSweepDutyTimeIdx;

/* 把 TIM4 改成固定 3600 计数周期并启动两路 PWM，在 MX_TIM4_Init() 之后调用 */
void PwmApp_Init(void);

/* 主循环里反复调用：推进扫频、把菜单改动同步到硬件 */
void PwmApp_Poll(void);

#ifdef __cplusplus
}
#endif

#endif /* __PWM_APP_H */
