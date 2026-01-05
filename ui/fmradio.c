/* Original work Copyright 2023 Dual Tachyon
 * https://github.com/DualTachyon
 *
 * Modified work Copyright 2024 kamilsss655
 * https://github.com/kamilsss655
 *
 * Copyright 2024 kamilsss655
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
#include "driver/st7565.h"
#include "external/printf/printf.h"
#include "settings.h"
#include "ui/helper.h"
#include <stdint.h>     // для uint8_t
#include "ui/ui.h"      // для gFrameBuffer и gStatusLine

void UI_DisplayFM(void)
{
	char String[16];

	// Очистка экрана
	memset(gFrameBuffer, 0, sizeof(gFrameBuffer));

	// === Текст "FM" большим шрифтом ===
	strcpy(String, "FM");
	UI_PrintString(String, 25, 10, 2, 10);

	// === Частота большим шрифтом ===
	memset(String, 0, sizeof(String));
	sprintf(String, "%3d.%d", gEeprom.FM_FrequencyPlaying / 10, gEeprom.FM_FrequencyPlaying % 10);
	UI_DisplayFrequency(String, 55, 2, true);




	// === ПУНКТИРНОСТЬ (меняй здесь) ===
	uint8_t step = 2;  // 1 = сплошная, 2 = •◦, 3 = •◦◦

	// === ДВЕ ГОРИЗОНТАЛЬНЫЕ ЛИНИИ ===
	// Верхняя (Y = 16)
	for (uint8_t x = 0; x < 128; x += step) {
		uint8_t y = 16;
		if (y < 8) gStatusLine[x] |= (1u << y);
		else gFrameBuffer[(y - 8) >> 3][x] |= (1u << ((y - 8) & 7));
	}

	// Нижняя (Y = 48)
	for (uint8_t x = 8; x < 125; x += step) {
		uint8_t y = 44;
		if (y < 8) gStatusLine[x] |= (1u << y);
		else gFrameBuffer[(y - 8) >> 3][x] |= (1u << ((y - 8) & 7));
	}

	// Нижняя (Y = 58)
	for (uint8_t x = 2; x < 126; x += step) {
		uint8_t y = 58;
		if (y < 8) gStatusLine[x] |= (1u << y);
		else gFrameBuffer[(y - 8) >> 3][x] |= (1u << ((y - 8) & 7));
	}

	// === ДВЕ ВЕРТИКАЛЬНЫЕ ЛИНИИ ===
	// Левая (X = 14, Y от 10 до 54)
	for (uint8_t y = 9; y <= 53; y += step) {
		uint8_t x = 14;
		if (y < 8) gStatusLine[x] |= (1u << y);
		else gFrameBuffer[(y - 8) >> 3][x] |= (1u << ((y - 8) & 7));
	}

	// Правая (X = 113, Y от 20 до 40)
	for (uint8_t y = 20; y <= 50; y += step) {
		uint8_t x = 120;
		if (y < 8) gStatusLine[x] |= (1u << y);
		else gFrameBuffer[(y - 8) >> 3][x] |= (1u << ((y - 8) & 7));
	}
	// === Надписи мелким дарк шрифтом ===
	GUI_DisplaySmallestDark("BROADCAST", 6, 6, false, true);
	GUI_DisplaySmallestDark("STATION", 66, 38, false, true);


	ST7565_BlitFullScreen();
}