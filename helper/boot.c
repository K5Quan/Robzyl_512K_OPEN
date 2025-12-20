/* Original work Copyright 2023 Dual Tachyon
 * https://github.com/DualTachyon
 *
 * Modified work Copyright 2024 kamilsss655
 * https://github.com/kamilsss655
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 *     Unless required by applicable law or agreed to in writing, software
 *     distributed under the License is distributed on an "AS IS" BASIS,
 *     WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 *     See the License for the specific language governing permissions and
 *     limitations under the License.
 */

#include <string.h>
#include "bsp/dp32g030/gpio.h"
#include "driver/bk4819.h"
#include "driver/keyboard.h"
#include "driver/gpio.h"
#include "driver/system.h"
#include "helper/boot.h"
#include "misc.h"
#include "radio.h"
#include "settings.h"
#include "ui/menu.h"
#include "ui/ui.h"

BOOT_Mode_t BOOT_GetMode(void)
{
KEY_Code_t   Keys[2];
Keys[0] = KEYBOARD_Poll();
SYSTEM_DelayMs(20);
gKeyReading0 = Keys[0];
gKeyReading1 = Keys[0];
gDebounceCounter = 2;
//if (Keys[0] == KEY_SIDE1) return BOOT_MODE_F_LOCK;
if (Keys[0] == KEY_0) return BOOT_MODE_F_LOCK; //hidden menu key
return BOOT_MODE_NORMAL;
}

void BOOT_ProcessMode(BOOT_Mode_t Mode)
{
if (Mode == BOOT_MODE_F_LOCK)
	{
		GUI_SelectNextDisplay(DISPLAY_MENU);
	}
	else
	{
		GUI_SelectNextDisplay(DISPLAY_MAIN);
	}
}
