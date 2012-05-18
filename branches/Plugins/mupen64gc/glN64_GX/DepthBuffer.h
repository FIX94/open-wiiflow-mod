/**
 * glN64_GX - DepthBuffer.h
 * Copyright (C) 2003 Orkin
 *
 * glN64 homepage: http://gln64.emulation64.com
 * Wii64 homepage: http://www.emulatemii.com
 *
**/

#ifndef DEPTHBUFFER_H
#define DEPTHBUFFER_H

#include "Types.h"

struct DepthBuffer
{
	DepthBuffer *higher, *lower;

	u32 address, cleared;
};

struct DepthBufferInfo
{
	DepthBuffer *top, *bottom, *current;
	int numBuffers;
};

extern DepthBufferInfo depthBuffer;

void DepthBuffer_Init();
void DepthBuffer_Destroy();
void DepthBuffer_SetBuffer( u32 address );
void DepthBuffer_RemoveBuffer( u32 address );
DepthBuffer *DepthBuffer_FindBuffer( u32 address );

#endif
