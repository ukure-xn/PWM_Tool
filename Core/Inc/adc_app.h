/*
 * ADC 应用层：内部温度传感器 + 内部参考电压（Vrefint）
 *
 * 采样链路：ADC1 规则组 Rank1 = 温度传感器，Rank2 = Vrefint，
 * 由 DMA1_Channel1 循环搬运到内存；CPU 只轮询结果，不产生 DMA 中断。
 * 换算结果放在两个 float 变量里，由 About 菜单的 List_FloatBox 直接显示。
 */
#ifndef __ADC_APP_H
#define __ADC_APP_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>

/* 换算结果，直接挂到 MenuItem.List_FloatBox 上。
   框架每帧用 "%.1f" 右对齐打印，所以这里不用拼字符串。 */
extern float g_adcTempC;    /* 芯片温度，单位 °C */
extern float g_adcVddV;     /* 供电电压，单位 V */

/* 重新配置通道、校准并启动连续转换 + DMA。
   必须在 MX_ADC1_Init() 之后调用。 */
void AdcApp_Init(void);

/* 主循环里反复调用：累积采样，每 500 ms 换算并更新上面两个变量。 */
void AdcApp_Poll(void);

#ifdef __cplusplus
}
#endif

#endif /* __ADC_APP_H */
