/**
 * glN64_GX - gDP.h
 * Copyright (C) 2003 Orkin
 * Copyright (C) 2008, 2009 sepp256 (Port to Wii/Gamecube/PS3)
 *
 * glN64 homepage: http://gln64.emulation64.com
 * Wii64 homepage: http://www.emulatemii.com
 * email address: sepp256@gmail.com
 *
**/

#ifndef GDP_H
#define GDP_H

#include "Types.h"
#include "FrameBuffer.h"

#define CHANGED_RENDERMODE		0x001
#define CHANGED_CYCLETYPE		0x002
#define CHANGED_SCISSOR			0x004
#define CHANGED_TMEM			0x008
#define CHANGED_TILE			0x010
#define CHANGED_COMBINE_COLORS	0x020
#define CHANGED_COMBINE			0x040
#define CHANGED_ALPHACOMPARE	0x080
#define CHANGED_FOGCOLOR		0x100

#define TEXTUREMODE_NORMAL		0
#define TEXTUREMODE_TEXRECT		1
#define TEXTUREMODE_BGIMAGE		2
#define TEXTUREMODE_FRAMEBUFFER	3

#define LOADTYPE_BLOCK			0
#define LOADTYPE_TILE			1

struct gDPCombine
{
	union
	{
		struct
		{
#ifndef _BIG_ENDIAN
			// muxs1
			unsigned	aA1		: 3;
			unsigned	sbA1	: 3;
			unsigned	aRGB1	: 3;
			unsigned	aA0		: 3;
			unsigned	sbA0	: 3;
			unsigned	aRGB0	: 3;
			unsigned	mA1		: 3;
			unsigned	saA1	: 3;
			unsigned	sbRGB1	: 4;
			unsigned	sbRGB0	: 4;

			// muxs0
			unsigned	mRGB1	: 5;
			unsigned	saRGB1	: 4;
			unsigned	mA0		: 3;
			unsigned	saA0	: 3;
			unsigned	mRGB0	: 5;
			unsigned	saRGB0	: 4;
			unsigned	pad0	: 8;
#else // !_BIG_ENDIAN -> This should fix for BE.
			// muxs0
			unsigned	pad0	: 8;
			unsigned	saRGB0	: 4;
			unsigned	mRGB0	: 5;
			unsigned	saA0	: 3;
			unsigned	mA0		: 3;
			unsigned	saRGB1	: 4;
			unsigned	mRGB1	: 5;

			// muxs1
			unsigned	sbRGB0	: 4;
			unsigned	sbRGB1	: 4;
			unsigned	saA1	: 3;
			unsigned	mA1		: 3;
			unsigned	aRGB0	: 3;
			unsigned	sbA0	: 3;
			unsigned	aA0		: 3;
			unsigned	aRGB1	: 3;
			unsigned	sbA1	: 3;
			unsigned	aA1		: 3;
#endif // _BIG_ENDIAN
		};

		struct
		{
#ifndef _BIG_ENDIAN
			u32			muxs1, muxs0;
#else // !_BIG_ENDIAN -> This should fix for BE.
			u32			muxs0, muxs1;
#endif // _BIG_ENDIAN
		};

		u64				mux;
	};
};

struct gDPTile
{
	u32 format, size, line, tmem, palette;

	union
	{
		struct
		{
#ifndef _BIG_ENDIAN
			unsigned	mirrort	: 1;
			unsigned	clampt	: 1;
			unsigned	pad0	: 30;

			unsigned	mirrors	: 1;
			unsigned	clamps	: 1;
			unsigned	pad1	: 30;
#else // !_BIG_ENDIAN -> Big Endian fix.
			unsigned	pad1	: 30;
			unsigned	clamps	: 1;
			unsigned	mirrors	: 1;

			unsigned	pad0	: 30;
			unsigned	clampt	: 1;
			unsigned	mirrort	: 1;
#endif // _BIG_ENDIAN
		};

		struct
		{
#ifndef _BIG_ENDIAN
			u32 cmt, cms;
#else // !_BIG_ENDIAN -> Big Endian fix.
			u32 cms, cmt;
#endif // _BIG_ENDIAN
		};
	};

	FrameBuffer *frameBuffer;
	u32 maskt, masks;
	u32 shiftt, shifts;
	f32 fuls, fult, flrs, flrt;
	u32 uls, ult, lrs, lrt;
};

struct gDPInfo
{
	struct
	{
		union
		{
			struct
			{
#ifndef _BIG_ENDIAN
				unsigned int alphaCompare : 2;
				unsigned int depthSource : 1;

//				struct
//				{
					unsigned int AAEnable : 1;
					unsigned int depthCompare : 1;
					unsigned int depthUpdate : 1;
					unsigned int imageRead : 1;
					unsigned int clearOnCvg : 1;

					unsigned int cvgDest : 2;
					unsigned int depthMode : 2;

					unsigned int cvgXAlpha : 1;
					unsigned int alphaCvgSel : 1;
					unsigned int forceBlender : 1;
					unsigned int textureEdge : 1;
//				} renderMode;

//				struct
//				{
					unsigned int c2_m2b : 2;
					unsigned int c1_m2b : 2;
					unsigned int c2_m2a : 2;
					unsigned int c1_m2a : 2;
					unsigned int c2_m1b : 2;
					unsigned int c1_m1b : 2;
					unsigned int c2_m1a : 2;
					unsigned int c1_m1a : 2;
//				} blender;

				unsigned int blendMask : 4;
				unsigned int alphaDither : 2;
				unsigned int colorDither : 2;
				
				unsigned int combineKey : 1;
				unsigned int textureConvert : 3;
				unsigned int textureFilter : 2;
				unsigned int textureLUT : 2;

				unsigned int textureLOD : 1;
				unsigned int textureDetail : 2;
				unsigned int texturePersp : 1;
				unsigned int cycleType : 2;
				unsigned int unusedColorDither : 1; // unsupported
				unsigned int pipelineMode : 1;

				unsigned int pad : 8;
#else // !_BIG_ENDIAN - Big Endian fix.
				unsigned int pad : 8;

				unsigned int pipelineMode : 1;
				unsigned int unusedColorDither : 1; // unsupported
				unsigned int cycleType : 2;
				unsigned int texturePersp : 1;
				unsigned int textureDetail : 2;
				unsigned int textureLOD : 1;

				unsigned int textureLUT : 2;
				unsigned int textureFilter : 2;
				unsigned int textureConvert : 3;
				unsigned int combineKey : 1;

				unsigned int colorDither : 2;
				unsigned int alphaDither : 2;
				unsigned int blendMask : 4;
				
//				struct
//				{
					unsigned int c1_m1a : 2;
					unsigned int c2_m1a : 2;
					unsigned int c1_m1b : 2;
					unsigned int c2_m1b : 2;
					unsigned int c1_m2a : 2;
					unsigned int c2_m2a : 2;
					unsigned int c1_m2b : 2;
					unsigned int c2_m2b : 2;
//				} blender;

//				struct
//				{
					unsigned int textureEdge : 1;
					unsigned int forceBlender : 1;
					unsigned int alphaCvgSel : 1;
					unsigned int cvgXAlpha : 1;

					unsigned int depthMode : 2;
					unsigned int cvgDest : 2;

					unsigned int clearOnCvg : 1;
					unsigned int imageRead : 1;
					unsigned int depthUpdate : 1;
					unsigned int depthCompare : 1;
					unsigned int AAEnable : 1;
//				} renderMode;

				unsigned int depthSource : 1;
				unsigned int alphaCompare : 2;
#endif // _BIG_ENDIAN
			};

			u64			_u64;

			struct
			{
#ifndef _BIG_ENDIAN
				u32			l, h;
#else // !_BIG_ENDIAN - Big Endian fix.
				u32			h, l;
#endif // _BIG_ENDIAN
			};
		};
	} otherMode;

	gDPCombine combine;

	gDPTile tiles[8], *loadTile;

	struct
	{
		f32 r, g, b, a;
	} fogColor,  blendColor, envColor;

	struct
	{
		f32 r, g, b, a;
		f32 z, dz;
	} fillColor;

	struct
	{
		u32 m;
		f32 l, r, g, b, a;
	} primColor;

	struct
	{
		f32 z, deltaZ;
	} primDepth;

	struct
	{
		u32 format, size, width, bpl;
		u32 address;
	} textureImage;

	struct
	{
		u32 format, size, width, height, bpl;
		u32 address, changed;
		u32 depthImage;
	} colorImage;

	u32	depthImageAddress;

	struct
	{
		u32 mode;
		f32 ulx, uly, lrx, lry;
	} scissor;

	struct
	{
		s32 k0, k1, k2, k3, k4, k5;
	} convert;

	struct
	{
		struct
		{
			f32 r, g, b, a;
		} center, scale, width;
	} key;

	struct
	{
		u32 width, height;
	} texRect;

	u32 changed;

	//u16 palette[256];
	u32 paletteCRC16[16];
	u32 paletteCRC256;
	u32 half_1, half_2;
	u32 textureMode;
	u32 loadType;
};

extern gDPInfo gDP;

void gDPSetOtherMode( u32 mode0, u32 mode1 );
void gDPSetPrimDepth( u16 z, u16 dz );
void gDPPipelineMode( u32 mode );
void gDPSetCycleType( u32 type );
void gDPSetTexturePersp( u32 enable );
void gDPSetTextureDetail( u32 type );
void gDPSetTextureLOD( u32 mode );
void gDPSetTextureLUT( u32 mode );
void gDPSetTextureFilter( u32 type );
void gDPSetTextureConvert( u32 type );
void gDPSetCombineKey( u32 type );
void gDPSetColorDither( u32 type );
void gDPSetAlphaDither( u32 type );
void gDPSetAlphaCompare( u32 mode );
void gDPSetDepthSource( u32 source );
void gDPSetRenderMode( u32 mode1, u32 mode2 );
void gDPSetCombine( s32 muxs0, s32 muxs1 );
void gDPSetColorImage( u32 format, u32 size, u32 width, u32 address );
void gDPSetTextureImage( u32 format, u32 size, u32 width, u32 address );
void gDPSetDepthImage( u32 address );
void gDPSetEnvColor( u32 r, u32 g, u32 b, u32 a );
void gDPSetBlendColor( u32 r, u32 g, u32 b, u32 a );
void gDPSetFogColor( u32 r, u32 g, u32 b, u32 a );
void gDPSetFillColor( u32 c );
void gDPSetPrimColor( u32 m, u32 l, u32 r, u32 g, u32 b, u32 a );
void gDPSetTile( u32 format, u32 size, u32 line, u32 tmem, u32 tile, u32 palette, u32 cmt, u32 cms, u32 maskt, u32 masks, u32 shiftt, u32 shifts );
void gDPSetTileSize( u32 tile, u32 uls, u32 ult, u32 lrs, u32 lrt );
void gDPLoadTile( u32 tile, u32 uls, u32 ult, u32 lrs, u32 lrt );
void gDPLoadBlock( u32 tile, u32 uls, u32 ult, u32 lrs, u32 dxt );
void gDPLoadTLUT( u32 tile, u32 uls, u32 ult, u32 lrs, u32 lrt );
void gDPSetScissor( u32 mode, f32 ulx, f32 uly, f32 lrx, f32 lry );
void gDPFillRectangle( s32 ulx, s32 uly, s32 lrx, s32 lry );
void gDPSetConvert( s32 k0, s32 k1, s32 k2, s32 k3, s32 k4, s32 k5 );
void gDPSetKeyR( u32 cR, u32 sR, u32 wR );
void gDPSetKeyGB(u32 cG, u32 sG, u32 wG, u32 cB, u32 sB, u32 wB );
void gDPTextureRectangle( f32 ulx, f32 uly, f32 lrx, f32 lry, s32 tile, f32 s, f32 t, f32 dsdx, f32 dtdy );
void gDPTextureRectangleFlip( f32 ulx, f32 uly, f32 lrx, f32 lry, s32 tile, f32 s, f32 t, f32 dsdx, f32 dtdy );
void gDPFullSync();
void gDPTileSync();
void gDPPipeSync();
void gDPLoadSync();
void gDPNoOp();

#endif

