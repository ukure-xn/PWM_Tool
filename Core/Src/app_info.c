#include "app_info.h"
#include "OLED_UI.h"
#include "OLED_UI_MenuData.h"

#include <stdio.h>
#include <string.h>

/* ---------------------------------------------------------------- */
/* Single source of truth: bump these, nothing else.                 */
/* ---------------------------------------------------------------- */
#define APP_HW_MAJOR    1
#define APP_HW_MINOR    0

#define APP_FW_MAJOR    1
#define APP_FW_MINOR    0
#define APP_FW_PATCH    0

#define APP_BL_MAJOR    0       /* placeholder until a real bootloader exists */
#define APP_BL_MINOR    0

#define APP_BUILD_NUM   1       /* +1 on every release build */

const AppInfo_t g_appInfo = {
    .hw_major   = APP_HW_MAJOR,
    .hw_minor   = APP_HW_MINOR,
    .fw_major   = APP_FW_MAJOR,
    .fw_minor   = APP_FW_MINOR,
    .fw_patch   = APP_FW_PATCH,
    .bl_major   = APP_BL_MAJOR,
    .bl_minor   = APP_BL_MINOR,
    .build_num  = APP_BUILD_NUM,
    .build_date = __DATE__,
};

/* "Mmm dd yyyy" -> "yyyy-mm-dd". Runs once at boot. */
static void DateToIso(const char *mdy, char *out)
{
    static const char *const mon[12] = {
        "Jan", "Feb", "Mar", "Apr", "May", "Jun",
        "Jul", "Aug", "Sep", "Oct", "Nov", "Dec"
    };
    uint8_t m = 0;
    uint8_t i;

    for (i = 0; i < 12; i++) {
        if (strncmp(mdy, mon[i], 3) == 0) {
            m = (uint8_t)(i + 1);
            break;
        }
    }

    out[0]  = mdy[7];                               /* yyyy */
    out[1]  = mdy[8];
    out[2]  = mdy[9];
    out[3]  = mdy[10];
    out[4]  = '-';
    out[5]  = (char)('0' + m / 10);                 /* mm   */
    out[6]  = (char)('0' + m % 10);
    out[7]  = '-';
    out[8]  = (mdy[4] == ' ') ? '0' : mdy[4];       /* dd   */
    out[9]  = mdy[5];
    out[10] = '\0';
}

/* Kept static: MenuItem.General_item_text is a plain char*, so these
   buffers must outlive the menu table. */
static char s_hw[12];
static char s_fw[14];
static char s_bl[12];
static char s_build[24];

void AppInfo_BindToMenu(void)
{
    char iso[11];

    DateToIso(g_appInfo.build_date, iso);

    snprintf(s_hw,  sizeof s_hw,  "HW:V%u.%u",    g_appInfo.hw_major, g_appInfo.hw_minor);
    snprintf(s_fw,  sizeof s_fw,  "FW:V%u.%u.%u", g_appInfo.fw_major, g_appInfo.fw_minor, g_appInfo.fw_patch);
    snprintf(s_build, sizeof s_build, "Build:%01u %s", g_appInfo.build_num, iso);

    if ((g_appInfo.bl_major == 0) && (g_appInfo.bl_minor == 0)) {
        snprintf(s_bl, sizeof s_bl, "BL:--");
    } else {
        snprintf(s_bl, sizeof s_bl, "BL:V%u.%u", g_appInfo.bl_major, g_appInfo.bl_minor);
    }

    /* AboutMenuItems[6..9]: HW / FW / BL / build, in that order. */
    AboutMenuItems[7].General_item_text = s_hw;
    AboutMenuItems[8].General_item_text = s_fw;
    AboutMenuItems[9].General_item_text = s_bl;
    AboutMenuItems[10].General_item_text = s_build;
}
