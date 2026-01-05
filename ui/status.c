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
#include <stdio.h>  // для sprintf

#include "app/fm.h"
#include "app/scanner.h"
#include "bitmaps.h"
#include "driver/keyboard.h"
#include "driver/st7565.h"
#include "external/printf/printf.h"
#include "functions.h"
#include "helper/battery.h"
#include "misc.h"
#include "settings.h"
#include "ui/battery.h"
#include "ui/helper.h"
#include "ui/ui.h"
#include "ui/status.h"
#include "app/main.h"  // для txTimeSeconds, rxTimeSeconds, isTxActive

void UI_DisplayStatus()
{
	char time_str[6];
	const uint8_t TIME_POS_X = 16;  // редактируемая позиция времени (X в пикселях, можно менять)

	memset(gStatusLine, 0, sizeof(gStatusLine));

	// === ОТДЕЛЬНЫЕ ПОЗИЦИИ ДЛЯ КАЖДОГО ИНДИКАТОРА ===
	const uint8_t POS_TX   = 0;    // начало "TX" при передаче
	const uint8_t POS_RX   = 0;    // начало "RX" при приёме
	const uint8_t POS_PS   = 0;    // начало "PS" при Power Save
	const uint8_t POS_LOCK = 72;   // Замок
	const uint8_t POS_F    = 80;   // "F"
	const uint8_t POS_B    = 88;   // "B"

	// === Индикатор передачи "TX" (двойной) ===
	if (gCurrentFunction == FUNCTION_TRANSMIT)
	{
		// "T"
		gStatusLine[POS_TX + 0] |= 0x7F;
		gStatusLine[POS_TX + 1] |= 0x7D;
		gStatusLine[POS_TX + 2] |= 0x41;
		gStatusLine[POS_TX + 3] |= 0x41;
		gStatusLine[POS_TX + 4] |= 0x7D;
		gStatusLine[POS_TX + 5] |= 0x7F;

		// "X" вплотную после "T" (без пробела)
		gStatusLine[POS_TX + 6] |= 0x5D;
		gStatusLine[POS_TX + 7] |= 0x49;
		gStatusLine[POS_TX + 8] |= 0x63;
		gStatusLine[POS_TX + 9] |= 0x49;
		gStatusLine[POS_TX + 10] |= 0x5D;
		gStatusLine[POS_TX + 11] |= 0x7F;

		// Время TX — тем же шрифтом, что и батарея
		sprintf(time_str, "%02u:%02u", txTimeSeconds / 60, txTimeSeconds % 60);
		UI_PrintStringSmallBuffer(time_str, gStatusLine + TIME_POS_X);
	}

	// === Индикатор приёма "RX" (двойной) ===
	if (gCurrentFunction == FUNCTION_RECEIVE ||
	    gCurrentFunction == FUNCTION_MONITOR ||
	    gCurrentFunction == FUNCTION_INCOMING)
	{
		// "R"
		gStatusLine[POS_RX + 0] |= 0x7F;
		gStatusLine[POS_RX + 1] |= 0x41;
		gStatusLine[POS_RX + 2] |= 0x75;
		gStatusLine[POS_RX + 3] |= 0x75;
		gStatusLine[POS_RX + 4] |= 0x49;
		gStatusLine[POS_RX + 5] |= 0x7F;

		// "X" вплотную после "R"
		gStatusLine[POS_RX + 6] |= 0x5D;
		gStatusLine[POS_RX + 7] |= 0x49;
		gStatusLine[POS_RX + 8] |= 0x63;
		gStatusLine[POS_RX + 9] |= 0x49;
		gStatusLine[POS_RX + 10] |= 0x5D;
		gStatusLine[POS_RX + 11] |= 0x7F;

		// Время RX — тем же шрифтом, что и батарея
		sprintf(time_str, "%02u:%02u", rxTimeSeconds / 60, rxTimeSeconds % 60);
		UI_PrintStringSmallBuffer(time_str, gStatusLine + TIME_POS_X);
	}

	// === Индикатор Power Save "PS" (двойной, расстояние уменьшено на 1 пиксель) ===
	if (gCurrentFunction == FUNCTION_POWER_SAVE)
	{
		// "P"
		gStatusLine[POS_PS + 0] |= 0x7F;
		gStatusLine[POS_PS + 1] |= 0x41;
		gStatusLine[POS_PS + 2] |= 0x75;
		gStatusLine[POS_PS + 3] |= 0x75;
		gStatusLine[POS_PS + 4] |= 0x71;
		gStatusLine[POS_PS + 5] |= 0x7F;

		// "S" вплотную после "P" (отступ 0 колонок)
		gStatusLine[POS_PS + 6]  |= 0x7F;
		gStatusLine[POS_PS + 7]  |= 0x51;
		gStatusLine[POS_PS + 8]  |= 0x55;
		gStatusLine[POS_PS + 9]  |= 0x55;
		gStatusLine[POS_PS + 10] |= 0x45;
		gStatusLine[POS_PS + 11] |= 0x7F;
	}

	// === Индикатор блокировки клавиатуры (замок) ===
	if (gEeprom.KEY_LOCK)
	{
		gStatusLine[POS_LOCK + 0] |= 0x7F;
		gStatusLine[POS_LOCK + 1] |= 0x41;
		gStatusLine[POS_LOCK + 2] |= 0x45;
		gStatusLine[POS_LOCK + 3] |= 0x45;
		gStatusLine[POS_LOCK + 4] |= 0x41;
		gStatusLine[POS_LOCK + 5] |= 0x7F;
	}

	// === Индикатор нажатой F-клавиши ===
	if (gWasFKeyPressed)
	{
		gStatusLine[POS_F + 0] |= 0x7F;
		gStatusLine[POS_F + 1] |= 0x41;
		gStatusLine[POS_F + 2] |= 0x41;
		gStatusLine[POS_F + 3] |= 0x75;
		gStatusLine[POS_F + 4] |= 0x75;
		gStatusLine[POS_F + 5] |= 0x7F;
	}

	// === Индикатор постоянной подсветки "B" ===
	if (gBacklightAlwaysOn)
	{
		gStatusLine[POS_B + 0] |= 0x7F;
		gStatusLine[POS_B + 1] |= 0x73;
		gStatusLine[POS_B + 2] |= 0x4D;
		gStatusLine[POS_B + 3] |= 0x4D;
		gStatusLine[POS_B + 4] |= 0x73;
		gStatusLine[POS_B + 5] |= 0x7F;
	}

	// === Battery voltage / percentage ===
	{
		char s[8];
		unsigned int space_needed;
		unsigned int x2 = LCD_WIDTH - 3;

		switch (gSetting_battery_text)
		{
			case 1: // voltage
			{
				const uint16_t voltage = (gBatteryVoltageAverage <= 999) ? gBatteryVoltageAverage : 999;
				sprintf(s, "%u.%02uV", voltage / 100, voltage % 100);
				space_needed = 7 * strlen(s);
				if (x2 >= space_needed)
					UI_PrintStringSmallBuffer(s, gStatusLine + x2 - space_needed);
				break;
			}
			case 2: // percentage
			{
				sprintf(s, "%u%%", BATTERY_VoltsToPercent(gBatteryVoltageAverage));
				space_needed = 7 * strlen(s);
				if (x2 >= space_needed)
					UI_PrintStringSmallBuffer(s, gStatusLine + x2 - space_needed);
				break;
			}
		}
	}

	/*/ === Вертикальные линии ===
	uint8_t line_x[] = {0, 127};
	uint8_t num_lines = sizeof(line_x) / sizeof(line_x[0]);
	uint8_t dash_step = 2;

	for (uint8_t i = 0; i < num_lines; i++) {
		uint8_t px = line_x[i];
		if (dash_step == 1) {
			gStatusLine[px] = 0xFF;
		} else {
			uint8_t pattern = 0;
			for (uint8_t bit = 0; bit < 8; bit += dash_step) {
				pattern |= (1 << bit);
			}
			gStatusLine[px] |= pattern;
		}
	}*/

	ST7565_BlitStatusLine();
}