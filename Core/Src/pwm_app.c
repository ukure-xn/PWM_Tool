/*
 * PWM 应用层：TIM4 CH3 / CH4 两路 PWM
 *
 * 频率和占空比的换算见 pwm_app.h 顶部注释。
 * 扫频由 PwmApp_Poll() 驱动，节拍取自 HAL_GetTick()，不依赖任何中断。
 *
 * 频率扫频在 PSC 域逐 1 步进，而不是在 8 档频率表上跳。
 * 起止频率换算成 PSC 后，从起点走到终点，到端点自动折返，
 * 所以天然是"从小到大再从大到小"反复循环。
 */
#include "pwm_app.h"
#include "main.h"
#include "tim.h"
#include "OLED_UI_MenuData.h"

/* 当前页面指针由 OLED_UI 框架维护，用来判断是否已经离开 PWM / Sweep 页面 */
extern MenuPage* CurrentMenuPage;

/* ---------------- 时基参数 ---------------- */
#define PWM_TIM_CLK_HZ      72000000UL          /* APB1 = 36 MHz，定时器时钟 x2 = 72 MHz */
#define PWM_ARR             3599UL              /* 3600 个计数一个 PWM 周期 */
#define PWM_PERIOD_COUNTS   (PWM_ARR + 1UL)
#define PWM_PSC_INVALID     0xFFFFU             /* 强制下一帧写 PSC 的哨兵值 */
#define PWM_POLL_DT_MAX_MS  100UL               /* 单帧时间上限，防止卡顿后一次跳很多步 */

/* 通道序号 -> HAL 通道号 */
static const uint32_t s_timChannel[PWM_CH_COUNT] = {TIM_CHANNEL_3, TIM_CHANNEL_4};

/* ---------------- 频率表 ---------------- */
const uint16_t PwmFreqTable[PWM_FREQ_COUNT] = {100, 200, 500, 1000, 2000, 5000, 10000, 20000};
const char*    PwmFreqNames[PWM_FREQ_COUNT] = {"100 Hz", "200 Hz", "500 Hz", "1 kHz",
                                              "2 kHz", "5 kHz", "10 kHz", "20 kHz"};

/* 扫频单程时间：从起点走到终点用多少秒 */
const uint16_t PwmSweepTimeTable[PWM_SWEEP_TIME_COUNT] = {2, 5, 10, 20, 30, 60};
const char*    PwmSweepTimeNames[PWM_SWEEP_TIME_COUNT] = {"2s", "5s", "10s", "20s", "30s", "60s"};

/* ---------------- 菜单绑定的变量 ---------------- */
int8_t g_pwmFreqIndex          = 3;                 /* 默认 1 kHz */
int8_t g_pwmDuty[PWM_CH_COUNT] = {50, 50};          /* 默认 50 % */
int8_t g_pwmOut[PWM_CH_COUNT]  = {0, 0};            /* 默认不输出 */

bool   g_pwmFreqSweep               = false;
bool   g_pwmDutySweep[PWM_CH_COUNT] = {false, false};

int8_t g_pwmSweepFreqFromIdx = 0;                   /* 100 Hz */
int8_t g_pwmSweepFreqToIdx   = 7;                   /* 20 kHz */
int8_t g_pwmSweepFreqTimeIdx = 2;                   /* 10 秒单程 */

int8_t g_pwmSweepDutyFrom[PWM_CH_COUNT] = {0, 0};
int8_t g_pwmSweepDutyTo[PWM_CH_COUNT]   = {100, 100};
int8_t g_pwmSweepDutyTimeIdx = 2;                   /* 10 秒单程 */

/* ---------------- 频率扫频运行状态 ---------------- */
static uint16_t s_freqPscFrom   = 199U;             /* 起点 PSC（100 Hz） */
static uint16_t s_freqPscTo     = 0U;               /* 终点 PSC（20 kHz） */
static uint16_t s_freqPsc       = 199U;             /* 当前 PSC */
static int8_t   s_freqDir       = -1;               /* -1 = PSC 递减（频率升） */
static uint32_t s_freqAccMs     = 0U;
static uint32_t s_freqStepMs    = 50U;
static int8_t   s_freqCfgFrom   = -1;               /* 配置快照，变化时重置扫频 */
static int8_t   s_freqCfgTo     = -1;
static int8_t   s_freqCfgTime   = -1;
static bool     s_freqSweepPrev = false;

/* ---------------- 脉宽扫频运行状态 ---------------- */
static int8_t   s_dutyFrom[PWM_CH_COUNT]    = {0, 0};
static int8_t   s_dutyTo[PWM_CH_COUNT]      = {100, 100};
static int8_t   s_dutyVal[PWM_CH_COUNT]     = {0, 0};
static int8_t   s_dutyDir[PWM_CH_COUNT]     = {1, 1};
static uint32_t s_dutyAccMs[PWM_CH_COUNT]   = {0U, 0U};
static uint32_t s_dutyStepMs[PWM_CH_COUNT]  = {100U, 100U};
static int8_t   s_dutyCfgFrom[PWM_CH_COUNT] = {-1, -1};
static int8_t   s_dutyCfgTo[PWM_CH_COUNT]   = {-1, -1};
static int8_t   s_dutyCfgTime               = -1;
static bool     s_dutySweepPrev[PWM_CH_COUNT] = {false, false};

static uint16_t s_appliedPsc   = PWM_PSC_INVALID;
static uint32_t s_lastPollTick = 0U;
static MenuPage* s_lastPage    = NULL;

/* 频率 -> 预分频。表内频点都能整除，不需要四舍五入 */
static uint16_t PwmApp_PscFromHz(uint16_t hz)
{
    uint32_t psc;

    if (hz == 0U) { return 0U; }

    psc = PWM_TIM_CLK_HZ / ((uint32_t)hz * PWM_PERIOD_COUNTS);
    return (uint16_t)(psc - 1UL);
}

/* 预分频 -> 频率，用于扫频关闭时把当前位置吸附回最近的档位 */
static uint16_t PwmApp_HzFromPsc(uint16_t psc)
{
    return (uint16_t)(PWM_TIM_CLK_HZ / ((uint32_t)(psc + 1U) * PWM_PERIOD_COUNTS));
}

/* 找离给定频率最近的档位索引 */
static int8_t PwmApp_NearestFreqIndex(uint16_t hz)
{
    uint8_t  i;
    uint8_t  best     = 0U;
    uint32_t bestDiff = 0xFFFFFFFFUL;

    for (i = 0U; i < PWM_FREQ_COUNT; i++) {
        uint32_t diff = (PwmFreqTable[i] > hz) ? ((uint32_t)PwmFreqTable[i] - hz)
                                               : ((uint32_t)hz - PwmFreqTable[i]);
        if (diff < bestDiff) {
            bestDiff = diff;
            best     = i;
        }
    }
    return (int8_t)best;
}

/* 按起止档位和时间重算扫频参数，并把当前位置拉回起点 */
static void PwmApp_ResetFreqSweep(void)
{
    uint16_t span;
    uint32_t sec;

    s_freqPscFrom = PwmApp_PscFromHz(PwmFreqTable[g_pwmSweepFreqFromIdx]);
    s_freqPscTo   = PwmApp_PscFromHz(PwmFreqTable[g_pwmSweepFreqToIdx]);

    span = (s_freqPscFrom > s_freqPscTo) ? (uint16_t)(s_freqPscFrom - s_freqPscTo)
                                         : (uint16_t)(s_freqPscTo - s_freqPscFrom);
    sec  = PwmSweepTimeTable[g_pwmSweepFreqTimeIdx];

    s_freqPsc    = s_freqPscFrom;
    s_freqDir    = (s_freqPscTo > s_freqPscFrom) ? 1 : -1;
    s_freqAccMs  = 0U;
    s_freqStepMs = (span == 0U) ? 0U : (sec * 1000UL / span);
    if ((s_freqStepMs == 0U) && (span != 0U)) { s_freqStepMs = 1U; }

    s_freqCfgFrom = g_pwmSweepFreqFromIdx;
    s_freqCfgTo   = g_pwmSweepFreqToIdx;
    s_freqCfgTime = g_pwmSweepFreqTimeIdx;
}

/* 频率扫频推进：在起止 PSC 之间往返，每步 1 个预分频单位 */
static void PwmApp_FreqAdvance(uint32_t dtMs)
{
    uint16_t lo = (s_freqPscFrom < s_freqPscTo) ? s_freqPscFrom : s_freqPscTo;
    uint16_t hi = (s_freqPscFrom < s_freqPscTo) ? s_freqPscTo   : s_freqPscFrom;

    if ((lo == hi) || (s_freqStepMs == 0U)) { return; }

    s_freqAccMs += dtMs;
    while (s_freqAccMs >= s_freqStepMs) {
        s_freqAccMs -= s_freqStepMs;

        if (s_freqDir < 0) {
            if (s_freqPsc <= lo) { s_freqPsc = lo; s_freqDir = 1; }
            else                 { s_freqPsc--; }
        } else {
            if (s_freqPsc >= hi) { s_freqPsc = hi; s_freqDir = -1; }
            else                 { s_freqPsc++; }
        }
    }
}

/* 按起止占空比和时间重算某个通道的扫频参数，并拉回起点 */
static void PwmApp_ResetDutySweep(uint8_t ch)
{
    int8_t   from = g_pwmSweepDutyFrom[ch];
    int8_t   to   = g_pwmSweepDutyTo[ch];
    int8_t   lo   = (from < to) ? from : to;
    int8_t   hi   = (from < to) ? to   : from;
    uint16_t span = (uint16_t)(hi - lo);
    uint32_t sec  = PwmSweepTimeTable[g_pwmSweepDutyTimeIdx];

    s_dutyFrom[ch]   = from;
    s_dutyTo[ch]     = to;
    s_dutyVal[ch]    = from;
    s_dutyDir[ch]    = (to > from) ? 1 : -1;
    s_dutyAccMs[ch]  = 0U;
    s_dutyStepMs[ch] = (span == 0U) ? 0U : (sec * 1000UL / span);
    if ((s_dutyStepMs[ch] == 0U) && (span != 0U)) { s_dutyStepMs[ch] = 1U; }

    s_dutyCfgFrom[ch] = from;
    s_dutyCfgTo[ch]   = to;
}

/* 脉宽扫频推进：在起止占空比之间往返，每步 1 % */
static void PwmApp_DutyAdvance(uint8_t ch, uint32_t dtMs)
{
    int8_t lo = (s_dutyFrom[ch] < s_dutyTo[ch]) ? s_dutyFrom[ch] : s_dutyTo[ch];
    int8_t hi = (s_dutyFrom[ch] < s_dutyTo[ch]) ? s_dutyTo[ch]   : s_dutyFrom[ch];

    if ((lo == hi) || (s_dutyStepMs[ch] == 0U)) { return; }

    s_dutyAccMs[ch] += dtMs;
    while (s_dutyAccMs[ch] >= s_dutyStepMs[ch]) {
        s_dutyAccMs[ch] -= s_dutyStepMs[ch];

        if (s_dutyDir[ch] < 0) {
            if (s_dutyVal[ch] <= lo) { s_dutyVal[ch] = lo; s_dutyDir[ch] = 1; }
            else                     { s_dutyVal[ch]--; }
        } else {
            if (s_dutyVal[ch] >= hi) { s_dutyVal[ch] = hi; s_dutyDir[ch] = -1; }
            else                     { s_dutyVal[ch]++; }
        }
    }
}

/* 判断页面是否属于 PWM / Sweep 页面树（含 Freq Setup / Duty Setup 子页）。
   向上回溯父页面，所以进入子页时仍然算在树里，只有回到主菜单才算离开。 */
static bool PwmApp_InPwmTree(MenuPage* page)
{
    while (page != NULL) {
        if ((page == &PwmMenuPage) || (page == &SweepMenuPage)) { return true; }
        page = page->General_ParentMenuPage;
    }
    return false;
}

/* 把某个通道的占空比写进比较寄存器；输出关闭时写 0，引脚恒为低电平 */
static void PwmApp_ApplyDuty(uint8_t ch)
{
    uint32_t ccr = 0U;

    if (g_pwmOut[ch]) {
        uint8_t duty = g_pwmDutySweep[ch] ? (uint8_t)s_dutyVal[ch]
                                          : (uint8_t)g_pwmDuty[ch];
        ccr = (uint32_t)duty * PWM_PERIOD_COUNTS / 100UL;
    }
    __HAL_TIM_SET_COMPARE(&htim4, s_timChannel[ch], ccr);
}

void PwmApp_Init(void)
{
    /* .ioc 里 TIM4 已配成 ARR=3599（3600 计数一个周期），这里再写一次做兜底；
       频率交给预分频，这样所有频点的占空比分辨率都是 1/3600 */
    htim4.Init.Period = PWM_ARR;
    __HAL_TIM_SET_AUTORELOAD(&htim4, PWM_ARR);

    s_appliedPsc = PwmApp_PscFromHz(PwmFreqTable[g_pwmFreqIndex]);
    __HAL_TIM_SET_PRESCALER(&htim4, s_appliedPsc);

    /* 立刻把 ARR / PSC 的影子寄存器装载进去，避免第一个周期还用旧值 */
    htim4.Instance->EGR = TIM_EGR_UG;
    htim4.Instance->SR  = 0U;

    HAL_TIM_PWM_Start(&htim4, TIM_CHANNEL_3);
    HAL_TIM_PWM_Start(&htim4, TIM_CHANNEL_4);

    PwmApp_ResetFreqSweep();
    PwmApp_ResetDutySweep(PWM_CH3);
    PwmApp_ResetDutySweep(PWM_CH4);
    s_dutyCfgTime = g_pwmSweepDutyTimeIdx;

    s_lastPollTick = HAL_GetTick();

    PwmApp_ApplyDuty(PWM_CH3);
    PwmApp_ApplyDuty(PWM_CH4);
}

void PwmApp_Poll(void)
{
    uint32_t now = HAL_GetTick();
    uint32_t dt  = now - s_lastPollTick;
    uint16_t psc;
    uint8_t  ch;

    s_lastPollTick = now;
    if (dt > PWM_POLL_DT_MAX_MS) { dt = PWM_POLL_DT_MAX_MS; }

    /* 离开 PWM / Sweep 页面树时立即停止输出，并关掉两路扫频开关。
       起止值、时间这些参数保留，下次进页面重新打开即可。 */
    if ((s_lastPage != NULL) && PwmApp_InPwmTree(s_lastPage) && !PwmApp_InPwmTree(CurrentMenuPage)) {
        g_pwmOut[PWM_CH3]       = 0;
        g_pwmOut[PWM_CH4]       = 0;
        g_pwmFreqSweep          = false;
        g_pwmDutySweep[PWM_CH3] = false;
        g_pwmDutySweep[PWM_CH4] = false;
    }
    s_lastPage = CurrentMenuPage;

    /* ---------- 频率扫频 ---------- */
    if ((g_pwmSweepFreqFromIdx != s_freqCfgFrom) ||
        (g_pwmSweepFreqToIdx   != s_freqCfgTo)   ||
        (g_pwmSweepFreqTimeIdx != s_freqCfgTime)) {
        PwmApp_ResetFreqSweep();
    }

    if (g_pwmFreqSweep && !s_freqSweepPrev) {
        PwmApp_ResetFreqSweep();                    /* 开启时从起点开始扫 */
    }
    if (!g_pwmFreqSweep && s_freqSweepPrev) {
        /* 关闭扫频：把当前位置吸附到最近的档位，让菜单显示和硬件一致 */
        g_pwmFreqIndex = PwmApp_NearestFreqIndex(PwmApp_HzFromPsc(s_freqPsc));
        s_appliedPsc   = PWM_PSC_INVALID;
    }
    s_freqSweepPrev = g_pwmFreqSweep;

    if (g_pwmFreqSweep) {
        PwmApp_FreqAdvance(dt);
    }

    /* ---------- 脉宽扫频 ---------- */
    if (g_pwmSweepDutyTimeIdx != s_dutyCfgTime) {
        s_dutyCfgTime = g_pwmSweepDutyTimeIdx;
        PwmApp_ResetDutySweep(PWM_CH3);
        PwmApp_ResetDutySweep(PWM_CH4);
    }

    for (ch = 0U; ch < PWM_CH_COUNT; ch++) {
        if ((g_pwmSweepDutyFrom[ch] != s_dutyCfgFrom[ch]) ||
            (g_pwmSweepDutyTo[ch]   != s_dutyCfgTo[ch])) {
            PwmApp_ResetDutySweep(ch);
        }

        if (g_pwmDutySweep[ch] && !s_dutySweepPrev[ch]) {
            PwmApp_ResetDutySweep(ch);              /* 开启时从起点开始扫 */
        }
        if (!g_pwmDutySweep[ch] && s_dutySweepPrev[ch]) {
            g_pwmDuty[ch] = s_dutyVal[ch];          /* 关闭时停在当前值 */
        }
        s_dutySweepPrev[ch] = g_pwmDutySweep[ch];

        if (g_pwmDutySweep[ch]) {
            PwmApp_DutyAdvance(ch, dt);
        }
    }

    /* ---------- 写硬件 ---------- */
    /* 频率只在目标 PSC 变化时写寄存器：写 PSC 要等下一个更新事件才生效，
       每帧都写没有意义 */
    psc = g_pwmFreqSweep ? s_freqPsc : PwmApp_PscFromHz(PwmFreqTable[g_pwmFreqIndex]);
    if (psc != s_appliedPsc) {
        __HAL_TIM_SET_PRESCALER(&htim4, psc);
        s_appliedPsc = psc;
    }

    /* 占空比每个循环都刷新，两条寄存器写，开销可以忽略 */
    PwmApp_ApplyDuty(PWM_CH3);
    PwmApp_ApplyDuty(PWM_CH4);
}
