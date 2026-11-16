#ifndef __OLED_UI_MENUDATA_H
#define __OLED_UI_MENUDATA_H

#ifdef __cplusplus
extern "C" {
#endif

#include "OLED_UI.h"

extern MenuItem MainMenuItems[];
extern MenuItem PwmMenuItems[];
extern MenuItem SweepMenuItems[];
extern MenuItem SweepFreqItems[];
extern MenuItem SweepDutyItems[];
extern MenuItem SettingsMenuItems[];
extern MenuItem AboutMenuItems[];


extern MenuPage MainMenuPage;
extern MenuPage PwmMenuPage;
extern MenuPage SweepMenuPage;
extern MenuPage SweepFreqPage;
extern MenuPage SweepDutyPage;
extern MenuPage SettingsMenuPage;
extern MenuPage AboutMenuPage;

/* PWM / Sweep 菜单绑定的变量定义在 pwm_app.h，这里不再重复声明 */

#ifdef __cplusplus
}
#endif

#endif
