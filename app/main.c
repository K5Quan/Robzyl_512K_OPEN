/* Original work Copyright 2023 Dual Tachyon
 * https://github.com/DualTachyon
 *
 * Modified work 2024 kamilsss655
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
#include <stdbool.h>

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

bool gBacklightAlwaysOn = false;

// Добавлено только это — время TX/RX
uint32_t txTimeSeconds = 0;
uint32_t rxTimeSeconds = 0;
bool isTxActive = false;
// Добавь эти две строки:
static uint8_t lastRadioState = 255;  // для отслеживания смены режима

static void MAIN_Key_STAR(bool closecall)
{
	if (gCurrentFunction == FUNCTION_TRANSMIT) return;
	gWasFKeyPressed          = false;
	SCANNER_Start(closecall);
	gRequestDisplayScreen = DISPLAY_SCANNER;
}

static void processFKeyFunction(const KEY_Code_t Key, const bool beep)
{
	(void)beep;

	if (gScreenToDisplay == DISPLAY_MENU)
	{
		return;
	}
	
	switch (Key)
	{
		case KEY_1:
			if (!IS_FREQ_CHANNEL(gTxVfo->CHANNEL_SAVE)) {
				gWasFKeyPressed = false;

#ifdef ENABLE_COPY_CHAN_TO_VFO
				if (gEeprom.VFO_OPEN && !gCssBackgroundScan)
				{
					if (IS_MR_CHANNEL(gEeprom.ScreenChannel))
					{
						const uint16_t Channel = FREQ_CHANNEL_FIRST + gEeprom.VfoInfo.Band;

						gEeprom.ScreenChannel = Channel;
						gEeprom.VfoInfo.CHANNEL_SAVE = Channel;

						RADIO_SelectVfos();
						RADIO_ApplyTxOffset(gTxVfo);
						RADIO_ConfigureSquelchAndOutputPower(gTxVfo);
						RADIO_SetupRegisters(true);
						
						gRequestSaveChannel = 1;
						
					}
				}
				
#endif
				return;
			}

			if(gTxVfo->pRX->Frequency < 100000000) {
					gTxVfo->Band = 7;
					gTxVfo->pRX->Frequency = 100000000;
					return;
			}
			gRequestSaveVFO            = true;
			gVfoConfigureMode          = VFO_CONFIGURE_RELOAD;
			gRequestDisplayScreen      = DISPLAY_MAIN;
			break;

		case KEY_2:
			if (++gTxVfo->SCANLIST > 15) gTxVfo->SCANLIST = 0;
			SETTINGS_UpdateChannel(gTxVfo->CHANNEL_SAVE, gTxVfo, true);
			gVfoConfigureMode = VFO_CONFIGURE;
			gFlagResetVfos    = true;
			break;

		case KEY_3:
			COMMON_SwitchVFOMode();
			break;

		case KEY_7:
			APP_RunSpectrum(1);  // F+7 — channel scan spectrum
			break;

		case KEY_8:
			APP_RunSpectrum(2);  // F+8 — band scan spectrum
			break;

		case KEY_9:
			APP_RunSpectrum(4);  // F+9 — basic spectrum
			break;

		default:
			gWasFKeyPressed = false;
			break;
	}
}

static void MAIN_Key_DIGITS(KEY_Code_t Key, bool bKeyPressed, bool bKeyHeld)
{
	if (bKeyHeld)
	{
		if (bKeyPressed && gScreenToDisplay == DISPLAY_MAIN)
		{
			if (gInputBoxIndex > 0)
			{
				gInputBoxIndex = 0;
				gRequestDisplayScreen = DISPLAY_MAIN;
			}
			gWasFKeyPressed = false;
			switch (Key)
			{
				case KEY_1:
					// Длинное 1 = F+1
					if (!IS_FREQ_CHANNEL(gTxVfo->CHANNEL_SAVE)) {
						gWasFKeyPressed = false;

#ifdef ENABLE_COPY_CHAN_TO_VFO
						if (gEeprom.VFO_OPEN && !gCssBackgroundScan)
						{
							if (IS_MR_CHANNEL(gEeprom.ScreenChannel))
							{
								const uint16_t Channel = FREQ_CHANNEL_FIRST + gEeprom.VfoInfo.Band;

								gEeprom.ScreenChannel = Channel;
								gEeprom.VfoInfo.CHANNEL_SAVE = Channel;

								RADIO_SelectVfos();
								RADIO_ApplyTxOffset(gTxVfo);
								RADIO_ConfigureSquelchAndOutputPower(gTxVfo);
								RADIO_SetupRegisters(true);
								
								gRequestSaveChannel = 1;
								
							}
						}
						
#endif
						return;
					}

					if(gTxVfo->pRX->Frequency < 100000000) {
							gTxVfo->Band = 7;
							gTxVfo->pRX->Frequency = 100000000;
							return;
					}
					gRequestSaveVFO            = true;
					gVfoConfigureMode          = VFO_CONFIGURE_RELOAD;
					gRequestDisplayScreen      = DISPLAY_MAIN;
					break;

				case KEY_2:
					// Длинное 2 = F+2
					if (++gTxVfo->SCANLIST > 15) gTxVfo->SCANLIST = 0;
					SETTINGS_UpdateChannel(gTxVfo->CHANNEL_SAVE, gTxVfo, true);
					gVfoConfigureMode = VFO_CONFIGURE;
					gFlagResetVfos    = true;
					break;

				/*case KEY_3:
					// Длинное 3 = F+3
					COMMON_SwitchVFOMode();
					break;*///чиним 512 длинное 3

					case KEY_3:
    if (bKeyHeld && bKeyPressed)  // длинное нажатие
    {
        COMMON_SwitchVFOMode();   // ← переключаем MR ↔ VFO
        
        gWasFKeyPressed = false;
        return;
    }
    // Короткое нажатие 3 — если нужно что-то другое, добавь здесь
    break;

				case KEY_4:
					// Длинное 4 — смена полосы
					gTxVfo->CHANNEL_BANDWIDTH = ACTION_NextBandwidth(gTxVfo->CHANNEL_BANDWIDTH, gTxVfo->Modulation != MODULATION_AM, 0);
					gRequestSaveChannel = 1;
					RADIO_SetupRegisters(true);
					
					break;
				case KEY_5:
					// Длинное 5 — смена шага по кругу
					if (++gTxVfo->STEP_SETTING >= ARRAY_SIZE(gStepFrequencyTable))
						gTxVfo->STEP_SETTING = 0;
					gRequestSaveChannel = 1;
					RADIO_SetupRegisters(true);
					
					break;
				case KEY_6:
					// Длинное 6 — смена мощности
					ACTION_Power();
					gRequestSaveChannel = 1;
					RADIO_SetupRegisters(true);
					
					break;
				case KEY_0:
					// Длинное 0 — смена демодуляции
					ACTION_SwitchDemodul();
					gRequestSaveChannel = 1;
					RADIO_SetupRegisters(true);
					
					break;
				case KEY_9:
					// Длинное 9 — toggle "подсветка всегда включена" (перенесено с 8)
					gBacklightAlwaysOn = !gBacklightAlwaysOn;
					if (gBacklightAlwaysOn) {
						gBacklightCountdown = 0;
						backlightOn = true;
					} else {
						backlightOn = true;
						if (gEeprom.BACKLIGHT_TIME == 7) {
							gBacklightCountdown = 0;
						} else {
							switch (gEeprom.BACKLIGHT_TIME)
							{
								case 1: gBacklightCountdown = 10; break;
								case 2: gBacklightCountdown = 20; break;
								case 3: gBacklightCountdown = 40; break;
								case 4: gBacklightCountdown = 120; break;
								case 5: gBacklightCountdown = 240; break;
								case 6: gBacklightCountdown = 480; break;
								default: gBacklightCountdown = 0; break;
							}
						}
					}
					
					break;
				default:
					break;
			}
		}
		return;
	}

	if (bKeyPressed)
	{
		return;
	}

	// F + 0 — FM-радио
	if (gWasFKeyPressed && Key == KEY_0)
	{
		ACTION_FM();
		gWasFKeyPressed = false;
		return;
	}

	// F + 4 — обычный сканер
	if (gWasFKeyPressed && Key == KEY_4)
	{
		SCANNER_Start(false);
		gWasFKeyPressed = false;
		return;
	}

	// Обычный ввод цифр
	if (!gWasFKeyPressed)
	{
		INPUTBOX_Append(Key);
		gRequestDisplayScreen = DISPLAY_MAIN;
		if (IS_MR_CHANNEL(gTxVfo->CHANNEL_SAVE))
		{	// user is entering channel number
			uint16_t Channel;
			if (gInputBoxIndex != 3)
			{
				gRequestDisplayScreen = DISPLAY_MAIN;
				return;
			}
			gInputBoxIndex = 0;
			Channel = ((gInputBox[0] * 100) + (gInputBox[1] * 10) + gInputBox[2]) - 1;
			if (!RADIO_CheckValidChannel(Channel, false, 0)) return;
			gEeprom.MrChannel     = Channel;
			gEeprom.ScreenChannel = Channel;
			gRequestSaveVFO       = true;
			gVfoConfigureMode     = VFO_CONFIGURE_RELOAD;
			return;
		}

		if (IS_FREQ_CHANNEL(gTxVfo->CHANNEL_SAVE))
		{
			uint32_t Frequency;
			bool isGigaF = gTxVfo->pRX->Frequency >= 100000000;
			if (gInputBoxIndex < 6 + isGigaF) return;
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

	processFKeyFunction(Key, true);
}

static void MAIN_Key_EXIT(bool bKeyPressed, bool bKeyHeld)
{
	if (!bKeyHeld && bKeyPressed)
	{
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

static void MAIN_Key_MENU()
{
	const bool bFlag = (gInputBoxIndex == 0);
	gInputBoxIndex = 0;

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

static void MAIN_Key_UP_DOWN(bool bKeyPressed, bool bKeyHeld, int8_t Direction)
{
    if (!bKeyPressed && !bKeyHeld) return;
    if (gInputBoxIndex > 0) return;

    // Проверяем, в частотном ли режиме VFO (не канал)
    if (IS_FREQ_CHANNEL(gTxVfo->CHANNEL_SAVE))
    {
        // VFO-режим (частота) — меняем частоту шагом
        uint32_t frequency = APP_SetFrequencyByStep(gTxVfo, Direction);
        if (RX_freq_check(frequency) == 0xFF) return;  // если вне диапазона — не меняем

        gTxVfo->freq_config_RX.Frequency = frequency;
        gTxVfo->freq_config_TX.Frequency = frequency + gTxVfo->TX_OFFSET_FREQUENCY;  // если есть offset
        BK4819_SetFrequency(frequency);
        gRequestSaveChannel = 1;
        
        return;
    }

    // MR-режим (канал) — переключаем каналы
    uint16_t Channel = gEeprom.ScreenChannel;
    uint16_t Next = RADIO_FindNextChannel(Channel + Direction, Direction, false, 0);
    if (Next == 0xFFFF || Next == Channel) return;

    gEeprom.MrChannel     = Next;
    gEeprom.ScreenChannel = Next;
    gRequestSaveVFO       = true;
    gVfoConfigureMode     = VFO_CONFIGURE_RELOAD;
    gPttWasReleased       = true;
}

void MAIN_ProcessKeys(KEY_Code_t Key, bool bKeyPressed, bool bKeyHeld)
{
	

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
			if (bKeyHeld)
			{
				gRequestDisplayScreen = DISPLAY_MAIN;
				gInputBoxIndex = 0;
			}
			else
			{
				MAIN_Key_MENU();
			}
			break;

		case KEY_UP:
		case KEY_DOWN:
			if (gWasFKeyPressed && bKeyPressed && !bKeyHeld)
			{
				if (Key == KEY_UP)
				{
					if (gEeprom.SQUELCH_LEVEL < 9)
						gEeprom.SQUELCH_LEVEL++;
				}
				else
				{
					if (gEeprom.SQUELCH_LEVEL > 0)
						gEeprom.SQUELCH_LEVEL--;
				}

				gRequestSaveSettings = true;
				

				RADIO_ConfigureSquelchAndOutputPower(gTxVfo);
				RADIO_ApplySquelch();
			}
			else
			{
				MAIN_Key_UP_DOWN(bKeyPressed, bKeyHeld, (Key == KEY_UP) ? 1 : -1);
			}
			gWasFKeyPressed = false;
			break;

		case KEY_EXIT:
			MAIN_Key_EXIT(bKeyPressed, bKeyHeld);
			break;

		case KEY_STAR:
			if (gWasFKeyPressed) MAIN_Key_STAR(1);
			else MAIN_Key_STAR(0);
			break;
	    case KEY_F:
        if (bKeyHeld && bKeyPressed) {  // ДЛИННОЕ нажатие — блокировка клавиатуры
            gEeprom.KEY_LOCK = !gEeprom.KEY_LOCK;
            gRequestSaveSettings = true;
            gKeypadLocked = 0;
            gWasFKeyPressed = false;
            return;
        }

        if (bKeyPressed && !bKeyHeld) {  // КОРОТКОЕ нажатие — F-режим
            gWasFKeyPressed = true;
            gKeyInputCountdown = 0;
        }
        return;

		case KEY_PTT:
			GENERIC_Key_PTT(bKeyPressed);

			// === СБРОС ТАЙМЕРА TX КАЖДЫЙ РАЗ ПРИ НАЖАТИИ PTT ===
			if (bKeyPressed) {
				txTimeSeconds = 0;  // сбрасываем при каждом нажатии (начало новой передачи)
			}
			isTxActive = bKeyPressed;
			break;

		default:
			break;
			
	}
    if (gCurrentFunction != lastRadioState) {
        if (gCurrentFunction == FUNCTION_TRANSMIT) {
            txTimeSeconds = 0;
        }
        lastRadioState = gCurrentFunction;
    }
}