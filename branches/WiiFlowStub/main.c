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

#define IOCTL_ES_LAUNCH					0x08
#define IOCTL_ES_GETVIEWCNT				0x12
#define IOCTL_ES_GETVIEWS				0x13

static struct ioctlv vecs[16] ALIGNED(64);

static int es_fd;

void memset32(u32 *addr, u32 data, u32 count) __attribute__ ((externally_visible));

void memset32(u32 *addr, u32 data, u32 count) 
{
	int sc = count;
	void *sa = addr;
	while(count--)
		*addr++ = data;
	sync_after_write(sa, 4*sc);
}

static int es_init(void)
{
	es_fd = ios_open("/dev/es", 0);
	return es_fd;
}

static int es_launchtitle(u64 titleID)
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

#define TITLE_ID(x,y) (((u64)(x) << 32) | (y))

void main (void)
{
	es_init();
	es_launchtitle(TITLE_ID(0x00010008,0x57494948));
}

