/***************************************************************************
 * Copyright (C) 2011
 * by Dimok
 * Modifications by xFede
 * Wiiflowized and heavily improved by Miigotu
 * 2012 Converted, corrected and extended by FIX94
 *
 * This software is provided 'as-is', without any express or implied
 * warranty. In no event will the authors be held liable for any
 * damages arising from the use of this software.
 *
 * Permission is granted to anyone to use this software for any
 * purpose, including commercial applications, and to alter it and
 * redistribute it freely, subject to the following restrictions:
 *
 * 1. The origin of this software must not be misrepresented; you
 * must not claim that you wrote the original software. If you use
 * this software in a product, an acknowledgment in the product
 * documentation would be appreciated but is not required.
 *
 * 2. Altered source versions must be plainly marked as such, and
 * must not be misrepresented as being the original software.
 *
 * 3. This notice may not be removed or altered from any source
 * distribution.
 ***************************************************************************/
#include <malloc.h>
#include <gccore.h>
#include <string.h>
#include <stdio.h>
#include <map>
#include "cios.hpp"
#include "fs.h"

#define TITLE_ID(x,y)		(((u64)(x) << 32) | (y))
signed_blob *GetTMD(u8 ios, u32 *TMD_Length)
{
	if(ES_GetStoredTMDSize(TITLE_ID(1, ios), TMD_Length) < 0)
		return NULL;

	signed_blob *TMD = (signed_blob*)memalign(32, *TMD_Length);
	if(TMD == NULL)
		return NULL;
	if(ES_GetStoredTMD(TITLE_ID(1, ios), TMD, *TMD_Length) < 0)
	{
		free(TMD);
		return NULL;
	}
	return TMD;
}

/*
 * Reads the ios info struct from the .app file.
 * @return pointer to iosinfo_t on success else NULL. The user is responsible for freeing the buffer.
 */

iosinfo_t *IOS_GetInfo(u8 ios)
{
	u32 TMD_Length;
	signed_blob *TMD = GetTMD(ios, &TMD_Length);
	if(TMD == NULL)
		return NULL;
	char filepath[ISFS_MAXPATH] ATTRIBUTE_ALIGN(32);
	sprintf(filepath, "/title/00000001/%08x/content/%08x.app", ios, *(u8 *)((u32)TMD+0x1E7));
	free(TMD);

	u32 size = 0;
	u8 *buffer = ISFS_GetFile(filepath, &size, sizeof(iosinfo_t));
	if(buffer == NULL || size == 0)
		return NULL;

	iosinfo_t *iosinfo = (iosinfo_t *)buffer;
	return iosinfo;
}

/* Check if the cIOS is a D2X. */
bool IOS_D2X(u8 ios, u8 *base)
{
	iosinfo_t *info = IOS_GetInfo(ios);
	if(!info)
		return false;

	if(info->magicword != 0x1ee7c105					/* Magic Word */
		|| info->magicversion != 1						/* Magic Version */
		|| info->version < 6							/* Version */
		|| strncasecmp(info->name, "d2x", 3) != 0)		/* Name */
	{
		free(info);
		return false;
	}

	*base = (u8)info->baseios;
	free(info);
	return true;
}

u32 Title_GetSize_FromTMD(tmd *tmd_data)
{
	u32 cnt, size = 0;

	/* Calculate title size */
	for(cnt = 0; cnt < tmd_data->num_contents; cnt++)
	{
		tmd_content *content = &tmd_data->contents[cnt];
		/* Add content size */
		size += content->size;
	}

	return size;
}

u8 IOS_GetType(u8 slot)
{
	/* Lets do this */
	u32 TMD_Length;
	signed_blob *TMD_Buffer = GetTMD(slot, &TMD_Length);
	if(TMD_Buffer == NULL)
		return IOS_TYPE_STUB;

	tmd *iosTMD = (tmd*)SIGNATURE_PAYLOAD(TMD_Buffer);
	if(Title_GetSize_FromTMD(iosTMD) < 0x100000 || iosTMD->title_version == 65280)
	{
		free(TMD_Buffer);
		return IOS_TYPE_STUB;
	}
	u32 title_rev = iosTMD->title_version;
	free(TMD_Buffer);

	u8 base = 0;
	switch(slot)
	{
		case 222:
		case 223:
		case 224:
		case 225:
			if(title_rev == 1)
				return IOS_TYPE_KWIIRK;
			else
				return IOS_TYPE_HERMES;
		case 245:
		case 246:
		case 247:
		case 248:
		case 249:
		case 250:
		case 251:
			if(IOS_D2X(slot, &base))
				return IOS_TYPE_D2X;
			else
				return IOS_TYPE_WANIN;
		default:
			if(IOS_D2X(slot, &base))
				return IOS_TYPE_D2X;
			else
				return IOS_TYPE_NORMAL_IOS;
	}
}

/* code from wiiflow so we use the same cIOS map */
std::map<u8, u8> _installed_cios;
void loadIOSlist()
{
	_installed_cios.clear();
	_installed_cios[0] = 1; //auto
	for(u8 slot = 200; slot < 254; slot++)
	{
		u8 base = 1;
		if(IOS_D2X(slot, &base))
		{
			printf("Found d2x base %u in slot %u\n", base, slot);
			_installed_cios[slot] = base;
		}
		else if(CustomIOS(IOS_GetType(slot)))
		{
			printf("Found cIOS in slot %u\n", slot);
			_installed_cios[slot] = slot;
		}
	}
}

std::map<u8, u8>::iterator itr;
void nextIOS(u8 *current)
{
	itr = _installed_cios.find(*current);

	if(itr != _installed_cios.end())
		itr++;

	if(itr == _installed_cios.end())
		itr = _installed_cios.begin();

	*current = itr->first;
}

void prevIOS(u8 *current)
{
	itr = _installed_cios.find(*current);

	if(itr == _installed_cios.begin())
		itr = _installed_cios.end();
	itr--;

	*current = itr->first;
}
