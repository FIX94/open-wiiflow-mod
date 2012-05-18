/*
	font functions	
*/

#include <gccore.h>
#include <malloc.h>
#include <stdio.h>
#include <stdlib.h>
#include <ogcsys.h>
#include <string.h>
#include "font.h"
#include <ogc/color.h>

/* Backdrop Frame Width (to avoid writing outside of the background frame) */
u16 back_framewidth = 640;

typedef struct
{
  unsigned short font_type, first_char, last_char, subst_char, ascent_units, descent_units, widest_char_width,
                 leading_space, cell_width, cell_height;
  unsigned long texture_size;
  unsigned short texture_format, texture_columns, texture_rows, texture_width, texture_height, offset_charwidth;
  unsigned long offset_tile, size_tile;
} FONT_HEADER;

static unsigned char fontFont[ 0x40000 ] __attribute__((aligned(32)));

/****************************************************************************
 * YAY0 Decoding
 ****************************************************************************/
/* Yay0 decompression */
void yay0_decode(unsigned char *s, unsigned char *d)
{
	int i, j, k, p, q, cnt;

	i = *(unsigned long *)(s + 4);	  // size of decoded data
	j = *(unsigned long *)(s + 8);	  // link table
	k = *(unsigned long *)(s + 12);	 // byte chunks and count modifiers

	q = 0;					// current offset in dest buffer
	cnt = 0;				// mask bit counter
	p = 16;					// current offset in mask table

	unsigned long r22 = 0, r5;
	
	do
	{
		// if all bits are done, get next mask
		if(cnt == 0)
		{
			// read word from mask data block
			r22 = *(unsigned long *)(s + p);
			p += 4;
			cnt = 32;   // bit counter
		}
		// if next bit is set, chunk is non-linked
		if(r22 & 0x80000000)
		{
			// get next byte
			*(unsigned char *)(d + q) = *(unsigned char *)(s + k);
			k++, q++;
		}
		// do copy, otherwise
		else
		{
			// read 16-bit from link table
			int r26 = *(unsigned short *)(s + j);
			j += 2;
			// 'offset'
			int r25 = q - (r26 & 0xfff);
			// 'count'
			int r30 = r26 >> 12;
			if(r30 == 0)
			{
				// get 'count' modifier
				r5 = *(unsigned char *)(s + k);
				k++;
				r30 = r5 + 18;
			}
			else r30 += 2;
			// do block copy
			unsigned char *pt = ((unsigned char*)d) + r25;
			int i;
			for(i=0; i<r30; i++)
			{
				*(unsigned char *)(d + q) = *(unsigned char *)(pt - 1);
				q++, pt++;
			}
		}
		// next bit in mask
		r22 <<= 1;
		cnt--;

	} while(q < i);
}

void untile(unsigned char *dst, unsigned char *src, int xres, int yres)
{
	// 8x8 tiles
	int x, y;
	int t=0;
	for (y = 0; y < yres; y += 8)
		for (x = 0; x < xres; x += 8)
		{
			t = !t;
			int iy, ix;
			for (iy = 0; iy < 8; ++iy, src+=2)
			{
				unsigned char *d = dst + (y + iy) * xres + x;
				for (ix = 0; ix < 2; ++ix)
				{
					int v = src[ix];
					*d++ = ((v>>6)&3);
					*d++ = ((v>>4)&3);
					*d++ = ((v>>2)&3);
					*d++ = ((v)&3);
				}
			}
		}
}

int font_offset[256], font_size[256], fheight;
extern void __SYS_ReadROM(void *buf,u32 len,u32 offset);

void init_font(void)
{
	static unsigned char fontWork[ 0x20000 ] __attribute__((aligned(32)));
	int i;

	// dont read system rom fonts because this breaks on qoob modchip
	__SYS_ReadROM((unsigned char *)&fontFont,0x3000,0x1FCF00);
	//memcpy(&fontFont, &arial, sizeof(arial));
	yay0_decode((unsigned char *)&fontFont, (unsigned char *)&fontWork);
	FONT_HEADER *fnt;

	fnt = ( FONT_HEADER * )&fontWork;

	untile((unsigned char*)&fontFont, (unsigned char*)&fontWork[fnt->offset_tile], fnt->texture_width, fnt->texture_height);

	for (i=0; i<256; ++i)
	{
		int c = i;

		if ((c < fnt->first_char) || (c > fnt->last_char)) c = fnt->subst_char;
		else c -= fnt->first_char;

		font_size[i] = ((unsigned char*)fnt)[fnt->offset_charwidth + c];

		int r = c / fnt->texture_columns;
		c %= fnt->texture_columns;
		font_offset[i] = (r * fnt->cell_height) * fnt->texture_width + (c * fnt->cell_width);
	}
	
	fheight = fnt->cell_height;
}

#define TRANSPARENCY (COLOR_WHITE)

unsigned int blit_lookup_inv[4] = {COLOR_WHITE, COLOR_WHITE, COLOR_WHITE, COLOR_WHITE};
unsigned int blit_lookup[4] = {COLOR_WHITE, COLOR_WHITE, COLOR_WHITE, COLOR_WHITE};
unsigned int blit_lookup_norm[4] = {COLOR_WHITE, COLOR_WHITE, COLOR_WHITE, COLOR_WHITE};

void blit_char(u32 **axfb,int whichfb,int x, int y, unsigned char c, unsigned int *lookup)
{
	unsigned char *fnt = ((unsigned char*)fontFont) + font_offset[c];
	int ay, ax;
	unsigned int llookup;

	for (ay=0; ay<fheight; ++ay)
	{
		int h = (ay + y) * 320;

		for (ax=0; ax<font_size[c]; ax++)
		{
			int v0 = fnt[ax];
			int p = h + (( ax + x ) >> 1);
			unsigned long o = axfb[whichfb][p];

			llookup = lookup[v0];

			if ((v0== 0) && (llookup == TRANSPARENCY))
				llookup = o;

			if ((ax+x) & 1)
			{
				o &= ~0x00FFFFFF;
				o |= llookup & 0x00FFFFFF;
			}
			else
			{
				o &= ~0xFF000000;
				o |= llookup & 0xFF000000;
			}

			axfb[whichfb][p] = o;
		}
		
		fnt += 512;
	}
}

u8 norm_blit = 1;

void write_font(int x, int y, char *string,u32 **axfb,int whichfb)
{
	int ox = x;
	while (*string && (x < (ox + back_framewidth)))
	{
		blit_char(axfb,whichfb,x, y, *string, norm_blit ? blit_lookup_norm : blit_lookup);
		x += font_size[*string];
		string++;
	}
}
