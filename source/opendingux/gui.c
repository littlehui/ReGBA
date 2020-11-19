/* ReGBA - In-application menu
 *
 * Copyright (C) 2013 Dingoonity user Nebuleon
 * 
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public Licens e as
 * published by the Free Software Foundation; either version 2 of
 * the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA
 */

#include "common.h"

#define COLOR_BACKGROUND            RGB888_TO_NATIVE(  0,  0,   72)
#define COLOR_ERROR_BACKGROUND      RGB888_TO_NATIVE(150,   0,   0)
#define COLOR_WARNING_BACKGROUND    RGB888_TO_NATIVE(180, 180,   0)
#define COLOR_INACTIVE_TEXT         RGB888_TO_NATIVE( 72,  72, 160)
#define COLOR_INACTIVE_OUTLINE      RGB888_TO_NATIVE(  0,   0,   0)
#define COLOR_ACTIVE_TEXT           RGB888_TO_NATIVE(255, 255, 255)
#define COLOR_ACTIVE_OUTLINE        RGB888_TO_NATIVE(  0,   0,   0)
#define COLOR_TITLE_TEXT            RGB888_TO_NATIVE(150, 150, 255)
#define COLOR_TITLE_OUTLINE         RGB888_TO_NATIVE(  0,  0,   96)
#define COLOR_ERROR_TEXT            RGB888_TO_NATIVE(255,  64,  64)
#define COLOR_ERROR_OUTLINE         RGB888_TO_NATIVE( 80,   0,   0)
#define COLOR_OFF_ACTICE_TEXT       RGB888_TO_NATIVE(255,  64,  64)
#define COLOR_OFF_INACTIVE_TEXT     RGB888_TO_NATIVE(127,  64,  64)
#define COLOR_ON_ACTICE_TEXT        RGB888_TO_NATIVE(64,  255,  64)
#define COLOR_ON_INACTIVE_TEXT      RGB888_TO_NATIVE(64,  127,  64)

#if (SCREEN_WIDTH == GBA_SCREEN_WIDTH) && (SCREEN_HEIGHT == GBA_SCREEN_HEIGHT)
#define NO_SCALING
#endif

// -- Shorthand for creating menu entries --

#define MENU_PER_GAME \
	.DisplayTitleFunction = DisplayPerGameTitleFunction

#define ENTRY_OPTION(_PersistentName, _Name, _Target) \
	.Kind = KIND_OPTION, .PersistentName = _PersistentName, .Name = _Name, .Target = _Target

#define ENTRY_DISPLAY(_Name, _Target, _DisplayType) \
	.Kind = KIND_DISPLAY, .Name = _Name, .Target = _Target, .DisplayType = _DisplayType

#define ENTRY_SUBMENU(_Name, _Target) \
	.Kind = KIND_SUBMENU, .Name = _Name, .Target = _Target

#define ENTRY_MANDATORY_MAPPING \
	.ChoiceCount = 0, \
	.ButtonLeftFunction = NullLeftFunction, .ButtonRightFunction = NullRightFunction, \
	.ButtonEnterFunction = ActionSetMapping, .DisplayValueFunction = DisplayButtonMappingValue, \
	.LoadFunction = LoadMappingFunction, .SaveFunction = SaveMappingFunction

#define ENTRY_OPTIONAL_MAPPING \
	.ChoiceCount = 0, \
	.ButtonLeftFunction = NullLeftFunction, .ButtonRightFunction = NullRightFunction, \
	.ButtonEnterFunction = ActionSetOrClearMapping, .DisplayValueFunction = DisplayButtonMappingValue, \
	.LoadFunction = LoadMappingFunction, .SaveFunction = SaveMappingFunction

#define ENTRY_MANDATORY_HOTKEY \
	.ChoiceCount = 0, \
	.ButtonLeftFunction = NullLeftFunction, .ButtonRightFunction = NullRightFunction, \
	.ButtonEnterFunction = ActionSetHotkey, .DisplayValueFunction = DisplayHotkeyValue, \
	.LoadFunction = LoadHotkeyFunction, .SaveFunction = SaveHotkeyFunction

#define ENTRY_OPTIONAL_HOTKEY \
	.ChoiceCount = 0, \
	.ButtonLeftFunction = NullLeftFunction, .ButtonRightFunction = NullRightFunction, \
	.ButtonEnterFunction = ActionSetOrClearHotkey, .DisplayValueFunction = DisplayHotkeyValue, \
	.LoadFunction = LoadHotkeyFunction, .SaveFunction = SaveHotkeyFunction

// -- Data --

static uint32_t SelectedState = 0;

// -- Forward declarations --

static struct Menu PerGameMainMenu;
static struct Menu DisplayMenu;
static struct Menu AudioMenu;
static struct Menu InputMenu;
static struct Menu HotkeyMenu;
static struct Menu ErrorScreen;
static struct Menu WarningScreen;
static struct Menu DebugMenu;

/*
 * A strut is an invisible line that cannot receive the focus, does not
 * display anything and does not act in any way.
 */
static struct MenuEntry Strut;

static void SavedStateUpdatePreview(struct Menu* ActiveMenu);

GAME_CONFIG game_config __attribute__((section(".noinit")));
char DEFAULT_CHEAT_DIR[PATH_MAX] __attribute__((section(".noinit")));

/*--------------------------------------------------------
  game cfg的初始化
--------------------------------------------------------*/
void init_game_config()
{
	memset(&game_config, 0, sizeof(game_config));
	uint32_t i;
	for (i = 0; i < MAX_CHEATS; i++) {
		game_config.cheats_flag[i].cheat_active = NO;
		game_config.cheats_flag[i].cheat_name[0] = '\0';
	}
	sprintf(DEFAULT_CHEAT_DIR, "%s/cheats", main_path);
}



static bool DefaultCanFocusFunction(struct Menu* ActiveMenu, struct MenuEntry* ActiveMenuEntry)
{
	if (ActiveMenuEntry->Kind == KIND_DISPLAY)
		return false;
	return true;
}

static uint32_t FindNullEntry(struct Menu* ActiveMenu)
{
	uint32_t Result = 0;
	while (ActiveMenu->Entries[Result] != NULL)
		Result++;
	return Result;
}

static bool MoveUp(struct Menu* ActiveMenu, uint32_t* ActiveMenuEntryIndex)
{
	if (*ActiveMenuEntryIndex == 0)  // Going over the top?
	{  // Go back to the bottom.
		uint32_t NullEntry = FindNullEntry(ActiveMenu);
		if (NullEntry == 0)
			return false;
		*ActiveMenuEntryIndex = NullEntry - 1;
		return true;
	}
	(*ActiveMenuEntryIndex)--;
	return true;
}

static bool MoveDown(struct Menu* ActiveMenu, uint32_t* ActiveMenuEntryIndex)
{
	if (*ActiveMenuEntryIndex == 0 && ActiveMenu->Entries[*ActiveMenuEntryIndex] == NULL)
		return false;
	if (ActiveMenu->Entries[*ActiveMenuEntryIndex] == NULL)  // Is the sentinel "active"?
		*ActiveMenuEntryIndex = 0;  // Go back to the top.
	(*ActiveMenuEntryIndex)++;
	if (ActiveMenu->Entries[*ActiveMenuEntryIndex] == NULL)  // Going below the bottom?
		*ActiveMenuEntryIndex = 0;  // Go back to the top.
	return true;
}


static void DefaultPageUpFunction(struct Menu** ActiveMenu, uint32_t* ActiveMenuEntryIndex)
{
	//nothing
}

static void DefaultPageDownFunction(struct Menu** ActiveMenu, uint32_t* ActiveMenuEntryIndex)
{
	//nothing
}



static void DefaultUpFunction(struct Menu** ActiveMenu, uint32_t* ActiveMenuEntryIndex)
{
	if (MoveUp(*ActiveMenu, ActiveMenuEntryIndex))
	{
		// Keep moving up until a menu entry can be focused.
		uint32_t Sentinel = *ActiveMenuEntryIndex;
		MenuEntryCanFocusFunction CanFocusFunction = (*ActiveMenu)->Entries[*ActiveMenuEntryIndex]->CanFocusFunction;
		if (CanFocusFunction == NULL) CanFocusFunction = DefaultCanFocusFunction;
		while (!(*CanFocusFunction)(*ActiveMenu, (*ActiveMenu)->Entries[*ActiveMenuEntryIndex]))
		{
			MoveUp(*ActiveMenu, ActiveMenuEntryIndex);
			if (*ActiveMenuEntryIndex == Sentinel)
			{
				// If we went through all of them, we cannot focus anything.
				// Place the focus on the NULL entry.
				*ActiveMenuEntryIndex = FindNullEntry(*ActiveMenu);
				return;
			}
			CanFocusFunction = (*ActiveMenu)->Entries[*ActiveMenuEntryIndex]->CanFocusFunction;
			if (CanFocusFunction == NULL) CanFocusFunction = DefaultCanFocusFunction;
		}
	}
}

static void DefaultDownFunction(struct Menu** ActiveMenu, uint32_t* ActiveMenuEntryIndex)
{
	if (MoveDown(*ActiveMenu, ActiveMenuEntryIndex))
	{
		// Keep moving down until a menu entry can be focused.
		uint32_t Sentinel = *ActiveMenuEntryIndex;
		MenuEntryCanFocusFunction CanFocusFunction = (*ActiveMenu)->Entries[*ActiveMenuEntryIndex]->CanFocusFunction;
		if (CanFocusFunction == NULL) CanFocusFunction = DefaultCanFocusFunction;
		while (!(*CanFocusFunction)(*ActiveMenu, (*ActiveMenu)->Entries[*ActiveMenuEntryIndex]))
		{
			MoveDown(*ActiveMenu, ActiveMenuEntryIndex);
			if (*ActiveMenuEntryIndex == Sentinel)
			{
				// If we went through all of them, we cannot focus anything.
				// Place the focus on the NULL entry.
				*ActiveMenuEntryIndex = FindNullEntry(*ActiveMenu);
				return;
			}
			CanFocusFunction = (*ActiveMenu)->Entries[*ActiveMenuEntryIndex]->CanFocusFunction;
			if (CanFocusFunction == NULL) CanFocusFunction = DefaultCanFocusFunction;
		}
	}
}

static void DefaultRightFunction(struct Menu* ActiveMenu, struct MenuEntry* ActiveMenuEntry)
{
	if (ActiveMenuEntry->Kind == KIND_OPTION
	|| (ActiveMenuEntry->Kind == KIND_CUSTOM /* chose to use this function */
	 && ActiveMenuEntry->Target != NULL)
	)
	{
		uint32_t* Target = (uint32_t*) ActiveMenuEntry->Target;
		(*Target)++;
		if (*Target >= ActiveMenuEntry->ChoiceCount)
			*Target = 0;
	}
}

static void DefaultLeftFunction(struct Menu* ActiveMenu, struct MenuEntry* ActiveMenuEntry)
{
	if (ActiveMenuEntry->Kind == KIND_OPTION
	|| (ActiveMenuEntry->Kind == KIND_CUSTOM /* chose to use this function */
	 && ActiveMenuEntry->Target != NULL)
	)
	{
		uint32_t* Target = (uint32_t*) ActiveMenuEntry->Target;
		if (*Target == 0)
			*Target = ActiveMenuEntry->ChoiceCount;
		(*Target)--;
	}
}

static void DefaultEnterFunction(struct Menu** ActiveMenu, uint32_t* ActiveMenuEntryIndex)
{
	if ((*ActiveMenu)->Entries[*ActiveMenuEntryIndex]->Kind == KIND_SUBMENU)
	{
		*ActiveMenu = (struct Menu*) (*ActiveMenu)->Entries[*ActiveMenuEntryIndex]->Target;
	}
}

static void DefaultLeaveFunction(struct Menu** ActiveMenu, uint32_t* ActiveMenuEntryIndex)
{
	*ActiveMenu = (*ActiveMenu)->Parent;
}

static void DefaultDisplayNameFunction(struct MenuEntry* DrawnMenuEntry, struct MenuEntry* ActiveMenuEntry, uint32_t Position)
{
	bool IsActive = (DrawnMenuEntry == ActiveMenuEntry);
	uint16_t TextColor = IsActive ? COLOR_ACTIVE_TEXT : COLOR_INACTIVE_TEXT;
	uint16_t OutlineColor = IsActive ? COLOR_ACTIVE_OUTLINE : COLOR_INACTIVE_OUTLINE;
	PrintStringOutline(DrawnMenuEntry->Name, TextColor, OutlineColor, OutputSurface->pixels, OutputSurface->pitch, 0, GetRenderedHeight(" ") * (Position + 2), SCREEN_WIDTH, GetRenderedHeight(" ") + 2, LEFT, TOP);
}

static void print_u64(char* Result, uint64_t Value)
{
	if (Value == 0)
		strcpy(Result, "0");
	else
	{
		uint_fast8_t Length = 0;
		uint64_t Temp = Value;
		while (Temp > 0)
		{
			Temp /= 10;
			Length++;
		}
		Result[Length] = '\0';
		while (Value > 0)
		{
			Length--;
			Result[Length] = '0' + (Value % 10);
			Value /= 10;
		}
	}
}

static void print_i64(char* Result, int64_t Value)
{
	if (Value < -9223372036854775807)
		strcpy(Result, "-9223372036854775808");
	else if (Value < 0)
	{
		Result[0] = '-';
		print_u64(Result + 1, (uint64_t) -Value);
	}
	else
		print_u64(Result, (uint64_t) Value);
}

static void DefaultDisplayValueFunction(struct MenuEntry* DrawnMenuEntry, struct MenuEntry* ActiveMenuEntry, uint32_t Position)
{
	if (DrawnMenuEntry->Kind == KIND_OPTION || DrawnMenuEntry->Kind == KIND_DISPLAY)
	{
		char Temp[21];
		Temp[0] = '\0';
		char* Value = Temp;
		bool Error = false;
		if (DrawnMenuEntry->Kind == KIND_OPTION)
		{
			if (*(uint32_t*) DrawnMenuEntry->Target < DrawnMenuEntry->ChoiceCount)
				Value = DrawnMenuEntry->Choices[*(uint32_t*) DrawnMenuEntry->Target].Pretty;
			else
			{
				Value = "Out of bounds";
				Error = true;
			}
		}
		else if (DrawnMenuEntry->Kind == KIND_DISPLAY)
		{
			switch (DrawnMenuEntry->DisplayType)
			{
				case TYPE_STRING:
					Value = (char*) DrawnMenuEntry->Target;
					break;
				case TYPE_INT32:
					sprintf(Temp, "%" PRIi32, *(int32_t*) DrawnMenuEntry->Target);
					break;
				case TYPE_UINT32:
					sprintf(Temp, "%" PRIu32, *(uint32_t*) DrawnMenuEntry->Target);
					break;
				case TYPE_INT64:
					print_i64(Temp, *(int64_t*) DrawnMenuEntry->Target);
					break;
				case TYPE_UINT64:
					print_u64(Temp, *(uint64_t*) DrawnMenuEntry->Target);
					break;
				default:
					Value = "Unknown type";
					Error = true;
					break;
			}
		}
		bool IsActive = (DrawnMenuEntry == ActiveMenuEntry);
		uint16_t TextColor;
		uint16_t OutlineColor;
		if (!strcmp("开",Value)){
			TextColor = Error ? COLOR_ERROR_TEXT : (IsActive ? COLOR_ON_ACTICE_TEXT : COLOR_ON_ACTICE_TEXT);
			OutlineColor = Error ? COLOR_ERROR_OUTLINE : (IsActive ? COLOR_ACTIVE_OUTLINE : COLOR_INACTIVE_OUTLINE);
		}else if(!strcmp("关",Value)){
			TextColor = Error ? COLOR_ERROR_TEXT : (IsActive ? COLOR_OFF_ACTICE_TEXT : COLOR_OFF_ACTICE_TEXT);
			OutlineColor = Error ? COLOR_ERROR_OUTLINE : (IsActive ? COLOR_ACTIVE_OUTLINE : COLOR_INACTIVE_OUTLINE);
		}else{
			TextColor = Error ? COLOR_ERROR_TEXT : (IsActive ? COLOR_ACTIVE_TEXT : COLOR_INACTIVE_TEXT);
			OutlineColor = Error ? COLOR_ERROR_OUTLINE : (IsActive ? COLOR_ACTIVE_OUTLINE : COLOR_INACTIVE_OUTLINE);
		}
		PrintStringOutline(Value, TextColor, OutlineColor, OutputSurface->pixels, OutputSurface->pitch, 0, GetRenderedHeight(" ") * (Position + 2), SCREEN_WIDTH, GetRenderedHeight(" ") + 2, RIGHT, TOP);
	}
}

static void DefaultDisplayBackgroundFunction(struct Menu* ActiveMenu)
{
	if (SDL_MUSTLOCK(OutputSurface))
		SDL_UnlockSurface(OutputSurface);
	//SDL_FillRect(OutputSurface, NULL, COLOR_BACKGROUND);
	
	char bgpicpath[MAX_PATH + 1];
	sprintf(bgpicpath, "%s/bg.png", executable_path);
	SDL_Surface* JustLoaded = loadPNG(bgpicpath, SCREEN_WIDTH, SCREEN_HEIGHT);
	if (JustLoaded != NULL)
	{
		if (JustLoaded->w == SCREEN_WIDTH && JustLoaded->h == SCREEN_HEIGHT)
		{
			SDL_BlitSurface(JustLoaded, NULL, OutputSurface, NULL);
		}
		SDL_FreeSurface(JustLoaded);
		JustLoaded = NULL;
	}
	
	if (SDL_MUSTLOCK(OutputSurface))
		SDL_LockSurface(OutputSurface);
}

static void DefaultDisplayDataFunction(struct Menu* ActiveMenu, struct MenuEntry* ActiveMenuEntry)
{
	uint32_t DrawnMenuEntryIndex = 0;
	struct MenuEntry* DrawnMenuEntry = ActiveMenu->Entries[0];
	for (; DrawnMenuEntry != NULL; DrawnMenuEntryIndex++, DrawnMenuEntry = ActiveMenu->Entries[DrawnMenuEntryIndex])
	{
		MenuEntryDisplayFunction Function = DrawnMenuEntry->DisplayNameFunction;
		if (Function == NULL) Function = &DefaultDisplayNameFunction;
		(*Function)(DrawnMenuEntry, ActiveMenuEntry, DrawnMenuEntryIndex);

		Function = DrawnMenuEntry->DisplayValueFunction;
		if (Function == NULL) Function = &DefaultDisplayValueFunction;
		(*Function)(DrawnMenuEntry, ActiveMenuEntry, DrawnMenuEntryIndex);

		DrawnMenuEntry++;
	}
}

static void DefaultDisplayTitleFunction(struct Menu* ActiveMenu)
{
	PrintStringOutline(ActiveMenu->Title, COLOR_TITLE_TEXT, COLOR_TITLE_OUTLINE, OutputSurface->pixels, OutputSurface->pitch, 0, 0, SCREEN_WIDTH, SCREEN_HEIGHT, CENTER, TOP);
}

static void DisplayPerGameTitleFunction(struct Menu* ActiveMenu)
{
	PrintStringOutline(ActiveMenu->Title, COLOR_TITLE_TEXT, COLOR_TITLE_OUTLINE, OutputSurface->pixels, OutputSurface->pitch, 0, 0, SCREEN_WIDTH, SCREEN_HEIGHT, CENTER, TOP);
	char ForGame[MAX_PATH * 2];
	char FileNameNoExt[MAX_PATH + 1];
	GetFileNameNoExtension(FileNameNoExt, CurrentGamePath);
	sprintf(ForGame, "%s 独立设置", FileNameNoExt);
	PrintStringOutline(ForGame, COLOR_TITLE_TEXT, COLOR_TITLE_OUTLINE, OutputSurface->pixels, OutputSurface->pitch, 0, GetRenderedHeight(" "), SCREEN_WIDTH, GetRenderedHeight(" ") + 2, CENTER, TOP);
}

void DefaultLoadFunction(struct MenuEntry* ActiveMenuEntry, char* Value)
{
	uint32_t i;
	for (i = 0; i < ActiveMenuEntry->ChoiceCount; i++)
	{
		if (strcasecmp(ActiveMenuEntry->Choices[i].Persistent, Value) == 0)
		{
			*(uint32_t*) ActiveMenuEntry->Target = i;
			return;
		}
	}
	ReGBA_Trace("W: Value '%s' for option '%s' not valid; ignored", Value, ActiveMenuEntry->PersistentName);
}

void DefaultSaveFunction(struct MenuEntry* ActiveMenuEntry, char* Value)
{
	snprintf(Value, 256, "%s = %s #%s\n", ActiveMenuEntry->PersistentName,
		ActiveMenuEntry->Choices[*((uint32_t*) ActiveMenuEntry->Target)].Persistent,
		ActiveMenuEntry->Choices[*((uint32_t*) ActiveMenuEntry->Target)].Pretty);
}



// -- Custom initialisation/finalisation --

static struct MenuEntry CheatsMenu_SelectedState = {
			ENTRY_OPTION("cheat status", &game_config.cheats_flag[0].cheat_name, &game_config.cheats_flag[0].cheat_active),
			.ChoiceCount = 2,
			.Choices = {{ "关", "off" }, { "开", "on" }},
}; 

static struct MenuEntry CheatsMenu_SelectedState1 = {
			ENTRY_OPTION("cheat status", &game_config.cheats_flag[1].cheat_name, &game_config.cheats_flag[1].cheat_active),
			.ChoiceCount = 2,
			.Choices = {{ "关", "off" }, { "开", "on" }},
}; 

static struct MenuEntry CheatsMenu_SelectedState2 = {
			ENTRY_OPTION("cheat status", &game_config.cheats_flag[2].cheat_name, &game_config.cheats_flag[2].cheat_active),
			.ChoiceCount = 2,
			.Choices = {{ "关", "off" }, { "开", "on" }},
}; 

static struct MenuEntry CheatsMenu_SelectedState3 = {
			ENTRY_OPTION("cheat status", &game_config.cheats_flag[3].cheat_name, &game_config.cheats_flag[3].cheat_active),
			.ChoiceCount = 2,
			.Choices = {{ "关", "off" }, { "开", "on" }},
}; 

static struct MenuEntry CheatsMenu_SelectedState4 = {
			ENTRY_OPTION("cheat status", &game_config.cheats_flag[4].cheat_name, &game_config.cheats_flag[4].cheat_active),
			.ChoiceCount = 2,
			.Choices = {{ "关", "off" }, { "开", "on" }},
}; 

static struct MenuEntry CheatsMenu_SelectedState5 = {
			ENTRY_OPTION("cheat status", &game_config.cheats_flag[5].cheat_name, &game_config.cheats_flag[5].cheat_active),
			.ChoiceCount = 2,
			.Choices = {{ "关", "off" }, { "开", "on" }},
}; 

static struct MenuEntry CheatsMenu_SelectedState6 = {
			ENTRY_OPTION("cheat status", &game_config.cheats_flag[6].cheat_name, &game_config.cheats_flag[6].cheat_active),
			.ChoiceCount = 2,
			.Choices = {{ "关", "off" }, { "开", "on" }},
}; 

static struct MenuEntry CheatsMenu_SelectedState7 = {
			ENTRY_OPTION("cheat status", &game_config.cheats_flag[7].cheat_name, &game_config.cheats_flag[7].cheat_active),
			.ChoiceCount = 2,
			.Choices = {{ "关", "off" }, { "开", "on" }},
}; 

static struct MenuEntry CheatsMenu_SelectedState8 = {
			ENTRY_OPTION("cheat status", &game_config.cheats_flag[8].cheat_name, &game_config.cheats_flag[8].cheat_active),
			.ChoiceCount = 2,
			.Choices = {{ "关", "off" }, { "开", "on" }},
}; 

static struct MenuEntry CheatsMenu_SelectedState9 = {
			ENTRY_OPTION("cheat status", &game_config.cheats_flag[9].cheat_name, &game_config.cheats_flag[9].cheat_active),
			.ChoiceCount = 2,
			.Choices = {{ "关", "off" }, { "开", "on" }},
}; 

static struct MenuEntry CheatsMenu_SelectedState10 = {
			ENTRY_OPTION("cheat status", &game_config.cheats_flag[10].cheat_name, &game_config.cheats_flag[10].cheat_active),
			.ChoiceCount = 2,
			.Choices = {{ "关", "off" }, { "开", "on" }},
}; 

static struct MenuEntry CheatsMenu_SelectedState11 = {
			ENTRY_OPTION("cheat status", &game_config.cheats_flag[11].cheat_name, &game_config.cheats_flag[11].cheat_active),
			.ChoiceCount = 2,
			.Choices = {{ "关", "off" }, { "开", "on" }},
}; 

static struct MenuEntry CheatsMenu_SelectedState12 = {
			ENTRY_OPTION("cheat status", &game_config.cheats_flag[12].cheat_name, &game_config.cheats_flag[12].cheat_active),
			.ChoiceCount = 2,
			.Choices = {{ "关", "off" }, { "开", "on" }},
}; 

static struct MenuEntry CheatsMenu_SelectedState13 = {
			ENTRY_OPTION("cheat status", &game_config.cheats_flag[13].cheat_name, &game_config.cheats_flag[13].cheat_active),
			.ChoiceCount = 2,
			.Choices = {{ "关", "off" }, { "开", "on" }},
}; 

static struct MenuEntry CheatsMenu_SelectedState14 = {
			ENTRY_OPTION("cheat status", &game_config.cheats_flag[14].cheat_name, &game_config.cheats_flag[14].cheat_active),
			.ChoiceCount = 2,
			.Choices = {{ "关", "off" }, { "开", "on" }},
}; 

static struct MenuEntry CheatsMenu_SelectedState15 = {
			ENTRY_OPTION("cheat status", &game_config.cheats_flag[15].cheat_name, &game_config.cheats_flag[15].cheat_active),
			.ChoiceCount = 2,
			.Choices = {{ "关", "off" }, { "开", "on" }},
}; 

static struct MenuEntry CheatsMenu_SelectedState16 = {
			ENTRY_OPTION("cheat status", &game_config.cheats_flag[16].cheat_name, &game_config.cheats_flag[16].cheat_active),
			.ChoiceCount = 2,
			.Choices = {{ "关", "off" }, { "开", "on" }},
}; 

static struct MenuEntry CheatsMenu_SelectedState17 = {
			ENTRY_OPTION("cheat status", &game_config.cheats_flag[17].cheat_name, &game_config.cheats_flag[17].cheat_active),
			.ChoiceCount = 2,
			.Choices = {{ "关", "off" }, { "开", "on" }},
}; 

static struct MenuEntry CheatsMenu_SelectedState18 = {
			ENTRY_OPTION("cheat status", &game_config.cheats_flag[18].cheat_name, &game_config.cheats_flag[18].cheat_active),
			.ChoiceCount = 2,
			.Choices = {{ "关", "off" }, { "开", "on" }},
}; 

static struct MenuEntry CheatsMenu_SelectedState19 = {
			ENTRY_OPTION("cheat status", &game_config.cheats_flag[19].cheat_name, &game_config.cheats_flag[19].cheat_active),
			.ChoiceCount = 2,
			.Choices = {{ "关", "off" }, { "开", "on" }},
}; 

static struct MenuEntry CheatsMenu_SelectedState20 = {
			ENTRY_OPTION("cheat status", &game_config.cheats_flag[20].cheat_name, &game_config.cheats_flag[20].cheat_active),
			.ChoiceCount = 2,
			.Choices = {{ "关", "off" }, { "开", "on" }},
}; 

static struct MenuEntry CheatsMenu_SelectedState21 = {
			ENTRY_OPTION("cheat status", &game_config.cheats_flag[21].cheat_name, &game_config.cheats_flag[21].cheat_active),
			.ChoiceCount = 2,
			.Choices = {{ "关", "off" }, { "开", "on" }},
}; 

static struct MenuEntry CheatsMenu_SelectedState22 = {
			ENTRY_OPTION("cheat status", &game_config.cheats_flag[22].cheat_name, &game_config.cheats_flag[22].cheat_active),
			.ChoiceCount = 2,
			.Choices = {{ "关", "off" }, { "开", "on" }},
}; 

static struct MenuEntry CheatsMenu_SelectedState23 = {
			ENTRY_OPTION("cheat status", &game_config.cheats_flag[23].cheat_name, &game_config.cheats_flag[23].cheat_active),
			.ChoiceCount = 2,
			.Choices = {{ "关", "off" }, { "开", "on" }},
};  

static struct MenuEntry CheatsMenu_SelectedState24 = {
			ENTRY_OPTION("cheat status", &game_config.cheats_flag[24].cheat_name, &game_config.cheats_flag[24].cheat_active),
			.ChoiceCount = 2,
			.Choices = {{ "关", "off" }, { "开", "on" }},
};  

static struct MenuEntry CheatsMenu_SelectedState25 = {
			ENTRY_OPTION("cheat status", &game_config.cheats_flag[25].cheat_name, &game_config.cheats_flag[25].cheat_active),
			.ChoiceCount = 2,
			.Choices = {{ "关", "off" }, { "开", "on" }},
};  

static struct MenuEntry CheatsMenu_SelectedState26 = {
			ENTRY_OPTION("cheat status", &game_config.cheats_flag[26].cheat_name, &game_config.cheats_flag[26].cheat_active),
			.ChoiceCount = 2,
			.Choices = {{ "关", "off" }, { "开", "on" }},
};  

static struct MenuEntry CheatsMenu_SelectedState27 = {
			ENTRY_OPTION("cheat status", &game_config.cheats_flag[27].cheat_name, &game_config.cheats_flag[27].cheat_active),
			.ChoiceCount = 2,                                                                              
			.Choices = {{ "关", "off" }, { "开", "on" }},
};                                                                                                         
                                                                                                           
static struct MenuEntry CheatsMenu_SelectedState28 = {                                                     
			ENTRY_OPTION("cheat status", &game_config.cheats_flag[28].cheat_name, &game_config.cheats_flag[28].cheat_active),
			.ChoiceCount = 2,                                                                              
			.Choices = {{ "关", "off" }, { "开", "on" }},
};                                                                                                         
                                                                                                           
static struct MenuEntry CheatsMenu_SelectedState29 = {                                                     
			ENTRY_OPTION("cheat status", &game_config.cheats_flag[29].cheat_name, &game_config.cheats_flag[29].cheat_active),
			.ChoiceCount = 2,                                                                              
			.Choices = {{ "关", "off" }, { "开", "on" }},
};                                                                                                         
                                                                                                           
static struct MenuEntry CheatsMenu_SelectedState30 = {                                                     
			ENTRY_OPTION("cheat status", &game_config.cheats_flag[30].cheat_name, &game_config.cheats_flag[30].cheat_active),
			.ChoiceCount = 2,                                                                              
			.Choices = {{ "关", "off" }, { "开", "on" }},
};                                                                                                         
                                                                                                           
static struct MenuEntry CheatsMenu_SelectedState31 = {                                                     
			ENTRY_OPTION("cheat status", &game_config.cheats_flag[31].cheat_name, &game_config.cheats_flag[31].cheat_active),
			.ChoiceCount = 2,                                                                              
			.Choices = {{ "关", "off" }, { "开", "on" }},
};                                                                                                         
                                                                                                           
static struct MenuEntry CheatsMenu_SelectedState32 = {                                                     
			ENTRY_OPTION("cheat status", &game_config.cheats_flag[32].cheat_name, &game_config.cheats_flag[32].cheat_active),
			.ChoiceCount = 2,                                                                              
			.Choices = {{ "关", "off" }, { "开", "on" }},
};                                                                                                         
                                                                                                           
static struct MenuEntry CheatsMenu_SelectedState33 = {                                                     
			ENTRY_OPTION("cheat status", &game_config.cheats_flag[33].cheat_name, &game_config.cheats_flag[33].cheat_active),
			.ChoiceCount = 2,                                                                              
			.Choices = {{ "关", "off" }, { "开", "on" }},
};                                                                                                         
                                                                                                           
static struct MenuEntry CheatsMenu_SelectedState34 = {                                                     
			ENTRY_OPTION("cheat status", &game_config.cheats_flag[34].cheat_name, &game_config.cheats_flag[34].cheat_active),
			.ChoiceCount = 2,                                                                              
			.Choices = {{ "关", "off" }, { "开", "on" }},
};                                                                                                         
                                                                                                           
static struct MenuEntry CheatsMenu_SelectedState35 = {                                                     
			ENTRY_OPTION("cheat status", &game_config.cheats_flag[35].cheat_name, &game_config.cheats_flag[35].cheat_active),
			.ChoiceCount = 2,                                                                              
			.Choices = {{ "关", "off" }, { "开", "on" }},
};                                                                                                         
                                                                                                           
static struct MenuEntry CheatsMenu_SelectedState36 = {                                                     
			ENTRY_OPTION("cheat status", &game_config.cheats_flag[36].cheat_name, &game_config.cheats_flag[36].cheat_active),
			.ChoiceCount = 2,                                                                              
			.Choices = {{ "关", "off" }, { "开", "on" }},
};                                                                                                         
                                                                                                           
static struct MenuEntry CheatsMenu_SelectedState37 = {                                                     
			ENTRY_OPTION("cheat status", &game_config.cheats_flag[37].cheat_name, &game_config.cheats_flag[37].cheat_active),
			.ChoiceCount = 2,                                                                              
			.Choices = {{ "关", "off" }, { "开", "on" }},
};                                                                                                         
                                                                                                           
static struct MenuEntry CheatsMenu_SelectedState38 = {                                                     
			ENTRY_OPTION("cheat status", &game_config.cheats_flag[38].cheat_name, &game_config.cheats_flag[38].cheat_active),
			.ChoiceCount = 2,                                                                              
			.Choices = {{ "关", "off" }, { "开", "on" }},
};                                                                                                         
                                                                                                           
static struct MenuEntry CheatsMenu_SelectedState39 = {                                                     
			ENTRY_OPTION("cheat status", &game_config.cheats_flag[39].cheat_name, &game_config.cheats_flag[39].cheat_active),
			.ChoiceCount = 2,                                                                              
			.Choices = {{ "关", "off" }, { "开", "on" }},
};                                                                                                         
                                                                                                           
static struct MenuEntry CheatsMenu_SelectedState40 = {                                                     
			ENTRY_OPTION("cheat status", &game_config.cheats_flag[40].cheat_name, &game_config.cheats_flag[40].cheat_active),
			.ChoiceCount = 2,                                                                              
			.Choices = {{ "关", "off" }, { "开", "on" }},
};                                                                                                         
                                                                                                           
static struct MenuEntry CheatsMenu_SelectedState41 = {                                                     
			ENTRY_OPTION("cheat status", &game_config.cheats_flag[41].cheat_name, &game_config.cheats_flag[41].cheat_active),
			.ChoiceCount = 2,                                                                              
			.Choices = {{ "关", "off" }, { "开", "on" }},
};                                                                                                         
                                                                                                           
static struct MenuEntry CheatsMenu_SelectedState42 = {                                                     
			ENTRY_OPTION("cheat status", &game_config.cheats_flag[42].cheat_name, &game_config.cheats_flag[42].cheat_active),
			.ChoiceCount = 2,                                                                              
			.Choices = {{ "关", "off" }, { "开", "on" }},
};                                                                                                         
                                                                                                           
static struct MenuEntry CheatsMenu_SelectedState43 = {                                                     
			ENTRY_OPTION("cheat status", &game_config.cheats_flag[43].cheat_name, &game_config.cheats_flag[43].cheat_active),
			.ChoiceCount = 2,                                                                              
			.Choices = {{ "关", "off" }, { "开", "on" }},
};                                                                                                         
                                                                                                           
static struct MenuEntry CheatsMenu_SelectedState44 = {                                                     
			ENTRY_OPTION("cheat status", &game_config.cheats_flag[44].cheat_name, &game_config.cheats_flag[44].cheat_active),
			.ChoiceCount = 2,                                                                              
			.Choices = {{ "关", "off" }, { "开", "on" }},
};                                                                                                         
                                                                                                           
static struct MenuEntry CheatsMenu_SelectedState45 = {                                                     
			ENTRY_OPTION("cheat status", &game_config.cheats_flag[45].cheat_name, &game_config.cheats_flag[45].cheat_active),
			.ChoiceCount = 2,                                                                              
			.Choices = {{ "关", "off" }, { "开", "on" }},
};                                                                                                         
                                                                                                           
static struct MenuEntry CheatsMenu_SelectedState46 = {                                                     
			ENTRY_OPTION("cheat status", &game_config.cheats_flag[46].cheat_name, &game_config.cheats_flag[46].cheat_active),
			.ChoiceCount = 2,                                                                              
			.Choices = {{ "关", "off" }, { "开", "on" }},
};                                                                                                         
                                                                                                           
static struct MenuEntry CheatsMenu_SelectedState47 = {                                                     
			ENTRY_OPTION("cheat status", &game_config.cheats_flag[47].cheat_name, &game_config.cheats_flag[47].cheat_active),
			.ChoiceCount = 2,                                                                              
			.Choices = {{ "关", "off" }, { "开", "on" }},
};                                                                                                         
                                                                                                           
static struct MenuEntry CheatsMenu_SelectedState48 = {                                                     
			ENTRY_OPTION("cheat status", &game_config.cheats_flag[48].cheat_name, &game_config.cheats_flag[48].cheat_active),
			.ChoiceCount = 2,                                                                              
			.Choices = {{ "关", "off" }, { "开", "on" }},
};                                                                                                         
                                                                                                           
static struct MenuEntry CheatsMenu_SelectedState49 = {                                                     
			ENTRY_OPTION("cheat status", &game_config.cheats_flag[49].cheat_name, &game_config.cheats_flag[49].cheat_active),
			.ChoiceCount = 2,                                                                              
			.Choices = {{ "关", "off" }, { "开", "on" }},
};                                                                                                         
                                                                                                           
static struct MenuEntry CheatsMenu_SelectedState50 = {                                                     
			ENTRY_OPTION("cheat status", &game_config.cheats_flag[50].cheat_name, &game_config.cheats_flag[50].cheat_active),
			.ChoiceCount = 2,                                                                              
			.Choices = {{ "关", "off" }, { "开", "on" }},
};                                                                                                         
                                                                                                           
static struct MenuEntry CheatsMenu_SelectedState51 = {                                                     
			ENTRY_OPTION("cheat status", &game_config.cheats_flag[51].cheat_name, &game_config.cheats_flag[51].cheat_active),
			.ChoiceCount = 2,                                                                              
			.Choices = {{ "关", "off" }, { "开", "on" }},
};                                                                                                         
                                                                                                           
static struct MenuEntry CheatsMenu_SelectedState52 = {                                                     
			ENTRY_OPTION("cheat status", &game_config.cheats_flag[52].cheat_name, &game_config.cheats_flag[52].cheat_active),
			.ChoiceCount = 2,                                                                              
			.Choices = {{ "关", "off" }, { "开", "on" }},
};                                                                                                         
                                                                                                           
static struct MenuEntry CheatsMenu_SelectedState53 = {                                                     
			ENTRY_OPTION("cheat status", &game_config.cheats_flag[53].cheat_name, &game_config.cheats_flag[53].cheat_active),
			.ChoiceCount = 2,                                                                              
			.Choices = {{ "关", "off" }, { "开", "on" }},
};                                                                                                         
                                                                                                           
static struct MenuEntry CheatsMenu_SelectedState54 = {                                                     
			ENTRY_OPTION("cheat status", &game_config.cheats_flag[54].cheat_name, &game_config.cheats_flag[54].cheat_active),
			.ChoiceCount = 2,                                                                              
			.Choices = {{ "关", "off" }, { "开", "on" }},
};                                                                                                         
                                                                                                           
static struct MenuEntry CheatsMenu_SelectedState55 = {                                                     
			ENTRY_OPTION("cheat status", &game_config.cheats_flag[55].cheat_name, &game_config.cheats_flag[55].cheat_active),
			.ChoiceCount = 2,                                                                              
			.Choices = {{ "关", "off" }, { "开", "on" }},
};                                                                                                         
                                                                                                           
static struct MenuEntry CheatsMenu_SelectedState56 = {                                                     
			ENTRY_OPTION("cheat status", &game_config.cheats_flag[56].cheat_name, &game_config.cheats_flag[56].cheat_active),
			.ChoiceCount = 2,                                                                              
			.Choices = {{ "关", "off" }, { "开", "on" }},
};                                                                                                         
                                                                                                           
static struct MenuEntry CheatsMenu_SelectedState57 = {                                                     
			ENTRY_OPTION("cheat status", &game_config.cheats_flag[57].cheat_name, &game_config.cheats_flag[57].cheat_active),
			.ChoiceCount = 2,                                                                              
			.Choices = {{ "关", "off" }, { "开", "on" }},
};                                                                                                         
                                                                                                           
static struct MenuEntry CheatsMenu_SelectedState58 = {                                                     
			ENTRY_OPTION("cheat status", &game_config.cheats_flag[58].cheat_name, &game_config.cheats_flag[58].cheat_active),
			.ChoiceCount = 2,                                                                              
			.Choices = {{ "关", "off" }, { "开", "on" }},
};                                                                                                         
                                                                                                           
static struct MenuEntry CheatsMenu_SelectedState59 = {                                                     
			ENTRY_OPTION("cheat status", &game_config.cheats_flag[59].cheat_name, &game_config.cheats_flag[59].cheat_active),
			.ChoiceCount = 2,                                                                              
			.Choices = {{ "关", "off" }, { "开", "on" }},
};                                                                                                         
                                                                                                           
static struct MenuEntry CheatsMenu_SelectedState60 = {                                                     
			ENTRY_OPTION("cheat status", &game_config.cheats_flag[60].cheat_name, &game_config.cheats_flag[60].cheat_active),
			.ChoiceCount = 2,                                                                              
			.Choices = {{ "关", "off" }, { "开", "on" }},
};                                                                                                         
                                                                                                           
static struct MenuEntry CheatsMenu_SelectedState61 = {                                                     
			ENTRY_OPTION("cheat status", &game_config.cheats_flag[61].cheat_name, &game_config.cheats_flag[61].cheat_active),
			.ChoiceCount = 2,                                                                              
			.Choices = {{ "关", "off" }, { "开", "on" }},
};                                                                                                         
                                                                                                           
static struct MenuEntry CheatsMenu_SelectedState62 = {                                                     
			ENTRY_OPTION("cheat status", &game_config.cheats_flag[62].cheat_name, &game_config.cheats_flag[62].cheat_active),
			.ChoiceCount = 2,                                                                              
			.Choices = {{ "关", "off" }, { "开", "on" }},
};                                                                                                         
                                                                                                           
static struct MenuEntry CheatsMenu_SelectedState63 = {                                                     
			ENTRY_OPTION("cheat status", &game_config.cheats_flag[63].cheat_name, &game_config.cheats_flag[63].cheat_active),
			.ChoiceCount = 2,                                                                              
			.Choices = {{ "关", "off" }, { "开", "on" }},
};                                                                                                         
                                                                                                           
static struct MenuEntry CheatsMenu_SelectedState64 = {                                                     
			ENTRY_OPTION("cheat status", &game_config.cheats_flag[64].cheat_name, &game_config.cheats_flag[64].cheat_active),
			.ChoiceCount = 2,                                                                              
			.Choices = {{ "关", "off" }, { "开", "on" }},
};                                                                                                         
                                                                                                           
static struct MenuEntry CheatsMenu_SelectedState65 = {                                                     
			ENTRY_OPTION("cheat status", &game_config.cheats_flag[65].cheat_name, &game_config.cheats_flag[65].cheat_active),
			.ChoiceCount = 2,                                                                              
			.Choices = {{ "关", "off" }, { "开", "on" }},
};                                                                                                         
                                                                                                           
static struct MenuEntry CheatsMenu_SelectedState66 = {                                                     
			ENTRY_OPTION("cheat status", &game_config.cheats_flag[66].cheat_name, &game_config.cheats_flag[66].cheat_active),
			.ChoiceCount = 2,                                                                              
			.Choices = {{ "关", "off" }, { "开", "on" }},
};                                                                                                         
                                                                                                           
static struct MenuEntry CheatsMenu_SelectedState67 = {                                                     
			ENTRY_OPTION("cheat status", &game_config.cheats_flag[67].cheat_name, &game_config.cheats_flag[67].cheat_active),
			.ChoiceCount = 2,                                                                              
			.Choices = {{ "关", "off" }, { "开", "on" }},
};                                                                                                         
                                                                                                           
static struct MenuEntry CheatsMenu_SelectedState68 = {                                                     
			ENTRY_OPTION("cheat status", &game_config.cheats_flag[68].cheat_name, &game_config.cheats_flag[68].cheat_active),
			.ChoiceCount = 2,                                                                              
			.Choices = {{ "关", "off" }, { "开", "on" }},
};                                                                                                         
                                                                                                           
static struct MenuEntry CheatsMenu_SelectedState69 = {                                                     
			ENTRY_OPTION("cheat status", &game_config.cheats_flag[69].cheat_name, &game_config.cheats_flag[69].cheat_active),
			.ChoiceCount = 2,                                                                              
			.Choices = {{ "关", "off" }, { "开", "on" }},
};                                                                                                         
                                                                                                           
static struct MenuEntry CheatsMenu_SelectedState70 = {                                                     
			ENTRY_OPTION("cheat status", &game_config.cheats_flag[70].cheat_name, &game_config.cheats_flag[70].cheat_active),
			.ChoiceCount = 2,                                                                              
			.Choices = {{ "关", "off" }, { "开", "on" }},
};                                                                                                         
                                                                                                           
static struct MenuEntry CheatsMenu_SelectedState71 = {                                                     
			ENTRY_OPTION("cheat status", &game_config.cheats_flag[71].cheat_name, &game_config.cheats_flag[71].cheat_active),
			.ChoiceCount = 2,                                                                              
			.Choices = {{ "关", "off" }, { "开", "on" }},
};                                                                                                         
                                                                                                           
static struct MenuEntry CheatsMenu_SelectedState72 = {                                                     
			ENTRY_OPTION("cheat status", &game_config.cheats_flag[72].cheat_name, &game_config.cheats_flag[72].cheat_active),
			.ChoiceCount = 2,                                                                              
			.Choices = {{ "关", "off" }, { "开", "on" }},
};                                                                                                         
                                                                                                           
static struct MenuEntry CheatsMenu_SelectedState73 = {                                                     
			ENTRY_OPTION("cheat status", &game_config.cheats_flag[73].cheat_name, &game_config.cheats_flag[73].cheat_active),
			.ChoiceCount = 2,                                                                              
			.Choices = {{ "关", "off" }, { "开", "on" }},
};                                                                                                         
                                                                                                           
static struct MenuEntry CheatsMenu_SelectedState74 = {                                                     
			ENTRY_OPTION("cheat status", &game_config.cheats_flag[74].cheat_name, &game_config.cheats_flag[74].cheat_active),
			.ChoiceCount = 2,                                                                              
			.Choices = {{ "关", "off" }, { "开", "on" }},
};                                                                                                         
                                                                                                           
static struct MenuEntry CheatsMenu_SelectedState75 = {                                                     
			ENTRY_OPTION("cheat status", &game_config.cheats_flag[75].cheat_name, &game_config.cheats_flag[75].cheat_active),
			.ChoiceCount = 2,                                                                              
			.Choices = {{ "关", "off" }, { "开", "on" }},
};                                                                                                         
                                                                                                           
static struct MenuEntry CheatsMenu_SelectedState76 = {                                                     
			ENTRY_OPTION("cheat status", &game_config.cheats_flag[76].cheat_name, &game_config.cheats_flag[76].cheat_active),
			.ChoiceCount = 2,                                                                              
			.Choices = {{ "关", "off" }, { "开", "on" }},
};                                                                                                         
                                                                                                           
static struct MenuEntry CheatsMenu_SelectedState77 = {                                                     
			ENTRY_OPTION("cheat status", &game_config.cheats_flag[77].cheat_name, &game_config.cheats_flag[77].cheat_active),
			.ChoiceCount = 2,                                                                              
			.Choices = {{ "关", "off" }, { "开", "on" }},
};                                                                                                         
                                                                                                           
static struct MenuEntry CheatsMenu_SelectedState78 = {                                                     
			ENTRY_OPTION("cheat status", &game_config.cheats_flag[78].cheat_name, &game_config.cheats_flag[78].cheat_active),
			.ChoiceCount = 2,                                                                              
			.Choices = {{ "关", "off" }, { "开", "on" }},
};                                                                                                         
                                                                                                           
static struct MenuEntry CheatsMenu_SelectedState79 = {                                                     
			ENTRY_OPTION("cheat status", &game_config.cheats_flag[79].cheat_name, &game_config.cheats_flag[79].cheat_active),
			.ChoiceCount = 2,                                                                              
			.Choices = {{ "关", "off" }, { "开", "on" }},
};                                                                                                         
                                                                                                           
static struct MenuEntry CheatsMenu_SelectedState80 = {                                                     
			ENTRY_OPTION("cheat status", &game_config.cheats_flag[80].cheat_name, &game_config.cheats_flag[80].cheat_active),
			.ChoiceCount = 2,                                                                              
			.Choices = {{ "关", "off" }, { "开", "on" }},
};                                                                                                         
                                                                                                           
static struct MenuEntry CheatsMenu_SelectedState81 = {                                                     
			ENTRY_OPTION("cheat status", &game_config.cheats_flag[81].cheat_name, &game_config.cheats_flag[81].cheat_active),
			.ChoiceCount = 2,                                                                              
			.Choices = {{ "关", "off" }, { "开", "on" }},
};                                                                                                         
                                                                                                           
static struct MenuEntry CheatsMenu_SelectedState82 = {                                                     
			ENTRY_OPTION("cheat status", &game_config.cheats_flag[82].cheat_name, &game_config.cheats_flag[82].cheat_active),
			.ChoiceCount = 2,                                                                              
			.Choices = {{ "关", "off" }, { "开", "on" }},
};                                                                                                         
                                                                                                           
static struct MenuEntry CheatsMenu_SelectedState83 = {                                                     
			ENTRY_OPTION("cheat status", &game_config.cheats_flag[83].cheat_name, &game_config.cheats_flag[83].cheat_active),
			.ChoiceCount = 2,                                                                              
			.Choices = {{ "关", "off" }, { "开", "on" }},
};                                                                                                         
                                                                                                           
static struct MenuEntry CheatsMenu_SelectedState84 = {                                                     
			ENTRY_OPTION("cheat status", &game_config.cheats_flag[84].cheat_name, &game_config.cheats_flag[84].cheat_active),
			.ChoiceCount = 2,                                                                              
			.Choices = {{ "关", "off" }, { "开", "on" }},
};                                                                                                         
                                                                                                           
static struct MenuEntry CheatsMenu_SelectedState85 = {                                                     
			ENTRY_OPTION("cheat status", &game_config.cheats_flag[85].cheat_name, &game_config.cheats_flag[85].cheat_active),
			.ChoiceCount = 2,                                                                              
			.Choices = {{ "关", "off" }, { "开", "on" }},
};                                                                                                         
                                                                                                           
static struct MenuEntry CheatsMenu_SelectedState86 = {                                                     
			ENTRY_OPTION("cheat status", &game_config.cheats_flag[86].cheat_name, &game_config.cheats_flag[86].cheat_active),
			.ChoiceCount = 2,                                                                              
			.Choices = {{ "关", "off" }, { "开", "on" }},
};                                                                                                         
                                                                                                           
static struct MenuEntry CheatsMenu_SelectedState87 = {                                                     
			ENTRY_OPTION("cheat status", &game_config.cheats_flag[87].cheat_name, &game_config.cheats_flag[87].cheat_active),
			.ChoiceCount = 2,                                                                              
			.Choices = {{ "关", "off" }, { "开", "on" }},
};                                                                                                         
                                                                                                           
static struct MenuEntry CheatsMenu_SelectedState88 = {                                                     
			ENTRY_OPTION("cheat status", &game_config.cheats_flag[88].cheat_name, &game_config.cheats_flag[88].cheat_active),
			.ChoiceCount = 2,                                                                              
			.Choices = {{ "关", "off" }, { "开", "on" }},
};                                                                                                         
                                                                                                           
static struct MenuEntry CheatsMenu_SelectedState89 = {                                                     
			ENTRY_OPTION("cheat status", &game_config.cheats_flag[89].cheat_name, &game_config.cheats_flag[89].cheat_active),
			.ChoiceCount = 2,                                                                              
			.Choices = {{ "关", "off" }, { "开", "on" }},
};                                                                                                         
                                                                                                           
static struct MenuEntry CheatsMenu_SelectedState90 = {                                                     
			ENTRY_OPTION("cheat status", &game_config.cheats_flag[90].cheat_name, &game_config.cheats_flag[90].cheat_active),
			.ChoiceCount = 2,                                                                              
			.Choices = {{ "关", "off" }, { "开", "on" }},
};                                                                                                         
                                                                                                           
static struct MenuEntry CheatsMenu_SelectedState91 = {                                                     
			ENTRY_OPTION("cheat status", &game_config.cheats_flag[91].cheat_name, &game_config.cheats_flag[91].cheat_active),
			.ChoiceCount = 2,                                                                              
			.Choices = {{ "关", "off" }, { "开", "on" }},
};                                                                                                         
                                                                                                           
static struct MenuEntry CheatsMenu_SelectedState92 = {                                                     
			ENTRY_OPTION("cheat status", &game_config.cheats_flag[92].cheat_name, &game_config.cheats_flag[92].cheat_active),
			.ChoiceCount = 2,                                                                              
			.Choices = {{ "关", "off" }, { "开", "on" }},
};                                                                                                         
                                                                                                           
static struct MenuEntry CheatsMenu_SelectedState93 = {                                                     
			ENTRY_OPTION("cheat status", &game_config.cheats_flag[93].cheat_name, &game_config.cheats_flag[93].cheat_active),
			.ChoiceCount = 2,                                                                              
			.Choices = {{ "关", "off" }, { "开", "on" }},
};                                                                                                         
                                                                                                           
static struct MenuEntry CheatsMenu_SelectedState94 = {                                                     
			ENTRY_OPTION("cheat status", &game_config.cheats_flag[94].cheat_name, &game_config.cheats_flag[94].cheat_active),
			.ChoiceCount = 2,                                                                              
			.Choices = {{ "关", "off" }, { "开", "on" }},
};                                                                                                         
                                                                                                           
static struct MenuEntry CheatsMenu_SelectedState95 = {                                                     
			ENTRY_OPTION("cheat status", &game_config.cheats_flag[95].cheat_name, &game_config.cheats_flag[95].cheat_active),
			.ChoiceCount = 2,                                                                              
			.Choices = {{ "关", "off" }, { "开", "on" }},
};                                                                                                         
                                                                                                           
static struct MenuEntry CheatsMenu_SelectedState96 = {                                                     
			ENTRY_OPTION("cheat status", &game_config.cheats_flag[96].cheat_name, &game_config.cheats_flag[96].cheat_active),
			.ChoiceCount = 2,                                                                              
			.Choices = {{ "关", "off" }, { "开", "on" }},
};                                                                                                         
                                                                                                           
static struct MenuEntry CheatsMenu_SelectedState97 = {                                                     
			ENTRY_OPTION("cheat status", &game_config.cheats_flag[97].cheat_name, &game_config.cheats_flag[97].cheat_active),
			.ChoiceCount = 2,                                                                              
			.Choices = {{ "关", "off" }, { "开", "on" }},
};                                                                                                         
                                                                                                           
static struct MenuEntry CheatsMenu_SelectedState98 = {                                                     
			ENTRY_OPTION("cheat status", &game_config.cheats_flag[98].cheat_name, &game_config.cheats_flag[98].cheat_active),
			.ChoiceCount = 2,                                                                                
			.Choices = {{ "关", "off" }, { "开", "on" }},  
};                                                                                                           
                                                                                                             
static struct MenuEntry CheatsMenu_SelectedState99 = {                                                       
			ENTRY_OPTION("cheat status", &game_config.cheats_flag[99].cheat_name, &game_config.cheats_flag[99].cheat_active),
			.ChoiceCount = 2,
			.Choices = {{ "关", "off" }, { "开", "on" }},
};

static void CheatListDownFunc(struct Menu** ActiveMenu){
	int index;
	if((*ActiveMenu)->ActiveEntryIndex == 12 && (*ActiveMenu)->Entries[0] ==&CheatsMenu_SelectedState && g_num_cheats > 13){
		(*ActiveMenu)->Title = "金手指列表 第2页";
		(*ActiveMenu)->Entries[0] = &CheatsMenu_SelectedState13;
		(*ActiveMenu)->Entries[1] = &CheatsMenu_SelectedState14;
		(*ActiveMenu)->Entries[2] = &CheatsMenu_SelectedState15;
		(*ActiveMenu)->Entries[3] = &CheatsMenu_SelectedState16;
		(*ActiveMenu)->Entries[4] = &CheatsMenu_SelectedState17;
		(*ActiveMenu)->Entries[5] = &CheatsMenu_SelectedState18;
		(*ActiveMenu)->Entries[6] = &CheatsMenu_SelectedState19;
		(*ActiveMenu)->Entries[7] = &CheatsMenu_SelectedState20;
		(*ActiveMenu)->Entries[8] = &CheatsMenu_SelectedState21;
		(*ActiveMenu)->Entries[9] = &CheatsMenu_SelectedState22;
		(*ActiveMenu)->Entries[10] = &CheatsMenu_SelectedState23;
		(*ActiveMenu)->Entries[11] = &CheatsMenu_SelectedState24;
		(*ActiveMenu)->Entries[12] = &CheatsMenu_SelectedState25;
		for(index =0 ;index <= 12;index++){
			if(!strcmp((*ActiveMenu)->Entries[index]->Name,"")){
				(*ActiveMenu)->Entries[index] = &Strut;
			}
		}
		(*ActiveMenu)->ActiveEntryIndex = 0;
	}else if((*ActiveMenu)->ActiveEntryIndex == 12 && (*ActiveMenu)->Entries[0] ==&CheatsMenu_SelectedState13 && g_num_cheats > 26){
		(*ActiveMenu)->Title = "金手指列表 第3页";
		(*ActiveMenu)->Entries[0] = &CheatsMenu_SelectedState26;
		(*ActiveMenu)->Entries[1] = &CheatsMenu_SelectedState27;
		(*ActiveMenu)->Entries[2] = &CheatsMenu_SelectedState28;
		(*ActiveMenu)->Entries[3] = &CheatsMenu_SelectedState29;
		(*ActiveMenu)->Entries[4] = &CheatsMenu_SelectedState30;
		(*ActiveMenu)->Entries[5] = &CheatsMenu_SelectedState31;
		(*ActiveMenu)->Entries[6] = &CheatsMenu_SelectedState32;
		(*ActiveMenu)->Entries[7] = &CheatsMenu_SelectedState33;
		(*ActiveMenu)->Entries[8] = &CheatsMenu_SelectedState34;
		(*ActiveMenu)->Entries[9] = &CheatsMenu_SelectedState35;
		(*ActiveMenu)->Entries[10] = &CheatsMenu_SelectedState36;
		(*ActiveMenu)->Entries[11] = &CheatsMenu_SelectedState37;
		(*ActiveMenu)->Entries[12] = &CheatsMenu_SelectedState38;
		for(index =0 ;index <= 12;index++){
			if(!strcmp((*ActiveMenu)->Entries[index]->Name,"")){
				(*ActiveMenu)->Entries[index] = &Strut;
			}
		}
		(*ActiveMenu)->ActiveEntryIndex = 0;
	}else if((*ActiveMenu)->ActiveEntryIndex == 12 && (*ActiveMenu)->Entries[0] ==&CheatsMenu_SelectedState26&& g_num_cheats > 39){
		(*ActiveMenu)->Title = "金手指列表 第4页";
		(*ActiveMenu)->Entries[0] = &CheatsMenu_SelectedState39;
		(*ActiveMenu)->Entries[1] = &CheatsMenu_SelectedState40;
		(*ActiveMenu)->Entries[2] = &CheatsMenu_SelectedState41;
		(*ActiveMenu)->Entries[3] = &CheatsMenu_SelectedState42;
		(*ActiveMenu)->Entries[4] = &CheatsMenu_SelectedState43;
		(*ActiveMenu)->Entries[5] = &CheatsMenu_SelectedState44;
		(*ActiveMenu)->Entries[6] = &CheatsMenu_SelectedState45;
		(*ActiveMenu)->Entries[7] = &CheatsMenu_SelectedState46;
		(*ActiveMenu)->Entries[8] = &CheatsMenu_SelectedState47;
		(*ActiveMenu)->Entries[9] = &CheatsMenu_SelectedState48;
		(*ActiveMenu)->Entries[10] = &CheatsMenu_SelectedState49;
		(*ActiveMenu)->Entries[11] = &CheatsMenu_SelectedState50;
		(*ActiveMenu)->Entries[12] = &CheatsMenu_SelectedState51;
		for(index =0 ;index <= 12;index++){
			if(!strcmp((*ActiveMenu)->Entries[index]->Name,"")){
				(*ActiveMenu)->Entries[index] = &Strut;
			}
		}
		(*ActiveMenu)->ActiveEntryIndex = 0;
	}else if((*ActiveMenu)->ActiveEntryIndex == 12 && (*ActiveMenu)->Entries[0] ==&CheatsMenu_SelectedState39 && g_num_cheats > 53){
		(*ActiveMenu)->Title = "金手指列表 第5页";
		(*ActiveMenu)->Entries[0] = &CheatsMenu_SelectedState52;
		(*ActiveMenu)->Entries[1] = &CheatsMenu_SelectedState53;
		(*ActiveMenu)->Entries[2] = &CheatsMenu_SelectedState54;
		(*ActiveMenu)->Entries[3] = &CheatsMenu_SelectedState55;
		(*ActiveMenu)->Entries[4] = &CheatsMenu_SelectedState56;
		(*ActiveMenu)->Entries[5] = &CheatsMenu_SelectedState57;
		(*ActiveMenu)->Entries[6] = &CheatsMenu_SelectedState58;
		(*ActiveMenu)->Entries[7] = &CheatsMenu_SelectedState59;
		(*ActiveMenu)->Entries[8] = &CheatsMenu_SelectedState60;
		(*ActiveMenu)->Entries[9] = &CheatsMenu_SelectedState61;
		(*ActiveMenu)->Entries[10] = &CheatsMenu_SelectedState62;
		(*ActiveMenu)->Entries[11] = &CheatsMenu_SelectedState63;
		(*ActiveMenu)->Entries[12] = &CheatsMenu_SelectedState64;
		for(index =0 ;index <= 12;index++){
			if(!strcmp((*ActiveMenu)->Entries[index]->Name,"")){
				(*ActiveMenu)->Entries[index] = &Strut;
			}
		}
		(*ActiveMenu)->ActiveEntryIndex = 0;
	}else if((*ActiveMenu)->ActiveEntryIndex == 12 && (*ActiveMenu)->Entries[0] ==&CheatsMenu_SelectedState52 && g_num_cheats > 65){
		(*ActiveMenu)->Title = "金手指列表 第6页";
		(*ActiveMenu)->Entries[0] = &CheatsMenu_SelectedState65;
		(*ActiveMenu)->Entries[1] = &CheatsMenu_SelectedState66;
		(*ActiveMenu)->Entries[2] = &CheatsMenu_SelectedState67;
		(*ActiveMenu)->Entries[3] = &CheatsMenu_SelectedState68;
		(*ActiveMenu)->Entries[4] = &CheatsMenu_SelectedState69;
		(*ActiveMenu)->Entries[5] = &CheatsMenu_SelectedState70;
		(*ActiveMenu)->Entries[6] = &CheatsMenu_SelectedState71;
		(*ActiveMenu)->Entries[7] = &CheatsMenu_SelectedState72;
		(*ActiveMenu)->Entries[8] = &CheatsMenu_SelectedState73;
		(*ActiveMenu)->Entries[9] = &CheatsMenu_SelectedState74;
		(*ActiveMenu)->Entries[10] = &CheatsMenu_SelectedState75;
		(*ActiveMenu)->Entries[11] = &CheatsMenu_SelectedState76;
		(*ActiveMenu)->Entries[12] = &CheatsMenu_SelectedState77;
		for(index =0 ;index <= 12;index++){
			if(!strcmp((*ActiveMenu)->Entries[index]->Name,"")){
				(*ActiveMenu)->Entries[index] = &Strut;
			}
		}
		(*ActiveMenu)->ActiveEntryIndex = 0;
	}else if((*ActiveMenu)->ActiveEntryIndex == 12 && (*ActiveMenu)->Entries[0] ==&CheatsMenu_SelectedState65 && g_num_cheats > 78){
		(*ActiveMenu)->Title = "金手指列表 第7页";
		(*ActiveMenu)->Entries[0] = &CheatsMenu_SelectedState78;
		(*ActiveMenu)->Entries[1] = &CheatsMenu_SelectedState79;
		(*ActiveMenu)->Entries[2] = &CheatsMenu_SelectedState80;
		(*ActiveMenu)->Entries[3] = &CheatsMenu_SelectedState81;
		(*ActiveMenu)->Entries[4] = &CheatsMenu_SelectedState82;
		(*ActiveMenu)->Entries[5] = &CheatsMenu_SelectedState83;
		(*ActiveMenu)->Entries[6] = &CheatsMenu_SelectedState84;
		(*ActiveMenu)->Entries[7] = &CheatsMenu_SelectedState85;
		(*ActiveMenu)->Entries[8] = &CheatsMenu_SelectedState86;
		(*ActiveMenu)->Entries[9] = &CheatsMenu_SelectedState87;
		(*ActiveMenu)->Entries[10] = &CheatsMenu_SelectedState88;
		(*ActiveMenu)->Entries[11] = &CheatsMenu_SelectedState89;
		(*ActiveMenu)->Entries[12] = &CheatsMenu_SelectedState90;
		for(index =0 ;index <= 12;index++){
			if(!strcmp((*ActiveMenu)->Entries[index]->Name,"")){
				(*ActiveMenu)->Entries[index] = &Strut;
			}
		}
		(*ActiveMenu)->ActiveEntryIndex = 0;
	}else if((*ActiveMenu)->ActiveEntryIndex == 12 && (*ActiveMenu)->Entries[0] ==&CheatsMenu_SelectedState78 && g_num_cheats > 91){
		(*ActiveMenu)->Title = "金手指列表 第8页";
		(*ActiveMenu)->Entries[0] = &CheatsMenu_SelectedState91;
		(*ActiveMenu)->Entries[1] = &CheatsMenu_SelectedState92;
		(*ActiveMenu)->Entries[2] = &CheatsMenu_SelectedState93;
		(*ActiveMenu)->Entries[3] = &CheatsMenu_SelectedState94;
		(*ActiveMenu)->Entries[4] = &CheatsMenu_SelectedState95;
		(*ActiveMenu)->Entries[5] = &CheatsMenu_SelectedState96;
		(*ActiveMenu)->Entries[6] = &CheatsMenu_SelectedState97;
		(*ActiveMenu)->Entries[7] = &CheatsMenu_SelectedState98;
		(*ActiveMenu)->Entries[8] = &CheatsMenu_SelectedState99;
		(*ActiveMenu)->Entries[9] = &Strut;
		(*ActiveMenu)->Entries[10] = &Strut;
		(*ActiveMenu)->Entries[11] = &Strut;
		(*ActiveMenu)->Entries[12] = &Strut;
		(*ActiveMenu)->ActiveEntryIndex = 0;
	}else if((*ActiveMenu)->ActiveEntryIndex == 8&& (*ActiveMenu)->Entries[0] == &CheatsMenu_SelectedState91){
		//nothing...
	}else if(!strcmp((*ActiveMenu)->Entries[(*ActiveMenu)->ActiveEntryIndex + 1]->Name,"")){
		//nothing
	}else{
		DefaultDownFunction(&(*ActiveMenu), &(*ActiveMenu)->ActiveEntryIndex);
	}
	
	
 }
 
static void CheatListUpFunc(struct Menu** ActiveMenu){
	
	if((*ActiveMenu)->ActiveEntryIndex == 0&& (*ActiveMenu)->Entries[0] ==&CheatsMenu_SelectedState13){
		(*ActiveMenu)->Title = "金手指列表 第1页";
		(*ActiveMenu)->Entries[0] = &CheatsMenu_SelectedState;
		(*ActiveMenu)->Entries[1] = &CheatsMenu_SelectedState1;
		(*ActiveMenu)->Entries[2] = &CheatsMenu_SelectedState2;
		(*ActiveMenu)->Entries[3] = &CheatsMenu_SelectedState3;
		(*ActiveMenu)->Entries[4] = &CheatsMenu_SelectedState4;
		(*ActiveMenu)->Entries[5] = &CheatsMenu_SelectedState5;
		(*ActiveMenu)->Entries[6] = &CheatsMenu_SelectedState6;
		(*ActiveMenu)->Entries[7] = &CheatsMenu_SelectedState7;
		(*ActiveMenu)->Entries[8] = &CheatsMenu_SelectedState8;
		(*ActiveMenu)->Entries[9] = &CheatsMenu_SelectedState9;
		(*ActiveMenu)->Entries[10] = &CheatsMenu_SelectedState10;
		(*ActiveMenu)->Entries[11] = &CheatsMenu_SelectedState11;
		(*ActiveMenu)->Entries[12] = &CheatsMenu_SelectedState12;
		(*ActiveMenu)->ActiveEntryIndex = 12;
	}else if((*ActiveMenu)->ActiveEntryIndex == 0&& (*ActiveMenu)->Entries[0] ==&CheatsMenu_SelectedState26){
		(*ActiveMenu)->Title = "金手指列表 第2页";
		(*ActiveMenu)->Entries[0] = &CheatsMenu_SelectedState13;
		(*ActiveMenu)->Entries[1] = &CheatsMenu_SelectedState14;
		(*ActiveMenu)->Entries[2] = &CheatsMenu_SelectedState15;
		(*ActiveMenu)->Entries[3] = &CheatsMenu_SelectedState16;
		(*ActiveMenu)->Entries[4] = &CheatsMenu_SelectedState17;
		(*ActiveMenu)->Entries[5] = &CheatsMenu_SelectedState18;
		(*ActiveMenu)->Entries[6] = &CheatsMenu_SelectedState19;
		(*ActiveMenu)->Entries[7] = &CheatsMenu_SelectedState20;
		(*ActiveMenu)->Entries[8] = &CheatsMenu_SelectedState21;
		(*ActiveMenu)->Entries[9] = &CheatsMenu_SelectedState22;
		(*ActiveMenu)->Entries[10] = &CheatsMenu_SelectedState23;
		(*ActiveMenu)->Entries[11] = &CheatsMenu_SelectedState24;
		(*ActiveMenu)->Entries[12] = &CheatsMenu_SelectedState25;
		(*ActiveMenu)->ActiveEntryIndex = 12;
	}else if((*ActiveMenu)->ActiveEntryIndex == 0 && (*ActiveMenu)->Entries[0] ==&CheatsMenu_SelectedState39){
		(*ActiveMenu)->Title = "金手指列表 第3页";
		(*ActiveMenu)->Entries[0] = &CheatsMenu_SelectedState26;
		(*ActiveMenu)->Entries[1] = &CheatsMenu_SelectedState27;
		(*ActiveMenu)->Entries[2] = &CheatsMenu_SelectedState28;
		(*ActiveMenu)->Entries[3] = &CheatsMenu_SelectedState29;
		(*ActiveMenu)->Entries[4] = &CheatsMenu_SelectedState30;
		(*ActiveMenu)->Entries[5] = &CheatsMenu_SelectedState31;
		(*ActiveMenu)->Entries[6] = &CheatsMenu_SelectedState32;
		(*ActiveMenu)->Entries[7] = &CheatsMenu_SelectedState33;
		(*ActiveMenu)->Entries[8] = &CheatsMenu_SelectedState34;
		(*ActiveMenu)->Entries[9] = &CheatsMenu_SelectedState35;
		(*ActiveMenu)->Entries[10] = &CheatsMenu_SelectedState36;
		(*ActiveMenu)->Entries[11] = &CheatsMenu_SelectedState37;
		(*ActiveMenu)->Entries[12] = &CheatsMenu_SelectedState38;
		(*ActiveMenu)->ActiveEntryIndex = 12;
	}else if((*ActiveMenu)->ActiveEntryIndex == 0 && (*ActiveMenu)->Entries[0] ==&CheatsMenu_SelectedState52){
		(*ActiveMenu)->Title = "金手指列表 第4页";
		(*ActiveMenu)->Entries[0] = &CheatsMenu_SelectedState39;
		(*ActiveMenu)->Entries[1] = &CheatsMenu_SelectedState40;
		(*ActiveMenu)->Entries[2] = &CheatsMenu_SelectedState41;
		(*ActiveMenu)->Entries[3] = &CheatsMenu_SelectedState42;
		(*ActiveMenu)->Entries[4] = &CheatsMenu_SelectedState43;
		(*ActiveMenu)->Entries[5] = &CheatsMenu_SelectedState44;
		(*ActiveMenu)->Entries[6] = &CheatsMenu_SelectedState45;
		(*ActiveMenu)->Entries[7] = &CheatsMenu_SelectedState46;
		(*ActiveMenu)->Entries[8] = &CheatsMenu_SelectedState47;
		(*ActiveMenu)->Entries[9] = &CheatsMenu_SelectedState48;
		(*ActiveMenu)->Entries[10] = &CheatsMenu_SelectedState49;
		(*ActiveMenu)->Entries[11] = &CheatsMenu_SelectedState50;
		(*ActiveMenu)->Entries[12] = &CheatsMenu_SelectedState51;
		(*ActiveMenu)->ActiveEntryIndex = 12;
	}else if((*ActiveMenu)->ActiveEntryIndex == 0 && (*ActiveMenu)->Entries[0] ==&CheatsMenu_SelectedState65){
		(*ActiveMenu)->Title = "金手指列表 第5页";
		(*ActiveMenu)->Entries[0] = &CheatsMenu_SelectedState52;
		(*ActiveMenu)->Entries[1] = &CheatsMenu_SelectedState53;
		(*ActiveMenu)->Entries[2] = &CheatsMenu_SelectedState54;
		(*ActiveMenu)->Entries[3] = &CheatsMenu_SelectedState55;
		(*ActiveMenu)->Entries[4] = &CheatsMenu_SelectedState56;
		(*ActiveMenu)->Entries[5] = &CheatsMenu_SelectedState57;
		(*ActiveMenu)->Entries[6] = &CheatsMenu_SelectedState58;
		(*ActiveMenu)->Entries[7] = &CheatsMenu_SelectedState59;
		(*ActiveMenu)->Entries[8] = &CheatsMenu_SelectedState60;
		(*ActiveMenu)->Entries[9] = &CheatsMenu_SelectedState61;
		(*ActiveMenu)->Entries[10] = &CheatsMenu_SelectedState62;
		(*ActiveMenu)->Entries[11] = &CheatsMenu_SelectedState63;
		(*ActiveMenu)->Entries[12] = &CheatsMenu_SelectedState64;
		(*ActiveMenu)->ActiveEntryIndex = 12;
	}else if((*ActiveMenu)->ActiveEntryIndex == 0 && (*ActiveMenu)->Entries[0] ==&CheatsMenu_SelectedState78){
		(*ActiveMenu)->Title = "金手指列表 第6页";
		(*ActiveMenu)->Entries[0] = &CheatsMenu_SelectedState65;
		(*ActiveMenu)->Entries[1] = &CheatsMenu_SelectedState66;
		(*ActiveMenu)->Entries[2] = &CheatsMenu_SelectedState67;
		(*ActiveMenu)->Entries[3] = &CheatsMenu_SelectedState68;
		(*ActiveMenu)->Entries[4] = &CheatsMenu_SelectedState69;
		(*ActiveMenu)->Entries[5] = &CheatsMenu_SelectedState70;
		(*ActiveMenu)->Entries[6] = &CheatsMenu_SelectedState71;
		(*ActiveMenu)->Entries[7] = &CheatsMenu_SelectedState72;
		(*ActiveMenu)->Entries[8] = &CheatsMenu_SelectedState73;
		(*ActiveMenu)->Entries[9] = &CheatsMenu_SelectedState74;
		(*ActiveMenu)->Entries[10] = &CheatsMenu_SelectedState75;
		(*ActiveMenu)->Entries[11] = &CheatsMenu_SelectedState76;
		(*ActiveMenu)->Entries[12] = &CheatsMenu_SelectedState77;
		(*ActiveMenu)->ActiveEntryIndex = 12;
	}else if((*ActiveMenu)->ActiveEntryIndex == 0 && (*ActiveMenu)->Entries[0] ==&CheatsMenu_SelectedState91){
		(*ActiveMenu)->Title = "金手指列表 第7页";
		(*ActiveMenu)->Entries[0] = &CheatsMenu_SelectedState78;
		(*ActiveMenu)->Entries[1] = &CheatsMenu_SelectedState79;
		(*ActiveMenu)->Entries[2] = &CheatsMenu_SelectedState80;
		(*ActiveMenu)->Entries[3] = &CheatsMenu_SelectedState81;
		(*ActiveMenu)->Entries[4] = &CheatsMenu_SelectedState82;
		(*ActiveMenu)->Entries[5] = &CheatsMenu_SelectedState83;
		(*ActiveMenu)->Entries[6] = &CheatsMenu_SelectedState84;
		(*ActiveMenu)->Entries[7] = &CheatsMenu_SelectedState85;
		(*ActiveMenu)->Entries[8] = &CheatsMenu_SelectedState86;
		(*ActiveMenu)->Entries[9] = &CheatsMenu_SelectedState87;
		(*ActiveMenu)->Entries[10] = &CheatsMenu_SelectedState88;
		(*ActiveMenu)->Entries[11] = &CheatsMenu_SelectedState89;
		(*ActiveMenu)->Entries[12] = &CheatsMenu_SelectedState90;
		(*ActiveMenu)->ActiveEntryIndex = 12;
	}else if((*ActiveMenu)->ActiveEntryIndex == 0&& (*ActiveMenu)->Entries[0] ==&CheatsMenu_SelectedState){
		//nothing...
	}else{
		DefaultUpFunction(&(*ActiveMenu), &(*ActiveMenu)->ActiveEntryIndex);
	}
 }

 static void CheatListMenuInit(struct Menu** ActiveMenu)
{
	(*ActiveMenu)->Title = "金手指列表 第1页";
	if((*ActiveMenu)->Entries[0] != &CheatsMenu_SelectedState 
	&& (*ActiveMenu)->Entries[0] != &CheatsMenu_SelectedState13
	&& (*ActiveMenu)->Entries[0] != &CheatsMenu_SelectedState26
	&& (*ActiveMenu)->Entries[0] != &CheatsMenu_SelectedState39
	&& (*ActiveMenu)->Entries[0] != &CheatsMenu_SelectedState52
	&& (*ActiveMenu)->Entries[0] != &CheatsMenu_SelectedState65
	&& (*ActiveMenu)->Entries[0] != &CheatsMenu_SelectedState78
	&& (*ActiveMenu)->Entries[0] != &CheatsMenu_SelectedState91){
		(*ActiveMenu)->Entries[0] = &CheatsMenu_SelectedState;
		(*ActiveMenu)->Entries[1] = &CheatsMenu_SelectedState1;
		(*ActiveMenu)->Entries[2] = &CheatsMenu_SelectedState2;
		(*ActiveMenu)->Entries[3] = &CheatsMenu_SelectedState3;
		(*ActiveMenu)->Entries[4] = &CheatsMenu_SelectedState4;
		(*ActiveMenu)->Entries[5] = &CheatsMenu_SelectedState5;
		(*ActiveMenu)->Entries[6] = &CheatsMenu_SelectedState6;
		(*ActiveMenu)->Entries[7] = &CheatsMenu_SelectedState7;
		(*ActiveMenu)->Entries[8] = &CheatsMenu_SelectedState8;
		(*ActiveMenu)->Entries[9] = &CheatsMenu_SelectedState9;
		(*ActiveMenu)->Entries[10] = &CheatsMenu_SelectedState10;
		(*ActiveMenu)->Entries[11] = &CheatsMenu_SelectedState11;
		(*ActiveMenu)->Entries[12] = &CheatsMenu_SelectedState12;
		/*(*ActiveMenu)->Entries[13] = &CheatsMenu_SelectedState13;
		(*ActiveMenu)->Entries[14] = &CheatsMenu_SelectedState14;
		(*ActiveMenu)->Entries[15] = &CheatsMenu_SelectedState15;
		(*ActiveMenu)->Entries[16] = &CheatsMenu_SelectedState16;
		(*ActiveMenu)->Entries[17] = &CheatsMenu_SelectedState17;
		(*ActiveMenu)->Entries[18] = &CheatsMenu_SelectedState18;
		(*ActiveMenu)->Entries[19] = &CheatsMenu_SelectedState19;
		(*ActiveMenu)->Entries[20] = &CheatsMenu_SelectedState20;
		(*ActiveMenu)->Entries[21] = &CheatsMenu_SelectedState21;
		(*ActiveMenu)->Entries[22] = &CheatsMenu_SelectedState22;
		(*ActiveMenu)->Entries[23] = &CheatsMenu_SelectedState23;
		(*ActiveMenu)->Entries[24] = &CheatsMenu_SelectedState24;
		(*ActiveMenu)->Entries[25] = &CheatsMenu_SelectedState25;
		(*ActiveMenu)->Entries[26] = &CheatsMenu_SelectedState26;
		(*ActiveMenu)->Entries[27] = &CheatsMenu_SelectedState27;
		(*ActiveMenu)->Entries[28] = &CheatsMenu_SelectedState28;
		(*ActiveMenu)->Entries[29] = &CheatsMenu_SelectedState29;
		(*ActiveMenu)->Entries[30] = &CheatsMenu_SelectedState30;
		(*ActiveMenu)->Entries[31] = &CheatsMenu_SelectedState31;
		(*ActiveMenu)->Entries[32] = &CheatsMenu_SelectedState32;
		(*ActiveMenu)->Entries[33] = &CheatsMenu_SelectedState33;
		(*ActiveMenu)->Entries[34] = &CheatsMenu_SelectedState34;
		(*ActiveMenu)->Entries[35] = &CheatsMenu_SelectedState35;
		(*ActiveMenu)->Entries[36] = &CheatsMenu_SelectedState36;
		(*ActiveMenu)->Entries[37] = &CheatsMenu_SelectedState37;
		(*ActiveMenu)->Entries[38] = &CheatsMenu_SelectedState38;
		(*ActiveMenu)->Entries[39] = &CheatsMenu_SelectedState39;
		(*ActiveMenu)->Entries[40] = &CheatsMenu_SelectedState40;
		(*ActiveMenu)->Entries[41] = &CheatsMenu_SelectedState41;
		(*ActiveMenu)->Entries[42] = &CheatsMenu_SelectedState42;
		(*ActiveMenu)->Entries[43] = &CheatsMenu_SelectedState43;
		(*ActiveMenu)->Entries[44] = &CheatsMenu_SelectedState44;
		(*ActiveMenu)->Entries[45] = &CheatsMenu_SelectedState45;
		(*ActiveMenu)->Entries[46] = &CheatsMenu_SelectedState46;
		(*ActiveMenu)->Entries[47] = &CheatsMenu_SelectedState47;
		(*ActiveMenu)->Entries[48] = &CheatsMenu_SelectedState48;
		(*ActiveMenu)->Entries[49] = &CheatsMenu_SelectedState49;
		(*ActiveMenu)->Entries[50] = &CheatsMenu_SelectedState50;
		(*ActiveMenu)->Entries[51] = &CheatsMenu_SelectedState51;
		(*ActiveMenu)->Entries[52] = &CheatsMenu_SelectedState52;
		(*ActiveMenu)->Entries[53] = &CheatsMenu_SelectedState53;
		(*ActiveMenu)->Entries[54] = &CheatsMenu_SelectedState54;
		(*ActiveMenu)->Entries[55] = &CheatsMenu_SelectedState55;
		(*ActiveMenu)->Entries[56] = &CheatsMenu_SelectedState56;
		(*ActiveMenu)->Entries[57] = &CheatsMenu_SelectedState57;
		(*ActiveMenu)->Entries[58] = &CheatsMenu_SelectedState58;
		(*ActiveMenu)->Entries[59] = &CheatsMenu_SelectedState59;
		(*ActiveMenu)->Entries[60] = &CheatsMenu_SelectedState60;
		(*ActiveMenu)->Entries[61] = &CheatsMenu_SelectedState61;
		(*ActiveMenu)->Entries[62] = &CheatsMenu_SelectedState62;
		(*ActiveMenu)->Entries[63] = &CheatsMenu_SelectedState63;
		(*ActiveMenu)->Entries[64] = &CheatsMenu_SelectedState64;
		(*ActiveMenu)->Entries[65] = &CheatsMenu_SelectedState65;
		(*ActiveMenu)->Entries[66] = &CheatsMenu_SelectedState66;
		(*ActiveMenu)->Entries[67] = &CheatsMenu_SelectedState67;
		(*ActiveMenu)->Entries[68] = &CheatsMenu_SelectedState68;
		(*ActiveMenu)->Entries[69] = &CheatsMenu_SelectedState69;
		(*ActiveMenu)->Entries[70] = &CheatsMenu_SelectedState70;
		(*ActiveMenu)->Entries[71] = &CheatsMenu_SelectedState71;
		(*ActiveMenu)->Entries[72] = &CheatsMenu_SelectedState72;
		(*ActiveMenu)->Entries[73] = &CheatsMenu_SelectedState73;
		(*ActiveMenu)->Entries[74] = &CheatsMenu_SelectedState74;
		(*ActiveMenu)->Entries[75] = &CheatsMenu_SelectedState75;
		(*ActiveMenu)->Entries[76] = &CheatsMenu_SelectedState76;
		(*ActiveMenu)->Entries[77] = &CheatsMenu_SelectedState77;
		(*ActiveMenu)->Entries[78] = &CheatsMenu_SelectedState78;
		(*ActiveMenu)->Entries[79] = &CheatsMenu_SelectedState79;
		(*ActiveMenu)->Entries[80] = &CheatsMenu_SelectedState80;
		(*ActiveMenu)->Entries[81] = &CheatsMenu_SelectedState81;
		(*ActiveMenu)->Entries[82] = &CheatsMenu_SelectedState82;
		(*ActiveMenu)->Entries[83] = &CheatsMenu_SelectedState83;
		(*ActiveMenu)->Entries[84] = &CheatsMenu_SelectedState84;
		(*ActiveMenu)->Entries[85] = &CheatsMenu_SelectedState85;
		(*ActiveMenu)->Entries[86] = &CheatsMenu_SelectedState86;
		(*ActiveMenu)->Entries[87] = &CheatsMenu_SelectedState87;
		(*ActiveMenu)->Entries[88] = &CheatsMenu_SelectedState88;
		(*ActiveMenu)->Entries[89] = &CheatsMenu_SelectedState89;
		(*ActiveMenu)->Entries[90] = &CheatsMenu_SelectedState90;
		(*ActiveMenu)->Entries[91] = &CheatsMenu_SelectedState91;
		(*ActiveMenu)->Entries[92] = &CheatsMenu_SelectedState92;
		(*ActiveMenu)->Entries[93] = &CheatsMenu_SelectedState93;
		(*ActiveMenu)->Entries[94] = &CheatsMenu_SelectedState94;
		(*ActiveMenu)->Entries[95] = &CheatsMenu_SelectedState95;
		(*ActiveMenu)->Entries[96] = &CheatsMenu_SelectedState96;
		(*ActiveMenu)->Entries[97] = &CheatsMenu_SelectedState97;
		(*ActiveMenu)->Entries[98] = &CheatsMenu_SelectedState98;
		(*ActiveMenu)->Entries[99] = &CheatsMenu_SelectedState99;*/
		(*ActiveMenu)->Entries[100] = NULL;
		/*
		int i;
		int looptimes = 100 - g_num_cheats;
		for(i=0;i < looptimes; i++){
			
			(*ActiveMenu)->Entries[g_num_cheats + i] = &Strut;
			
		};
		*/
		int index;
		for(index =0 ;index <= 12;index++){
			if(!strcmp((*ActiveMenu)->Entries[index]->Name,"")){
				(*ActiveMenu)->Entries[index] = &Strut;
			}
		}
		(*ActiveMenu)->ActiveEntryIndex = 0;
	}
	(*ActiveMenu)->ActiveEntryIndex = ((*ActiveMenu)->ActiveEntryIndex > 0) ? (*ActiveMenu)->ActiveEntryIndex : 0;

}

static void CheatListPageUpFunc(struct Menu** ActiveMenu){
	
	int i;
	for(i=0;i<13;i++){
		CheatListUpFunc(ActiveMenu);
	}
	/*if((*ActiveMenu)->Entries[0] !=&CheatsMenu_SelectedState){
		(*ActiveMenu)->ActiveEntryIndex = 0;
	}*/
	
}


static void CheatListPageDownFunc(struct Menu** ActiveMenu){
	int i;
	for(i=0;i<13;i++){
		CheatListDownFunc(ActiveMenu);
	}
	/*
	int index;
	if((*ActiveMenu)->Entries[0] ==&CheatsMenu_SelectedState&&!strcmp((*ActiveMenu)->Entries[13]->Name,"")){
		(*ActiveMenu)->Entries[0] = &CheatsMenu_SelectedState13;
		(*ActiveMenu)->Entries[1] = &CheatsMenu_SelectedState14;
		(*ActiveMenu)->Entries[2] = &CheatsMenu_SelectedState15;
		(*ActiveMenu)->Entries[3] = &CheatsMenu_SelectedState16;
		(*ActiveMenu)->Entries[4] = &CheatsMenu_SelectedState17;
		(*ActiveMenu)->Entries[5] = &CheatsMenu_SelectedState18;
		(*ActiveMenu)->Entries[6] = &CheatsMenu_SelectedState19;
		(*ActiveMenu)->Entries[7] = &CheatsMenu_SelectedState20;
		(*ActiveMenu)->Entries[8] = &CheatsMenu_SelectedState21;
		(*ActiveMenu)->Entries[9] = &CheatsMenu_SelectedState22;
		(*ActiveMenu)->Entries[10] = &CheatsMenu_SelectedState23;
		(*ActiveMenu)->Entries[11] = &CheatsMenu_SelectedState24;
		(*ActiveMenu)->Entries[12] = &CheatsMenu_SelectedState25;
		for(index =0 ;index <= 12;index++){
			if(!strcmp((*ActiveMenu)->Entries[index]->Name,"")){
				(*ActiveMenu)->Entries[index] = &Strut;
			}
		}
		(*ActiveMenu)->ActiveEntryIndex = 0;
	}else if((*ActiveMenu)->Entries[0] ==&CheatsMenu_SelectedState13&&!strcmp((*ActiveMenu)->Entries[13]->Name,"")){
		(*ActiveMenu)->Entries[0] = &CheatsMenu_SelectedState26;
		(*ActiveMenu)->Entries[1] = &CheatsMenu_SelectedState27;
		(*ActiveMenu)->Entries[2] = &CheatsMenu_SelectedState28;
		(*ActiveMenu)->Entries[3] = &CheatsMenu_SelectedState29;
		(*ActiveMenu)->Entries[4] = &CheatsMenu_SelectedState30;
		(*ActiveMenu)->Entries[5] = &CheatsMenu_SelectedState31;
		(*ActiveMenu)->Entries[6] = &CheatsMenu_SelectedState32;
		(*ActiveMenu)->Entries[7] = &CheatsMenu_SelectedState33;
		(*ActiveMenu)->Entries[8] = &CheatsMenu_SelectedState34;
		(*ActiveMenu)->Entries[9] = &CheatsMenu_SelectedState35;
		(*ActiveMenu)->Entries[10] = &CheatsMenu_SelectedState36;
		(*ActiveMenu)->Entries[11] = &CheatsMenu_SelectedState37;
		(*ActiveMenu)->Entries[12] = &CheatsMenu_SelectedState38;
		for(index =0 ;index <= 12;index++){
			if(!strcmp((*ActiveMenu)->Entries[index]->Name,"")){
				(*ActiveMenu)->Entries[index] = &Strut;
			}
		}
		(*ActiveMenu)->ActiveEntryIndex = 0;
	}else if((*ActiveMenu)->Entries[0] ==&CheatsMenu_SelectedState26&&!strcmp((*ActiveMenu)->Entries[13]->Name,"")){
		(*ActiveMenu)->Entries[0] = &CheatsMenu_SelectedState39;
		(*ActiveMenu)->Entries[1] = &CheatsMenu_SelectedState40;
		(*ActiveMenu)->Entries[2] = &CheatsMenu_SelectedState41;
		(*ActiveMenu)->Entries[3] = &CheatsMenu_SelectedState42;
		(*ActiveMenu)->Entries[4] = &CheatsMenu_SelectedState43;
		(*ActiveMenu)->Entries[5] = &CheatsMenu_SelectedState44;
		(*ActiveMenu)->Entries[6] = &CheatsMenu_SelectedState45;
		(*ActiveMenu)->Entries[7] = &CheatsMenu_SelectedState46;
		(*ActiveMenu)->Entries[8] = &CheatsMenu_SelectedState47;
		(*ActiveMenu)->Entries[9] = &CheatsMenu_SelectedState48;
		(*ActiveMenu)->Entries[10] = &CheatsMenu_SelectedState49;
		(*ActiveMenu)->Entries[11] = &CheatsMenu_SelectedState50;
		(*ActiveMenu)->Entries[12] = &CheatsMenu_SelectedState51;
		for(index =0 ;index <= 12;index++){
			if(!strcmp((*ActiveMenu)->Entries[index]->Name,"")){
				(*ActiveMenu)->Entries[index] = &Strut;
			}
		}
		(*ActiveMenu)->ActiveEntryIndex = 0;
	}else if((*ActiveMenu)->Entries[0] ==&CheatsMenu_SelectedState39&&!strcmp((*ActiveMenu)->Entries[13]->Name,"")){
		(*ActiveMenu)->Entries[0] = &CheatsMenu_SelectedState52;
		(*ActiveMenu)->Entries[1] = &CheatsMenu_SelectedState53;
		(*ActiveMenu)->Entries[2] = &CheatsMenu_SelectedState54;
		(*ActiveMenu)->Entries[3] = &CheatsMenu_SelectedState55;
		(*ActiveMenu)->Entries[4] = &CheatsMenu_SelectedState56;
		(*ActiveMenu)->Entries[5] = &CheatsMenu_SelectedState57;
		(*ActiveMenu)->Entries[6] = &CheatsMenu_SelectedState58;
		(*ActiveMenu)->Entries[7] = &CheatsMenu_SelectedState59;
		(*ActiveMenu)->Entries[8] = &CheatsMenu_SelectedState60;
		(*ActiveMenu)->Entries[9] = &CheatsMenu_SelectedState61;
		(*ActiveMenu)->Entries[10] = &CheatsMenu_SelectedState62;
		(*ActiveMenu)->Entries[11] = &CheatsMenu_SelectedState63;
		(*ActiveMenu)->Entries[12] = &CheatsMenu_SelectedState64;
		for(index =0 ;index <= 12;index++){
			if(!strcmp((*ActiveMenu)->Entries[index]->Name,"")){
				(*ActiveMenu)->Entries[index] = &Strut;
			}
		}
		(*ActiveMenu)->ActiveEntryIndex = 0;
	}else if((*ActiveMenu)->Entries[0] ==&CheatsMenu_SelectedState52&&!strcmp((*ActiveMenu)->Entries[13]->Name,"")){
		(*ActiveMenu)->Entries[0] = &CheatsMenu_SelectedState65;
		(*ActiveMenu)->Entries[1] = &CheatsMenu_SelectedState66;
		(*ActiveMenu)->Entries[2] = &CheatsMenu_SelectedState67;
		(*ActiveMenu)->Entries[3] = &CheatsMenu_SelectedState68;
		(*ActiveMenu)->Entries[4] = &CheatsMenu_SelectedState69;
		(*ActiveMenu)->Entries[5] = &CheatsMenu_SelectedState70;
		(*ActiveMenu)->Entries[6] = &CheatsMenu_SelectedState71;
		(*ActiveMenu)->Entries[7] = &CheatsMenu_SelectedState72;
		(*ActiveMenu)->Entries[8] = &CheatsMenu_SelectedState73;
		(*ActiveMenu)->Entries[9] = &CheatsMenu_SelectedState74;
		(*ActiveMenu)->Entries[10] = &CheatsMenu_SelectedState75;
		(*ActiveMenu)->Entries[11] = &CheatsMenu_SelectedState76;
		(*ActiveMenu)->Entries[12] = &CheatsMenu_SelectedState77;
		for(index =0 ;index <= 12;index++){
			if(!strcmp((*ActiveMenu)->Entries[index]->Name,"")){
				(*ActiveMenu)->Entries[index] = &Strut;
			}
		}
		(*ActiveMenu)->ActiveEntryIndex = 0;
	}else if((*ActiveMenu)->Entries[0] ==&CheatsMenu_SelectedState65&&!strcmp((*ActiveMenu)->Entries[13]->Name,"")){
		(*ActiveMenu)->Entries[0] = &CheatsMenu_SelectedState78;
		(*ActiveMenu)->Entries[1] = &CheatsMenu_SelectedState79;
		(*ActiveMenu)->Entries[2] = &CheatsMenu_SelectedState80;
		(*ActiveMenu)->Entries[3] = &CheatsMenu_SelectedState81;
		(*ActiveMenu)->Entries[4] = &CheatsMenu_SelectedState82;
		(*ActiveMenu)->Entries[5] = &CheatsMenu_SelectedState83;
		(*ActiveMenu)->Entries[6] = &CheatsMenu_SelectedState84;
		(*ActiveMenu)->Entries[7] = &CheatsMenu_SelectedState85;
		(*ActiveMenu)->Entries[8] = &CheatsMenu_SelectedState86;
		(*ActiveMenu)->Entries[9] = &CheatsMenu_SelectedState87;
		(*ActiveMenu)->Entries[10] = &CheatsMenu_SelectedState88;
		(*ActiveMenu)->Entries[11] = &CheatsMenu_SelectedState89;
		(*ActiveMenu)->Entries[12] = &CheatsMenu_SelectedState90;
		for(index =0 ;index <= 12;index++){
			if(!strcmp((*ActiveMenu)->Entries[index]->Name,"")){
				(*ActiveMenu)->Entries[index] = &Strut;
			}
		}
		(*ActiveMenu)->ActiveEntryIndex = 0;
	}else if((*ActiveMenu)->Entries[0] ==&CheatsMenu_SelectedState78&&!strcmp((*ActiveMenu)->Entries[13]->Name,"")){
		(*ActiveMenu)->Entries[0] = &CheatsMenu_SelectedState91;
		(*ActiveMenu)->Entries[1] = &CheatsMenu_SelectedState92;
		(*ActiveMenu)->Entries[2] = &CheatsMenu_SelectedState93;
		(*ActiveMenu)->Entries[3] = &CheatsMenu_SelectedState94;
		(*ActiveMenu)->Entries[4] = &CheatsMenu_SelectedState95;
		(*ActiveMenu)->Entries[5] = &CheatsMenu_SelectedState96;
		(*ActiveMenu)->Entries[6] = &CheatsMenu_SelectedState97;
		(*ActiveMenu)->Entries[7] = &CheatsMenu_SelectedState98;
		(*ActiveMenu)->Entries[8] = &CheatsMenu_SelectedState99;
		(*ActiveMenu)->Entries[9] = &Strut;
		(*ActiveMenu)->Entries[10] = &Strut;
		(*ActiveMenu)->Entries[11] = &Strut;
		(*ActiveMenu)->Entries[12] = &Strut;
		(*ActiveMenu)->ActiveEntryIndex = 0;
	}else if((*ActiveMenu)->Entries[0] ==&CheatsMenu_SelectedState91){
		//nothing
	}
	*/
}

static void SavedStateMenuInit(struct Menu** ActiveMenu)
{
	(*ActiveMenu)->UserData = calloc(GBA_SCREEN_WIDTH * GBA_SCREEN_HEIGHT, sizeof(uint16_t));
	if ((*ActiveMenu)->UserData == NULL)
	{
		ReGBA_Trace("E: Memory allocation error while entering the Saved States menu");
		*ActiveMenu = (*ActiveMenu)->Parent;
	}
	else
		SavedStateUpdatePreview(*ActiveMenu);
}


static void SavedStateMenuEnd(struct Menu* ActiveMenu)
{
	if (ActiveMenu->UserData != NULL)
	{
		free(ActiveMenu->UserData);
		ActiveMenu->UserData = NULL;
	}
}

// -- Custom display --

static char* OpenDinguxButtonText[OPENDINGUX_BUTTON_COUNT] = {
	"L",
	"R",
	"D-pad Down",
	"D-pad Up",
	"D-pad Left",
	"D-pad Right",
	"Start",
	"Select",
	"B",
	"A",
	LEFT_FACE_BUTTON_NAME,
	TOP_FACE_BUTTON_NAME,
	"Analog Down",
	"Analog Up",
	"Analog Left",
	"Analog Right",
	"Menu",
	"L3",
	"R3",
	"L2",
	"R2",
};

/*
 * Retrieves the button text for a single OpenDingux button.
 * Input:
 *   Button: The single button to describe. If this value is 0, the value is
 *   considered valid and "None" is the description text.
 * Output:
 *   Valid: A pointer to a Boolean variable which is updated with true if
 *     Button was a single button or none, or false otherwise.
 * Returns:
 *   A pointer to a null-terminated string describing Button. This string must
 *   never be freed, as it is statically allocated.
 */
static char* GetButtonText(enum OpenDingux_Buttons Button, bool* Valid)
{
	uint_fast8_t i;
	if (Button == 0)
	{
		*Valid = true;
		return "空";
	}
	else
	{
		for (i = 0; i < OPENDINGUX_BUTTON_COUNT; i++)
		{
			if (Button == 1 << i)
			{
				*Valid = true;
				return OpenDinguxButtonText[i];
			}
		}
		*Valid = false;
		return "无效";
	}
}

/*
 * Retrieves the button text for an OpenDingux button combination.
 * Input:
 *   Button: The buttons to describe. If this value is 0, the description text
 *   is "None". If there are multiple buttons in the bitfield, they are all
 *   added, separated by a '+' character.
 * Output:
 *   Result: A pointer to a buffer which is updated with the description of
 *   the button combination.
 */
static void GetButtonsText(enum OpenDingux_Buttons Buttons, char* Result)
{
	uint_fast8_t i;
	if (Buttons == 0)
	{
		strcpy(Result, "空");
	}
	else
	{
		Result[0] = '\0';
		bool AfterFirst = false;
		for (i = 0; i < OPENDINGUX_BUTTON_COUNT; i++)
		{
			if ((Buttons & 1 << i) != 0)
			{
				if (AfterFirst)
					strcat(Result, "+");
				AfterFirst = true;
				strcat(Result, OpenDinguxButtonText[i]);
			}
		}
	}
}

static void DisplayButtonMappingValue(struct MenuEntry* DrawnMenuEntry, struct MenuEntry* ActiveMenuEntry, uint32_t Position)
{
	bool Valid;
	char* Value = GetButtonText(*(uint32_t*) DrawnMenuEntry->Target, &Valid);

	bool IsActive = (DrawnMenuEntry == ActiveMenuEntry);
	uint16_t TextColor = Valid ? (IsActive ? COLOR_ACTIVE_TEXT : COLOR_INACTIVE_TEXT) : COLOR_ERROR_TEXT;
	uint16_t OutlineColor = Valid ? (IsActive ? COLOR_ACTIVE_OUTLINE : COLOR_INACTIVE_OUTLINE) : COLOR_ERROR_OUTLINE;
	PrintStringOutline(Value, TextColor, OutlineColor, OutputSurface->pixels, OutputSurface->pitch, 0, GetRenderedHeight(" ") * (Position + 2), SCREEN_WIDTH, GetRenderedHeight(" ") + 2, RIGHT, TOP);
}

static void DisplayHotkeyValue(struct MenuEntry* DrawnMenuEntry, struct MenuEntry* ActiveMenuEntry, uint32_t Position)
{
	char Value[256];
	GetButtonsText(*(uint32_t*) DrawnMenuEntry->Target, Value);

	bool IsActive = (DrawnMenuEntry == ActiveMenuEntry);
	uint16_t TextColor = IsActive ? COLOR_ACTIVE_TEXT : COLOR_INACTIVE_TEXT;
	uint16_t OutlineColor = IsActive ? COLOR_ACTIVE_OUTLINE : COLOR_INACTIVE_OUTLINE;
	PrintStringOutline(Value, TextColor, OutlineColor, OutputSurface->pixels, OutputSurface->pitch, 0, GetRenderedHeight(" ") * (Position + 2), SCREEN_WIDTH, GetRenderedHeight(" ") + 2, RIGHT, TOP);
}

static void DisplayErrorBackgroundFunction(struct Menu* ActiveMenu)
{
	if (SDL_MUSTLOCK(OutputSurface))
		SDL_UnlockSurface(OutputSurface);
	SDL_FillRect(OutputSurface, NULL, COLOR_ERROR_BACKGROUND);
	if (SDL_MUSTLOCK(OutputSurface))
		SDL_LockSurface(OutputSurface);
}

static void DisplayWarningBackgroundFunction(struct Menu* ActiveMenu)
{
	if (SDL_MUSTLOCK(OutputSurface))
		SDL_UnlockSurface(OutputSurface);
	SDL_FillRect(OutputSurface, NULL, COLOR_WARNING_BACKGROUND);
	if (SDL_MUSTLOCK(OutputSurface))
		SDL_LockSurface(OutputSurface);
}

static void SavedStateMenuDisplayData(struct Menu* ActiveMenu, struct MenuEntry* ActiveMenuEntry)
{
	PrintStringOutline("预览", COLOR_INACTIVE_TEXT, COLOR_INACTIVE_OUTLINE, OutputSurface->pixels, OutputSurface->pitch, SCREEN_WIDTH - GBA_SCREEN_WIDTH / 2, GetRenderedHeight(" ") * 2, GBA_SCREEN_WIDTH / 2, GetRenderedHeight(" ") + 2, LEFT, TOP);

	gba_render_half((uint16_t*) OutputSurface->pixels, (uint16_t*) ActiveMenu->UserData,
		SCREEN_WIDTH - GBA_SCREEN_WIDTH / 2,
		GetRenderedHeight(" ") * 3 + 1,
		GBA_SCREEN_WIDTH * sizeof(uint16_t),
		OutputSurface->pitch);

	DefaultDisplayDataFunction(ActiveMenu, ActiveMenuEntry);
}

static void SavedStateSelectionDisplayValue(struct MenuEntry* DrawnMenuEntry, struct MenuEntry* ActiveMenuEntry, uint32_t Position)
{
	char Value[11];
	sprintf(Value, "%" PRIu32, *(uint32_t*) DrawnMenuEntry->Target + 1);

	bool IsActive = (DrawnMenuEntry == ActiveMenuEntry);
	uint16_t TextColor = IsActive ? COLOR_ACTIVE_TEXT : COLOR_INACTIVE_TEXT;
	uint16_t OutlineColor = IsActive ? COLOR_ACTIVE_OUTLINE : COLOR_INACTIVE_OUTLINE;
	PrintStringOutline(Value, TextColor, OutlineColor, OutputSurface->pixels, OutputSurface->pitch, 0, GetRenderedHeight(" ") * (Position + 2), SCREEN_WIDTH - GBA_SCREEN_WIDTH / 2 - 16, GetRenderedHeight(" ") + 2, RIGHT, TOP);
}

static void SavedStateUpdatePreview(struct Menu* ActiveMenu)
{
	memset(ActiveMenu->UserData, 0, GBA_SCREEN_WIDTH * GBA_SCREEN_HEIGHT * sizeof(u16));
	char SavedStateFilename[MAX_PATH + 1];
	if (!ReGBA_GetSavedStateFilename(SavedStateFilename, CurrentGamePath, SelectedState))
	{
		ReGBA_Trace("W: Failed to get the name of saved state #%d for '%s'", SelectedState, CurrentGamePath);
		return;
	}

	FILE_TAG_TYPE fp = FILE_TAG_INVALID;
	FILE_OPEN(fp, SavedStateFilename, READ);
	if (!FILE_CHECK_VALID(fp))
		return;

	FILE_SEEK(fp, SVS_HEADER_SIZE + sizeof(struct ReGBA_RTC), SEEK_SET);

	FILE_READ(fp, ActiveMenu->UserData, GBA_SCREEN_WIDTH * GBA_SCREEN_HEIGHT * sizeof(u16));

	FILE_CLOSE(fp);
}

// -- Custom saving --

static char OpenDinguxButtonSave[OPENDINGUX_BUTTON_COUNT] = {
	'L',
	'R',
	'v', // D-pad directions.
	'^',
	'<',
	'>', // (end)
	'S',
	's',
	'B',
	'A',
	'Y', // Using the SNES/DS/A320 mapping, this is the left face button.
	'X', // Using the SNES/DS/A320 mapping, this is the upper face button.
	'd', // Analog nub directions (GCW Zero).
	'u',
	'l',
	'r', // (end)
	'm',
	'/',
	'.',
	'U',
	'D',
};

static void LoadMappingFunction(struct MenuEntry* ActiveMenuEntry, char* Value)
{
	uint32_t Mapping = 0;
	if (Value[0] != 'x')
	{
		uint_fast8_t i;
		for (i = 0; i < OPENDINGUX_BUTTON_COUNT; i++)
			if (Value[0] == OpenDinguxButtonSave[i])
			{
				Mapping = 1 << i;
				break;
			}
	}
	*(uint32_t*) ActiveMenuEntry->Target = Mapping;
}

static void SaveMappingFunction(struct MenuEntry* ActiveMenuEntry, char* Value)
{
	char Temp[32];
	Temp[0] = '\0';
	uint_fast8_t i;
	for (i = 0; i < OPENDINGUX_BUTTON_COUNT; i++)
		if (*(uint32_t*) ActiveMenuEntry->Target == 1 << i)
		{
			Temp[0] = OpenDinguxButtonSave[i];
			sprintf(&Temp[1], " #%s", OpenDinguxButtonText[i]);
			break;
		}
	if (Temp[0] == '\0')
		strcpy(Temp, "x #空");
	snprintf(Value, 256, "%s = %s\n", ActiveMenuEntry->PersistentName, Temp);
}

static void LoadHotkeyFunction(struct MenuEntry* ActiveMenuEntry, char* Value)
{
	uint32_t Hotkey = 0;
	if (Value[0] != 'x')
	{
		char* Ptr = Value;
		while (*Ptr)
		{
			uint_fast8_t i;
			for (i = 0; i < OPENDINGUX_BUTTON_COUNT; i++)
				if (*Ptr == OpenDinguxButtonSave[i])
				{
					Hotkey |= 1 << i;
					break;
				}
			Ptr++;
		}
	}
	*(uint32_t*) ActiveMenuEntry->Target = Hotkey;
}

static void SaveHotkeyFunction(struct MenuEntry* ActiveMenuEntry, char* Value)
{
	char Temp[192];
	char* Ptr = Temp;
	uint_fast8_t i;
	for (i = 0; i < OPENDINGUX_BUTTON_COUNT; i++)
		if ((*(uint32_t*) ActiveMenuEntry->Target & (1 << i)) != 0)
		{
			*Ptr++ = OpenDinguxButtonSave[i];
		}
	if (Ptr == Temp)
		strcpy(Temp, "x #空");
	else
	{
		*Ptr++ = ' ';
		*Ptr++ = '#';
		GetButtonsText(*(uint32_t*) ActiveMenuEntry->Target, Ptr);
	}
	snprintf(Value, 256, "%s = %s\n", ActiveMenuEntry->PersistentName, Temp);
}

// -- Custom actions --

static void ActionExit(struct Menu** ActiveMenu, uint32_t* ActiveMenuEntryIndex)
{
	// Please ensure that the Main Menu itself does not have entries of type
	// KIND_OPTION. The on-demand writing of settings to storage will not
	// occur after quit(), since it acts after the action function returns.
	quit();
	*ActiveMenu = NULL;
}

static void ActionReturn(struct Menu** ActiveMenu, uint32_t* ActiveMenuEntryIndex)
{
	*ActiveMenu = NULL;
}

static void ActionReset(struct Menu** ActiveMenu, uint32_t* ActiveMenuEntryIndex)
{
	reset_gba();
	reg[CHANGED_PC_STATUS] = 1;
	*ActiveMenu = NULL;
}

static void NullLeftFunction(struct Menu* ActiveMenu, struct MenuEntry* ActiveMenuEntry)
{
}

static void NullRightFunction(struct Menu* ActiveMenu, struct MenuEntry* ActiveMenuEntry)
{
}

static enum OpenDingux_Buttons GrabButton(struct Menu* ActiveMenu, char* Text)
{
	MenuFunction DisplayBackgroundFunction = ActiveMenu->DisplayBackgroundFunction;
	if (DisplayBackgroundFunction == NULL) DisplayBackgroundFunction = DefaultDisplayBackgroundFunction;
	enum OpenDingux_Buttons Buttons;
	// Wait for the buttons that triggered the action to be released.
	while (GetPressedOpenDinguxButtons() != 0)
	{
		(*DisplayBackgroundFunction)(ActiveMenu);
		ReGBA_VideoFlip();
		usleep(5000); // for platforms that don't sync their flips
	}
	// Wait until a button is pressed.
	while ((Buttons = GetPressedOpenDinguxButtons()) == 0)
	{
		(*DisplayBackgroundFunction)(ActiveMenu);
		PrintStringOutline(Text, COLOR_ACTIVE_TEXT, COLOR_ACTIVE_OUTLINE, OutputSurface->pixels, OutputSurface->pitch, 0, 0, SCREEN_WIDTH, SCREEN_HEIGHT, CENTER, MIDDLE);
		ReGBA_VideoFlip();
		usleep(5000); // for platforms that don't sync their flips
	}
	// Accumulate buttons until they're all released.
	enum OpenDingux_Buttons ButtonTotal = Buttons;
	while ((Buttons = GetPressedOpenDinguxButtons()) != 0)
	{
		ButtonTotal |= Buttons;
		(*DisplayBackgroundFunction)(ActiveMenu);
		PrintStringOutline(Text, COLOR_ACTIVE_TEXT, COLOR_ACTIVE_OUTLINE, OutputSurface->pixels, OutputSurface->pitch, 0, 0, SCREEN_WIDTH, SCREEN_HEIGHT, CENTER, MIDDLE);
		ReGBA_VideoFlip();
		usleep(5000); // for platforms that don't sync their flips
	}
	return ButtonTotal;
}

static enum OpenDingux_Buttons GrabButtons(struct Menu* ActiveMenu, char* Text)
{
	MenuFunction DisplayBackgroundFunction = ActiveMenu->DisplayBackgroundFunction;
	if (DisplayBackgroundFunction == NULL) DisplayBackgroundFunction = DefaultDisplayBackgroundFunction;
	enum OpenDingux_Buttons Buttons;
	// Wait for the buttons that triggered the action to be released.
	while (GetPressedOpenDinguxButtons() != 0)
	{
		(*DisplayBackgroundFunction)(ActiveMenu);
		ReGBA_VideoFlip();
		usleep(5000); // for platforms that don't sync their flips
	}
	// Wait until a button is pressed.
	while ((Buttons = GetPressedOpenDinguxButtons()) == 0)
	{
		(*DisplayBackgroundFunction)(ActiveMenu);
		PrintStringOutline(Text, COLOR_ACTIVE_TEXT, COLOR_ACTIVE_OUTLINE, OutputSurface->pixels, OutputSurface->pitch, 0, 0, SCREEN_WIDTH, SCREEN_HEIGHT, CENTER, MIDDLE);
		ReGBA_VideoFlip();
		usleep(5000); // for platforms that don't sync their flips
	}
	// Accumulate buttons until they're all released.
	enum OpenDingux_Buttons ButtonTotal = Buttons;
	while ((Buttons = GetPressedOpenDinguxButtons()) != 0)
	{
		// a) If the old buttons are a strict subset of the new buttons,
		//    add the new buttons.
		if ((Buttons | ButtonTotal) == Buttons)
			ButtonTotal |= Buttons;
		// b) If the new buttons are a strict subset of the old buttons,
		//    do nothing. (The user is releasing the buttons to return.)
		else if ((Buttons | ButtonTotal) == ButtonTotal)
			;
		// c) If the new buttons are on another path, replace the buttons
		//    completely, for example, R+X turning into R+Y.
		else
			ButtonTotal = Buttons;
		(*DisplayBackgroundFunction)(ActiveMenu);
		PrintStringOutline(Text, COLOR_ACTIVE_TEXT, COLOR_ACTIVE_OUTLINE, OutputSurface->pixels, OutputSurface->pitch, 0, 0, SCREEN_WIDTH, SCREEN_HEIGHT, CENTER, MIDDLE);
		ReGBA_VideoFlip();
		usleep(5000); // for platforms that don't sync their flips
	}
	return ButtonTotal;
}

static enum OpenDingux_Buttons GrabYesOrNo(struct Menu* ActiveMenu, char* Text)
{
	return GrabButtons(ActiveMenu, Text) == OPENDINGUX_BUTTON_FACE_RIGHT;
}

void ShowErrorScreen(const char* Format, ...)
{
	char* line = malloc(82);
	va_list args;
	int linelen;

	va_start(args, Format);
	if ((linelen = vsnprintf(line, 82, Format, args)) >= 82)
	{
		va_end(args);
		va_start(args, Format);
		free(line);
		line = malloc(linelen + 1);
		vsnprintf(line, linelen + 1, Format, args);
	}
	va_end(args);
	GrabButton(&ErrorScreen, line);
	free(line);
}

enum OpenDingux_Buttons ShowWarningScreen(const char* Format, ...)
{
	/*
	char* line = malloc(82);
	va_list args;
	int linelen;

	va_start(args, Format);
	if ((linelen = vsnprintf(line, 82, Format, args)) >= 82)
	{
		va_end(args);
		va_start(args, Format);
		free(line);
		line = malloc(linelen + 1);
		vsnprintf(line, linelen + 1, Format, args);
	}
	va_end(args);
	GrabButton(&WarningScreen, line);
	free(line);
	*/
	return GrabYesOrNo(&WarningScreen,Format);
}

static void ActionSetMapping(struct Menu** ActiveMenu, uint32_t* ActiveMenuEntryIndex)
{
	char Text[256];
	bool Valid;
	sprintf(Text, "正在设置热键绑定到 %s\n当前为 %s\n"
		"按下一个新按钮 或者按 B键 离开热键绑定",
		(*ActiveMenu)->Entries[*ActiveMenuEntryIndex]->Name,
		GetButtonText(*(uint32_t*) (*ActiveMenu)->Entries[*ActiveMenuEntryIndex]->Target, &Valid));

	enum OpenDingux_Buttons ButtonTotal = GrabButton(*ActiveMenu, Text);
	// If there's more than one button, change nothing.
	uint_fast8_t BitCount = 0, i;
	for (i = 0; i < OPENDINGUX_BUTTON_COUNT; i++)
		if ((ButtonTotal & (1 << i)) != 0)
			BitCount++;
	if (BitCount == 1)
		*(uint32_t*) (*ActiveMenu)->Entries[*ActiveMenuEntryIndex]->Target = ButtonTotal;
}

static void ActionSetOrClearMapping(struct Menu** ActiveMenu, uint32_t* ActiveMenuEntryIndex)
{
	char Text[256];
	bool Valid;
	sprintf(Text, "正在设置热键绑定到 %s\n当前为 %s\n"
		"按下一个新按钮 或者按 B键 清除热键绑定",
		(*ActiveMenu)->Entries[*ActiveMenuEntryIndex]->Name,
		GetButtonText(*(uint32_t*) (*ActiveMenu)->Entries[*ActiveMenuEntryIndex]->Target, &Valid));

	enum OpenDingux_Buttons ButtonTotal = GrabButton(*ActiveMenu, Text);
	// If there's more than one button, clear the mapping.
	uint_fast8_t BitCount = 0, i;
	for (i = 0; i < OPENDINGUX_BUTTON_COUNT; i++)
		if ((ButtonTotal & (1 << i)) != 0)
			BitCount++;
	*(uint32_t*) (*ActiveMenu)->Entries[*ActiveMenuEntryIndex]->Target = (BitCount == 1)
		? ButtonTotal
		: 0;
}

static void ActionSetHotkey(struct Menu** ActiveMenu, uint32_t* ActiveMenuEntryIndex)
{
	char Text[256];
	char Current[256];
	GetButtonsText(*(uint32_t*) (*ActiveMenu)->Entries[*ActiveMenuEntryIndex]->Target, Current);
	sprintf(Text, "正在设置热键绑定到 %s\n当前为 %s\n"
		"按下一个新按钮 （可以为组合键）\n或者按 B键 离开热键绑定",
		(*ActiveMenu)->Entries[*ActiveMenuEntryIndex]->Name,
		Current);

	enum OpenDingux_Buttons ButtonTotal = GrabButtons(*ActiveMenu, Text);
	if (ButtonTotal != OPENDINGUX_BUTTON_FACE_DOWN)
		*(uint32_t*) (*ActiveMenu)->Entries[*ActiveMenuEntryIndex]->Target = ButtonTotal;
}

static void ActionSetOrClearHotkey(struct Menu** ActiveMenu, uint32_t* ActiveMenuEntryIndex)
{
	char Text[256];
	char Current[256];
	GetButtonsText(*(uint32_t*) (*ActiveMenu)->Entries[*ActiveMenuEntryIndex]->Target, Current);
	sprintf(Text, "正在设置热键绑定到 %s\n当前为 %s\n"
		"按下一个新按钮 （可以为组合键）\n或者按 B键 清除热键绑定",
		(*ActiveMenu)->Entries[*ActiveMenuEntryIndex]->Name,
		Current);

	enum OpenDingux_Buttons ButtonTotal = GrabButtons(*ActiveMenu, Text);
	*(uint32_t*) (*ActiveMenu)->Entries[*ActiveMenuEntryIndex]->Target = (ButtonTotal == OPENDINGUX_BUTTON_FACE_DOWN)
		? 0
		: ButtonTotal;
}

static void SavedStateSelectionLeft(struct Menu* ActiveMenu, struct MenuEntry* ActiveMenuEntry)
{
	DefaultLeftFunction(ActiveMenu, ActiveMenuEntry);
	SavedStateUpdatePreview(ActiveMenu);
}

static void SavedStateSelectionRight(struct Menu* ActiveMenu, struct MenuEntry* ActiveMenuEntry)
{
	DefaultRightFunction(ActiveMenu, ActiveMenuEntry);
	SavedStateUpdatePreview(ActiveMenu);
}

static void ActionSavedStateRead(struct Menu** ActiveMenu, uint32_t* ActiveMenuEntryIndex)
{
	switch (load_state(SelectedState))
	{
		case 0:
			*ActiveMenu = NULL;
			break;

		case 1:
			if (errno != 0)
				ShowErrorScreen("读取保存的状态 #%" PRIu32 " 失败:\n%s", SelectedState + 1, strerror(errno));
			else
				ShowErrorScreen("读取保存的状态 #%" PRIu32 " 失败:\n不完整的文件", SelectedState + 1);
			break;

		case 2:
			ShowErrorScreen("读取保存的状态 #%" PRIu32 " 失败:\n文件格式不正确", SelectedState + 1);
			break;
	}
}

static void ActionSavedStateWrite(struct Menu** ActiveMenu, uint32_t* ActiveMenuEntryIndex)
{
	// 1. If the file already exists, ask the user if s/he wants to overwrite it.
	char SavedStateFilename[MAX_PATH + 1];
	if (!ReGBA_GetSavedStateFilename(SavedStateFilename, CurrentGamePath, SelectedState))
	{
		ReGBA_Trace("W: Failed to get the name of saved state #%d for '%s'", SelectedState, CurrentGamePath);
		ShowErrorScreen("Preparing to write saved state #%" PRIu32 " failed:\nPath too long", SelectedState + 1);
		return;
	}
	FILE_TAG_TYPE dummy;
	FILE_OPEN(dummy, SavedStateFilename, READ);
	if (FILE_CHECK_VALID(dummy))
	{
		FILE_CLOSE(dummy);
		char Temp[1024];
		sprintf(Temp, "你想覆盖保存的状态 #%" PRIu32 "?\n[A] = 是  其他键 = 否", SelectedState + 1);
		if (!GrabYesOrNo(*ActiveMenu, Temp))
			return;
	}
	
	// 2. If the file didn't exist or the user wanted to overwrite it, save.
	uint32_t ret = save_state(SelectedState, MainMenu.UserData /* preserved screenshot */);
	if (ret != 1)
	{
		if (errno != 0)
			ShowErrorScreen("写入保存状态 #%" PRIu32 " 失败:\n%s", SelectedState + 1, strerror(errno));
		else
			ShowErrorScreen("写入保存状态 #%" PRIu32 " 失败:\n未知错误", SelectedState + 1);
	}
	SavedStateUpdatePreview(*ActiveMenu);
}





static void ActionSavedStateDelete(struct Menu** ActiveMenu, uint32_t* ActiveMenuEntryIndex)
{
	// 1. Ask the user if s/he wants to delete this saved state.
	char SavedStateFilename[MAX_PATH + 1];
	if (!ReGBA_GetSavedStateFilename(SavedStateFilename, CurrentGamePath, SelectedState))
	{
		ReGBA_Trace("W: Failed to get the name of saved state #%d for '%s'", SelectedState, CurrentGamePath);
		ShowErrorScreen("Preparing to delete saved state #%" PRIu32 " failed:\nPath too long", SelectedState + 1);
		return;
	}
	char Temp[1024];
	sprintf(Temp, "你想删除保存的状态 #%" PRIu32 "吗?\n[A] = 是  其他键 = 否", SelectedState + 1);
	if (!GrabYesOrNo(*ActiveMenu, Temp))
		return;
	
	// 2. If the user wants to, delete the saved state.
	if (remove(SavedStateFilename) != 0)
	{
		ShowErrorScreen("删除保存的状态 #%" PRIu32 " 失败:\n%s", SelectedState + 1, strerror(errno));
	}
	SavedStateUpdatePreview(*ActiveMenu);
}

static void ActionShowVersion(struct Menu** ActiveMenu, uint32_t* ActiveMenuEntryIndex)
{
	char Text[1024];
#ifdef GIT_VERSION_STRING
	sprintf(Text, "ReGBA version %s\nNebuleon/ReGBA commit %s", REGBA_VERSION_STRING, GIT_VERSION_STRING);
#else
	sprintf(Text, "ReGBA version %s", REGBA_VERSION_STRING);
#endif
	GrabButtons(*ActiveMenu, Text);
}

// -- Strut --

static bool CanNeverFocusFunction(struct Menu* ActiveMenu, struct MenuEntry* ActiveMenuEntry)
{
	return false;
}

static struct MenuEntry Strut = {
	.Kind = KIND_CUSTOM, .Name = "",
	.CanFocusFunction = CanNeverFocusFunction
};

// -- Debug > Native code stats --

static struct MenuEntry NativeCodeMenu_ROPeak = {
	ENTRY_DISPLAY("Read-only bytes at peak", &Stats.TranslationBytesPeak[TRANSLATION_REGION_READONLY], TYPE_UINT64)
};

static struct MenuEntry NativeCodeMenu_RWPeak = {
	ENTRY_DISPLAY("Writable bytes at peak", &Stats.TranslationBytesPeak[TRANSLATION_REGION_WRITABLE], TYPE_UINT64)
};

static struct MenuEntry NativeCodeMenu_ROFlushed = {
	ENTRY_DISPLAY("Read-only bytes flushed", &Stats.TranslationBytesFlushed[TRANSLATION_REGION_READONLY], TYPE_UINT64)
};

static struct MenuEntry NativeCodeMenu_RWFlushed = {
	ENTRY_DISPLAY("Writable bytes flushed", &Stats.TranslationBytesFlushed[TRANSLATION_REGION_WRITABLE], TYPE_UINT64)
};

static struct Menu NativeCodeMenu = {
	.Parent = &DebugMenu, .Title = "Native code statistics",
	.Entries = { &NativeCodeMenu_ROPeak, &NativeCodeMenu_RWPeak, &NativeCodeMenu_ROFlushed, &NativeCodeMenu_RWFlushed, NULL }
};

static struct MenuEntry DebugMenu_NativeCode = {
	ENTRY_SUBMENU("Native code statistics...", &NativeCodeMenu)
};

// -- Debug > Metadata stats --

static struct MenuEntry MetadataMenu_ROFull = {
	ENTRY_DISPLAY("Read-only area full", &Stats.TranslationFlushCount[TRANSLATION_REGION_READONLY][FLUSH_REASON_FULL_CACHE], TYPE_UINT64)
};

static struct MenuEntry MetadataMenu_RWFull = {
	ENTRY_DISPLAY("Writable area full", &Stats.TranslationFlushCount[TRANSLATION_REGION_WRITABLE][FLUSH_REASON_FULL_CACHE], TYPE_UINT64)
};

static struct MenuEntry MetadataMenu_BIOSLastTag = {
	ENTRY_DISPLAY("BIOS tags full", &Stats.MetadataClearCount[METADATA_AREA_BIOS][CLEAR_REASON_LAST_TAG], TYPE_UINT64)
};

static struct MenuEntry MetadataMenu_EWRAMLastTag = {
	ENTRY_DISPLAY("EWRAM tags full", &Stats.MetadataClearCount[METADATA_AREA_EWRAM][CLEAR_REASON_LAST_TAG], TYPE_UINT64)
};

static struct MenuEntry MetadataMenu_IWRAMLastTag = {
	ENTRY_DISPLAY("IWRAM tags full", &Stats.MetadataClearCount[METADATA_AREA_IWRAM][CLEAR_REASON_LAST_TAG], TYPE_UINT64)
};

static struct MenuEntry MetadataMenu_VRAMLastTag = {
	ENTRY_DISPLAY("VRAM tags full", &Stats.MetadataClearCount[METADATA_AREA_VRAM][CLEAR_REASON_LAST_TAG], TYPE_UINT64)
};

static struct MenuEntry MetadataMenu_PartialClears = {
	ENTRY_DISPLAY("Partial clears", &Stats.PartialFlushCount, TYPE_UINT64)
};

static struct Menu MetadataMenu = {
	.Parent = &DebugMenu, .Title = "Metadata clear statistics",
	.Entries = { &MetadataMenu_ROFull, &MetadataMenu_RWFull, &MetadataMenu_BIOSLastTag, &MetadataMenu_EWRAMLastTag, &MetadataMenu_IWRAMLastTag, &MetadataMenu_VRAMLastTag, &MetadataMenu_PartialClears, NULL }
};

static struct MenuEntry DebugMenu_Metadata = {
	ENTRY_SUBMENU("Metadata clear statistics...", &MetadataMenu)
};

// -- Debug > Execution stats --

static struct MenuEntry ExecutionMenu_SoundUnderruns = {
	ENTRY_DISPLAY("Sound buffer underruns", &Stats.SoundBufferUnderrunCount, TYPE_UINT64)
};

static struct MenuEntry ExecutionMenu_FramesEmulated = {
	ENTRY_DISPLAY("Frames emulated", &Stats.TotalEmulatedFrames, TYPE_UINT64)
};

static struct MenuEntry ExecutionMenu_FramesRendered = {
	ENTRY_DISPLAY("Frames rendered", &Stats.TotalRenderedFrames, TYPE_UINT64)
};

#ifdef PERFORMANCE_IMPACTING_STATISTICS
static struct MenuEntry ExecutionMenu_ARMOps = {
	ENTRY_DISPLAY("ARM opcodes decoded", &Stats.ARMOpcodesDecoded, TYPE_UINT64)
};

static struct MenuEntry ExecutionMenu_ThumbOps = {
	ENTRY_DISPLAY("Thumb opcodes decoded", &Stats.ThumbOpcodesDecoded, TYPE_UINT64)
};

static struct MenuEntry ExecutionMenu_ThumbROMConsts = {
	ENTRY_DISPLAY("Thumb ROM constants", &Stats.ThumbROMConstants, TYPE_UINT64)
};

static struct MenuEntry ExecutionMenu_MemAccessors = {
	ENTRY_DISPLAY("Memory accessors patched", &Stats.WrongAddressLineCount, TYPE_UINT32)
};
#endif

static struct Menu ExecutionMenu = {
	.Parent = &DebugMenu, .Title = "Execution statistics",
	.Entries = { &ExecutionMenu_SoundUnderruns, &ExecutionMenu_FramesEmulated, &ExecutionMenu_FramesRendered
#ifdef PERFORMANCE_IMPACTING_STATISTICS
	, &ExecutionMenu_ARMOps, &ExecutionMenu_ThumbOps, &ExecutionMenu_ThumbROMConsts, &ExecutionMenu_MemAccessors
#endif
	, NULL }
};

static struct MenuEntry DebugMenu_Execution = {
	ENTRY_SUBMENU("Execution statistics...", &ExecutionMenu)
};

// -- Debug > Code reuse stats --

#ifdef PERFORMANCE_IMPACTING_STATISTICS
static struct MenuEntry ReuseMenu_OpsRecompiled = {
	ENTRY_DISPLAY("Opcodes recompiled", &Stats.OpcodeRecompilationCount, TYPE_UINT64)
};

static struct MenuEntry ReuseMenu_BlocksRecompiled = {
	ENTRY_DISPLAY("Blocks recompiled", &Stats.BlockRecompilationCount, TYPE_UINT64)
};

static struct MenuEntry ReuseMenu_OpsReused = {
	ENTRY_DISPLAY("Opcodes reused", &Stats.OpcodeReuseCount, TYPE_UINT64)
};

static struct MenuEntry ReuseMenu_BlocksReused = {
	ENTRY_DISPLAY("Blocks reused", &Stats.BlockReuseCount, TYPE_UINT64)
};

static struct Menu ReuseMenu = {
	.Parent = &DebugMenu, .Title = "Code reuse statistics",
	.Entries = { &ReuseMenu_OpsRecompiled, &ReuseMenu_BlocksRecompiled, &ReuseMenu_OpsReused, &ReuseMenu_BlocksReused, NULL }
};

static struct MenuEntry DebugMenu_Reuse = {
	ENTRY_SUBMENU("Code reuse statistics...", &ReuseMenu)
};
#endif

static struct MenuEntry ROMInfoMenu_GameName = {
	ENTRY_DISPLAY("game_name =", gamepak_title, TYPE_STRING)
};

static struct MenuEntry ROMInfoMenu_GameCode = {
	ENTRY_DISPLAY("game_code =", gamepak_code, TYPE_STRING)
};

static struct MenuEntry ROMInfoMenu_VendorCode = {
	ENTRY_DISPLAY("vender_code =", gamepak_maker, TYPE_STRING)
};

static struct Menu ROMInfoMenu = {
	.Parent = &DebugMenu, .Title = "ROM information",
	.Entries = { &ROMInfoMenu_GameName, &ROMInfoMenu_GameCode, &ROMInfoMenu_VendorCode, NULL }
};

static struct MenuEntry DebugMenu_ROMInfo = {
	ENTRY_SUBMENU("ROM information...", &ROMInfoMenu)
};

static struct MenuEntry DebugMenu_VersionInfo = {
	.Kind = KIND_CUSTOM, .Name = "ReGBA version information...",
	.ButtonEnterFunction = &ActionShowVersion
};

// -- Debug --

static struct Menu DebugMenu = {
	.Parent = &MainMenu, .Title = "Performance and debugging",
	.Entries = { &DebugMenu_NativeCode, &DebugMenu_Metadata, &DebugMenu_Execution
#ifdef PERFORMANCE_IMPACTING_STATISTICS
	, &DebugMenu_Reuse
#endif
	, &Strut, &DebugMenu_ROMInfo, &Strut, &DebugMenu_VersionInfo, NULL }
};

// -- Display Settings --

static struct MenuEntry PerGameDisplayMenu_BootSource = {
	ENTRY_OPTION("boot_from", "启动器", &PerGameBootFromBIOS),
	.ChoiceCount = 3, .Choices = { { "未覆盖", "" }, { "GBA BIOS", "gba_bios" }, { "Cartridge ROM", "cartridge" } }
};
static struct MenuEntry DisplayMenu_BootSource = {
	ENTRY_OPTION("boot_from", "启动器", &BootFromBIOS),
	.ChoiceCount = 2, .Choices = { { "GBA BIOS", "gba_bios" }, { "Cartridge ROM", "cartridge" } }
};

static struct MenuEntry PerGameDisplayMenu_FPSCounter = {
	ENTRY_OPTION("fps_counter", "FPS统计", &PerGameShowFPS),
	.ChoiceCount = 3, .Choices = { { "未覆盖", "" }, { "隐藏", "hide" }, { "显示", "show" } }
};
static struct MenuEntry DisplayMenu_FPSCounter = {
	ENTRY_OPTION("fps_counter", "FPS统计", &ShowFPS),
	.ChoiceCount = 2, .Choices = { { "隐藏", "hide" }, { "显示", "show" } }
};

#ifndef NO_SCALING
static struct MenuEntry PerGameDisplayMenu_ScaleMode = {
	ENTRY_OPTION("image_size", "图像设置", &PerGameScaleMode),
	.ChoiceCount = 14, .Choices = { { "未覆盖", "" }, { "拉伸, 快速渲染", "aspect" }, { "全屏, 快速渲染", "fullscreen" }, { "拉伸, 双线性", "aspect_bilinear" }, { "全屏, 双线性", "fullscreen_bilinear" }, { "拉伸, 子像素卷积", "aspect_subpixel" }, { "全屏, 子像素卷积", "fullscreen_subpixel" }, { "经典", "original" }, { "硬件渲染", "hardware" } , { "双倍硬件渲染", "hardware_2x" }, { "双倍硬件渲染，竖扫描线", "hardware_2x_scanline_vert" }, { "双倍硬件渲染，横扫描线", "hardware_2x_scanline_horz" }, { "双倍硬件渲染，网格扫描线", "hardware_2x_scanline_grid" }, { "硬件渲染，双倍拉伸", "hardware_scale2x" } }
};
static struct MenuEntry DisplayMenu_ScaleMode = {
	ENTRY_OPTION("image_size", "图像设置", &ScaleMode),
	.ChoiceCount = 13, .Choices = { { "拉伸, 快速渲染", "aspect" }, { "全屏, 快速渲染", "fullscreen" }, { "拉伸, 双线性", "aspect_bilinear" }, { "全屏, 双线性", "fullscreen_bilinear" }, { "拉伸, 子像素卷积", "aspect_subpixel" }, { "全屏, 子像素卷积", "fullscreen_subpixel" }, { "经典", "original" }, { "硬件渲染", "hardware" } , { "双倍硬件渲染", "hardware_2x" }, { "双倍硬件渲染，竖扫描线", "hardware_2x_scanline_vert" }, { "双倍硬件渲染，横扫描线", "hardware_2x_scanline_horz" }, { "双倍硬件渲染，网格扫描线", "hardware_2x_scanline_grid" }, { "硬件渲染，双倍拉伸", "hardware_scale2x" } }
};
#endif

static struct MenuEntry PerGameDisplayMenu_ColorCorrection = {
	ENTRY_OPTION("color_correction", "颜色校正", &PerGameColorCorrection),
	.ChoiceCount = 3, .Choices = { { "未覆盖", "" }, { "闭", "off" }, { "开", "on" } }
};
static struct MenuEntry DisplayMenu_ColorCorrection = {
	ENTRY_OPTION("color_correction", "颜色校正", &ColorCorrection),
	.ChoiceCount = 2, .Choices = { { "闭", "off" }, { "开", "on" } }
};

static struct MenuEntry PerGameDisplayMenu_InterframeBlending = {
	ENTRY_OPTION("interframe_blending", "帧间混合", &PerGameInterframeBlending),
	.ChoiceCount = 3, .Choices = { { "未覆盖", "" }, { "闭", "off" }, { "开", "on" } }
};
static struct MenuEntry DisplayMenu_InterframeBlending = {
	ENTRY_OPTION("interframe_blending", "帧间混合", &InterframeBlending),
	.ChoiceCount = 2, .Choices = { { "闭", "off" }, { "开", "on" } }
};

static struct MenuEntry PerGameDisplayMenu_Frameskip = {
	ENTRY_OPTION("frameskip", "跳帧", &PerGameUserFrameskip),
	.ChoiceCount = 6, .Choices = { { "未覆盖", "" }, { "自动", "auto" }, { "0 (~60 FPS)", "0" }, { "1 (~30 FPS)", "1" }, { "2 (~20 FPS)", "2" }, { "3 (~15 FPS)", "3" } }
};
static struct MenuEntry DisplayMenu_Frameskip = {
	ENTRY_OPTION("frameskip", "跳帧", &UserFrameskip),
	.ChoiceCount = 5, .Choices = { { "自动", "auto" }, { "0 (~60 FPS)", "0" }, { "1 (~30 FPS)", "1" }, { "2 (~20 FPS)", "2" }, { "3 (~15 FPS)", "3" } }
};

static struct MenuEntry PerGameDisplayMenu_FastForwardTarget = {
	ENTRY_OPTION("fast_forward_target", "快进等级", &PerGameFastForwardTarget),
	.ChoiceCount = 6, .Choices = { { "未覆盖", "" }, { "2x (~120 FPS)", "2" }, { "3x (~180 FPS)", "3" }, { "4x (~240 FPS)", "4" }, { "5x (~300 FPS)", "5" }, { "6x (~360 FPS)", "6" } }
};
static struct MenuEntry DisplayMenu_FastForwardTarget = {
	ENTRY_OPTION("fast_forward_target", "快进等级", &FastForwardTarget),
	.ChoiceCount = 5, .Choices = { { "2x (~120 FPS)", "2" }, { "3x (~180 FPS)", "3" }, { "4x (~240 FPS)", "4" }, { "5x (~300 FPS)", "5" }, { "6x (~360 FPS)", "6" } }
};

static struct Menu PerGameDisplayMenu = {
	.Parent = &PerGameMainMenu, .Title = "显示设置",
	MENU_PER_GAME,
	.AlternateVersion = &DisplayMenu,
	.Entries = { &PerGameDisplayMenu_BootSource, &PerGameDisplayMenu_FPSCounter,
#ifndef NO_SCALING
		&PerGameDisplayMenu_ScaleMode,
#endif
		&PerGameDisplayMenu_ColorCorrection, &PerGameDisplayMenu_InterframeBlending,
		&PerGameDisplayMenu_Frameskip, &PerGameDisplayMenu_FastForwardTarget, NULL }
};

static struct Menu DisplayMenu = {
	.Parent = &MainMenu, .Title = "显示设置",
	.AlternateVersion = &PerGameDisplayMenu,
	.Entries = { &DisplayMenu_BootSource, &DisplayMenu_FPSCounter,
#ifndef NO_SCALING
		&DisplayMenu_ScaleMode,
#endif
		&DisplayMenu_ColorCorrection, &DisplayMenu_InterframeBlending,
		&DisplayMenu_Frameskip, &DisplayMenu_FastForwardTarget, NULL }
};

// -- Audio Settings --
static struct MenuEntry PerGameAudioMenu_FastForwardVolume = {
	ENTRY_OPTION("fast_forward_volume", "快进时音量", &PerGameFastForwardVolume),
	.ChoiceCount = 5, .Choices = { { "未覆盖", "" }, { "100%", "100" }, { "50%", "50" }, { "25%", "25" }, { "静音", "0" } }
};
static struct MenuEntry AudioMenu_FastForwardVolume = {
	ENTRY_OPTION("fast_forward_volume", "快进时音量", &FastForwardVolume),
	.ChoiceCount = 4, .Choices = { { "100%", "100" }, { "50%", "50" }, { "25%", "25" }, { "静音", "0" } }
};

static struct Menu PerGameAudioMenu = {
	.Parent = &PerGameMainMenu, .Title = "音频设置",
	MENU_PER_GAME,
	.AlternateVersion = &AudioMenu,
	.Entries = { &PerGameAudioMenu_FastForwardVolume, NULL }
};

static struct Menu AudioMenu = {
	.Parent = &MainMenu, .Title = "音频设置",
	.AlternateVersion = &PerGameAudioMenu,
	.Entries = { &AudioMenu_FastForwardVolume, NULL }
};

// -- Input Settings --
static struct MenuEntry PerGameInputMenu_A = {
	ENTRY_OPTION("gba_a", "GBA A", &PerGameKeypadRemapping[0]),
	ENTRY_OPTIONAL_MAPPING
};
static struct MenuEntry InputMenu_A = {
	ENTRY_OPTION("gba_a", "GBA A", &KeypadRemapping[0]),
	ENTRY_MANDATORY_MAPPING
};

static struct MenuEntry PerGameInputMenu_B = {
	ENTRY_OPTION("gba_b", "GBA B", &PerGameKeypadRemapping[1]),
	ENTRY_OPTIONAL_MAPPING
};
static struct MenuEntry InputMenu_B = {
	ENTRY_OPTION("gba_b", "GBA B", &KeypadRemapping[1]),
	ENTRY_MANDATORY_MAPPING
};

static struct MenuEntry PerGameInputMenu_Start = {
	ENTRY_OPTION("gba_start", "GBA Start", &PerGameKeypadRemapping[3]),
	ENTRY_OPTIONAL_MAPPING
};
static struct MenuEntry InputMenu_Start = {
	ENTRY_OPTION("gba_start", "GBA Start", &KeypadRemapping[3]),
	ENTRY_MANDATORY_MAPPING
};

static struct MenuEntry PerGameInputMenu_Select = {
	ENTRY_OPTION("gba_select", "GBA Select", &PerGameKeypadRemapping[2]),
	ENTRY_OPTIONAL_MAPPING
};
static struct MenuEntry InputMenu_Select = {
	ENTRY_OPTION("gba_select", "GBA Select", &KeypadRemapping[2]),
	ENTRY_MANDATORY_MAPPING
};

static struct MenuEntry PerGameInputMenu_L = {
	ENTRY_OPTION("gba_l", "GBA L", &PerGameKeypadRemapping[9]),
	ENTRY_OPTIONAL_MAPPING
};
static struct MenuEntry InputMenu_L = {
	ENTRY_OPTION("gba_l", "GBA L", &KeypadRemapping[9]),
	ENTRY_MANDATORY_MAPPING
};

static struct MenuEntry PerGameInputMenu_R = {
	ENTRY_OPTION("gba_r", "GBA R", &PerGameKeypadRemapping[8]),
	ENTRY_OPTIONAL_MAPPING
};
static struct MenuEntry InputMenu_R = {
	ENTRY_OPTION("gba_r", "GBA R", &KeypadRemapping[8]),
	ENTRY_MANDATORY_MAPPING
};

static struct MenuEntry PerGameInputMenu_RapidA = {
	ENTRY_OPTION("rapid_a", "连发 A", &PerGameKeypadRemapping[10]),
	ENTRY_OPTIONAL_MAPPING
};
static struct MenuEntry InputMenu_RapidA = {
	ENTRY_OPTION("rapid_a", "连发 A", &KeypadRemapping[10]),
	ENTRY_OPTIONAL_MAPPING
};

static struct MenuEntry PerGameInputMenu_RapidB = {
	ENTRY_OPTION("rapid_b", "连发 B", &PerGameKeypadRemapping[11]),
	ENTRY_OPTIONAL_MAPPING
};
static struct MenuEntry InputMenu_RapidB = {
	ENTRY_OPTION("rapid_b", "连发 B", &KeypadRemapping[11]),
	ENTRY_OPTIONAL_MAPPING
};

#ifdef GCW_ZERO
static struct MenuEntry PerGameInputMenu_AnalogSensitivity = {
	ENTRY_OPTION("analog_sensitivity", "摇杆灵敏度", &PerGameAnalogSensitivity),
	.ChoiceCount = 6, .Choices = { { "不覆盖", "" }, { "非常低", "lowest" }, { "低", "low" }, { "中", "medium" }, { "高", "high" }, { "非常高", "highest" } }
};
static struct MenuEntry InputMenu_AnalogSensitivity = {
	ENTRY_OPTION("analog_sensitivity", "摇杆灵敏度", &AnalogSensitivity),
	.ChoiceCount = 5, .Choices = { { "非常低", "lowest" }, { "低", "low" }, { "中", "medium" }, { "高", "high" }, { "非常高", "highest" } }
};

static struct MenuEntry PerGameInputMenu_AnalogAction = {
	ENTRY_OPTION("analog_action", "绑定摇杆到方向键", &PerGameAnalogAction),
	.ChoiceCount = 3, .Choices = { { "未覆盖", "" }, { "无", "none" }, { "GBA D-pad", "dpad" } }
};
static struct MenuEntry InputMenu_AnalogAction = {
	ENTRY_OPTION("analog_action", "绑定摇杆到方向键", &AnalogAction),
	.ChoiceCount = 2, .Choices = { { "无", "none" }, { "GBA D-pad", "dpad" } }
};

static struct MenuEntry PerGameInputMenu_VibrateLevel = {
	ENTRY_OPTION("vibrate_level", "振动等级", &PerGameVibrateLevel),
	.ChoiceCount = 5, .Choices = { { "未覆盖", "" }, { "关闭", "none" }, { "弱", "weak" }, { "中", "medium" }, { "强", "strong" } }
};
static struct MenuEntry InputMenu_Vibratelevel = {
	ENTRY_OPTION("vibrate_level", "振动等级", &VibrateLevel),
	.ChoiceCount = 4, .Choices = { { "关闭", "none" }, { "弱", "weak" }, { "中", "medium" }, { "强", "strong" } }
};
#endif

static struct Menu PerGameInputMenu = {
	.Parent = &PerGameMainMenu, .Title = "输入设置",
	MENU_PER_GAME,
	.AlternateVersion = &InputMenu,
	.Entries = { &PerGameInputMenu_A, &PerGameInputMenu_B, &PerGameInputMenu_Start, &PerGameInputMenu_Select, &PerGameInputMenu_L, &PerGameInputMenu_R, &PerGameInputMenu_RapidA, &PerGameInputMenu_RapidB
#ifdef GCW_ZERO
	, &Strut, &PerGameInputMenu_AnalogSensitivity, &PerGameInputMenu_AnalogAction, &PerGameInputMenu_VibrateLevel
#endif
	, NULL }
};
static struct Menu InputMenu = {
	.Parent = &MainMenu, .Title = "输入设置",
	.AlternateVersion = &PerGameInputMenu,
	.Entries = { &InputMenu_A, &InputMenu_B, &InputMenu_Start, &InputMenu_Select, &InputMenu_L, &InputMenu_R, &InputMenu_RapidA, &InputMenu_RapidB
#ifdef GCW_ZERO
	, &Strut, &InputMenu_AnalogSensitivity, &InputMenu_AnalogAction, &InputMenu_Vibratelevel
#endif
	, NULL }
};

// -- Hotkeys --

static struct MenuEntry PerGameHotkeyMenu_FastForward = {
	ENTRY_OPTION("hotkey_fast_forward", "按住快进", &PerGameHotkeys[0]),
	ENTRY_OPTIONAL_HOTKEY
};
static struct MenuEntry HotkeyMenu_FastForward = {
	ENTRY_OPTION("hotkey_fast_forward", "按住快进", &Hotkeys[0]),
	ENTRY_OPTIONAL_HOTKEY
};

#if !defined GCW_ZERO
static struct MenuEntry HotkeyMenu_Menu = {
	ENTRY_OPTION("hotkey_menu", "Menu", &Hotkeys[1]),
	ENTRY_MANDATORY_HOTKEY
};
#endif

static struct MenuEntry PerGameHotkeyMenu_FastForwardToggle = {
	ENTRY_OPTION("hotkey_fast_forward_toggle", "开启/关闭快进", &PerGameHotkeys[2]),
	ENTRY_OPTIONAL_HOTKEY
};
static struct MenuEntry HotkeyMenu_FastForwardToggle = {
	ENTRY_OPTION("hotkey_fast_forward_toggle", "开启/关闭快进", &Hotkeys[2]),
	ENTRY_OPTIONAL_HOTKEY
};

static struct MenuEntry PerGameHotkeyMenu_QuickLoadState = {
	ENTRY_OPTION("hotkey_quick_load_state", "快速加载状态 #1", &PerGameHotkeys[3]),
	ENTRY_OPTIONAL_HOTKEY
};
static struct MenuEntry HotkeyMenu_QuickLoadState = {
	ENTRY_OPTION("hotkey_quick_load_state", "快速加载状态 #1", &Hotkeys[3]),
	ENTRY_OPTIONAL_HOTKEY
};

static struct MenuEntry PerGameHotkeyMenu_QuickSaveState = {
	ENTRY_OPTION("hotkey_quick_save_state", "快速保存状态 #1", &PerGameHotkeys[4]),
	ENTRY_OPTIONAL_HOTKEY
};
static struct MenuEntry HotkeyMenu_QuickSaveState = {
	ENTRY_OPTION("hotkey_quick_save_state", "快速保存状态 #1", &Hotkeys[4]),
	ENTRY_OPTIONAL_HOTKEY
};

static struct Menu PerGameHotkeyMenu = {
	.Parent = &PerGameMainMenu, .Title = "热键",
	MENU_PER_GAME,
	.AlternateVersion = &HotkeyMenu,
	.Entries = { &Strut, &PerGameHotkeyMenu_FastForward, &PerGameHotkeyMenu_FastForwardToggle, &PerGameHotkeyMenu_QuickLoadState, &PerGameHotkeyMenu_QuickSaveState, NULL }
};
static struct Menu HotkeyMenu = {
	.Parent = &MainMenu, .Title = "热键",
	.AlternateVersion = &PerGameHotkeyMenu,
	.Entries = {
#if !defined GCW_ZERO
		&HotkeyMenu_Menu,
#else
		&Strut,
#endif
		&HotkeyMenu_FastForward, &HotkeyMenu_FastForwardToggle, &HotkeyMenu_QuickLoadState, &HotkeyMenu_QuickSaveState, NULL
	}
};

// -- Saved States --

static struct MenuEntry SavedStateMenu_SelectedState = {
	.Kind = KIND_CUSTOM, .Name = "插槽 #", .PersistentName = "",
	.Target = &SelectedState,
	.ChoiceCount = 100,
	.ButtonLeftFunction = SavedStateSelectionLeft, .ButtonRightFunction = SavedStateSelectionRight,
	.DisplayValueFunction = SavedStateSelectionDisplayValue
};

static struct MenuEntry SavedStateMenu_Read = {
	.Kind = KIND_CUSTOM, .Name = "从选择的插槽加载",
	.ButtonEnterFunction = ActionSavedStateRead
};

static struct MenuEntry SavedStateMenu_Write = {
	.Kind = KIND_CUSTOM, .Name = "保存到选择的插槽",
	.ButtonEnterFunction = ActionSavedStateWrite
};

static struct MenuEntry SavedStateMenu_Delete = {
	.Kind = KIND_CUSTOM, .Name = "删除选择的状态",
	.ButtonEnterFunction = ActionSavedStateDelete
};


static struct Menu CheatsMenu = {
	.Parent = &MainMenu, .Title = "金手指列表",
	.InitFunction = CheatListMenuInit,
	.ButtonUpFunction = CheatListUpFunc,
	.ButtonDownFunction = CheatListDownFunc,
	.ButtonPageUpFunction = CheatListPageUpFunc,
	.ButtonPageDownFunction = CheatListPageDownFunc,
	.Entries = {&Strut,&Strut,&Strut,&Strut,&Strut,&Strut,&Strut,&Strut,&Strut,&Strut,&Strut,&Strut,&Strut,&Strut,&Strut,&Strut,&Strut,&Strut,&Strut,&Strut,&Strut,&Strut,&Strut,&Strut,&Strut,&Strut,&Strut,&Strut,&Strut,&Strut,&Strut,&Strut,&Strut,&Strut,&Strut,&Strut,&Strut,&Strut,&Strut,&Strut,&Strut,&Strut,&Strut,&Strut,&Strut,&Strut,&Strut,&Strut,&Strut,&Strut,&Strut,&Strut,&Strut,&Strut,&Strut,&Strut,&Strut,&Strut,&Strut,&Strut,&Strut,&Strut,&Strut,&Strut,&Strut,&Strut,&Strut,&Strut,&Strut,&Strut,&Strut,&Strut,&Strut,&Strut,&Strut,&Strut,&Strut,&Strut,&Strut,&Strut,&Strut,&Strut,&Strut,&Strut,&Strut,&Strut,&Strut,&Strut,&Strut,&Strut,&Strut,&Strut,&Strut,&Strut,&Strut,&Strut,&Strut,&Strut,&Strut,&Strut,NULL}
};

static struct Menu SavedStateMenu = {
	.Parent = &MainMenu, .Title = "保存的状态",
	.InitFunction = SavedStateMenuInit, .EndFunction = SavedStateMenuEnd,
	.DisplayDataFunction = SavedStateMenuDisplayData,
	.Entries = { &SavedStateMenu_SelectedState, &Strut, &SavedStateMenu_Read, &SavedStateMenu_Write, &SavedStateMenu_Delete, NULL }
};

// -- Main Menu --

static struct MenuEntry PerGameMainMenu_Display = {
	ENTRY_SUBMENU("显示设置...", &PerGameDisplayMenu)
};
static struct MenuEntry MainMenu_Display = {
	ENTRY_SUBMENU("显示设置...", &DisplayMenu)
};

static struct MenuEntry PerGameMainMenu_Audio = {
	ENTRY_SUBMENU("音频设置...", &PerGameAudioMenu)
};
static struct MenuEntry MainMenu_Audio = {
	ENTRY_SUBMENU("音频设置...", &AudioMenu)
};

static struct MenuEntry PerGameMainMenu_Input = {
	ENTRY_SUBMENU("输入设置...", &PerGameInputMenu)
};
static struct MenuEntry MainMenu_Input = {
	ENTRY_SUBMENU("输入设置...", &InputMenu)
};

static struct MenuEntry PerGameMainMenu_Hotkey = {
	ENTRY_SUBMENU("热键...", &PerGameHotkeyMenu)
};
static struct MenuEntry MainMenu_Hotkey = {
	ENTRY_SUBMENU("热键...", &HotkeyMenu)
};
//Cheats button
static struct MenuEntry MainMenu_Cheats = {
	ENTRY_SUBMENU("金手指...", &CheatsMenu)
};

static struct MenuEntry MainMenu_SavedStates = {
	ENTRY_SUBMENU("保存的状态...", &SavedStateMenu)
};

static struct MenuEntry MainMenu_Debug = {
	ENTRY_SUBMENU("性能与调试...", &DebugMenu)
};

static struct MenuEntry MainMenu_Reset = {
	.Kind = KIND_CUSTOM, .Name = "重置游戏",
	.ButtonEnterFunction = &ActionReset
};

static struct MenuEntry MainMenu_Return = {
	.Kind = KIND_CUSTOM, .Name = "返回到游戏",
	.ButtonEnterFunction = &ActionReturn
};

static struct MenuEntry MainMenu_Exit = {
	.Kind = KIND_CUSTOM, .Name = "退出",
	.ButtonEnterFunction = &ActionExit
};

static struct Menu PerGameMainMenu = {
	.Parent = NULL, .Title = "ReGBA 中文版 V8.0 - 开发:jdgleaver, Cynric 整合:littlehui",
	MENU_PER_GAME,
	.AlternateVersion = &MainMenu,
#if SCREEN_HEIGHT >= 240
	.Entries = { &PerGameMainMenu_Display, &PerGameMainMenu_Audio, &PerGameMainMenu_Input, &PerGameMainMenu_Hotkey, &Strut, &Strut, &Strut, &Strut, &Strut, &Strut, &MainMenu_Reset, &MainMenu_Return, &MainMenu_Exit, NULL }
#else
	.Entries = { &PerGameMainMenu_Display, &PerGameMainMenu_Input, &PerGameMainMenu_Hotkey, &Strut, &Strut, &MainMenu_Reset, &MainMenu_Return, &MainMenu_Exit, NULL }
#endif
};
struct Menu MainMenu = {
	.Parent = NULL, .Title = "ReGBA 中文版 V8.0 - 开发:jdgleaver, Cynric 整合:littlehui",
	.AlternateVersion = &PerGameMainMenu,
#if SCREEN_HEIGHT >= 240
	.Entries = { &MainMenu_Display, &MainMenu_Audio, &MainMenu_Input, &MainMenu_Hotkey, &MainMenu_Cheats, &MainMenu_SavedStates, &Strut, &Strut, &MainMenu_Debug, &Strut, &MainMenu_Reset, &MainMenu_Return, &MainMenu_Exit, NULL }
#else
	.Entries = { &MainMenu_Display, &MainMenu_Input, &MainMenu_Hotkey, &MainMenu_SavedStates, &MainMenu_Debug, &MainMenu_Reset, &MainMenu_Return, &MainMenu_Exit, NULL }
#endif
};

/* Do not make this the active menu */
static struct Menu ErrorScreen = {
	.Parent = &MainMenu, .Title = "错误",
	.DisplayBackgroundFunction = DisplayErrorBackgroundFunction,
	.Entries = { NULL }
};

static struct Menu WarningScreen = {
	.Parent = &MainMenu, .Title = "警告",
	.DisplayBackgroundFunction = DisplayWarningBackgroundFunction,
	.Entries = { NULL }
};

u32 ReGBA_Menu(enum ReGBA_MenuEntryReason EntryReason)
{
	SDL_PauseAudio(SDL_ENABLE);
	MainMenu.UserData = copy_screen();
	ScaleModeUnapplied();

	// Avoid entering the menu with menu keys pressed (namely the one bound to
	// the per-game option switching key, Select).
	while (ReGBA_GetPressedButtons() != 0)
	{
		usleep(5000);
	}

	SetMenuResolution();

	struct Menu *ActiveMenu = &MainMenu, *PreviousMenu = ActiveMenu;
	if (MainMenu.InitFunction != NULL)
	{
		(*(MainMenu.InitFunction))(&ActiveMenu);
		while (PreviousMenu != ActiveMenu)
		{
			if (PreviousMenu->EndFunction != NULL)
				(*(PreviousMenu->EndFunction))(PreviousMenu);
			PreviousMenu = ActiveMenu;
			if (ActiveMenu != NULL && ActiveMenu->InitFunction != NULL)
				(*(ActiveMenu->InitFunction))(&ActiveMenu);
		}
	}

	void* PreviousGlobal = ReGBA_PreserveMenuSettings(&MainMenu);
	void* PreviousPerGame = ReGBA_PreserveMenuSettings(MainMenu.AlternateVersion);

	while (ActiveMenu != NULL)
	{
		// Draw.
		MenuFunction DisplayBackgroundFunction = ActiveMenu->DisplayBackgroundFunction;
		if (DisplayBackgroundFunction == NULL) DisplayBackgroundFunction = DefaultDisplayBackgroundFunction;
		(*DisplayBackgroundFunction)(ActiveMenu);

		MenuFunction DisplayTitleFunction = ActiveMenu->DisplayTitleFunction;
		if (DisplayTitleFunction == NULL) DisplayTitleFunction = DefaultDisplayTitleFunction;
		(*DisplayTitleFunction)(ActiveMenu);

		MenuEntryFunction DisplayDataFunction = ActiveMenu->DisplayDataFunction;
		if (DisplayDataFunction == NULL) DisplayDataFunction = DefaultDisplayDataFunction;
		(*DisplayDataFunction)(ActiveMenu, ActiveMenu->Entries[ActiveMenu->ActiveEntryIndex]);

		ReGBA_VideoFlip();
		
		// Wait. (This is for platforms on which flips don't wait for vertical
		// sync.)
		usleep(5000);

		// Get input.
		enum GUI_Action Action = GetGUIAction();
		
		switch (Action)
		{
			case GUI_ACTION_ENTER:
				if (ActiveMenu->Entries[ActiveMenu->ActiveEntryIndex] != NULL)
				{
					MenuModifyFunction ButtonEnterFunction = ActiveMenu->Entries[ActiveMenu->ActiveEntryIndex]->ButtonEnterFunction;
					if (ButtonEnterFunction == NULL) ButtonEnterFunction = DefaultEnterFunction;
					(*ButtonEnterFunction)(&ActiveMenu, &ActiveMenu->ActiveEntryIndex);
					break;
				}
				// otherwise, no entry has the focus, so ENTER acts like LEAVE
				// (fall through)

			case GUI_ACTION_LEAVE:
			{
				MenuModifyFunction ButtonLeaveFunction = ActiveMenu->ButtonLeaveFunction;
				if (ButtonLeaveFunction == NULL) ButtonLeaveFunction = DefaultLeaveFunction;
				(*ButtonLeaveFunction)(&ActiveMenu, &ActiveMenu->ActiveEntryIndex);
				break;
			}
			
			case GUI_ACTION_PAGEUP:
			{
				MenuModifyFunction ButtonPageUpFunction = ActiveMenu->ButtonPageUpFunction;
				if (ButtonPageUpFunction == NULL) ButtonPageUpFunction = DefaultPageUpFunction;
				(*ButtonPageUpFunction)(&ActiveMenu, &ActiveMenu->ActiveEntryIndex);
				break;
			}
			
			case GUI_ACTION_PAGEDOWN:
			{
				MenuModifyFunction ButtonPageDownFunction = ActiveMenu->ButtonPageDownFunction;
				if (ButtonPageDownFunction == NULL) ButtonPageDownFunction = DefaultPageDownFunction;
				(*ButtonPageDownFunction)(&ActiveMenu, &ActiveMenu->ActiveEntryIndex);
				break;
			}
			
			
			case GUI_ACTION_UP:
			{
				MenuModifyFunction ButtonUpFunction = ActiveMenu->ButtonUpFunction;
				if (ButtonUpFunction == NULL) ButtonUpFunction = DefaultUpFunction;
				(*ButtonUpFunction)(&ActiveMenu, &ActiveMenu->ActiveEntryIndex);
				break;
			}

			case GUI_ACTION_DOWN:
			{
				MenuModifyFunction ButtonDownFunction = ActiveMenu->ButtonDownFunction;
				if (ButtonDownFunction == NULL) ButtonDownFunction = DefaultDownFunction;
				(*ButtonDownFunction)(&ActiveMenu, &ActiveMenu->ActiveEntryIndex);
				break;
			}

			case GUI_ACTION_LEFT:
				if (ActiveMenu->Entries[ActiveMenu->ActiveEntryIndex] != NULL)
				{
					MenuEntryFunction ButtonLeftFunction = ActiveMenu->Entries[ActiveMenu->ActiveEntryIndex]->ButtonLeftFunction;
					if (ButtonLeftFunction == NULL) ButtonLeftFunction = DefaultLeftFunction;
					(*ButtonLeftFunction)(ActiveMenu, ActiveMenu->Entries[ActiveMenu->ActiveEntryIndex]);
				}
				break;

			case GUI_ACTION_RIGHT:
				if (ActiveMenu->Entries[ActiveMenu->ActiveEntryIndex] != NULL)
				{
					MenuEntryFunction ButtonRightFunction = ActiveMenu->Entries[ActiveMenu->ActiveEntryIndex]->ButtonRightFunction;
					if (ButtonRightFunction == NULL) ButtonRightFunction = DefaultRightFunction;
					(*ButtonRightFunction)(ActiveMenu, ActiveMenu->Entries[ActiveMenu->ActiveEntryIndex]);
				}
				break;

			case GUI_ACTION_ALTERNATE:
				if (IsGameLoaded && ActiveMenu->AlternateVersion != NULL)
					ActiveMenu = ActiveMenu->AlternateVersion;
				break;

			default:
				break;
		}

		// Possibly finalise this menu and activate and initialise a new one.
		while (ActiveMenu != PreviousMenu)
		{
			if (PreviousMenu->EndFunction != NULL)
				(*(PreviousMenu->EndFunction))(PreviousMenu);

			// Save settings if they have changed.
			void* Global = ReGBA_PreserveMenuSettings(&MainMenu);

			if (!ReGBA_AreMenuSettingsEqual(&MainMenu, PreviousGlobal, Global))
			{
				ReGBA_SaveSettings("global_config", false);
			}
			free(PreviousGlobal);
			PreviousGlobal = Global;

			void* PerGame = ReGBA_PreserveMenuSettings(MainMenu.AlternateVersion);
			if (!ReGBA_AreMenuSettingsEqual(MainMenu.AlternateVersion, PreviousPerGame, PerGame) && IsGameLoaded)
			{
				char FileNameNoExt[MAX_PATH + 1];
				GetFileNameNoExtension(FileNameNoExt, CurrentGamePath);
				ReGBA_SaveSettings(FileNameNoExt, true);
			}
			free(PreviousPerGame);
			PreviousPerGame = PerGame;

			// Keep moving down until a menu entry can be focused, if
			// the first one can't be.
			if (ActiveMenu != NULL && ActiveMenu->Entries[ActiveMenu->ActiveEntryIndex] != NULL)
			{
				uint32_t Sentinel = ActiveMenu->ActiveEntryIndex;
				MenuEntryCanFocusFunction CanFocusFunction = ActiveMenu->Entries[ActiveMenu->ActiveEntryIndex]->CanFocusFunction;
				if (CanFocusFunction == NULL) CanFocusFunction = DefaultCanFocusFunction;
				while (!(*CanFocusFunction)(ActiveMenu, ActiveMenu->Entries[ActiveMenu->ActiveEntryIndex]))
				{
					MoveDown(ActiveMenu, &ActiveMenu->ActiveEntryIndex);
					if (ActiveMenu->ActiveEntryIndex == Sentinel)
					{
						// If we went through all of them, we cannot focus anything.
						// Place the focus on the NULL entry.
						ActiveMenu->ActiveEntryIndex = FindNullEntry(ActiveMenu);
						break;
					}
					CanFocusFunction = ActiveMenu->Entries[ActiveMenu->ActiveEntryIndex]->CanFocusFunction;
					if (CanFocusFunction == NULL) CanFocusFunction = DefaultCanFocusFunction;
				}
			}

			PreviousMenu = ActiveMenu;
			if (ActiveMenu != NULL && ActiveMenu->InitFunction != NULL)
				(*(ActiveMenu->InitFunction))(&ActiveMenu);
		}
	}

	free(PreviousGlobal);
	free(PreviousPerGame);

	// Avoid leaving the menu with GBA keys pressed (namely the one bound to
	// the native exit button, B).
	while (ReGBA_GetPressedButtons() != 0)
	{
		usleep(5000);
	}

	SetGameResolution();

	SDL_PauseAudio(SDL_DISABLE);
	StatsStopFPS();
	timespec Now;
	clock_gettime(CLOCK_MONOTONIC, &Now);
	Stats.LastFPSCalculationTime = Now;
	if (MainMenu.UserData != NULL)
		free(MainMenu.UserData);
	return 0;
}
