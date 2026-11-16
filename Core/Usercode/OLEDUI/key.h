#ifndef __KEY_H
#define __KEY_H

#include "main.h"

/*
 * PWM_Tool 的双按键硬件：
 *   PB1 -> 确认（短按确认/进入，长按返回）
 *   PB0 -> 下移（短按下移一格，长按上移一格）
 *
 * 所有按键由 20 ms 的 TIM4 中断里的 Key_Scan() 采样，
 * OLED_UI 框架只读取缓存后的结果。
 */
#define KEY_ENTER_GPIO_PORT     GPIOB
#define KEY_ENTER_GPIO_PIN      GPIO_PIN_1

#define KEY_DOWN_GPIO_PORT      GPIOB
#define KEY_DOWN_GPIO_PIN       GPIO_PIN_0

/* 1 = 按下时读到高电平（内部下拉）；0 = 按下时读到低电平（内部上拉） */
#define KEY_ACTIVE_HIGH         0

/* 长按时间，单位毫秒 */
#define KEY_LONG_PRESS_MS       600U

/* UI 时基周期：TIM4 更新中断的间隔，也用于长按计数 */
#define KEY_SCAN_PERIOD_MS      20U

void Key_Init(void);
void Key_Scan(void);

uint8_t Key_GetEnterStatus(void);
uint8_t Key_GetBackStatus(void);
uint8_t Key_GetUpStatus(void);
uint8_t Key_GetDownStatus(void);
uint8_t Key_IsAnyPressed(void);

/* 以下延时函数实现在 OLED.c 中 */
void Delay_us(uint32_t xus);
void Delay_ms(uint32_t xms);
void Delay_s(uint32_t xs);

#endif
