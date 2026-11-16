#include "OLED_UI_MenuData.h"
#include "OLED_UI.h"
#include "OLED_Fonts.h"
#include "adc_app.h"
#include "sys_stat.h"
#include "pwm_app.h"
/* Menu animation speed (framework unit, larger = faster) */
#define SPEED   5

/* Globals owned by the UI framework (OLED_UI.c) */
extern int16_t OLED_UI_Brightness;
extern bool    OLED_UI_SaverEnabled;
extern bool    OLED_UI_ShowFps;

/* ---------------------------------------------------------------- */
/* PWM / Sweep 菜单绑定的变量和选择器                                */
/* ---------------------------------------------------------------- */
/* TIM4 的 CH3 / CH4 共用一个时基，两路频率必然相同，所以两个 Freq 行
   绑定同一个 g_pwmFreqIndex：改任意一个，两行同时变化。
   变量本体在 pwm_app.c 里，菜单只持有指针。 */

static MenuSelector PwmFreqSelector = {
    .Items             = PwmFreqNames,
    .Count             = PWM_FREQ_COUNT,
    .Index             = &g_pwmFreqIndex,
};

static MenuSelector PwmCh3DutySelector = {
    .Items             = NULL,          /* 数值模式：Min + Index * Step */
    .Count             = 101,           /* 0..100 %，步进 1 */
    .Index             = &g_pwmDuty[PWM_CH3],
    .Min               = 0,
    .Max               = 100,
    .Step              = 1,
};

static MenuSelector PwmCh4DutySelector = {
    .Items             = NULL,
    .Count             = 101,
    .Index             = &g_pwmDuty[PWM_CH4],
    .Min               = 0,
    .Max               = 100,
    .Step              = 1,
};

/* 输出使能：用滚动选择器显示 OFF / ON */
static const char* PwmOnOffNames[2] = {"[OFF]", "[ON]"};

static MenuSelector PwmOut3Selector = {
    .Items             = PwmOnOffNames,
    .Count             = 2,
    .Index             = &g_pwmOut[PWM_CH3],
};

static MenuSelector PwmOut4Selector = {
    .Items             = PwmOnOffNames,
    .Count             = 2,
    .Index             = &g_pwmOut[PWM_CH4],
};

/* 频率扫频：起止档位 + 单程时间 */
static MenuSelector PwmSweepFreqFromSelector = {
    .Items             = PwmFreqNames,
    .Count             = PWM_FREQ_COUNT,
    .Index             = &g_pwmSweepFreqFromIdx,
};

static MenuSelector PwmSweepFreqToSelector = {
    .Items             = PwmFreqNames,
    .Count             = PWM_FREQ_COUNT,
    .Index             = &g_pwmSweepFreqToIdx,
};

static MenuSelector PwmSweepFreqTimeSelector = {
    .Items             = PwmSweepTimeNames,
    .Count             = PWM_SWEEP_TIME_COUNT,
    .Index             = &g_pwmSweepFreqTimeIdx,
};

/* 脉宽扫频：两通道各自的起止占空比，共用一个单程时间 */
static MenuSelector PwmSweepDutyFrom3Selector = {
    .Items             = NULL,          /* 数值模式：Min + Index * Step */
    .Count             = 101,
    .Index             = &g_pwmSweepDutyFrom[PWM_CH3],
    .Min               = 0,
    .Max               = 100,
    .Step              = 1,
};

static MenuSelector PwmSweepDutyTo3Selector = {
    .Items             = NULL,
    .Count             = 101,
    .Index             = &g_pwmSweepDutyTo[PWM_CH3],
    .Min               = 0,
    .Max               = 100,
    .Step              = 1,
};

static MenuSelector PwmSweepDutyFrom4Selector = {
    .Items             = NULL,
    .Count             = 101,
    .Index             = &g_pwmSweepDutyFrom[PWM_CH4],
    .Min               = 0,
    .Max               = 100,
    .Step              = 1,
};

static MenuSelector PwmSweepDutyTo4Selector = {
    .Items             = NULL,
    .Count             = 101,
    .Index             = &g_pwmSweepDutyTo[PWM_CH4],
    .Min               = 0,
    .Max               = 100,
    .Step              = 1,
};

static MenuSelector PwmSweepDutyTimeSelector = {
    .Items             = PwmSweepTimeNames,
    .Count             = PWM_SWEEP_TIME_COUNT,
    .Index             = &g_pwmSweepDutyTimeIdx,
};

/* ---------------------------------------------------------------- */
/* Backlight / anti burn-in                                         */
/* ---------------------------------------------------------------- */
static int8_t BrightnessIndex = 0;  /* 10 + 0 * 10 = 10 */

static void BrightnessChanged(int8_t index)
{
    OLED_UI_Brightness = (int16_t)(10 + index * 10);
}

static MenuSelector BrightnessSelector = {
    .Items             = NULL,
    .Count             = 10,            /* 10..100 with step 10 */
    .Index             = &BrightnessIndex,
    .Callback          = BrightnessChanged,
    .Min               = 10,
    .Max               = 100,
    .Step              = 10,
    .CallbackImmediate = true,
};


/* ---------------------------------------------------------------- */
/* Menu items                                                       */
/* ---------------------------------------------------------------- */
MenuItem PwmMenuItems[] = {
	  {.General_item_text = "[PWM]",      .General_callback = OLED_UI_Back},
    {.General_item_text = "Ch3&4 Freq",   .List_Selector = &PwmFreqSelector},
    {.General_item_text = "Ch3 Duty%",  .List_Selector = &PwmCh3DutySelector},
    {.General_item_text = "Ch3 Out",    .List_Selector = &PwmOut3Selector},
    {.General_item_text = "Ch4 Duty",  .List_Selector = &PwmCh4DutySelector},
    {.General_item_text = "Ch4 Out",    .List_Selector = &PwmOut4Selector},
    {.General_item_text = NULL},
};

MenuItem SweepMenuItems[] = {
    {.General_item_text = "[Sweep]",      .General_callback = OLED_UI_Back},
    {.General_item_text = "FreqSw",       .List_BoolRadioBox = &g_pwmFreqSweep},
    {.General_item_text = "Freq Setup",   .General_SubMenuPage = &SweepFreqPage},
    {.General_item_text = "Ch3 DtySw",    .List_BoolRadioBox = &g_pwmDutySweep[PWM_CH3]},
    {.General_item_text = "Ch4 DtySw",    .List_BoolRadioBox = &g_pwmDutySweep[PWM_CH4]},
    {.General_item_text = "Duty Setup",   .General_SubMenuPage = &SweepDutyPage},
    {.General_item_text = "Ch3 Out",      .List_Selector = &PwmOut3Selector},
    {.General_item_text = "Ch4 Out",      .List_Selector = &PwmOut4Selector},
    {.General_item_text = NULL},
};

MenuItem SweepFreqItems[] = {
    {.General_item_text = "[Freq Set]",   .General_callback = OLED_UI_Back},
    {.General_item_text = "From",         .List_Selector = &PwmSweepFreqFromSelector},
    {.General_item_text = "To",           .List_Selector = &PwmSweepFreqToSelector},
    {.General_item_text = "Time",         .List_Selector = &PwmSweepFreqTimeSelector},
    {.General_item_text = NULL},
};

MenuItem SweepDutyItems[] = {
    {.General_item_text = "[Duty Set]",   .General_callback = OLED_UI_Back},
    {.General_item_text = "Ch3 From",     .List_Selector = &PwmSweepDutyFrom3Selector},
    {.General_item_text = "Ch3 To",       .List_Selector = &PwmSweepDutyTo3Selector},
    {.General_item_text = "Ch4 From",     .List_Selector = &PwmSweepDutyFrom4Selector},
    {.General_item_text = "Ch4 To",       .List_Selector = &PwmSweepDutyTo4Selector},
    {.General_item_text = "Time",         .List_Selector = &PwmSweepDutyTimeSelector},
    {.General_item_text = NULL},
};

MenuItem SettingsMenuItems[] = {
	  {.General_item_text = "[Seting]",     .General_callback = OLED_UI_Back},
    {.General_item_text = "Brightness", .List_Selector = &BrightnessSelector},
    {.General_item_text = "Anti-burn",  .List_BoolRadioBox = &OLED_UI_SaverEnabled},
    {.General_item_text = "Show Stat",  .List_BoolRadioBox = &OLED_UI_ShowFps},

    {.General_item_text = NULL},
};


MenuItem AboutMenuItems[] = {
    {.General_item_text = "[About]", .General_callback = OLED_UI_Back},
		{.General_item_text = "Device:PWM Tool"},
    {.General_item_text = "MCU:STM32F103C8"},
    {.General_item_text = "OLED:SSD1306"},
    {.General_item_text = "UI: OLED_UI"},
		{.General_item_text = "ROM: 64K"},
		{.General_item_text = "RAM: 20K"},
    /* HW / FW / BL / build: text pointers are filled by AppInfo_BindToMenu() */
    {.General_item_text = "HW:  --"},
    {.General_item_text = "FW:  --"},
    {.General_item_text = "BL:  --"},
    {.General_item_text = "Build:  -"},
		{.General_item_text = "-------------------"},

    /* TEMP / VDD：数值由框架的 List_FloatBox 右对齐打印 */
    {.General_item_text = "TEMP(C):", .List_FloatBox = &g_adcTempC},
    {.General_item_text = "VDD(V):",  .List_FloatBox = &g_adcVddV},

    /* CPU / RAM / Flash 占用率，由 sys_stat 每 1 秒刷新一次 */
    {.General_item_text = "CPU(%%):", .List_FloatBox = &g_sysCpuPct, .List_FloatBoxDecimals = 1U},
    {.General_item_text = "RAM(%%):", .List_FloatBox = &g_sysRamPct, .List_FloatBoxDecimals = 1U},
    {.General_item_text = "ROM(%%):", .List_FloatBox = &g_sysRomPct, .List_FloatBoxDecimals = 1U},

    {.General_item_text = NULL},
};

MenuItem MainMenuItems[] = {
    {.General_item_text = "PWM",      .General_SubMenuPage = &PwmMenuPage,      .Tiles_Icon = IconPwm},
		{.General_item_text = "Sweep",    .General_SubMenuPage = &SweepMenuPage,    .Tiles_Icon = IconSweep},
    {.General_item_text = "Settings", .General_SubMenuPage = &SettingsMenuPage, .Tiles_Icon = IconSettings},
    {.General_item_text = "About",   .General_SubMenuPage = &AboutMenuPage,   .Tiles_Icon = IconAbout},
    {.General_item_text = NULL},
};

/* ---------------------------------------------------------------- */
/* Menu pages                                                       */
/* ---------------------------------------------------------------- */
MenuPage PwmMenuPage = {
    .General_MenuType     = MENU_TYPE_LIST,
    .General_CursorStyle  = REVERSE_ROUNDRECTANGLE,
    .General_FontSize     = OLED_UI_FONT_8,
    .General_ParentMenuPage = &MainMenuPage,
    .General_LineSpace    = 2,
    .General_MoveStyle    = UNLINEAR,
    .General_MovingSpeed  = SPEED,
    .General_MenuItems    = PwmMenuItems,

    .List_MenuArea        = {0, 0, 128, 32},
    .List_IfDrawFrame     = false,
    .List_IfDrawLinePerfix = false,
    .List_IfScrollBar     = true,
    .List_IfExecuteOnSelect = false,
    .List_StartPointX     = 2,
    .List_StartPointY     = 2,
};

MenuPage SettingsMenuPage = {
    .General_MenuType     = MENU_TYPE_LIST,
    .General_CursorStyle  = REVERSE_ROUNDRECTANGLE,
    .General_FontSize     = OLED_UI_FONT_8,
    .General_ParentMenuPage = &MainMenuPage,
    .General_LineSpace    = 2,
    .General_MoveStyle    = UNLINEAR,
    .General_MovingSpeed  = SPEED,
    .General_MenuItems    = SettingsMenuItems,

    .List_MenuArea        = {0, 0, 128, 32},
    .List_IfDrawFrame     = false,
    .List_IfDrawLinePerfix = false,
    .List_IfScrollBar     = true,
    .List_IfExecuteOnSelect = false,
    .List_StartPointX     = 2,
    .List_StartPointY     = 2,
};

MenuPage AboutMenuPage = {
    .General_MenuType     = MENU_TYPE_LIST,
    .General_CursorStyle  = REVERSE_ROUNDRECTANGLE,
    .General_FontSize     = OLED_UI_FONT_8,
    .General_ParentMenuPage = &MainMenuPage,
    .General_LineSpace    = 2,
    .General_MoveStyle    = UNLINEAR,
    .General_MovingSpeed  = SPEED,
    .General_MenuItems    = AboutMenuItems,

    .List_MenuArea        = {0, 0, 128, 32},
    .List_IfDrawFrame     = false,
    .List_IfDrawLinePerfix = false,
    .List_IfScrollBar     = true,
    .List_IfExecuteOnSelect = false,
    .List_StartPointX     = 2,
    .List_StartPointY     = 2,
};

MenuPage SweepMenuPage = {
    .General_MenuType     = MENU_TYPE_LIST,
    .General_CursorStyle  = REVERSE_ROUNDRECTANGLE,
    .General_FontSize     = OLED_UI_FONT_8,
    .General_ParentMenuPage = &MainMenuPage,
    .General_LineSpace    = 2,
    .General_MoveStyle    = UNLINEAR,
    .General_MovingSpeed  = SPEED,
    .General_MenuItems    = SweepMenuItems,

    .List_MenuArea        = {0, 0, 128, 32},
    .List_IfDrawFrame     = false,
    .List_IfDrawLinePerfix = false,
    .List_IfScrollBar     = true,
    .List_IfExecuteOnSelect = false,
    .List_StartPointX     = 2,
    .List_StartPointY     = 2,
};

MenuPage SweepFreqPage = {
    .General_MenuType     = MENU_TYPE_LIST,
    .General_CursorStyle  = REVERSE_ROUNDRECTANGLE,
    .General_FontSize     = OLED_UI_FONT_8,
    .General_ParentMenuPage = &SweepMenuPage,
    .General_LineSpace    = 2,
    .General_MoveStyle    = UNLINEAR,
    .General_MovingSpeed  = SPEED,
    .General_MenuItems    = SweepFreqItems,

    .List_MenuArea        = {0, 0, 128, 32},
    .List_IfDrawFrame     = false,
    .List_IfDrawLinePerfix = false,
    .List_IfScrollBar     = true,
    .List_IfExecuteOnSelect = false,
    .List_StartPointX     = 2,
    .List_StartPointY     = 2,
};

MenuPage SweepDutyPage = {
    .General_MenuType     = MENU_TYPE_LIST,
    .General_CursorStyle  = REVERSE_ROUNDRECTANGLE,
    .General_FontSize     = OLED_UI_FONT_8,
    .General_ParentMenuPage = &SweepMenuPage,
    .General_LineSpace    = 2,
    .General_MoveStyle    = UNLINEAR,
    .General_MovingSpeed  = SPEED,
    .General_MenuItems    = SweepDutyItems,

    .List_MenuArea        = {0, 0, 128, 32},
    .List_IfDrawFrame     = false,
    .List_IfDrawLinePerfix = false,
    .List_IfScrollBar     = true,
    .List_IfExecuteOnSelect = false,
    .List_StartPointX     = 2,
    .List_StartPointY     = 2,
};

MenuPage MainMenuPage = {
    .General_MenuType     = MENU_TYPE_TILES,
    .General_CursorStyle  = REVERSE_ROUNDRECTANGLE,
    .General_FontSize     = OLED_UI_FONT_8,
    .General_ParentMenuPage = NULL,
    .General_LineSpace    = 16,
    .General_MoveStyle    = UNLINEAR,
    .General_MovingSpeed  = SPEED,
    .General_MenuItems    = MainMenuItems,

    .List_MenuArea        = {0, 0, 128, 32},
    .List_IfDrawFrame     = false,
    .List_IfDrawLinePerfix = false,
    .List_IfScrollBar     = false,
    .List_IfExecuteOnSelect = false,
    .List_StartPointX     = 2,
    .List_StartPointY     = 2,

    .Tiles_ScreenWidth    = 128,
    .Tiles_ScreenHeight   = 32,
    .Tiles_TileWidth      = 30,
    .Tiles_TileHeight     = 24,
};
