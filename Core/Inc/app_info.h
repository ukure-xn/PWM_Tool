/* Version record for the About page.
 *
 * Everything here is compile-time constant, so the whole struct lives in
 * flash and costs no RAM. Bump the numbers in app_info.c only - the About
 * page, and any future updater, read them from here.
 */
#ifndef __APP_INFO_H
#define __APP_INFO_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>

typedef struct {
    uint8_t  hw_major;
    uint8_t  hw_minor;
    uint8_t  fw_major;
    uint8_t  fw_minor;
    uint8_t  fw_patch;
    uint8_t  bl_major;          /* 0.0 = no standalone bootloader (placeholder) */
    uint8_t  bl_minor;
    uint16_t build_num;         /* bumped by hand on every release build */
    char     build_date[12];    /* __DATE__, "Mmm dd yyyy" */
} AppInfo_t;

extern const AppInfo_t g_appInfo;

/* Format the version lines once and point the About page items at them.
   Call before OLED_UI_Init(), i.e. before the UI tick starts. */
void AppInfo_BindToMenu(void);

#ifdef __cplusplus
}
#endif

#endif /* __APP_INFO_H */
