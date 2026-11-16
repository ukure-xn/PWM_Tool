#include "key.h"

/*
 * 按键映射
 *   PB1（确认）：原始电平直接交给框架，框架自己把短按识别为“确认”，
 *                把 600 ms 长按识别为“返回”。
 *   PB0（下移）：短按松手 -> 下移一格
 *                长按 600 ms -> 上移一格（只触发一次）
 * 每个脉冲只持续一个 20 ms 扫描周期，框架看到的是一个干净的上升沿。
 */
#define KEY_LONG_PRESS_TICKS    (KEY_LONG_PRESS_MS / KEY_SCAN_PERIOD_MS)

/* Cached results read by the framework */
static uint8_t KeyEnterLevel = 0;
static uint8_t KeyBackLevel  = 0;
static uint8_t KeyUpLevel    = 0;
static uint8_t KeyDownLevel  = 0;
static uint8_t KeyAnyPressed = 0;

/* PB1 hold detection */
static uint16_t DownHoldTicks = 0;
static uint8_t  DownLongFired = 0;

static uint8_t Key_ReadPin(GPIO_TypeDef *port, uint16_t pin)
{
    GPIO_PinState state = HAL_GPIO_ReadPin(port, pin);

#if KEY_ACTIVE_HIGH
    return (state == GPIO_PIN_SET) ? 1U : 0U;
#else
    return (state == GPIO_PIN_RESET) ? 1U : 0U;
#endif
}

/**
 * @brief  把 PB0 / PB1 配置为输入。
 * @note   必须在 MX_GPIO_Init() 之后调用。
 */
void Key_Init(void)
{
    GPIO_InitTypeDef GPIO_InitStruct = {0};

    __HAL_RCC_GPIOB_CLK_ENABLE();

    GPIO_InitStruct.Pin  = KEY_ENTER_GPIO_PIN | KEY_DOWN_GPIO_PIN;
    GPIO_InitStruct.Mode = GPIO_MODE_INPUT;
#if KEY_ACTIVE_HIGH
    GPIO_InitStruct.Pull = GPIO_PULLDOWN;
#else
    GPIO_InitStruct.Pull = GPIO_PULLUP;
#endif
    HAL_GPIO_Init(GPIOB, &GPIO_InitStruct);

    KeyEnterLevel = Key_ReadPin(KEY_ENTER_GPIO_PORT, KEY_ENTER_GPIO_PIN);
    KeyBackLevel  = 0;
    KeyUpLevel    = 0;
    KeyDownLevel  = 0;
    KeyAnyPressed = 0;
    DownHoldTicks = 0;
    DownLongFired = 0;
}

/**
 * @brief  采样两个按键，由 TIM4 的 20 ms 中断调用。
 */
void Key_Scan(void)
{
    uint8_t enter = Key_ReadPin(KEY_ENTER_GPIO_PORT, KEY_ENTER_GPIO_PIN);
    uint8_t down  = Key_ReadPin(KEY_DOWN_GPIO_PORT, KEY_DOWN_GPIO_PIN);
    uint8_t up_step   = 0;
    uint8_t down_step = 0;

    KeyEnterLevel = enter;          /* PB1 原始电平直接交给框架，短按/长按由框架判断 */
    KeyBackLevel  = 0;
    KeyAnyPressed = (uint8_t)(enter | down);

    if (down) {
        if (DownHoldTicks < 0xFFFFU) {
            DownHoldTicks++;
        }
        if ((DownLongFired == 0U) && (DownHoldTicks >= KEY_LONG_PRESS_TICKS)) {
            DownLongFired = 1;
            up_step = 1;                /* 长按达到阈值：产生一次上移脉冲 */
        }
    } else {
        if ((DownHoldTicks > 0U) && (DownLongFired == 0U)) {
            down_step = 1;              /* 短按：松手时产生一次下移脉冲 */
        }
        DownHoldTicks = 0;
        DownLongFired = 0;
    }

    KeyUpLevel   = up_step;
    KeyDownLevel = down_step;
}

uint8_t Key_GetEnterStatus(void)
{
    return KeyEnterLevel;
}

uint8_t Key_GetBackStatus(void)
{
    return KeyBackLevel;
}

uint8_t Key_GetUpStatus(void)
{
    return KeyUpLevel;
}

uint8_t Key_GetDownStatus(void)
{
    return KeyDownLevel;
}

/**
 * @brief  任意按键是否按下（原始电平），供息屏唤醒判断使用。
 */
uint8_t Key_IsAnyPressed(void)
{
    return KeyAnyPressed;
}
