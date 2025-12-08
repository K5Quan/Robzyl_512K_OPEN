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
#include <stdlib.h>  // abs()
#include "bitmaps.h"
#include "board.h"
#include "driver/bk4819.h"
#include "driver/st7565.h"
#include "external/printf/printf.h"
#include "functions.h"
#include "helper/battery.h"
#include "misc.h"
#include "radio.h"
#include "settings.h"
#include "ui/helper.h"
#include "ui/inputbox.h"
#include "ui/main.h"
#include "ui/ui.h"
center_line_t center_line = CENTER_LINE_NONE;

// ***************************************************************************

static void DrawSmallAntennaAndBars(uint8_t *p, unsigned int level)
{
	if(level>6)
		level = 6;

	//memcpy(p, BITMAP_Antenna, ARRAY_SIZE(BITMAP_Antenna));

	for(uint8_t i = 1; i <= level; i++) {
		char bar = (0xff << (6-i)) & 0x7F;
		memset(p + 2 + i*3, bar, 2);
	}
}

void DrawLevelBar(uint8_t xpos, uint8_t line, uint8_t level)
{
	const char hollowBar[] = {
		0b01111111,
		0b01000001,
		0b01000001,
		0b01111111
	};

	uint8_t *p_line = gFrameBuffer[line];
	//memset(p_line, 0, LCD_WIDTH);
	level = MIN(level, 13);

	for(uint8_t i = 0; i < level; i++) {
		if(i < 9) {
			for(uint8_t j = 0; j < 4; j++)
				p_line[xpos + i * 5 + j] = (~(0x7F >> (i+1))) & 0x7F;
		}
		else {
			memcpy(p_line + (xpos + i * 5), &hollowBar, ARRAY_SIZE(hollowBar));
		}
	}
//	ST7565_BlitFullScreen();
}

#ifdef ENABLE_AUDIO_BAR

unsigned int sqrt16(unsigned int value)
{	// return square root of 'value'
	unsigned int shift = 16;         // number of bits supplied in 'value' .. 2 ~ 32
	unsigned int bit   = 1u << --shift;
	unsigned int sqrti = 0;
	while (bit)
	{
		const unsigned int temp = ((sqrti << 1) | bit) << shift--;
		if (value >= temp) {
			value -= temp;
			sqrti |= bit;
		}
		bit >>= 1;
	}
	return sqrti;
}

void UI_DisplayAudioBar(void)
{
	if(gLowBattery && !gLowBatteryConfirmed)
		return;

	if (gCurrentFunction != FUNCTION_TRANSMIT ||
		gScreenToDisplay != DISPLAY_MAIN
		)
	{
		return;  // screen is in use
	}
			
	#if defined(ENABLE_TX1750)
		if (gAlarmState != ALARM_STATE_OFF)
			return;
	#endif
	const unsigned int voice_amp  = BK4819_GetVoiceAmplitudeOut();  // 15:0

	// make non-linear to make more sensitive at low values
	const unsigned int level      = MIN(voice_amp * 8, 65535u);
	const unsigned int sqrt_level = MIN(sqrt16(level), 124u);
	uint8_t bars = 13 * sqrt_level / 124;
	DrawLevelBar(62, 1, bars);
	
}
#endif

void DisplayRSSIBar(const int16_t rssi)
{	if (gCurrentFunction == FUNCTION_RECEIVE ||	gCurrentFunction == FUNCTION_MONITOR ||	gCurrentFunction == FUNCTION_INCOMING) {
			const unsigned int line = 1;
			uint8_t           *p_line        = gFrameBuffer[line];
			char               str[16];
			if (gEeprom.KEY_LOCK && gKeypadLocked > 0) return;     // display is in use
			if (gCurrentFunction == FUNCTION_TRANSMIT || gScreenToDisplay != DISPLAY_MAIN) return;     // display is in use
			memset(p_line, 0, LCD_WIDTH);
			sLevelAttributes sLevelAtt;
			sLevelAtt = GetSLevelAttributes(rssi, gTxVfo->freq_config_RX.Frequency);
			uint8_t overS9Bars = MIN(sLevelAtt.over/10, 4);
			if(overS9Bars == 0) {sprintf(str, "%d S%d", sLevelAtt.dBmRssi, sLevelAtt.sLevel);}
			else {
				sprintf(str, "%d+%d", sLevelAtt.dBmRssi, sLevelAtt.over);
			}
			UI_PrintStringSmall(str, 2, 0, line,0);
			DrawLevelBar(62, line, sLevelAtt.sLevel + overS9Bars);
	}
}

void UI_DisplayMain(void)
{
	const unsigned int line0 = 0;  // text screen line
	char               String[22];
	
	center_line = CENTER_LINE_NONE;

	// clear the screen ОЧИСТКА ЭКРАНА
	memset(gFrameBuffer, 0, sizeof(gFrameBuffer));


	if(gLowBattery && !gLowBatteryConfirmed) {
		UI_DisplayPopup("LOW BATTERY");
		return;
	}

	if (gEeprom.KEY_LOCK && gKeypadLocked > 0)
	{	// tell user how to unlock the keyboard
		UI_PrintString("Long press #", 0, LCD_WIDTH, 1, 8);
		UI_PrintString("to unlock",    0, LCD_WIDTH, 3, 8);
		ST7565_BlitFullScreen();
		return;
	}
	const unsigned int line       = line0 ;
	uint8_t           *p_line    = gFrameBuffer[line + 5];
	unsigned int       mode       = 0;

		if (IS_MR_CHANNEL(gEeprom.ScreenChannel))
		{	// Channel mode ИМЯ КАНАЛА
			const bool inputting = (gInputBoxIndex == 0 ) ? false : true;
			if (!inputting)
				sprintf(String, "M%u", gEeprom.ScreenChannel + 1);
			else
				sprintf(String, "M%.3s", INPUTBOX_GetAscii());  // show the input text
			UI_PrintString(String , 0, 0, line+2 ,8);
		}
		
		unsigned int state = VfoState;
		uint32_t frequency = gEeprom.VfoInfo.pRX->Frequency;

		if (state != VFO_STATE_NORMAL)
		{
			const char *state_list[] = {"", "BUSY", "BAT LOW", "TX DISABLE", "TIMEOUT", "ALARM", "VOLT HIGH"};
			if (state < ARRAY_SIZE(state_list))
			UI_PrintString(state_list[state], 31, 0, line+2, 8);
		}
		
		//if (gInputBoxIndex > 0 && IS_FREQ_CHANNEL(gEeprom.ScreenChannel)){	// user entering a frequency ЧАСТОТА 1000
		if (gInputBoxIndex > 0 && gEeprom.ScreenChannel > MR_CHANNEL_LAST){	// user entering a frequency
				const char * ascii = INPUTBOX_GetAscii();
				bool isGigaF = frequency>=100000000;
				sprintf(String, "%.*s.%.3s", 3 + isGigaF, ascii, ascii + 3 + isGigaF);
				// show the remaining 2 small frequency digits
				UI_PrintStringSmall(String + 7, 85, 0, line + 4,0); //temp
				String[7] = 0;
				// show the main large frequency digits
				UI_DisplayFrequency(String, 16, line+4, false);
			}
			else
			{
				if (gCurrentFunction == FUNCTION_TRANSMIT)
				{	// transmitting
				frequency = gEeprom.VfoInfo.pTX->Frequency;
				} else {
					if(gEeprom.ScreenChannel <= MR_CHANNEL_LAST) frequency = BOARD_fetchChannelFrequency(gEeprom.ScreenChannel);
				}
				// Always show frequency
				sprintf(String, "%3u.%05u", frequency / 100000, frequency % 100000);   //temp
				// show the remaining 2 small frequency digits МАЛЫЕ НУЛИ
					UI_PrintString(String + 7, 105, 0, line + 4,8); //шрифт стандартный старое
					//UI_PrintStringSmall(String + 7, 107, 0, line + 6, 0); //шрифт тонкий координаты внизу
					//UI_PrintStringSmall(String + 7, 105, 0, line + 5, 0); //мелкие нули
				String[7] = 0;
				// show the main large frequency digits БОЛЬШАЯ ЧАСТОТА
				UI_DisplayFrequency(String, 19, line+4, false);

				if (IS_MR_CHANNEL(gEeprom.ScreenChannel) && state == VFO_STATE_NORMAL)
				{	// it's a Channel НОМЕР СПИСКА СКАНИРОВАНИЯ
					const ChannelAttributes_t att = gMR_ChannelAttributes[gEeprom.ScreenChannel];
					if (att.scanlist > 0) {
						sprintf(String, "S%d", att.scanlist);
						//GUI_DisplaySmallest(String, 117, 25,false,true); // 117.34 старые
						GUI_DisplaySmallest(String, 2, 42, false, true);
					}
					const bool inputting = (gInputBoxIndex == 0 ) ? false : true;
					if (!inputting) {
						char DisplayString[22];
						uint16_t ch_num = gEeprom.ScreenChannel + 1;

						SETTINGS_FetchChannelName(String, gEeprom.ScreenChannel);
						if (String[0] == 0)
						{   // pas de nom, afficher juste le numéro
						    sprintf(DisplayString, "M%u", ch_num);
						}
						else
						{
						    // concaténer "M<num> " devant le nom ЗНАК ИМЯ КАНАЛА
						    sprintf(DisplayString, "M%u:%s", ch_num, String);
						}
					
						// afficher à l’écran
						UI_PrintString(DisplayString, 0, 0, line+2, 8);
					}
				}
			}

		// show the TX/RX level
		uint8_t Level = 0;
		if (mode == 1)
		{	// TX power level
			switch (gTxVfo->OUTPUT_POWER)
			{
				case OUTPUT_POWER_LOW:  Level = 2; break;
				case OUTPUT_POWER_MID:  Level = 4; break;
				case OUTPUT_POWER_HIGH: Level = 6; break;
			}
		}

		if(Level) DrawSmallAntennaAndBars(p_line + LCD_WIDTH, Level);
		String[0] = '\0';
		
		// show the modulation symbol ЗНАК МОДУЛЯЦИИ FM
		const char * s = "";
		const ModulationMode_t mod = gEeprom.VfoInfo.Modulation;
		switch (mod){
			case MODULATION_FM: {
				const FREQ_Config_t *pConfig = (mode == 1) ? gEeprom.VfoInfo.pTX : gEeprom.VfoInfo.pRX;
				const unsigned int code_type = pConfig->CodeType;
				const char *code_list[] = {"FM", "CT", "DCS", "DCR"};
				if (code_type < ARRAY_SIZE(code_list))
					s = code_list[code_type];
				break;
			}
			default:
				s = gModulationStr[mod];
			break;
		}		
		GUI_DisplaySmallest(s, 2, 35, false, true); //ЗНАК МОДУЛЯЦИИ FM, второй знак после скобки сдвиг вправо 6, 10, 14
		//UI_PrintStringSmall(s, LCD_WIDTH + 25, 0, line +5,0); СТАРАЯ

		// show the Ptt_Toggle_Mode КНОПКА ПТТ
		
		if (Ptt_Toggle_Mode) 	UI_PrintStringSmall("T", LCD_WIDTH + 8, 0, line +5,0);

		if (state == VFO_STATE_NORMAL || state == VFO_STATE_ALARM)
		
		{	// show the TX power МОЩНОСТЬ
			const char pwr_list[] = "LMH";
			const unsigned int i = gEeprom.VfoInfo.OUTPUT_POWER;
			String[0] = (i < ARRAY_SIZE(pwr_list)) ? pwr_list[i] : '\0';
			String[1] = '\0';
			UI_PrintStringSmall(String, LCD_WIDTH + 80, 0, line + 5,0);
		}

		if (gEeprom.VfoInfo.freq_config_RX.Frequency != gEeprom.VfoInfo.freq_config_TX.Frequency)
		{	// show the TX offset symbol ЗНАК СМЕЩЕНИЯ +/-
			const char dir_list[] = "\0+-";
			const unsigned int i = gEeprom.VfoInfo.TX_OFFSET_FREQUENCY_DIRECTION;
			String[0] = (i < sizeof(dir_list)) ? dir_list[i] : '?';
			String[1] = '\0';
			UI_PrintStringSmall(String, LCD_WIDTH + 0, 0, line + 5,0);
		}

		// === ПОЛОСА + ШАГ — в нижней строке (точно как ты хотел) ===
		{
			char stepStr[8];
			const uint16_t step = gStepFrequencyTable[gTxVfo->STEP_SETTING];

			// Формируем шаг: 12.5 → "12.5", 25 → "25", 6.25 → "6.25"
		if (step == 833) {
				strcpy(stepStr, "8.33k");
			}
			else {
				uint32_t v = (uint32_t)step * 10;        // 1250 → 12500, 250 →2500, 25000 →250000
				uint16_t integer = v / 1000;             // целая часть
				uint16_t decimal = (v % 1000) / 10;      // две цифры после запятой

				if (integer == 0) {
					sprintf(stepStr, "0.%02uk", decimal);        // 0.25k, 0.50k
				}
				else if (integer >= 100) {
					sprintf(stepStr, "%uk", integer);            // 100k, 125k и выше
				}
				else {
					sprintf(stepStr, "%u.%02uk", integer, decimal); // 1.25k, 6.25k, 12.50k, 25.00k
				}
			}

			// Шаг — правее полосы
			UI_PrintStringSmall(stepStr, LCD_WIDTH + 90, 0, line + 5, 0);

			// 1. Формируем строку SQL: "SQL:X"
            char sqlStr[6];
            // ИСПОЛЬЗУЕМ КОРРЕКТНЫЙ ПУТЬ: gEeprom.SQUELCH_LEVEL
            sprintf(sqlStr, "U%u", gEeprom.SQUELCH_LEVEL); 

            // 2. Выводим SQL в самом левом углу
            UI_PrintStringSmall(sqlStr, LCD_WIDTH + 20, 0, line + 5, 0);

			// Полоса
			UI_PrintStringSmall(bwNames[gEeprom.VfoInfo.CHANNEL_BANDWIDTH], LCD_WIDTH + 40, 0, line + 5, 0);

			// SCR (если включён)
			if (gEeprom.VfoInfo.SCRAMBLING_TYPE > 0 && gSetting_ScrambleEnable)
				UI_PrintStringSmall("SCR", LCD_WIDTH + 106, 0, line + 5, 0);
		}
	if (center_line == CENTER_LINE_NONE)
	{	
		if (gCurrentFunction == FUNCTION_TRANSMIT) {
			center_line = CENTER_LINE_AUDIO_BAR;
			UI_DisplayAudioBar();
		}

	}

	// МНОЖЕСТВО ЛИНИЙ — РАБОТАЕТ БЕЗ ОШИБОК
    uint8_t line_positions[] = { 0, 37 };   // ← меняй эти числа
    uint8_t num_lines = sizeof(line_positions) / sizeof(line_positions[0]);

    for (uint8_t i = 0; i < num_lines; i++)
    {
        uint8_t y = line_positions[i];
        for (uint8_t x = 0; x < 128; x++)
        {
            if (y < 8)
                gStatusLine[x] |= (1u << y);
            else
                gFrameBuffer[(y - 8) >> 3][x] |= (1u << ((y - 8) & 7));
        }
    }
	ST7565_BlitFullScreen();
}

