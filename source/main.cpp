/****************************************************************************
 * Copyright (C) 2014 FIX94
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 * 
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 ****************************************************************************/
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <gccore.h>
#include <unistd.h>
#include <wiiuse/wpad.h>
#include "nand_save.hpp"
#include "cios.hpp"
#include "identify.h"
#include "sys.h"

#define HW_AHBPROT		((vu32*)0xCD800064)

void InitVideo()
{
	VIDEO_Init();
	GXRModeObj *rmode = VIDEO_GetPreferredMode(NULL);
	void *xfb = MEM_K0_TO_K1(SYS_AllocateFramebuffer(rmode));
	VIDEO_Configure(rmode);
	VIDEO_SetNextFramebuffer(xfb);
	VIDEO_SetBlack(FALSE);
	VIDEO_Flush();
	VIDEO_WaitVSync();
	if(rmode->viTVMode&VI_NON_INTERLACE)
		VIDEO_WaitVSync();
	CON_InitEx(rmode, 24, 32, rmode->fbWidth - (32), rmode->xfbHeight - (48));
	VIDEO_ClearFrameBuffer(rmode, xfb, COLOR_BLACK);
}

void MainLoop()
{
	VIDEO_WaitVSync();
	printf("\x1b[2J");
	printf("\x1b[37m");
	printf("WiiFlow Start Configurator v1.0 by FIX94\n");
	WPAD_ScanPads();
	PAD_ScanPads();
}

static void quit()
{
	printf("Returning to Loader...\n");
	VIDEO_WaitVSync();

	ISFS_Deinitialize();
	exit(0);
}

int select_pos = 0;
static void select(int pos)
{
	if(pos == select_pos)
		printf("\x1b[33m");
	else
		printf("\x1b[37m");
}

int main()
{
	InitVideo();
	printf("Running on IOS%u v%u\n", IOS_GetVersion(), IOS_GetRevision());
	if(*HW_AHBPROT != 0xFFFFFFFF)
	{
		printf("ERROR: HW_AHBPROT is enabled, please make sure you start this program with the latest Homebrew Channel.\n");
		quit();
	}
	if(PatchIOS(true) == 0)
		printf("WARNING: No IOS Patches applied, please make sure you have the latest IOS58 installed!\n");

	usleep(1000);
	ISFS_Initialize();

	if(InternalSave.CheckSave() == false)
	{
		printf("You need to install the latest IOS58 version and the latest Homebrew Channel in order to use WiiFlow properly.\n");
		quit();
	}

	Sys_Init();
	Open_Inputs();
	loadIOSlist();
	InternalSave.LoadSettings();

	bool new_load = cur_load;
	u8 new_ios = cur_ios;
	u8 new_port = cur_port;

	while(!Sys_Exiting())
	{
		MainLoop();
		if((WPAD_ButtonsDown(0) == WPAD_BUTTON_HOME) || (PAD_ButtonsDown(0) == PAD_BUTTON_START))
			Input_Reset();
		/* selected option */
		if((WPAD_ButtonsDown(0) == WPAD_BUTTON_UP) || (PAD_ButtonsDown(0) == PAD_BUTTON_UP))
			select_pos--;
		if((WPAD_ButtonsDown(0) == WPAD_BUTTON_DOWN) || (PAD_ButtonsDown(0) == PAD_BUTTON_DOWN))
			select_pos++;
		if(select_pos < 0)
			select_pos = 2;
		else if(select_pos > 2)
			select_pos = 0;
		/* option changer */
		if((WPAD_ButtonsDown(0) == WPAD_BUTTON_RIGHT) || (PAD_ButtonsDown(0) == PAD_BUTTON_RIGHT))
		{
			if(select_pos == 0)
				new_load = !new_load;
			else if(select_pos == 1)
				nextIOS(&new_ios);
			else if(select_pos == 2)
				new_port = (new_port == 0) ? 1 : 0;
		}
		if((WPAD_ButtonsDown(0) == WPAD_BUTTON_LEFT) || (PAD_ButtonsDown(0) == PAD_BUTTON_LEFT))
		{
			if(select_pos == 0)
				cur_load = !cur_load;
			else if(select_pos == 1)
				prevIOS(&new_ios);
			else if(select_pos == 2)
				new_port = (new_port == 0) ? 1 : 0;
		}

		select(0);
		printf("Force cIOS Mode: <<< %s >>>\n", (new_load ? "yes" : "no"));
		select(1);
		if(new_ios == 0)
			printf("Force cIOS: <<< AUTO >>>\n");
		else
			printf("Force cIOS: <<< %u >>>\n", new_ios);
		select(2);
		printf("HDD Port: <<< %d >>>\n", new_port);
	}

	Close_Inputs();
	printf("\x1b[37m");
	printf(" \nDone!\n");
	if(new_load != cur_load || new_ios != cur_ios)
	{
		cur_load = new_load;
		cur_ios = new_ios;
		InternalSave.SaveIOS();
	}
	if(new_port != cur_port)
	{
		cur_port = new_port;
		InternalSave.SavePort();
	}
	quit();
}