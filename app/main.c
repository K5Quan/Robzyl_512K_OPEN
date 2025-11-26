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

#include "app/action.h"
#include "app/app.h"
#include "app/common.h"
#include "app/fm.h"
#include "app/generic.h"
#include "app/main.h"
#include "app/scanner.h"
#include "app/spectrum.h"

#include "board.h"
#include "driver/bk4819.h"
#include "frequencies.h"
#include "misc.h"
#include "radio.h"
#include "settings.h"
#include "ui/inputbox.h"
#include "ui/ui.h"
#include <stdlib.h>
//#include "debugging.h"

static void MAIN_Key_STAR(bool closecall)
{
	if (gCurrentFunction == FUNCTION_TRANSMIT)
		return;
	gWasFKeyPressed          = false;
	gUpdateStatus            = true;		
	SCANNER_Start(closecall);
	gRequestDisplayScreen = DISPLAY_SCANNER;
}

static void processFKeyFunction(const KEY_Code_t Key, const bool beep)
{
	if (gScreenToDisplay == DISPLAY_MENU)
	{
		return;
	}
	
	switch (Key)
	{
		case KEY_0:
			ACTION_FM();
			break;

		case KEY_1:
			if (!IS_FREQ_CHANNEL(gTxVfo->CHANNEL_SAVE)) {
				gWasFKeyPressed = false;
				gUpdateStatus   = true;

#ifdef ENABLE_COPY_CHAN_TO_VFO
				if (gEeprom.VFO_OPEN && !gCssBackgroundScan)
				{

					if (IS_MR_CHANNEL(gEeprom.ScreenChannel))
					{	// copy Channel to VFO, then swap to the VFO

						const uint16_t Channel = FREQ_CHANNEL_FIRST + gEeprom.VfoInfo.Band;

						gEeprom.ScreenChannel = Channel;
						gEeprom.VfoInfo.CHANNEL_SAVE = Channel;

						RADIO_SelectVfos();
						RADIO_ApplyTxOffset(gTxVfo);
						RADIO_ConfigureSquelchAndOutputPower(gTxVfo);
						RADIO_SetupRegisters(true);
						
						gRequestSaveChannel = 1;
						gUpdateDisplay = true;
					}
				}
				
#endif
				return;
			}

			if(gTxVfo->pRX->Frequency < 100000000) { //Robby69 directly go to 1Ghz
			//if(gTxVfo->Band == 6 && gTxVfo->pRX->Frequency < 100000000) {
					gTxVfo->Band = 7;
					gTxVfo->pRX->Frequency = 100000000;
					return;
			}
			gRequestSaveVFO            = true;
			gVfoConfigureMode          = VFO_CONFIGURE_RELOAD;
			gRequestDisplayScreen      = DISPLAY_MAIN;


		case KEY_2:
		//Nothing

			break;

		case KEY_3:
			COMMON_SwitchVFOMode();
			break;
		case KEY_4:
			if (beep) APP_RunSpectrum(1); //Channel scan
			break;
		case KEY_5:
			if (beep) APP_RunSpectrum(4); //basic spectrum}
			break;

		case KEY_6:
			if (beep) APP_RunSpectrum(2); // Band scan
			else {
			gTxVfo->FrequencyReverse = gTxVfo->FrequencyReverse == false;
			gRequestSaveChannel = 1;
			}
			break;
		
		case KEY_7:
			ACTION_SwitchDemodul();
			break;

		case KEY_8:
			ACTION_Power();
			break;
		case KEY_9:
			gTxVfo->CHANNEL_BANDWIDTH =
				ACTION_NextBandwidth(gTxVfo->CHANNEL_BANDWIDTH, gTxVfo->Modulation != MODULATION_AM, 0);
			break;


		default:
			gUpdateStatus   = true;
			gWasFKeyPressed = false;
			break;
	}
}

static void MAIN_Key_DIGITS(KEY_Code_t Key, bool bKeyPressed, bool bKeyHeld)
{
	if (bKeyHeld)
	{	// key held down
		if (bKeyPressed)
		{
			if (gScreenToDisplay == DISPLAY_MAIN)
			{
				if (gInputBoxIndex > 0)
				{	// delete any inputted chars
					gInputBoxIndex        = 0;
					gRequestDisplayScreen = DISPLAY_MAIN;
				}
				gWasFKeyPressed = false;
				gUpdateStatus   = true;
				processFKeyFunction(Key, false);
			}
		}
		return;
	}

	if (bKeyPressed)
	{	// key is pressed
		return; // don't use the key till it's released
	}

	if (!gWasFKeyPressed)
	{	// F-key wasn't pressed
		// Vérifier si on est en mode VFO (fréquence)
		bool isFreqMode = IS_FREQ_CHANNEL(gTxVfo->CHANNEL_SAVE);
		
		// Pas de timeout pour l'entrée de fréquence, seulement pour les canaux
		if (!isFreqMode)
			gKeyInputCountdown = key_input_timeout_500ms;
		
		
		// Chiffres normaux
		INPUTBOX_Append(Key);
		gRequestDisplayScreen = DISPLAY_MAIN;

		// Gestion des canaux MR
		if (!isFreqMode)
		{
			uint16_t Channel;
			
			// Si on a 1 ou 2 chiffres, attendre le timeout
			if (gInputBoxIndex == 1 || gInputBoxIndex == 2)
				return;
				
			// 3 chiffres = validation immédiate
			if (gInputBoxIndex == 3)
			{
				Channel = ((gInputBox[0] * 100) + (gInputBox[1] * 10) + gInputBox[2]) - 1;
				
				if (!RADIO_CheckValidChannel(Channel, false, 0)) 
				{
					gInputBoxIndex = 0;
					return;
				}

				gEeprom.MrChannel     = Channel;
				gEeprom.ScreenChannel = Channel;
				gRequestSaveVFO       = true;
				gVfoConfigureMode     = VFO_CONFIGURE_RELOAD;
				gInputBoxIndex        = 0;
				return;
			}

			gInputBoxIndex = 0;

			Channel = ((gInputBox[0] * 100) + (gInputBox[1] * 10) + gInputBox[2]) - 1;

			if (!RADIO_CheckValidChannel(Channel, false,0)) return;

			gEeprom.MrChannel     = Channel;
			gEeprom.ScreenChannel = Channel;
			gRequestSaveVFO       = true;
			gVfoConfigureMode     = VFO_CONFIGURE_RELOAD;

			return;
		}
		if (IS_FREQ_CHANNEL(gTxVfo->CHANNEL_SAVE))
		{	// user is entering a frequency

			uint32_t Frequency;
			bool isGigaF = gTxVfo->pRX->Frequency >= 100000000;
			if (gInputBoxIndex < 6 + isGigaF){return;}

			gInputBoxIndex = 0;
			Frequency = StrToUL(INPUTBOX_GetAscii()) * 100;

			SETTINGS_SetVfoFrequency(Frequency);
			
			gRequestSaveChannel = 1;
			return;
		}
		gRequestDisplayScreen = DISPLAY_MAIN;
		return;
	}

	gWasFKeyPressed = false;
	gUpdateStatus   = true;

	processFKeyFunction(Key, true);
}

static void MAIN_Key_EXIT(bool bKeyPressed, bool bKeyHeld)
{
	if (!bKeyHeld && bKeyPressed)
	{	// exit key pressed

		if (!gFmRadioMode)
		{

				if (gInputBoxIndex == 0)
					return;
				gInputBox[--gInputBoxIndex] = 10;

				gKeyInputCountdown = key_input_timeout_500ms;

			gRequestDisplayScreen = DISPLAY_MAIN;
			return;
		}
		ACTION_FM();
		return;
	}

}

static void MAIN_Key_MENU(const bool bKeyPressed, const bool bKeyHeld)
{
	if (bKeyPressed && !bKeyHeld)
		// menu key pressed

	if (bKeyHeld)
	{	// menu key held down (long press)

		if (bKeyPressed)
		{	// long press MENU key

			gWasFKeyPressed = false;

			if (gScreenToDisplay == DISPLAY_MAIN)
			{
				if (gInputBoxIndex > 0)
				{	// delete any inputted chars
					gInputBoxIndex        = 0;
					gRequestDisplayScreen = DISPLAY_MAIN;
				}

				gWasFKeyPressed = false;
				gUpdateStatus   = true;

				ACTION_Handle(KEY_MENU, bKeyPressed, bKeyHeld);
			}
		}

		return;
	}

	if (!bKeyPressed )
	{	// menu key released

		const bool bFlag = (gInputBoxIndex == 0);
		gInputBoxIndex   = 0;

		if (bFlag)
		{
			gFlagRefreshSetting = true;
			gRequestDisplayScreen = DISPLAY_MENU;
		}
		else
		{
			gRequestDisplayScreen = DISPLAY_MAIN;
		}
	}
}

static void MAIN_Key_UP_DOWN(bool bKeyPressed, bool bKeyHeld, int8_t Direction)
{
	uint16_t Channel = gEeprom.ScreenChannel;
	if (!bKeyPressed && !bKeyHeld) return;
	if (gInputBoxIndex > 0)	return;

	uint16_t Next;

	if (IS_FREQ_CHANNEL(Channel))
	{
		uint32_t frequency = APP_SetFrequencyByStep(gTxVfo, Direction);
		if (RX_freq_check(frequency) == 0xFF) return;
		gTxVfo->freq_config_RX.Frequency = frequency;
		BK4819_SetFrequency(frequency);
		gRequestSaveChannel = 1;
		return;
	}
	Next = RADIO_FindNextChannel(Channel + Direction, Direction, false, 0);
	if (Next == 0xFFFF || Next == Channel) return;
	gEeprom.MrChannel     = Next;
	gEeprom.ScreenChannel = Next;
	gRequestSaveVFO       = true;
	gVfoConfigureMode     = VFO_CONFIGURE_RELOAD;
	gPttWasReleased       = true;
}


void MAIN_ProcessKeys(KEY_Code_t Key, bool bKeyPressed, bool bKeyHeld)
{
	if (Key == KEY_F)
	{
		if (bKeyPressed && !bKeyHeld)
		{
			gWasFKeyPressed = true;
			// Pas de timeout pour F, reste actif jusqu'à action
			gKeyInputCountdown = 0; // ou une valeur très grande
		}
		return;
	}
	if (gFmRadioMode && Key != KEY_PTT && Key != KEY_EXIT)
		{
			if (!bKeyHeld && bKeyPressed)
			return;
		}
	switch (Key)
	{
		case KEY_0:
		case KEY_1:
		case KEY_2:
		case KEY_3:
		case KEY_4:
		case KEY_5:
		case KEY_6:
		case KEY_7:
		case KEY_8:
		case KEY_9:
			MAIN_Key_DIGITS(Key, bKeyPressed, bKeyHeld);
			break;
		case KEY_MENU:
			MAIN_Key_MENU(bKeyPressed, bKeyHeld);
			break;
		case KEY_UP:
			MAIN_Key_UP_DOWN(bKeyPressed, bKeyHeld, 1);
			break;
		case KEY_DOWN:
			MAIN_Key_UP_DOWN(bKeyPressed, bKeyHeld, -1);
			break;
		case KEY_EXIT:
			MAIN_Key_EXIT(bKeyPressed, bKeyHeld);
			break;
		case KEY_STAR:
			if (gWasFKeyPressed) MAIN_Key_STAR(1);
			else MAIN_Key_STAR(0);
			break;
		case KEY_F:
			GENERIC_Key_F(bKeyPressed, bKeyHeld);
			break;
		case KEY_PTT:
			GENERIC_Key_PTT(bKeyPressed);
			break;
		default:
			
			break;
	}
}
