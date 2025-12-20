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
char gSubtone_String[16] = "";

// ***************************************************************************



// РИСУЕМ АУДИОБАР И БАР СМЕТРА ОБЩИЙ стирает строку всю
// РИСУЕМ АУДИОБАР СЕКЦИЯМИ — ширина 80 пикселей, секции 5 пикселей шириной, пробел 2 пикселя (шаг 7), высота 7 пикселей (с отступами)
void DrawLevelBar(uint8_t xpos, uint8_t y_pos, uint8_t level_percent)
{
    const uint8_t bar_width = 80;              // фиксированная ширина 80 пикселей
    const uint8_t section_width = 5;           // ширина секции
    const uint8_t gap = 2;                     // пробел между секциями
    const uint8_t step = section_width + gap;  // шаг 7 пикселей
    const uint8_t max_sections = 11;           // 11 секций = 55 + 20 = 75 пикселей (оставляем запас для 80)

    uint8_t sections = (max_sections * level_percent) / 100;
    if (sections > max_sections) sections = max_sections;

    uint8_t filled_width = sections * step;  // ширина заполненной части
    if (filled_width > bar_width) filled_width = bar_width;

    // Высота 7 пикселей (5 пикселей бар + 1 сверху + 1 снизу)
    for (uint8_t dy = 0; dy < 7; dy++) {
        uint8_t y = y_pos + dy;
        if (y >= LCD_HEIGHT) continue;

        uint8_t *p_line;
        uint8_t bit_shift;
        if (y < 8) {
            p_line = gStatusLine;
            bit_shift = y;
        } else {
            p_line = gFrameBuffer[(y - 8) / 8];
            bit_shift = (y - 8) % 8;
        }

        // Очищаем только область бара (80 пикселей) + справа до конца экрана
        for (uint8_t x = xpos; x < LCD_WIDTH; x++) {
            p_line[x] &= ~(1u << bit_shift);
        }
    }

    // Заливаем заполненные секции (только чёрные)
    for (uint8_t s = 0; s < sections; s++) {
        uint8_t section_x = xpos + s * step;

        for (uint8_t dy = 1; dy < 6; dy++) {  // заливаем средние 5 пикселей (отступ 1 сверху/снизу)
            uint8_t y = y_pos + dy;
            if (y >= LCD_HEIGHT) continue;

            uint8_t *p_line;
            uint8_t bit_shift;
            if (y < 8) {
                p_line = gStatusLine;
                bit_shift = y;
            } else {
                p_line = gFrameBuffer[(y - 8) / 8];
                bit_shift = (y - 8) % 8;
            }

            for (uint8_t dx = 0; dx < section_width; dx++) {
                uint8_t x = section_x + dx;
                if (x >= LCD_WIDTH || x >= xpos + bar_width) break;
                p_line[x] |= (1u << bit_shift);
            }
        }
    }
}

// АУДИОБАР ПРИ ПЕРЕДАЧЕ — 80x4 пикселя, раздельные Y для MR и VFO
// АУДИОБАР ПРИ ПЕРЕДАЧЕ — секции 3 пикселя + пробел 2 пикселя, раздельные X и Y для MR и VFO
void UI_DisplayAudioBar(void)
{
	if(gLowBattery && !gLowBatteryConfirmed) return;

	if (gCurrentFunction != FUNCTION_TRANSMIT || gScreenToDisplay != DISPLAY_MAIN) 
		return;
			
	#if defined(ENABLE_TX1750)
		if (gAlarmState != ALARM_STATE_OFF)	return;
	#endif

	const unsigned int voice_amp  = BK4819_GetVoiceAmplitudeOut();

	// Увеличенная чувствительность — тихий голос показывает больше секций
	unsigned int amplified = voice_amp * 3;  // коэффициент усиления (3 — хороший баланс, меняй на 4-5 для ещё больше)
	if (amplified > 65535u) amplified = 65535u;

	uint8_t level_percent = amplified * 100 / 65535u;

	// РАЗДЕЛЬНЫЕ КООРДИНАТЫ ДЛЯ MR И VFO
	uint8_t xpos_mr  = 46;   // MR: X = 5 (отступ слева)
	uint8_t xpos_vfo = 46;  // VFO: X = 10 (отступ слева)
	uint8_t y_mr     = 9;  // MR: Y = 50 пикселей
	uint8_t y_vfo    = 9;  // VFO: Y = 42 пикселя

	uint8_t xpos = IS_MR_CHANNEL(gEeprom.ScreenChannel) ? xpos_mr : xpos_vfo;
	uint8_t y_pos = IS_MR_CHANNEL(gEeprom.ScreenChannel) ? y_mr : y_vfo;

	DrawLevelBar(xpos, y_pos, level_percent);
}

// S-МЕТР + AFC + RSSI — бар скрыт, строка AFC/RSSI в MR и VFO при приёме
void DisplayRSSIBar(const int16_t rssi)
{
	if (gCurrentFunction != FUNCTION_RECEIVE && 
	    gCurrentFunction != FUNCTION_MONITOR && 
	    gCurrentFunction != FUNCTION_INCOMING)
		return;

	if (gEeprom.KEY_LOCK && gKeypadLocked > 0) 
		return;

	if (gCurrentFunction == FUNCTION_TRANSMIT || gScreenToDisplay != DISPLAY_MAIN) 
		return;

	sLevelAttributes sLevelAtt = GetSLevelAttributes(rssi, gTxVfo->freq_config_RX.Frequency);
	uint8_t overS9Bars = MIN(sLevelAtt.over / 10, 4);
	// uint8_t bar_level = sLevelAtt.sLevel + overS9Bars;  // не используется — бар скрыт

	// === БАР S-МЕТРА СКРЫТ ПОЛНОСТЬЮ (в MR и VFO) ===
	// DrawLevelBar(1, 6, bar_level);  // закомментировано — бар не рисуется нигде

	// === СУБТОН (CTCSS/CDCSS) — сканируем в MR и VFO (для нижнего вывода)
	gSubtone_String[0] = '\0';

	BK4819_WriteRegister(BK4819_REG_51,
		BK4819_REG_51_ENABLE_CxCSS |
		BK4819_REG_51_AUTO_CDCSS_BW_ENABLE |
		BK4819_REG_51_AUTO_CTCSS_BW_ENABLE |
		(51u << BK4819_REG_51_SHIFT_CxCSS_TX_GAIN1));

	uint32_t cdcssFreq;
	uint16_t ctcssFreq;
	BK4819_CssScanResult_t scanResult = BK4819_GetCxCSSScanResult(&cdcssFreq, &ctcssFreq);

	if (scanResult == BK4819_CSS_RESULT_CTCSS) {
		uint8_t code = DCS_GetCtcssCode(ctcssFreq);
		if (code < ARRAY_SIZE(CTCSS_Options))
			sprintf(gSubtone_String, "%u.%uHz", CTCSS_Options[code] / 10, CTCSS_Options[code] % 10);
	}
	else if (scanResult == BK4819_CSS_RESULT_CDCSS) {
		uint8_t code = DCS_GetCdcssCode(cdcssFreq);
		if (code != 0xFF)
			sprintf(gSubtone_String, "D%03oN", DCS_Options[code]);
	}

	// === AFC, RSSI и S-meter — РАЗДЕЛЬНЫЕ НЕЗАВИСИМЫЕ СТРОКИ САМЫМ МАЛЕНЬКИМ ШРИФТОМ ===
	// Очищаем строку 6 (если нужно — можно убрать)
	// memset(gFrameBuffer[6], 0, LCD_WIDTH);

	char afcStr[16] = "";
	char rssiStr[16] = "";
	char smeterStr[16] = "";

	// AFC
	int32_t hz = ((int64_t)(int16_t)BK4819_ReadRegister(0x6D) * 1000LL) / 291LL;
	if (hz != 0) {
		sprintf(afcStr, "AFC:%+d", (int)hz);
	}

	// RSSI
	sprintf(rssiStr, "dBm%d", sLevelAtt.dBmRssi);

	// S-meter
	if (overS9Bars == 0)
		sprintf(smeterStr, "S%d", sLevelAtt.sLevel);
	else
		sprintf(smeterStr, "+%ddB", sLevelAtt.over);

	// РАЗДЕЛЬНЫЕ КООРДИНАТЫ ДЛЯ КАЖДОЙ СТРОКИ В MR И VFO
	// Формат: { y_mr, x_mr, y_vfo, x_vfo }

	// AFC
	if (afcStr[0] != '\0')
	{
		uint8_t y_mr = 2; uint8_t x_mr = 2;   // MR: Y=48 (строка 6), X=10 (слева)
		uint8_t y_vfo = 2; uint8_t x_vfo = 4; // VFO: Y=48, X=10

		uint8_t y = IS_MR_CHANNEL(gEeprom.ScreenChannel) ? y_mr : y_vfo;
		uint8_t x = IS_MR_CHANNEL(gEeprom.ScreenChannel) ? x_mr : x_vfo;

		GUI_DisplaySmallestDark(afcStr, x, y, false, true);
	}

	// RSSI
	if (rssiStr[0] != '\0')
	{
		uint8_t y_mr = 2; uint8_t x_mr = 84;   // MR: центр
		uint8_t y_vfo =2; uint8_t x_vfo = 84; // VFO: центр

		uint8_t y = IS_MR_CHANNEL(gEeprom.ScreenChannel) ? y_mr : y_vfo;
		uint8_t x = IS_MR_CHANNEL(gEeprom.ScreenChannel) ? x_mr : x_vfo;

		GUI_DisplaySmallestDark(rssiStr, x, y, false, true);
	}

	// S-meter
	if (smeterStr[0] != '\0')
	{
		uint8_t y_mr = 2; uint8_t x_mr = 70;   // MR: справа
		uint8_t y_vfo = 2; uint8_t x_vfo = 70; // VFO: справа

		uint8_t y = IS_MR_CHANNEL(gEeprom.ScreenChannel) ? y_mr : y_vfo;
		uint8_t x = IS_MR_CHANNEL(gEeprom.ScreenChannel) ? x_mr : x_vfo;

		GUI_DisplaySmallestDark(smeterStr, x, y, false, true);
	}

		/*/ Независимые позиции строки AFC + RSSI
	uint8_t text_line;
	uint8_t x_pos;

	if (IS_MR_CHANNEL(gEeprom.ScreenChannel)) {
		text_line = 6;  // ← строка в MR (меняй 0-7)
		x_pos = 64;     // ← позиция в MR (центр или фиксированная)
	} else {
		text_line = 6;  // ← строка в VFO
		x_pos = (LCD_WIDTH - text_width) / 2;  // центр или фиксированная
	}

	memset(gFrameBuffer[text_line], 0, LCD_WIDTH);
	UI_PrintStringSmall(fullStr, x_pos, 0, text_line, 0);*/
}

//***********************ЭКРАН ЧАСТОТЫ И ниже--------------------------------------//

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

	int line = line0;   // теперь можно использовать line - 10, line - 20 и т.д. без ошибок
	//unsigned int state = VfoState;
		uint32_t frequency = gEeprom.VfoInfo.pRX->Frequency;
	//const unsigned int line       = line0 ;
	//uint8_t           *p_line    = gFrameBuffer[line + 5];
	unsigned int       mode       = 0;




	//****************НОМЕР КАНАЛА ВВОД **************Channel mode
		if (IS_MR_CHANNEL(gEeprom.ScreenChannel))
		{
			const bool inputting = (gInputBoxIndex == 0 ) ? false : true;
			if (!inputting)
				sprintf(String, "M%u", gEeprom.ScreenChannel + 1);
			else
				sprintf(String, "M%.3s", INPUTBOX_GetAscii());  // show the input text
			//UI_PrintString(String , 1, 0, line+1 ,8);
			//	UI_PrintStringSmall(String, 1, 0, line + 3, 0); //Тот же шрифт 6×8, то же место (4 пикселя слева, line+2)
			UI_PrintString(String, 18 - (strlen(String) * 4), 0, line + 1, 8); // по центру
		}
		
	
						// ВВОД ЧАСТОТЫ В VFO — МЕНЯЙ ЗДЕСЬ
				if (gInputBoxIndex > 0 && gEeprom.ScreenChannel > MR_CHANNEL_LAST) {
					const char *ascii = INPUTBOX_GetAscii();
					bool isGigaF = frequency >= 100000000;
					sprintf(String, "%.*s.%.3s", 3 + isGigaF, ascii, ascii + 3 + isGigaF);
					UI_PrintStringSmall(String + 7, 85, 0, line + 2, 0); // ← X нулей: 85 | ← Y нулей: line + 4
					String[7] = 0;
					UI_DisplayFrequency(String, 25, line + 2, false);   // ← X больших: 16 | ← Y больших: line + 4
				}
				else {
					if (gCurrentFunction == FUNCTION_TRANSMIT) frequency = gEeprom.VfoInfo.pTX->Frequency;
					else if (gEeprom.ScreenChannel <= MR_CHANNEL_LAST) frequency = BOARD_fetchChannelFrequency(gEeprom.ScreenChannel);
								
				// МЕЛКИЕ НУЛИ ЧАСТОТЫ
				sprintf(String, "%3u.%05u", frequency / 100000, frequency % 100000);
					uint8_t small_y = (IS_MR_CHANNEL(gEeprom.ScreenChannel)) ? line + 3 : line + 2;
					uint8_t small_x_center = (IS_MR_CHANNEL(gEeprom.ScreenChannel)) ? 116 : 114;  // ← МX-центр нулей в MR/VFO
					UI_PrintString(String + 7, 
					               small_x_center - (strlen(String + 7) * 6 / 2), 
					               0, small_y, 8);
					String[7] = 0;

				// БОЛЬШАЯ ЧАСТОТА — в MR тем же шрифтом, что нули (6x8), на той же линии
				uint8_t big_y = (IS_MR_CHANNEL(gEeprom.ScreenChannel)) ? line + 3 : line + 2;
				uint8_t big_x_center = (IS_MR_CHANNEL(gEeprom.ScreenChannel)) ? 68 : 50;

				if (IS_MR_CHANNEL(gEeprom.ScreenChannel))
				{
					// В MR — основная часть (до точки) шрифтом 6x8
					String[7] = '\0';  // обрезаем до точки (String = "446.025")
					UI_PrintString(String, big_x_center - (strlen(String) * 6 / 2), 0, big_y, 8);
				}
				else
				{
					// В VFO — большие цифры как было
					UI_DisplayFrequency(String, big_x_center - (strlen(String) * 8 / 2), big_y, false);
				}


								// ИМЯ КАНАЛА 
				if (IS_MR_CHANNEL(gEeprom.ScreenChannel))
				{
					const ChannelAttributes_t att = gMR_ChannelAttributes[gEeprom.ScreenChannel];
					if (att.scanlist > 0) {
						sprintf(String, "L%d", att.scanlist);
						GUI_DisplaySmallestDark(String, 20, 26, false, true); // СПИСОК СКАНИРОВАНИЯ
					}

					const bool inputting = (gInputBoxIndex == 0) ? false : true;
					if (!inputting) {
						char DisplayString[22];

						// Пробуем взять имя канала
						SETTINGS_FetchChannelName(DisplayString, gEeprom.ScreenChannel);

						// Если имени нет — берём текущую частоту (как в VFO-режиме)
						if (DisplayString[0] == 0) {
							uint32_t freq = BOARD_fetchChannelFrequency(gEeprom.ScreenChannel);
							sprintf(DisplayString, "%u.%05u", freq / 100000, freq % 100000);  // ← ПОЛНАЯ ЧАСТОТА С НУЛЯМИ
						}

						// Выводим ИМЯ КАНАЛА (или частоту с нулями)
						UI_PrintString(DisplayString, 85 - (strlen(DisplayString) * 4), 0, line + 1, 8); // по центру
					}
				}
			}


		String[0] = '\0';
		

		//-------------------------------///ЗНАК МОДУЛЯЦИИ FM — ТЕМ ЖЕ ШРИФТОМ, ЧТО И T (PTT), ПО ЦЕНТРУ, ОТДЕЛЬНО ДЛЯ MR/VFO-----------------------
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

		if (s[0] != '\0')
		{
			// ОТДЕЛЬНЫЕ НАСТРОЙКИ ДЛЯ MR И VFO
			uint8_t x_mr  = 116;   uint8_t y_mr  = 6;  // MR: X=11 (центр), Y=34
			uint8_t x_vfo =116;   uint8_t y_vfo = 6;  // VFO: X=11 (центр), Y=34

			// Компенсация полпикселя (для ровного центрирования)
			int8_t comp_mr  = -1;
			int8_t comp_vfo = -1;

			// Выбираем координаты в зависимости от режима
			uint8_t mod_x_base = IS_MR_CHANNEL(gEeprom.ScreenChannel) ? x_mr : x_vfo;
			int8_t  comp       = IS_MR_CHANNEL(gEeprom.ScreenChannel) ? comp_mr : comp_vfo;
			uint8_t mod_y      = IS_MR_CHANNEL(gEeprom.ScreenChannel) ? y_mr : y_vfo;

			// Центрирование по X
			uint8_t mod_x = mod_x_base - (strlen(s) * 7 / 2) + comp;

			// Выводим тем же шрифтом, что и T (PTT) — UI_PrintStringSmall
			UI_PrintStringSmall(s, mod_x, 0, mod_y, 0);
		}


		// ───────────────────── РЕЖИМ PTT (T) ───────────────────────MR/VFO
	if (Ptt_Toggle_Mode) {
		uint8_t x_mr = 4;    uint8_t y_mr = line + 2;
		uint8_t x_vfo = 4;   uint8_t y_vfo = line + 1;
		uint8_t x = IS_MR_CHANNEL(gEeprom.ScreenChannel) ? x_mr : x_vfo;
		uint8_t y = IS_MR_CHANNEL(gEeprom.ScreenChannel) ? y_mr : y_vfo;
		if (x != 255 && y != 255)
			UI_PrintStringSmall("S", LCD_WIDTH + x, 0, y, 0);
	}

		
// ───────────────────── МОЩНОСТЬ (L/M/H) — В НИЖНЕЙ СТРОКЕ, ПОКАЗЫВАЕТСЯ ВСЕГДА, Y ОТДЕЛЬНО ДЛЯ MR/VFO ─────────────────────
{
    uint8_t x_mr  = 91;   // MR: X=100 (рядом с полосой BW)
    uint8_t x_vfo = 91;   // VFO: X=100
    uint8_t y_mr  = line + 5;  // MR: Y=line+5 (нижняя строка)
    uint8_t y_vfo = line + 5;  // VFO: Y=line+5 (нижняя строка)

    const char pwr[] = "LMH";
    char p = gEeprom.VfoInfo.OUTPUT_POWER < 3 ? pwr[gEeprom.VfoInfo.OUTPUT_POWER] : '?';

    uint8_t x = IS_MR_CHANNEL(gEeprom.ScreenChannel) ? x_mr : x_vfo;
    uint8_t y = IS_MR_CHANNEL(gEeprom.ScreenChannel) ? y_mr : y_vfo;

    if (x != 255 && y != 255)
        UI_PrintStringSmall((char[]){p, 0}, LCD_WIDTH + x, 0, y, 0);
}


		// ───────────────────── СМЕЩЕНИЕ (+ / –) ─────────────────────MR/VFO
	if (gEeprom.VfoInfo.freq_config_RX.Frequency != gEeprom.VfoInfo.freq_config_TX.Frequency) {
		uint8_t x_mr  = 4;    uint8_t y_mr  = line + 3;
		uint8_t x_vfo = 4;    uint8_t y_vfo = line + 2;
		const char *dir = "";
		switch (gEeprom.VfoInfo.TX_OFFSET_FREQUENCY_DIRECTION) {
			case 0: dir = "";  break;  // нет смещения
			case 1: dir = "+"; break;  // плюс
			case 2: dir = "-"; break;  // минус
		}

		uint8_t x = IS_MR_CHANNEL(gEeprom.ScreenChannel) ? x_mr : x_vfo;
		uint8_t y = IS_MR_CHANNEL(gEeprom.ScreenChannel) ? y_mr : y_vfo;
		if (x != 255 && y != 255 && dir[0] != 0)
			UI_PrintStringSmall(dir, LCD_WIDTH + x, 0, y, 0);
			// ← МЕНЯЙ x_mr/x_vfo и y_mr/y_vfo
	}

	// ───────────────────── ШАГ ─────────────────────MR/VFO
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
		// ───────────────────── ШАГ — полностью разделён по MR/VFO, без дублей и ошибок ─────────────────────
	{
		// Настройки для MR и VFO
		uint8_t base_x_mr  = 66;   // MR: от какого числа идёт центрирование (можно менять)
		uint8_t base_x_vfo = 66;   // VFO: от какого числа идёт центрирование (было 105)
		uint8_t y_mr        = line + 5;
		uint8_t y_vfo       = line + 5;

		// Формируем строку шага (твой 100% рабочий код — оставляем как есть)
		char stepStr[8];
		const uint16_t step = gStepFrequencyTable[gTxVfo->STEP_SETTING];

		if (step == 833) {
			strcpy(stepStr, "8.33k");
		}
		else {
			uint32_t v = (uint32_t)step * 10;
			uint16_t integer = v / 1000;
			uint16_t decimal = (v % 1000) / 10;

			if (integer == 0) {
				sprintf(stepStr, "0.%02u", decimal);
			}
			else if (integer >= 100) {
				sprintf(stepStr, "%u", integer);
			}
			else {
				sprintf(stepStr, "%u.%02u", integer, decimal);
			}
		}

		// Выбираем нужные координаты в зависимости от режима
		uint8_t base_x = IS_MR_CHANNEL(gEeprom.ScreenChannel) ? base_x_mr : base_x_vfo;
		uint8_t y      = IS_MR_CHANNEL(gEeprom.ScreenChannel) ? y_mr       : y_vfo;

		// Рисуем шаг
		if (y != 255) {
			UI_PrintStringSmall(stepStr, LCD_WIDTH + base_x - (strlen(stepStr) * 3), 0, y, 0);
		}
		// ← Чтобы скрыть шаг в MR: base_x_mr = 255
		// ← Чтобы скрыть шаг в VFO: base_x_vfo = 255
	}
// ─────────────────────── ШУМОДАВ (U0-U9) ───────────────────────
	{
		uint8_t x_mr  = 8;   uint8_t y_mr  = line + 5;   // ← MR:  X=20
		uint8_t x_vfo = 8;   uint8_t y_vfo = line + 5;   // ← VFO: X=20
		char sqlStr[6];
		sprintf(sqlStr, "%u", gEeprom.SQUELCH_LEVEL);
		uint8_t x = IS_MR_CHANNEL(gEeprom.ScreenChannel) ? x_mr : x_vfo;
		uint8_t y = IS_MR_CHANNEL(gEeprom.ScreenChannel) ? y_mr : y_vfo;
		if (x != 255 && y != 255)
			UI_PrintStringSmall(sqlStr, LCD_WIDTH + x, 0, y, 0); // ← МЕНЯЙ x_mr/x_vfo и y_mr/y_vfo
	}

// ───────────────────── ПОЛОСА — полностью разделена по MR/VFO, как шумодав ─────────────────────
{
    // Настройки для MR и VFO отдельно
    uint8_t x_mr  = 34;   uint8_t y_mr  = line + 5;   // ← MR: X и Y
    uint8_t x_vfo = 34;   uint8_t y_vfo = line + 5;   // ← VFO: X и Y

    //const char *bw = bwNames[gEeprom.VfoInfo.CHANNEL_BANDWIDTH];
	char bwStr[8];
	// Убираем "k" из конца строки
	strcpy(bwStr, bwNames[gEeprom.VfoInfo.CHANNEL_BANDWIDTH]);
	size_t len = strlen(bwStr);
	if (len > 0 && bwStr[len-1] == 'k') bwStr[len-1] = '\0';
	//*************end */
const char *bw = bwStr;

    uint8_t x = IS_MR_CHANNEL(gEeprom.ScreenChannel) ? x_mr : x_vfo;
    uint8_t y = IS_MR_CHANNEL(gEeprom.ScreenChannel) ? y_mr : y_vfo;

    if (y != 255)
    {
        // Центрирование по X (как раньше, но теперь с отдельным x)
        UI_PrintStringSmall(bw, LCD_WIDTH + x - (strlen(bw) * 3), 0, y, 0);
    }

    // ← МЕНЯЙ x_mr/x_vfo и y_mr/y_vfo отдельно для каждого режима
}


			// SCR (если включён)!!!
			if (gEeprom.VfoInfo.SCRAMBLING_TYPE > 0 && gSetting_ScrambleEnable)
				UI_PrintStringSmall("SCR", LCD_WIDTH + 106, 0, line + 5, 0);
		}


	if (center_line == CENTER_LINE_NONE)
	{	
		if (gCurrentFunction == FUNCTION_TRANSMIT) {
			center_line = CENTER_LINE_AUDIO_BAR;
			//UI_DisplayAudioBar();
		}

	}


	// ───────────────────── СУБТОН (CTCSS/CDCSS) — САМЫЙ МАЛЕНЬКИЙ ШРИФТ, ПО ЦЕНТРУ, ОТДЕЛЬНО ДЛЯ VFO И MR ─────────────────────
		
//if (gSubtone_String[0] != '\0')
if (!IS_MR_CHANNEL(gEeprom.ScreenChannel) && gSubtone_String[0] != '\0') //скрыть мр
{
    // ОТДЕЛЬНЫЕ НАСТРОЙКИ ДЛЯ VFO И MR
    uint8_t x_vfo = 115;   uint8_t y_vfo = 32;  // VFO: центр по X (64), строка 2
    uint8_t x_mr  = 60;   uint8_t y_mr  = 2;  // MR: центр по X (64), строка 2

    // Выбираем координаты в зависимости от режима
    uint8_t subtone_x = IS_MR_CHANNEL(gEeprom.ScreenChannel) ? x_mr : x_vfo;
    uint8_t subtone_y = IS_MR_CHANNEL(gEeprom.ScreenChannel) ? y_mr : y_vfo;

    // Центрирование по X (автоматическое)
    uint8_t text_len = strlen(gSubtone_String);
    uint8_t text_width = text_len * 4;  // ширина символа в GUI_DisplaySmallest — 4 пикселя
    subtone_x = subtone_x - (text_width / 2);

    // Выводим самым маленьким шрифтом
    GUI_DisplaySmallest(gSubtone_String, subtone_x, subtone_y, false, true);

}

//************************УВЕДОМЛЕНИЯ — ОБЕ ПОЛОВИНЫ С БЕЛЫМ ФОНОМ, Y ОТДЕЛЬНО ДЛЯ MR/VFO************************** */
unsigned int state = VfoState;

if (state != VFO_STATE_NORMAL)
{
    const char *state_list[] = {"", "BUSY", "BAT LOW", "TX DISABLE", "TIMEOUT", "ALARM", "VOLT HIGH"};
    if (state < ARRAY_SIZE(state_list))
    {
        const char *msg = state_list[state];

        // ОТДЕЛЬНЫЕ НАСТРОЙКИ ДЛЯ Y — МЕНЯЙ ЗДЕСЬ
        uint8_t y_mr  = line + 3;  // MR: выше
        uint8_t y_vfo = line + 2;  // VFO: стандарт

        // Выбираем Y в зависимости от режима
        uint8_t y_pos = IS_MR_CHANNEL(gEeprom.ScreenChannel) ? y_mr : y_vfo;

        // Очищаем ТЕКУЩУЮ и СЛЕДУЮЩУЮ строку — ОБЕ ПОЛОВИНЫ с белым фоном
        memset(gFrameBuffer[y_pos], 0, LCD_WIDTH);      // Верхняя половина
        memset(gFrameBuffer[y_pos + 1], 0, LCD_WIDTH);  // Нижняя половина

        // Центрирование по X (большой шрифт 8x16)
        uint8_t text_width = strlen(msg) * 8;
        uint8_t x_pos = (LCD_WIDTH - text_width) / 2;

        UI_PrintString(msg, x_pos, 0, y_pos, 8);
    }
}


//****************************************ЛИНИИ******LINES******************************************//


// КАСТОМНЫЕ ГОРИЗОНТАЛЬНЫЕ ЛИНИИ — ОТДЕЛЬНО ДЛЯ VFO И MR
// ============================================================================

typedef struct {
	uint8_t y;        // высота линии (0..63)
	uint8_t x_start;  // начало по X (отступ слева)
	uint8_t x_end;    // конец по X (отступ справа)
	uint8_t step;     // шаг пунктира: 1 = сплошная, 2 = •◦, 3 = •◦◦ и т.д.
} dashed_line_t;

// ─────────────────────────────── VFO РЕЖИМ ───────────────────────────────
static const dashed_line_t vfo_lines[] = {
	{ 8,  0, 127, 2 },   // верхняя линия — частый пунктир
	{ 16,  0, 127, 2 },   // верхняя линия — частый пунктир
	{ 21,   0, 127, 2 },   // центральная — сплошная
	{ 48,  0, 127, 2 },   // нижняя — редкий пунктир
	{ 52,  0, 127, 2 },   // нижняя — редкий пунктир
};

// ─────────────────────────────── MR РЕЖИМ ────────────────────────────────
static const dashed_line_t mr_lines[] = {
	{ 8,  0, 127, 2 },   // верхняя линия — частый пунктир
	{ 16,  0, 127, 2 },   // верхняя линия — частый пунктир
	{ 30,  0, 127, 2 },   // нижняя — редкий пунктирир
	{ 48,  0, 127, 2 },   // нижняя — редкий пунктир
	{ 52,  0, 127, 2 },   // нижняя — редкий пунктир
};

// Выбираем нужный массив и рисуем
{
	const dashed_line_t *lines;
	uint8_t              num_lines;

	if (IS_MR_CHANNEL(gEeprom.ScreenChannel)) {
		lines     = mr_lines;
		num_lines = ARRAY_SIZE(mr_lines);
	} else {
		lines     = vfo_lines;
		num_lines = ARRAY_SIZE(vfo_lines);
	}

	for (uint8_t i = 0; i < num_lines; i++) {
		const dashed_line_t *l = &lines[i];
		const uint8_t y = l->y;

		for (uint8_t x = l->x_start; x <= l->x_end; x += l->step) {
			if (y < 8)
				gStatusLine[x] |= (1u << y);                                         // статусная строка
			else
				gFrameBuffer[(y - 8) >> 3][x] |= (1u << ((y - 8) & 7));              // основной экран
		}
	}
}

// КАСТОМНЫЕ ВЕРТИКАЛЬНЫЕ ПУНКТИРНЫЕ ЛИНИИ — ОТДЕЛЬНО ДЛЯ VFO И MR
// ============================================================================

typedef struct {
	uint8_t x;        // позиция по X (0..127)
	uint8_t y_start;  // начало по Y (сверху)
	uint8_t y_end;    // конец по Y (снизу)
	uint8_t step;     // шаг пунктира: 1 = сплошная, 2 = •◦, 3 = •◦◦ и т.д.
} vertical_dashed_t;

// ─────────────────────────────── VFO РЕЖИМ ───────────────────────────────
static const vertical_dashed_t vfo_vlines[] = {
	{  14, 23, 46, 2 },   // левая сплошная (обрамляет частоту)
	// { 50, 15, 50, 4 },
};

// ─────────────────────────────── MR РЕЖИМ ────────────────────────────────
static const vertical_dashed_t mr_vlines[] = {
	{  35, 18, 28, 2 },   // чуть правее, чем в VFO — под Mxxx
	{  14, 32, 47, 2 },  

};

// Рисуем все вертикальные линии
{
	const vertical_dashed_t *vlines;
	uint8_t                 num_vlines;

	if (IS_MR_CHANNEL(gEeprom.ScreenChannel)) {
		vlines    = mr_vlines;
		num_vlines = ARRAY_SIZE(mr_vlines);
	} else {
		vlines    = vfo_vlines;
		num_vlines = ARRAY_SIZE(vfo_vlines);
	}

	for (uint8_t i = 0; i < num_vlines; i++) {
		const vertical_dashed_t *l = &vlines[i];
		const uint8_t x = l->x;

		for (uint8_t y = l->y_start; y <= l->y_end; y += l->step) {
			if (y < 8)
				gStatusLine[x] |= (1u << y);
			else
				gFrameBuffer[(y - 8) >> 3][x] |= (1u << ((y - 8) & 7));
		}
	}
}

// ───────────────────── СВОБОДНЫЕ ТЕКСТЫ — 4 независимые строки ─────────────────────
{
	if (IS_MR_CHANNEL(gEeprom.ScreenChannel))
	{
		// MR-режим — две строки
		//GUI_DisplaySmallest("MR MODE",     1, 2, false, true); 
		GUI_DisplaySmallestDark	("MR MODE",     2, 2, false, true);    // false, true шаг между символами
		GUI_DisplaySmallest		("CHANEL NAME",     82, 2, false, true);   // X=8,  Y=18
		GUI_DisplaySmallestDark("SQL",  6, 40, false, false);
		GUI_DisplaySmallestDark("BAND", 28, 40, false, false);
		GUI_DisplaySmallestDark("STEP", 58, 40, false, false);
		GUI_DisplaySmallestDark("POW",  88, 40, false, false);
		GUI_DisplaySmallestDark("MOD",  110, 40, false, false);
	
	}
	else
	{
		// VFO-режим — две строки
		//GUI_DisplaySmallest("VFO MODE",     1, 2, false, true);   // X=15, Y=10
		GUI_DisplaySmallestDark("VFO",     1, 2, false, true); 
		GUI_DisplaySmallestDark("MODE",     20, 2, false, true); 
		GUI_DisplaySmallest	   ("FREQUENCY",  90, 2, false, true);   // X=22, Y=18
		GUI_DisplaySmallestDark("SQL",  6, 40, false, false);
		GUI_DisplaySmallestDark("BAND", 28, 40, false, false);
		GUI_DisplaySmallestDark("STEP", 58, 40, false, false);
		GUI_DisplaySmallestDark("POW",  88, 40, false, false);
		GUI_DisplaySmallestDark("MOD",  110, 40, false, false);
		
		
		
	}
}


	/*/ === БУКВА "B" (ИНВЕРТИРОВАННАЯ) ПРИ "ПОДСВЕТКА ВСЕГДА ВКЛЮЧЕНА" (F+8) ===
	if (gBacklightAlwaysOn) {
		// Позиция рядом с батареей — X = 110, Y = 0 (статусная строка)
		GUI_DisplaySmallestDark("B", 70, 2, false, false);
	}*/

		// АУДИОБАР — рисуем ПОСЛЕДНИМ, чтобы был поверх всех надписей
	UI_DisplayAudioBar();

	ST7565_BlitFullScreen();
}