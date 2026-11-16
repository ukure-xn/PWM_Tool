#include "adc_app.h"
#include "adc.h"
#include "dma.h"

/* 显示刷新周期，单位毫秒 */
#define ADC_APP_REFRESH_MS      500U

/* F103 出厂标定值：25 °C 时 1.43 V，平均斜率 4.3 mV/°C */
#define ADC_V25_V               1.43f
#define ADC_SLOPE_V_PER_C       0.0043f

/* Vrefint 典型值 1.20 V；12 位满量程 4095 */
#define ADC_VREFINT_V           1.20f
#define ADC_FULL_SCALE          4095

/* DMA 目标缓冲区：[0] = 温度传感器，[1] = Vrefint，顺序与规则组 Rank 一致 */
static __IO uint16_t s_adcDma[2];

/* 两次刷新之间累积采样，用平均值压噪声 */
static uint32_t s_sumTemp;
static uint32_t s_sumVref;
static uint32_t s_count;
static uint32_t s_lastTick;

/* 菜单直接读这两个变量，框架负责格式化显示 */
float g_adcTempC = 0.0f;
float g_adcVddV  = 0.0f;

void AdcApp_Init(void)
{
    ADC_ChannelConfTypeDef sConfig = {0};

    /* 1) 确认 ADC 时钟是 PCLK2 / 6 = 12 MHz。
       F103 的 ADC 时钟上限是 14 MHz，.ioc 里已经配成 DIV6；
       这里再写一次寄存器做兜底，防止以后改回 DIV2 把 ADC 超频。 */
    MODIFY_REG(RCC->CFGR, RCC_CFGR_ADCPRE, RCC_CFGR_ADCPRE_DIV6);

    /* 2) 强制打开连续转换。
       .ioc 里 Continuous Conversion Mode 是 DISABLE 时，一次软件启动只转
       一轮（Rank1 + Rank2）就停，DMA 缓冲区不再更新，温度和电压会一直卡住。
       这里覆盖成 ENABLE 并重新初始化，和后面的循环 DMA 配套。 */
    hadc1.Init.ContinuousConvMode = ENABLE;
    if (HAL_ADC_Init(&hadc1) != HAL_OK)
    {
        Error_Handler();
    }

    /* 3) Rank1 = 内部温度传感器，239.5 个采样周期（12 MHz 下约 20 us） */
    sConfig.Channel      = ADC_CHANNEL_TEMPSENSOR;
    sConfig.Rank         = ADC_REGULAR_RANK_1;
    sConfig.SamplingTime = ADC_SAMPLETIME_239CYCLES_5;
    if (HAL_ADC_ConfigChannel(&hadc1, &sConfig) != HAL_OK)
    {
        Error_Handler();
    }

    /* 4) Rank2 = 内部参考电压 Vrefint */
    sConfig.Channel      = ADC_CHANNEL_VREFINT;
    sConfig.Rank         = ADC_REGULAR_RANK_2;
    sConfig.SamplingTime = ADC_SAMPLETIME_239CYCLES_5;
    if (HAL_ADC_ConfigChannel(&hadc1, &sConfig) != HAL_OK)
    {
        Error_Handler();
    }

    /* 5) 重新校准，必须在启动转换之前做 */
    if (HAL_ADCEx_Calibration_Start(&hadc1) != HAL_OK)
    {
        Error_Handler();
    }

    /* 6) 连续转换 + 循环 DMA，缓冲区里始终是最新的两个码值 */
    if (HAL_ADC_Start_DMA(&hadc1, (uint32_t *)s_adcDma, 2U) != HAL_OK)
    {
        Error_Handler();
    }

    /* 7) 关掉 DMA 中断：我们只轮询缓冲区。
       否则每 42 us 就会进一次 DMA1_Channel1 中断，白白吃掉 CPU 时间。 */
    __HAL_DMA_DISABLE_IT(hadc1.DMA_Handle, DMA_IT_TC | DMA_IT_HT | DMA_IT_TE);
}

/* 按 F103 数据手册的公式换算：
     Vdda  = 1.20 V * 4095 / Vrefint_raw
     Vtemp = Temp_raw * Vdda / 4095
     Temp  = 25 °C + (1.43 V - Vtemp) / 4.3 mV */
static void AdcApp_Format(uint32_t tempRaw, uint32_t vrefRaw)
{
    float vdda;
    float vtemp;

    /* Vrefint 码值为 0 说明还没采到有效数据，保留上一次的值 */
    if ((vrefRaw == 0U) || (vrefRaw > ADC_FULL_SCALE))
    {
        return;
    }

    vdda  = ADC_VREFINT_V * (float)ADC_FULL_SCALE / (float)vrefRaw;
    vtemp = (float)tempRaw * vdda / (float)ADC_FULL_SCALE;

    g_adcTempC = 25.0f + (ADC_V25_V - vtemp) / ADC_SLOPE_V_PER_C;
    g_adcVddV  = vdda;
}

void AdcApp_Poll(void)
{
    uint32_t tempRaw;
    uint32_t vrefRaw;

    /* DMA 一直在改写缓冲区，这里各取一次快照即可 */
    tempRaw = s_adcDma[0];
    vrefRaw = s_adcDma[1];

    s_sumTemp += tempRaw;
    s_sumVref += vrefRaw;
    s_count++;

    if ((HAL_GetTick() - s_lastTick) < ADC_APP_REFRESH_MS)
    {
        return;
    }
    s_lastTick = HAL_GetTick();

    if (s_count != 0U)
    {
        AdcApp_Format(s_sumTemp / s_count, s_sumVref / s_count);
    }

    s_sumTemp = 0U;
    s_sumVref = 0U;
    s_count   = 0U;
}
