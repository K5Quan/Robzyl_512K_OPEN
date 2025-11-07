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

#include "app/app.h"
#include "app/common.h"
#include "app/fm.h"
#include "app/generic.h"
#include "app/menu.h"
#include "app/scanner.h"

#include "driver/keyboard.h"
#include "external/printf/printf.h"
#include "functions.h"
#include "misc.h"
#include "settings.h"
#include "ui/inputbox.h"
#include "ui/ui.h"
#include "driver/bk4819.h"

void GENERIC_Key_F(bool bKeyPressed, bool bKeyHeld)
{
	if (gInputBoxIndex > 0)
	{
		return;
	}

	if (bKeyHeld || !bKeyPressed) // held or released
	{
		if (bKeyHeld || bKeyPressed) // held or pressed (cannot be held and not pressed I guess, so it checks only if HELD?)
		{
			if (!bKeyHeld) // won't ever pass
				return;

			if (!bKeyPressed) // won't ever pass
				return;

			COMMON_KeypadLockToggle();
		}
		else // released
		{

			if (gScreenToDisplay != DISPLAY_MAIN)
				return;

			gWasFKeyPressed = !gWasFKeyPressed; // toggle F function

			if (gWasFKeyPressed)
				gKeyInputCountdown = key_input_timeout_500ms;
			gUpdateStatus = true;
		}
	}
	else // short pressed
	{
		if (gScreenToDisplay != DISPLAY_FM)
		{
			return;
		}

		gPttWasReleased = true;
	}
}

void GENERIC_Key_PTT(bool bKeyPressed)
{
	gInputBoxIndex = 0;

	if (!bKeyPressed || gSerialConfigCountDown_500ms > 0)
	{	// PTT released
		if (gCurrentFunction == FUNCTION_TRANSMIT)
		{	// we are transmitting .. stop

			if (gFlagEndTransmission)
			{
				FUNCTION_Select(FUNCTION_FOREGROUND);
			}
			else
			{
				APP_EndTransmission(true);
				FUNCTION_Select(FUNCTION_FOREGROUND);
			}

			gFlagEndTransmission = false;

			RADIO_SetVfoState(VFO_STATE_NORMAL);

			if (gScreenToDisplay != DISPLAY_MENU)     // 1of11 .. don't close the menu
				gRequestDisplayScreen = DISPLAY_MAIN;
		}

		return;
	}

	// PTT pressed

	// set maximum squelch to protect the screen from glitching
	//BK4819_SetupSquelch(255, 255, 127, 127, 255, 255);

	if (gFM_ScanState != FM_SCAN_OFF)
	{	// FM radio is scanning .. stop
		FM_Start();
		gRequestDisplayScreen = DISPLAY_FM;
		goto cancel_tx;
	}


	if (gScreenToDisplay == DISPLAY_FM)
		goto start_tx;	// listening to the FM radio .. start TX'ing


	if (gCurrentFunction == FUNCTION_TRANSMIT)
	{	// already transmitting
		gInputBoxIndex = 0;
		return;
	}

	if (gScreenToDisplay != DISPLAY_MENU)     // 1of11 .. don't close the menu
		gRequestDisplayScreen = DISPLAY_MAIN;


start_tx:
	// request start TX
	gFlagPrepareTX = true;
	goto done;
	
cancel_tx:
	if (gPttIsPressed)
	{
		gPttWasPressed = true;
	}

done:	
	gPttDebounceCounter = 0;
	if (gScreenToDisplay != DISPLAY_MENU && gRequestDisplayScreen != DISPLAY_FM)     // 1of11 .. don't close the menu
		gRequestDisplayScreen = DISPLAY_MAIN;
	gUpdateStatus  = true;
	gUpdateDisplay = true;

	//done tx - restore squelch here
	RADIO_ConfigureSquelchAndOutputPower(gCurrentVfo);
}
