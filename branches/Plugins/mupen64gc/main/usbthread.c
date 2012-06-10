
//============================================================================
// Name        : usbthread.c
// Copyright   : 2012 FIX94
//============================================================================

#include <ogc/usbstorage.h>
#include <ogc/cache.h>
#include <ogc/lwp.h>
#include <unistd.h>

#include "usbthread.h"

lwp_t USB_Thread = LWP_THREAD_NULL;
bool CheckUSB = false;
int USB_Thread_Active = 0;

void *KeepUSBAlive(void *nothing)
{
	USB_Thread_Active = 1;
	DCFlushRange(&USB_Thread_Active, sizeof(USB_Thread_Active));
	time_t start = time(0);
	u8 sector[4096];
	while(CheckUSB)
	{
		if(time(0) - start > 29 && __io_usbstorage.isInserted())
		{
			start = time(0);
			__io_usbstorage.readSectors(0, 1, sector);
			__io_usbstorage.readSectors(1, 1, sector);
		}
		usleep(1000);
	}
	USB_Thread_Active = 0;
	DCFlushRange(&USB_Thread_Active, sizeof(USB_Thread_Active));
	return nothing;
}

void CreateUSBKeepAliveThread()
{
	CheckUSB = true;
	LWP_CreateThread(&USB_Thread, KeepUSBAlive, NULL, NULL, 0, LWP_PRIO_IDLE);
}

void KillUSBKeepAliveThread()
{
	CheckUSB = false;
	while(USB_Thread_Active)
		usleep(100);
	if(USB_Thread != LWP_THREAD_NULL)
	{
		LWP_JoinThread(USB_Thread, NULL);
		USB_Thread = LWP_THREAD_NULL;
	}
}
