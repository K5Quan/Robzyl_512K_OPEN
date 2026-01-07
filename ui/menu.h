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

#ifndef UI_MENU_H
#define UI_MENU_H

#include <stdbool.h>
#include <stdint.h>
#include "settings.h"

typedef struct {
	const char  name[7];    // menu display area only has room for 6 characters
	uint8_t     menu_id;
} t_menu_item;

enum
{  
	MENU_STEP = 0      ,
	MENU_SQL           ,
	MENU_W_N           ,
	MENU_AM            ,
	MENU_ROGER         ,
	MENU_SCREN         , 
	MENU_SCR           ,
	MENU_S_LIST        , 
	MENU_TOT           ,
	MENU_MIC           ,	
	MENU_ABR           ,
	MENU_ABR_MIN       ,
	MENU_ABR_MAX       ,
	MENU_ABR_ON_TX_RX  ,
	MENU_SQL_TONE      ,
	MENU_R_DCS         ,
	MENU_R_CTCS        ,
	MENU_T_DCS         ,
	MENU_T_CTCS        ,
	MENU_SFT_D         ,
	MENU_OFFSET        ,
	MENU_RX_OFFSET     ,
	MENU_SAVE          ,
	MENU_MEM_CH        ,
	MENU_F1SHRT        ,
	MENU_F1LONG        ,
	MENU_F2SHRT        ,
	MENU_F2LONG        ,
	MENU_BATCAL        ,
	MENU_BATTYP        ,
	MENU_TXP           ,
	MENU_VOL           ,
	MENU_PONMSG        ,
	MENU_BAT_TXT       ,	
	MENU_DEL_CH        ,
	MENU_MEM_NAME      ,	
	MENU_RESET         ,
	MENU_F_LOCK        
};

extern const uint8_t FIRST_HIDDEN_MENU_ITEM;
extern const t_menu_item MenuList[];

extern const char        gSubMenu_TXP[3][5];
extern const char        gSubMenu_SFT_D[3][4];
extern const char        gSubMenu_W_N[2][7];
extern const char        gSubMenu_OFF_ON[2][4];
extern const char        gSubMenu_SAVE[5][4];
extern const char        gSubMenu_TOT[11][7];
extern const char        gSubMenu_PONMSG[4][8];
extern const char        gSubMenu_ROGER[4][7];
extern const char        gSubMenu_RESET[2][9];
extern const char* const gSubMenu_F_LOCK[F_LOCK_LEN];
extern const char        gSubMenu_BACKLIGHT[8][7];
extern const char        gSubMenu_RX_TX[4][6];
extern const char        gSubMenu_BAT_TXT[3][8];
extern const char 		 gSubMenu_BATTYP[2][9];
extern const char        gSubMenu_SCRAMBLER[11][7];
typedef struct {char* name; uint8_t id;} t_sidefunction;
extern const uint8_t 		 gSubMenu_SIDEFUNCTIONS_size;
extern const t_sidefunction* gSubMenu_SIDEFUNCTIONS;
				         
extern bool              gIsInSubMenu;
				         
extern uint8_t           gMenuCursor;

extern int32_t           gSubMenuSelection;
				         
extern char              edit_original[17];
extern char              edit[17];
extern int               edit_index;

void UI_DisplayMenu(void);
int UI_MENU_GetCurrentMenuId();
uint8_t UI_MENU_GetMenuIdx(uint8_t id);
bool UI_MENU_IsAllowedToEdit(int menu_id);

#endif
