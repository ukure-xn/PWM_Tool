/*
 * 系统资源占用统计：CPU 负载 / RAM 占用率 / Flash 占用率
 *
 * ROM、RAM 占用率由链接器符号在 SysStat_Init() 里算一次；
 * CPU 负载用 DWT 周期计数器统计主循环的实际执行时间，每 1 秒结算。
 * 三个结果都是 float 全局变量，由 About 菜单的 List_FloatBox 直接显示。
 */
#ifndef __SYS_STAT_H
#define __SYS_STAT_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>

/* 下面三个变量直接挂到 MenuItem.List_FloatBox 上，单位 % */
extern float g_sysCpuPct;    /* CPU 负载，每 1 秒刷新一次 */
extern float g_sysRamPct;    /* RAM 静态占用率，初始化时算一次 */
extern float g_sysRomPct;    /* Flash 占用率，初始化时算一次 */

/* 计算 ROM/RAM 占用率并使能 DWT 周期计数器，在 main() 里调用一次 */
void SysStat_Init(void);

/* 主循环一轮开始：记录起始周期计数 */
void SysStat_FrameBegin(void);

/* 主循环一轮结束：累加本帧忙周期，每 1 秒结算一次 CPU 负载 */
void SysStat_FrameEnd(void);

#ifdef __cplusplus
}
#endif

#endif /* __SYS_STAT_H */
