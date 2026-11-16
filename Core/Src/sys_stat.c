/*
 * 系统资源占用统计：CPU 负载 / RAM 占用率 / Flash 占用率
 *
 * 主循环保持全速运行、不做帧率限制，因此 UI 动画速度与加统计之前完全一致。
 *
 * CPU 占用率的口径是“主循环一轮的平均耗时占 20 ms 参考周期的比例”，
 * 用来衡量还剩多少性能余量：显示 15% 表示一帧只用掉 20 ms 预算的 15%。
 * 之所以不锁帧，是因为本框架的动画按帧计数（每帧固定位移），
 * 一旦限制帧率，动画速度就会跟着变慢。
 *
 * CPU% = 平均单帧耗时 / 20 ms
 */
#include "sys_stat.h"
#include "main.h"

/* ---------------- 可调参数 ---------------- */
#define SYS_STAT_SYSCLK_HZ      72000000U                              /* 系统主频 72 MHz */
#define SYS_STAT_REF_MS         20U                                    /* CPU 占用率的参考周期，不影响实际帧率 */
#define SYS_STAT_REF_CYCLES     (SYS_STAT_SYSCLK_HZ / 1000U * SYS_STAT_REF_MS)

/* ---------------- 存储器容量 ---------------- */
#define SYS_STAT_FLASH_BASE     0x08000000U
#define SYS_STAT_FLASH_TOTAL    (64U * 1024U)                          /* STM32F103C8T6 片内 Flash 64 KB */
#define SYS_STAT_RAM_BASE       0x20000000U
#define SYS_STAT_RAM_TOTAL      (20U * 1024U)                          /* STM32F103C8T6 片内 SRAM 20 KB */

/* Keil 链接器生成的符号：符号的地址就是它代表的边界值
 *   Load$$RW_IRAM1$$Limit     : RW 初值在 Flash 中的结束地址 = Flash 实际占用末尾
 *   Image$$RW_IRAM1$$ZI$$Limit: RAM 执行域 ZI 末尾，含 Stack / Heap 预留
 */
extern uint32_t Load$$RW_IRAM1$$Limit[];
extern uint32_t Image$$RW_IRAM1$$ZI$$Limit[];

float g_sysCpuPct = 0.0f;
float g_sysRamPct = 0.0f;
float g_sysRomPct = 0.0f;

static uint32_t s_frameStart;       /* 当前帧的起始 DWT 周期 */
static uint32_t s_busyCycles;       /* 本次统计窗口内累计的忙周期 */
static uint32_t s_frameCount;       /* 本次统计窗口内跑过的帧数 */
static uint32_t s_lastTick;         /* 上次结算时的 HAL tick */

void SysStat_Init(void)
{
  uint32_t romUsed;
  uint32_t ramUsed;

  /* 链接器符号的地址减去存储区基址，就是已占用的字节数 */
  romUsed = (uint32_t)Load$$RW_IRAM1$$Limit - SYS_STAT_FLASH_BASE;
  ramUsed = (uint32_t)Image$$RW_IRAM1$$ZI$$Limit - SYS_STAT_RAM_BASE;

  g_sysRomPct = (float)romUsed * 100.0f / (float)SYS_STAT_FLASH_TOTAL;
  g_sysRamPct = (float)ramUsed * 100.0f / (float)SYS_STAT_RAM_TOTAL;

  /* 使能 DWT 周期计数器，Cortex-M3 自带，不占用任何外设 */
  CoreDebug->DEMCR |= CoreDebug_DEMCR_TRCENA_Msk;
  DWT->CYCCNT = 0U;
  DWT->CTRL |= DWT_CTRL_CYCCNTENA_Msk;

  s_frameStart = DWT->CYCCNT;
  s_busyCycles = 0U;
  s_frameCount = 0U;
  s_lastTick   = HAL_GetTick();
}

void SysStat_FrameBegin(void)
{
  s_frameStart = DWT->CYCCNT;
}

void SysStat_FrameEnd(void)
{
  uint32_t now;
  uint32_t dt;
  float    total;

  /* 累加本帧的实际执行周期 */
  s_busyCycles += DWT->CYCCNT - s_frameStart;
  s_frameCount++;

  /* 每 1 秒结算一次：分子是本窗口内的总忙周期，分母是“帧数 × 参考周期” */
  now = HAL_GetTick();
  dt  = now - s_lastTick;
  if (dt >= 1000U)
  {
    total       = (float)s_frameCount * (float)SYS_STAT_REF_CYCLES;
    g_sysCpuPct = (float)s_busyCycles * 100.0f / total;
    if (g_sysCpuPct > 100.0f) { g_sysCpuPct = 100.0f; }
    s_busyCycles = 0U;
    s_frameCount = 0U;
    s_lastTick   = now;
  }
}
