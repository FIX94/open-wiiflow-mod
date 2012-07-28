/*
	TinyLoad - a simple region free (original) game launcher in 4k

# This code is licensed to you under the terms of the GNU GPL, version 2;
# see file COPYING or http://www.gnu.org/licenses/old-licenses/gpl-2.0.txt
*/

/* This code comes from HBC's stub which was based on dhewg's geckoloader stub */
// Copyright 2008-2009  Andre Heider  <dhewg@wiibrew.org>
// Copyright 2008-2009  Hector Martin  <marcan@marcansoft.com>

#include "utils.h"
#include "ios.h"
#include "cache.h"
#include "usbgecko.h"
#include "memory.h"

#define IOCTL_ES_LAUNCH					0x08
#define IOCTL_ES_GETVIEWCNT				0x12
#define IOCTL_ES_GETVIEWS				0x13

#define TITLE_ID(x,y)					(((u64)(x) << 32) | (y))
#define TITLE_LOWER						0x00010008
#define TITLE_UPPER						0x57494948
#define SYSMENU_LOWER					0x00000001
#define SYSMENU_UPPER					0x00000002

static struct ioctlv vecs[16] ALIGNED(64);

static int es_fd;

static int
es_init(void)
{
	es_fd = ios_open("/dev/es", 0);
	return es_fd;
}

static int
es_launchtitle(u64 titleID)
{
	static u64 xtitleID __attribute__((aligned(32)));
	static u32 cntviews __attribute__((aligned(32)));
	static u8 tikviews[0xd8*4] __attribute__((aligned(32)));
	int ret;
	
	xtitleID = titleID;
	vecs[0].data = &xtitleID;
	vecs[0].len = 8;
	vecs[1].data = &cntviews;
	vecs[1].len = 4;
	ret = ios_ioctlv(es_fd, IOCTL_ES_GETVIEWCNT, 1, 1, vecs);
	if(ret<0) return ret;
	if(cntviews>4) return -1;

	vecs[2].data = tikviews;
	vecs[2].len = 0xd8*cntviews;
	ret = ios_ioctlv(es_fd, IOCTL_ES_GETVIEWS, 2, 1, vecs);
	if(ret<0) return ret;
	vecs[1].data = tikviews;
	vecs[1].len = 0xd8;
	ret = ios_ioctlvreboot(es_fd, IOCTL_ES_LAUNCH, 2, 0, vecs);
	return ret;
}

void
_main(void)
{
#if DEBUG
	usbgecko_init();
	usbgecko_printf("_main()\n");
#endif
	ios_cleanup();
#if DEBUG
	usbgecko_printf("ios_cleanup()\n");
#endif
	es_init();
#if DEBUG
	usbgecko_printf("es_init()\n");
#endif
	u64 ios_titleid = TITLE_ID(1, 58);
	es_launchtitle(ios_titleid);
#if DEBUG
	usbgecko_printf("es_launchtitle()\n");
#endif
	es_init();
#if DEBUG
	usbgecko_printf("es_init()\n");
#endif
	es_launchtitle(TITLE_ID(TITLE_LOWER, TITLE_UPPER));
#if DEBUG
	usbgecko_printf("es_launchtitle()\n");
#endif
	es_launchtitle(TITLE_ID(SYSMENU_LOWER,SYSMENU_UPPER));

	while (1);
}

#define SYSCALL_VECTOR	((u8*)0x80000C00)
void
__init_syscall()
{
	u8* sc_vector = SYSCALL_VECTOR;
	u32 bytes = (u32)DCFlashInvalidate - (u32)__temp_abe;
	u8* from = (u8*)__temp_abe;
	for ( ; bytes != 0 ; --bytes )
	{
		*sc_vector = *from;
		sc_vector++;
		from++;
	}

	sync_after_write(SYSCALL_VECTOR, 0x100);
	ICInvalidateRange(SYSCALL_VECTOR, 0x100);
}

