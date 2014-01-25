
#include <ogc/machine/processor.h>
#include <ogc/lwp_threads.h>
#include <ogc/lwp_watchdog.h>
#include <stdio.h>
#include <ogcsys.h>
#include <stdlib.h>
#include <string.h>
#include <wiiuse/wpad.h>

/* Variables */
bool reset = false;
bool shutdown = false;

void Input_Reset(void)
{
	reset = true;
}

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

bool Sys_Exiting(void)
{
	DCFlushRange(&reset, 32);
	DCFlushRange(&shutdown, 32);
	return reset || shutdown;
}


/* KILL IT */
s32 __IOS_LoadStartupIOS() { return 0; }
