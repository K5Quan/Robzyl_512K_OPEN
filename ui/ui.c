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
#include "app/fm.h"
#include "driver/keyboard.h"
#include "misc.h"
#include "ui/fmradio.h"
#include "ui/inputbox.h"
#include "ui/main.h"
#include "ui/menu.h"
#include "ui/scanner.h"
#include "ui/ui.h"

GUI_DisplayType_t gScreenToDisplay;
GUI_DisplayType_t gRequestDisplayScreen = DISPLAY_INVALID;

uint8_t           gAskForConfirmation;

void GUI_DisplayScreen(void)
{
	switch (gScreenToDisplay)
	{
		case DISPLAY_MAIN:
			UI_DisplayMain();
			break;

		case DISPLAY_FM:
			UI_DisplayFM();
			break;
		
		case DISPLAY_MENU:
			UI_DisplayMenu();
			break;

		case DISPLAY_SCANNER:
			UI_DisplayScanner();
			break;


		default:
			break;
	}
}

void GUI_SelectNextDisplay(GUI_DisplayType_t Display)
{
	if (Display == DISPLAY_INVALID)
		return;

	if (gScreenToDisplay != Display)
	{
		gInputBoxIndex       = 0;
		gIsInSubMenu         = false;
		gCssBackgroundScan         = false;
		gFM_ScanState    = FM_SCAN_OFF;
		gAskForConfirmation  = 0;
		gWasFKeyPressed      = false;
		        
	}
	gScreenToDisplay = Display;
}
