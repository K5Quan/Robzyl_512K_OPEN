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
#include "app/menu.h"
#include "bitmaps.h"
#include "board.h"
#include "dcs.h"
#include "driver/backlight.h"
#include "driver/bk4819.h"
#include "driver/eeprom.h"
#include "driver/st7565.h"
#include "external/printf/printf.h"
#include "frequencies.h"
#include "helper/battery.h"
#include "misc.h"
#include "settings.h"
#include "ui/helper.h"
#include "ui/inputbox.h"
#include "ui/menu.h"
#include "ui/ui.h"

const t_menu_item MenuList[] =
{
    //   text,     menu ID
	{"Step",    MENU_STEP          },
	{"Sql",     MENU_SQL           },
	{"Bandw",   MENU_W_N           },
	{"Demodu",  MENU_AM            },
	{"Roger",   MENU_ROGER         },
	{"ScraEn",  MENU_SCREN         }, 
	{"Scramb",  MENU_SCR           },
	{"ScanLi",  MENU_S_LIST        }, 
	{"TxTOut",  MENU_TOT           },
	{"Mic",     MENU_MIC           },	
	{"BackLt",  MENU_ABR           },
	{"BLMin",   MENU_ABR_MIN       },
	{"BLMax",   MENU_ABR_MAX       },
	{"BltTRX",  MENU_ABR_ON_TX_RX  },
	{"SqTone",  MENU_SQL_TONE      },
	{"RxDCS",   MENU_R_DCS         },
	{"RxCTCS",  MENU_R_CTCS        },
	{"TxDCS",   MENU_T_DCS         },
	{"TxCTCS",  MENU_T_CTCS        },
	{"TxODir",  MENU_SFT_D         },
	{"TxOffs",  MENU_OFFSET        },
	{"RxOffs",  MENU_RX_OFFSET     },
	{"BatSav",  MENU_SAVE          },
	{"ChSave",  MENU_MEM_CH        },
	// hidden menu items from here on
	// enabled if pressing 0 at power-on
	{"F1Shrt",  MENU_F1SHRT        },
	{"F1Long",  MENU_F1LONG        },
	{"F2Shrt",  MENU_F2SHRT        },
	{"F2Long",  MENU_F2LONG        },
	{"BatCal",  MENU_BATCAL        },
	{"BatTyp",  MENU_BATTYP        },
	{"TxPwr",   MENU_TXP           },
	{"BatVol",  MENU_VOL           },
	{"POnMsg",  MENU_PONMSG        },
	{"KeyLck",  MENU_AUTOLK        },
	{"BatTxt",  MENU_BAT_TXT       },	
	{"ChDele",  MENU_DEL_CH        },
	{"ChName",  MENU_MEM_NAME      },	
	{"Reset",   MENU_RESET         },
	{"F Lock",  MENU_F_LOCK        },
	{"",        0xff               }  // end of list - DO NOT delete or move this
};


const int CHANNEL_ONLY_SETTINGS[] = {
	MENU_S_LIST,
	MENU_DEL_CH,
	MENU_MEM_NAME,
};

const uint8_t FIRST_HIDDEN_MENU_ITEM = 0xFF; //unhide menus

const char gSubMenu_TXP[][5] =
{
	"LOW",
	"MID",
	"HIGH"
};

const char gSubMenu_SFT_D[][4] =
{
	"OFF",
	"+",
	"-"
};

const char gSubMenu_OFF_ON[][4] =
{
	"OFF",
	"ON"
};

const char gSubMenu_SAVE[][4] =
{
	"OFF",
	"50%",
	"67%",
	"75%",
	"80%"
};

const char gSubMenu_TOT[][7] =
{
	"30 sec",
	"1 min",
	"2 min",
	"3 min",
	"4 min",
	"5 min",
	"6 min",
	"7 min",
	"8 min",
	"9 min",
	"15 min"
};

const char gSubMenu_PONMSG[][8] =
{
	"FULL",
	"MESSAGE",
	"VOLTAGE",
	"NONE"
};

const char gSubMenu_ROGER[][7] =
{
	"OFF",
	"MARIO",
	"BLAST",
	"R2D2"
};

const char gSubMenu_RESET[][9] =
{
	"KEEP CH",
	"ALL"
};

const char * const gSubMenu_F_LOCK[] =
{
	"PMR446\nONLY",
	"136 TO\n500MHz\nONLY",
	"ENABLE\nALL",
	"DISABLE\nALL"
};

const char gSubMenu_BACKLIGHT[][7] =
{
	"OFF",
	"5 sec",
	"10 sec",
	"20 sec",
	"1 min",
	"2 min",
	"4 min",
	"ON"
};

const char gSubMenu_RX_TX[][6] =
{
	"OFF",
	"TX",
	"RX",
	"TX/RX"
};

const char gSubMenu_BAT_TXT[][8] =
{
	"NONE",
	"VOLTAGE",
	"PERCENT"
};

const char gSubMenu_BATTYP[][9] =
{
	"1600mAh",
	"2200mAh"
};

const char gSubMenu_SCRAMBLER[][7] =
{
	"OFF",
	"2600Hz",
	"2700Hz",
	"2800Hz",
	"2900Hz",
	"3000Hz",
	"3100Hz",
	"3200Hz",
	"3300Hz",
	"3400Hz",
	"3500Hz"
};

const t_sidefunction SIDEFUNCTIONS[] =
{
	{"NONE",			ACTION_OPT_NONE},
	{"FLASH\nLIGHT",	ACTION_OPT_FLASHLIGHT},
	{"TOGGLE\nPTT",		ACTION_OPT_TOGGLE_PTT},
	{"POWER",			ACTION_OPT_POWER},
	{"MONITOR",			ACTION_OPT_MONITOR},
	{"FM RADIO",		ACTION_OPT_FM},
#ifdef ENABLE_TX1750	
	{"1750HZ",			ACTION_OPT_1750},
#endif
	{"LOCK\nKEYPAD",	ACTION_OPT_KEYLOCK},
	{"VFO/MR",			ACTION_OPT_VFO_MR},
	{"SWITCH\nDEMODUL",	ACTION_OPT_SWITCH_DEMODUL},
	{"SWITCH\nBANDWID",	ACTION_OPT_BANDWIDTH},
	//{"SPECTRUM",		ACTION_OPT_SPECTRUM},
#ifdef ENABLE_BLMIN_TMP_OFF
	{"BLMIN\nTMP OFF",  ACTION_OPT_BLMIN_TMP_OFF}, 		//BackLight Minimum Temporay OFF
#endif
};
const t_sidefunction* gSubMenu_SIDEFUNCTIONS = SIDEFUNCTIONS;
const uint8_t gSubMenu_SIDEFUNCTIONS_size = ARRAY_SIZE(SIDEFUNCTIONS);
void MENU_PrintNotAllowed();

const unsigned int menu_list_width = 6; // max no. of characters on the menu list (left side)
const unsigned int menu_item_x1    = (8 * menu_list_width) + 2;
const unsigned int menu_item_x2    = LCD_WIDTH - 1;

bool    gIsInSubMenu;
uint8_t gMenuCursor;
int UI_MENU_GetCurrentMenuId() {
	if(gMenuCursor < ARRAY_SIZE(MenuList))
		return MenuList[gMenuCursor].menu_id;
	else
		return MenuList[ARRAY_SIZE(MenuList)-1].menu_id;
}

uint8_t UI_MENU_GetMenuIdx(uint8_t id)
{
	for(uint8_t i = 0; i < ARRAY_SIZE(MenuList); i++)
		if(MenuList[i].menu_id == id)
			return i;
	return 0;
}

int32_t gSubMenuSelection;

// edit box
char    edit_original[17]; // a copy of the text before editing so that we can easily test for changes/difference
char    edit[17];
int     edit_index;

void UI_DisplayMenu(void)
{
	unsigned int       i;
	char               String[64];  // bigger cuz we can now do multi-line in one string (use '\n' char)

	// clear the screen buffer
	memset(gFrameBuffer, 0, sizeof(gFrameBuffer));

	
	{	// new menu layout .. experimental & unfinished

		const int menu_index = gMenuCursor;  // current selected menu item
		i = 1;

		if (!gIsInSubMenu)
		{
			while (i < 2)
			{	// leading menu items - small text
				const int k = menu_index + i - 2;
				if (k < 0)
					UI_PrintStringSmall(MenuList[gMenuListCount + k].name, 0, 0, i,0);  // wrap-a-round
				else
				if (k >= 0 && k < (int)gMenuListCount)
					UI_PrintStringSmall(MenuList[k].name, 0, 0, i,0);
				i++;
			}

			// current menu item - keep big n fat
			if (menu_index >= 0 && menu_index < (int)gMenuListCount)
				UI_PrintString(MenuList[menu_index].name, 0, 0, 2, 8);
			i++;

			while (i < 4)
			{	// trailing menu item - small text
				const int k = menu_index + i - 2;
				if (k >= 0 && k < (int)gMenuListCount)
					UI_PrintStringSmall(MenuList[k].name, 0, 0, 1 + i,0);
				else
				if (k >= (int)gMenuListCount)
					UI_PrintStringSmall(MenuList[gMenuListCount - k].name, 0, 0, 1 + i,0);  // wrap-a-round
				i++;
			}

			// draw the menu index number/count НОМЕРА ПУНКТОВ
			sprintf(String, "%2u.%u", 1 + gMenuCursor, gMenuListCount);
			UI_PrintStringSmall(String, 2, 0, 6,0);
		}



		
		else
		if (menu_index >= 0 && menu_index < (int)gMenuListCount)
		{	// current menu item
			strcpy(String, MenuList[menu_index].name);
//			strcat(String, ":");
			UI_PrintString(String, 0, 0, 0, 8);
//			UI_PrintStringSmall(String, 0, 0, 0,0);
		}
	}


	// **************

	memset(String, 0, sizeof(String));

	bool already_printed = false;

	// Return if we are not allowed to edit
	if(UI_MENU_IsAllowedToEdit(UI_MENU_GetCurrentMenuId())==false)
	{
		MENU_PrintNotAllowed();
		already_printed = true;
	}
	else
	{
		switch (UI_MENU_GetCurrentMenuId())
		{
			case MENU_SQL:
				sprintf(String, "%d", gSubMenuSelection);
				break;

			case MENU_MIC:
				{	// display the mic gain in actual dB rather than just an index number
					const uint8_t mic = gMicGain_dB2[gSubMenuSelection];
					sprintf(String, "+%u.%01udB", mic / 2, mic % 2);
				}
				break;

			case MENU_STEP: {
				uint16_t step = gStepFrequencyTable[FREQUENCY_GetStepIdxFromSortedIdx(gSubMenuSelection)];
				sprintf(String, "%d.%02ukHz", step / 100, step % 100);
				break;
			}

			case MENU_TXP:
				strcpy(String, gSubMenu_TXP[gSubMenuSelection]);
				break;

			case MENU_R_DCS:
			case MENU_T_DCS:
				if (gSubMenuSelection == 0)
					strcpy(String, "OFF");
				else if (gSubMenuSelection < 105)
					sprintf(String, "D%03oN", DCS_Options[gSubMenuSelection -   1]);
				else
					sprintf(String, "D%03oI", DCS_Options[gSubMenuSelection - 105]);
				break;

			case MENU_R_CTCS:
			case MENU_T_CTCS:
			{
				if (gSubMenuSelection == 0)
					strcpy(String, "OFF");
				else
					sprintf(String, "%u.%uHz", CTCSS_Options[gSubMenuSelection - 1] / 10, CTCSS_Options[gSubMenuSelection - 1] % 10);
				break;
			}

			case MENU_SQL_TONE:
				sprintf(String, "%u.%uHz", CTCSS_Options[gSubMenuSelection] / 10, CTCSS_Options[gSubMenuSelection] % 10);
				break;

			case MENU_SFT_D:
				strcpy(String, gSubMenu_SFT_D[gSubMenuSelection]);
				break;

			case MENU_OFFSET:
			case MENU_RX_OFFSET:
				if (!gIsInSubMenu || gInputBoxIndex == 0)
				{
					sprintf(String, "%3d.%05u", gSubMenuSelection / 100000, abs(gSubMenuSelection) % 100000);
					UI_PrintString(String, menu_item_x1, menu_item_x2, 1, 8);
				}
				else
				{
					const char * ascii = INPUTBOX_GetAscii();
					sprintf(String, "%.3s.%.3s  ",ascii, ascii + 3);
					UI_PrintString(String, menu_item_x1, menu_item_x2, 1, 8);
				}

				UI_PrintString("MHz",  menu_item_x1, menu_item_x2, 3, 8);

				already_printed = true;
				break;

			case MENU_W_N:
				strcpy(String, bwNames[gSubMenuSelection]);
				break;

			case MENU_SCR:
				strcpy(String, gSubMenu_SCRAMBLER[gSubMenuSelection]);
				#if 1
					if (gSubMenuSelection > 0 && gSetting_ScrambleEnable)
						BK4819_EnableScramble(gSubMenuSelection - 1);
					else
						BK4819_DisableScramble();
				#endif
				break;

			case MENU_ABR:
				strcpy(String, gSubMenu_BACKLIGHT[gSubMenuSelection]);
				break;

			case MENU_ABR_MIN:
			case MENU_ABR_MAX:
				sprintf(String, "%d", gSubMenuSelection);
				if(gIsInSubMenu)
					BACKLIGHT_SetBrightness(gSubMenuSelection);
				break;	

			case MENU_AM:
				strcpy(String, gModulationStr[gSubMenuSelection]);
				break;
			

			case MENU_AUTOLK:
				strcpy(String, (gSubMenuSelection == 0) ? "OFF" : "AUTO");
				break;

			case MENU_ABR_ON_TX_RX:
				strcpy(String, gSubMenu_RX_TX[gSubMenuSelection]);
				break;

			case MENU_S_LIST:
				sprintf(String, "%d", gSubMenuSelection);
				break;

			case MENU_SCREN:
				strcpy(String, gSubMenu_OFF_ON[gSubMenuSelection]);
				break;
			case MENU_MEM_CH:
			case MENU_DEL_CH:
				UI_GenerateChannelStringEx(String, 1, gSubMenuSelection);
				UI_PrintString(String, menu_item_x1, menu_item_x2, 0, 8);
				SETTINGS_FetchChannelName(String, gSubMenuSelection);
				if (String[0] == 0)	strcpy(String, "----");
				UI_PrintString(String, menu_item_x1, menu_item_x2, 2, 8);
				if (!gAskForConfirmation)
				{	// show the frequency so that the user knows the Channels frequency
					const uint32_t frequency = BOARD_fetchChannelFrequency(gSubMenuSelection);
					if (frequency == 0) {strcpy(String, "----");}
                    else sprintf(String, "%u.%05u", frequency / 100000, frequency % 100000);
					UI_PrintString(String, menu_item_x1, menu_item_x2, 4, 8);
				}
				already_printed = true;
				break;

			case MENU_MEM_NAME:
				UI_GenerateChannelStringEx(String, 1, gSubMenuSelection);
				UI_PrintString(String, menu_item_x1, menu_item_x2, 0, 8);


				const uint32_t frequency = BOARD_fetchChannelFrequency(gSubMenuSelection);

				if (!gIsInSubMenu || edit_index < 0)
				{	// show the Channel name
					SETTINGS_FetchChannelName(String, gSubMenuSelection);
					if (String[0] == 0)
						strcpy(String, "--");
					UI_PrintString(String, menu_item_x1, menu_item_x2, 2, 8);
				}
				else
				{	// show the Channel name being edited
					UI_PrintString(edit, menu_item_x1, 0, 2, 8);
					if (edit_index < 10)
						UI_PrintString(     "^", menu_item_x1 + (8 * edit_index), 0, 4, 8);  // show the cursor
				}

				if (!gAskForConfirmation)
				{	// show the frequency so that the user knows the Channels frequency
					if (frequency == 0xFFFFFFFF) {strcpy(String, "Empty");}
					else sprintf(String, "%u.%05u", frequency / 100000, frequency % 100000);
					if (!gIsInSubMenu || edit_index < 0)
						UI_PrintString(String, menu_item_x1, menu_item_x2, 4, 8);
					else
						UI_PrintString(String, menu_item_x1, menu_item_x2, 5, 8);
				}

				already_printed = true;
				break;

			case MENU_SAVE:
				strcpy(String, gSubMenu_SAVE[gSubMenuSelection]);
				break;

			case MENU_TOT:
				strcpy(String, gSubMenu_TOT[gSubMenuSelection]);
				break;
				
			case MENU_BAT_TXT:
				strcpy(String, gSubMenu_BAT_TXT[gSubMenuSelection]);
				break;

			case MENU_PONMSG:
				strcpy(String, gSubMenu_PONMSG[gSubMenuSelection]);
				break;

			case MENU_ROGER:
				strcpy(String, gSubMenu_ROGER[gSubMenuSelection]);
				break;
			
			case MENU_VOL:
				sprintf(String, "%u.%02uV\n%u%%",
					gBatteryVoltageAverage / 100, gBatteryVoltageAverage % 100,
					BATTERY_VoltsToPercent(gBatteryVoltageAverage));
				break;

			case MENU_RESET:
				strcpy(String, gSubMenu_RESET[gSubMenuSelection]);
				break;

			case MENU_F_LOCK:
				strcpy(String, gSubMenu_F_LOCK[gSubMenuSelection]);
				break;

			case MENU_BATCAL:
			{
				const uint16_t vol = (uint32_t)gBatteryVoltageAverage * gBatteryCalibration[3] / gSubMenuSelection;
				sprintf(String, "%u.%02uV\n%u", vol / 100, vol % 100, gSubMenuSelection);
				break;
			}

			case MENU_BATTYP:
				strcpy(String, gSubMenu_BATTYP[gSubMenuSelection]);
				break;	

			case MENU_F1SHRT:
			case MENU_F1LONG:
			case MENU_F2SHRT:
			case MENU_F2LONG:
				strcpy(String, gSubMenu_SIDEFUNCTIONS[gSubMenuSelection].name);
				break;

		}
	}

	if (!already_printed)
	{	// we now do multi-line text in a single string

		unsigned int y;
		unsigned int lines = 1;
		unsigned int len   = strlen(String);
		bool         small = false;

		if (len > 0)
		{
			// count number of lines
			for (i = 0; i < len; i++)
			{
				if (String[i] == '\n' && i < (len - 1))
				{	// found new line char
					lines++;
					String[i] = 0;  // null terminate the line
				}
			}

			if (lines > 3)
			{	// use small text
				small = true;
				if (lines > 7)
					lines = 7;
			}

			// center vertically'ish
			if (small)
				y = 3 - ((lines + 0) / 2);  // untested
			else
				y = 2 - ((lines + 0) / 2);

			// draw the text lines
			for (i = 0; i < len && lines > 0; lines--)
			{
				if (small)
					UI_PrintStringSmall(String + i, menu_item_x1, menu_item_x2, y,0);
				else
					UI_PrintString(String + i, menu_item_x1, menu_item_x2, y, 8);

				// look for start of next line
				while (i < len && String[i] >= 32)
					i++;

				// hop over the null term char(s)
				while (i < len && String[i] < 32)
					i++;

				y += small ? 1 : 2;
			}
		}
	}

	if ((UI_MENU_GetCurrentMenuId() == MENU_R_CTCS || UI_MENU_GetCurrentMenuId() == MENU_R_DCS) && gCssBackgroundScan)
		UI_PrintString("SCAN", menu_item_x1, menu_item_x2, 4, 8);

//****************************МЕЛКИЕ ПОКАЗАТЕЛИ ВЕРХ***************** */
	if (UI_MENU_GetCurrentMenuId() == MENU_R_CTCS ||
    UI_MENU_GetCurrentMenuId() == MENU_T_CTCS ||
    UI_MENU_GetCurrentMenuId() == MENU_R_DCS  ||
    UI_MENU_GetCurrentMenuId() == MENU_T_DCS  ||
	UI_MENU_GetCurrentMenuId() == MENU_SQL_TONE	
	)
{
    // ОТДЕЛЬНЫЕ НАСТРОЙКИ ДЛЯ X/Y — МЕНЯЙ ЗДЕСЬ
    uint8_t base_x = 90;  // Базовый X (центр экрана, можно менять)
    uint8_t y_pos  = 1;   // Y=0 (верхняя строка, меняй на 1, 2 и т.д.)

    sprintf(String, "%2d", gSubMenuSelection); // Номер значения (01, 02 и т.д.)

    // Центрирование по X (автоматическое)
    uint8_t text_len = strlen(String);
    uint8_t text_width = text_len * 4;  // ширина символа в GUI_DisplaySmallestDark — 4 пикселя
    uint8_t x_pos = base_x - (text_width / 2);  // центр относительно base_x

    // Выводим dark-шрифтом (самый маленький)
    GUI_DisplaySmallestDark(String, x_pos, y_pos, false, true);
}

	if ((UI_MENU_GetCurrentMenuId() == MENU_RESET    ||
	     UI_MENU_GetCurrentMenuId() == MENU_MEM_CH   ||
	     UI_MENU_GetCurrentMenuId() == MENU_MEM_NAME ||
	     UI_MENU_GetCurrentMenuId() == MENU_DEL_CH) && gAskForConfirmation)
	{	// display confirmation
		strcpy(String, (gAskForConfirmation == 1) ? "SURE?" : "WAIT!");
		UI_PrintString(String, menu_item_x1, menu_item_x2, 5, 8);
	}




//   НЕСКОЛЬКО КОРОТКИХ ПУНКТИРНЫХ ЛИНИЙ ГОРИЗОНТ — ПОЛНАЯ НАСТРОЙКА КАЖДОЙ!
// ────────────────────────────────────────────────────────────────
typedef struct {
	uint8_t y;        // высота (0..63)
	uint8_t x_start;  // отступ слева
	uint8_t x_end;    // отступ справа
	uint8_t step;     // шаг пунктира: 1 = сплошная, 2 = •◦, 3 = •◦◦, 4 = •◦◦◦ и т.д.
} dashed_line_t;

// ─────── ТУТ НАСТРАИВАЙ СКОЛЬКО УГОДНО ЛИНИЙ ───────
static const dashed_line_t dashed_lines[] = {
	{ 8,  0, 127, 2 },   // линия 1: частый пунктир
	{ 52,  0, 47, 2 },   // линия 2: сплошная (step = 1)
	// хочешь ещё? — добавляй:
	// { 24,  15, 113, 3 },
	// { 40,   8, 120, 5 },
};

const uint8_t num_lines = ARRAY_SIZE(dashed_lines);

// Рисуем все линии
for (uint8_t i = 0; i < num_lines; i++)
{
	const dashed_line_t *l = &dashed_lines[i];
	const uint8_t y = l->y;

	for (uint8_t x = l->x_start; x <= l->x_end; x += l->step)
	{
		if (y < 8)
			gStatusLine[x] |= (1u << y);                                         // статусная строка
		else
			gFrameBuffer[(y - 8) >> 3][x] |= (1u << ((y - 8) & 7));              // основной экран
	}
}
// ────────────────────────────────────────────────────────────────
//   НЕСКОЛЬКО ВЕРТИКАЛЬНЫХ ПУНКТИРНЫХ ЛИНИЙ — ПОЛНАЯ НАСТРОЙКА КАЖДОЙ!
// ────────────────────────────────────────────────────────────────
typedef struct {
	uint8_t x;        // позиция по горизонтали (0..127)
	uint8_t y_start;  // отступ сверху
	uint8_t y_end;    // отступ снизу
	uint8_t step;     // шаг пунктира: 1 = сплошная, 2 = •◦, 3 = •◦◦ и т.д.
} vertical_line_t;

// ─────── НАСТРАИВАЙ СКОЛЬКО УГОДНО ВЕРТИКАЛЬНЫХ ЛИНИЙ ───────
static const vertical_line_t vertical_lines[] = {
	{  49,  10, 64, 2 },   // левая линия — частый пунктир
	// добавляй свои:
	// { 30, 15, 50, 4 },
	// { 90, 12, 52, 1 },
};

const uint8_t num_vlines = ARRAY_SIZE(vertical_lines);

// Рисуем все вертикальные линии
for (uint8_t i = 0; i < num_vlines; i++)
{
	const vertical_line_t *l = &vertical_lines[i];
	const uint8_t x = l->x;

	for (uint8_t y = l->y_start; y <= l->y_end; y += l->step)
	{
		if (y < 8)
			gStatusLine[x] |= (1u << y);
		else
			gFrameBuffer[(y - 8) >> 3][x] |= (1u << ((y - 8) & 7));
	}
}
// ────────────────────────────────────────────────────────────────
	ST7565_BlitFullScreen();
}

void MENU_PrintNotAllowed()
{
	char String[8];
	strcpy(String, "NOT");
	UI_PrintString(String, menu_item_x1, menu_item_x2, 0, 8);
	strcpy(String, "ALLOWED");
	UI_PrintString(String, menu_item_x1, menu_item_x2, 2, 8);
	strcpy(String, "IN VFO");
	UI_PrintString(String, menu_item_x1, menu_item_x2, 4, 8);
}

bool UI_MENU_IsAllowedToEdit(int menu_id)
{
	bool isChannelOnlySetting;
	isChannelOnlySetting =
		IsValueInArray(menu_id, CHANNEL_ONLY_SETTINGS, sizeof(CHANNEL_ONLY_SETTINGS));
	
	if (isChannelOnlySetting && !IS_MR_CHANNEL(gTxVfo->CHANNEL_SAVE)) return false;
	return true;
	
}