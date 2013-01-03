
#include <ogc/machine/processor.h>
#include <ogc/lwp_threads.h>
#include <ogc/lwp_watchdog.h>
#include <stdio.h>
#include <ogcsys.h>
#include <stdlib.h>
#include <string.h>

#include "sha1.h"
#include "fs.h"
#include "sys.h"
#include "loader/nk.h"
#include "gecko/gecko.hpp"
#include "memory/mem2.hpp"
#include "memory/memory.h"
#include "wiiuse/wpad.h"

/* Variables */
bool reset = false;
bool shutdown = false;
volatile u8 ExitOption = 0;
const char *NeekPath = NULL;

void __Wpad_PowerCallback()
{
	shutdown = 1;
}

void Open_Inputs(void)
{
	/* Initialize Wiimote subsystem */
	PAD_Init();
	WPAD_Init();

	/* Set POWER button callback */
	WPAD_SetPowerButtonCallback(__Wpad_PowerCallback);
	
	WPAD_SetDataFormat(WPAD_CHAN_ALL, WPAD_FMT_BTNS_ACC_IR);
	
	WPAD_SetIdleTimeout(60 * 2); // idle after 2 minutes
}

void Close_Inputs(void)
{
	u32 cnt;

	/* Disconnect Wiimotes */
	for(cnt = 0; cnt < 4; cnt++)
		WPAD_Disconnect(cnt);

	/* Shutdown Wiimote subsystem */
	WPAD_Shutdown();
}

bool Sys_Exiting(void)
{
	DCFlushRange(&reset, 32);
	DCFlushRange(&shutdown, 32);
	if(reset || shutdown)
		ExitOption = BUTTON_CALLBACK;
	return (reset || shutdown);
}

void Sys_Exit(void)
{
	/* Shutdown Inputs */
	Close_Inputs();
	/* Just shutdown  console*/
	if(ExitOption == BUTTON_CALLBACK)
		SYS_ResetSystem(SYS_POWEROFF_STANDBY, 0, 0);
	/* We wanna to boot sth */
	WII_Initialize();
	WII_LaunchTitle(HBC_LULZ);
	WII_LaunchTitle(HBC_108);
	WII_LaunchTitle(HBC_JODI);
	WII_LaunchTitle(HBC_HAXX);
	/* else Return to Menu */
	SYS_ResetSystem(SYS_RETURNTOMENU, 0, 0);
	exit(1);
}

void __Sys_ResetCallback(void)
{
	reset = true;
}

void __Sys_PowerCallback(void)
{
	shutdown = true;
}

void Sys_Init(void)
{
	/* Set RESET/POWER button callback */
	SYS_SetResetCallback(__Sys_ResetCallback);
	SYS_SetPowerCallback(__Sys_PowerCallback);
}

bool AHBRPOT_Patched(void)
{
	return (*HW_AHBPROT == 0xFFFFFFFF);
}
