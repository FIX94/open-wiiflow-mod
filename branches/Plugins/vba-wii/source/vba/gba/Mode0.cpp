/*
Mode 0 is the tiled graphics mode, with all the layers available.
There is no rotation or scaling in this mode.
It can be either 16 colours (with 16 different palettes) or 256 colors.
There are 1024 tiles available.

These routines only render a single line at a time, because of the way the GBA does events.
*/
#include "GBA.h"
#include "Globals.h"
#include "GBAGfx.h"

void mode0RenderLine()
{
  u16 *palette = (u16 *)paletteRAM;

  if(DISPCNT & 0x80) {
	int x = 232;	//240 - 8  
	do{
		lineMix[x  ] =
		lineMix[x+1] =
		lineMix[x+2] =
		lineMix[x+3] =
		lineMix[x+4] =
		lineMix[x+5] =
		lineMix[x+6] =
		lineMix[x+7] = 0x7fff;
		x-=8;
	}while(x>=0);
    return;
  }

  if(layerEnable & 0x0100) {
    gfxDrawTextScreen(BG0CNT, BG0HOFS, BG0VOFS, line0);
  }

  if(layerEnable & 0x0200) {
    gfxDrawTextScreen(BG1CNT, BG1HOFS, BG1VOFS, line1);
  }

  if(layerEnable & 0x0400) {
    gfxDrawTextScreen(BG2CNT, BG2HOFS, BG2VOFS, line2);
  }

  if(layerEnable & 0x0800) {
    gfxDrawTextScreen(BG3CNT, BG3HOFS, BG3VOFS, line3);
  }

  gfxDrawSprites(lineOBJ);

  u32 backdrop;
  if(customBackdropColor == -1) {
    backdrop = (READ16LE(&palette[0]) | 0x30000000);
  } else {
    backdrop = ((customBackdropColor & 0x7FFF) | 0x30000000);
  }

  for(u32 x = 0; x < 240u; ++x) {
    u32 color = backdrop;
    u8 top = 0x20;
	//--DCN
	//
	// !NON-PORTABLE!!NON-PORTABLE!
	//
	// This takes advantage of the fact that the Wii has far more registers 
	// (32 vs 8) than IA-32 based processors processors (Intel, AMD). 
	// This actually runs SLOWER on those. This code will only show 
	// improvements on a PowerPC machine! (19.5% improvement: isolated tests)
	//*
	u8 li1 = (u8)(line1[x]>>24);
	u8 li2 = (u8)(line2[x]>>24);
	u8 li3 = (u8)(line3[x]>>24);
	u8 li4 = (u8)(lineOBJ[x]>>24);	
	
	u8 r = 	(li2 < li1) ? (li2) : (li1);
	
	if(li3 < r) {
		r = (li4 < li3) ? (li4) : (li3);
	}else if(li4 < r){
		r = (li4);
	}
	
	if(line0[x] < backdrop) {
	  color = line0[x];
	  top = 0x01;
	}
	
	if(r < (u8)(color >> 24)) {
		if(r == li1){
			color = line1[x];
			top = 0x02;
		}else if(r == li2){
			color = line2[x];
			top = 0x04;
		}else if(r == li3){
			color = line3[x];
			top = 0x08;
		}else if(r == li4){
			color = lineOBJ[x];
			top = 0x10;
		}
	}	
	
	//Original
	/*
	if(line0[x] < color) {
      color = line0[x];
      top = 0x01;
    }

    if((u8)(line1[x]>>24) < (u8)(color >> 24)) {
      color = line1[x];
      top = 0x02;
    }

    if((u8)(line2[x]>>24) < (u8)(color >> 24)) {
      color = line2[x];
      top = 0x04;
    }
	if((u8)(line3[x]>>24) < (u8)(color >> 24)) {
      color = line3[x];
      top = 0x08;
    }
    if((u8)(lineOBJ[x]>>24) < (u8)(color >> 24)) {
      color = lineOBJ[x];
      top = 0x10;
    }
	//*/

    if((top & 0x10) && (color & 0x00010000)) {
      // semi-transparent OBJ
      u32 back = backdrop;
      u8 top2 = 0x20;

		u8 li0 = (u8)(line0[x]>>24);
		u8 li1 = (u8)(line1[x]>>24);
		u8 li2 = (u8)(line2[x]>>24);
		u8 li3 = (u8)(line3[x]>>24);	
		
		u8 r = 	(li1 < li0) ? (li1) : (li0);
		
		if(li2 < r) {
			r = (li3 < li2) ? (li3) : (li2);
		}else if(li3 < r){
			r = 	(li3);
		}
		
		if(r < (u8)(color >> 24)) {
			if(r == li0){
				back = line0[x];
				top2 = 0x01;
			}else if(r == li1){
				back = line1[x];
				top2 = 0x02;
			}else if(r == li2){
				back = line2[x];
				top2 = 0x04;
			}else if(r == li3){
				back = line3[x];
				top2 = 0x08;
			}
		}

      if(top2 & (BLDMOD>>8))
        color = gfxAlphaBlend(color, back,
                              coeff[COLEV & 0x1F],
                              coeff[(COLEV >> 8) & 0x1F]);
      else {
        switch((BLDMOD >> 6) & 3) {
        case 2:
          if(BLDMOD & top)
            color = gfxIncreaseBrightness(color, coeff[COLY & 0x1F]);
          break;
        case 3:
          if(BLDMOD & top)
            color = gfxDecreaseBrightness(color, coeff[COLY & 0x1F]);
          break;
        }
      }
    }

    lineMix[x] = color;
  }
}

void mode0RenderLineNoWindow()
{
  u16 *palette = (u16 *)paletteRAM;

  if(DISPCNT & 0x80) {

	int x = 232;	//240 -  8  
	do{
		lineMix[x  ] =
		lineMix[x+1] =
		lineMix[x+2] =
		lineMix[x+3] =
		lineMix[x+4] =
		lineMix[x+5] =
		lineMix[x+6] =
		lineMix[x+7] = 0x7fff;
		x-=8;
	}while(x>=0);

    return;
  }

  if(layerEnable & 0x0100) {
    gfxDrawTextScreen(BG0CNT, BG0HOFS, BG0VOFS, line0);
  }

  if(layerEnable & 0x0200) {
    gfxDrawTextScreen(BG1CNT, BG1HOFS, BG1VOFS, line1);
  }

  if(layerEnable & 0x0400) {
    gfxDrawTextScreen(BG2CNT, BG2HOFS, BG2VOFS, line2);
  }

  if(layerEnable & 0x0800) {
    gfxDrawTextScreen(BG3CNT, BG3HOFS, BG3VOFS, line3);
  }

  gfxDrawSprites(lineOBJ);

  u32 backdrop;
  if(customBackdropColor == -1) {
    backdrop = (READ16LE(&palette[0]) | 0x30000000);
  } else {
    backdrop = ((customBackdropColor & 0x7FFF) | 0x30000000);
  }

  int effect = (BLDMOD >> 6) & 3;

  for(int x = 0; x < 240; x++) {
    u32 color = backdrop;
	u8 top = 0x20;

	u8 li1 = (u8)(line1[x]>>24);
	u8 li2 = (u8)(line2[x]>>24);
	u8 li3 = (u8)(line3[x]>>24);
	u8 li4 = (u8)(lineOBJ[x]>>24);	
	
	u8 r = 	(li2 < li1) ? (li2) : (li1);
	
	if(li3 < r) {
		r = (li4 < li3) ? (li4) : (li3);
	}else if(li4 < r){
		r = (li4);
	}
	
	if(line0[x] < backdrop) {
	  color = line0[x];
	  top = 0x01;
	}
	
	if(r < (u8)(color >> 24)) {
		if(r == li1){
			color = line1[x];
			top = 0x02;
		}else if(r == li2){
			color = line2[x];
			top = 0x04;
		}else if(r == li3){
			color = line3[x];
			top = 0x08;
		}else if(r == li4){
			color = lineOBJ[x];
			top = 0x10;
		}
	}	

    if(!(color & 0x00010000)) {
      switch(effect) {
      case 0:
        break;
      case 1:
        {
          if(top & BLDMOD) {
            u32 back = backdrop;
			u8 top2 = 0x20;

            if((top != 0x01) && line0[x] < back) {
                back = line0[x];
                top2 = 0x01;
            }
            if((top != 0x02) && line1[x] < (back & 0xFF000000)) {
                back = line1[x];
                top2 = 0x02;
            }

            if((top != 0x04) && line2[x] < (back & 0xFF000000)) {
                back = line2[x];
                top2 = 0x04;
            }

            if((top != 0x08) && line3[x] < (back & 0xFF000000)) {
                back = line3[x];
                top2 = 0x08;
            }

            if((top != 0x10) && lineOBJ[x] < (back & 0xFF000000)) {
                back = lineOBJ[x];
                top2 = 0x10;
            }

            if(top2 & (BLDMOD>>8))
              color = gfxAlphaBlend(color, back,
                                    coeff[COLEV & 0x1F],
                                    coeff[(COLEV >> 8) & 0x1F]);

          }
        }
        break;
      case 2:
        if(BLDMOD & top)
          color = gfxIncreaseBrightness(color, coeff[COLY & 0x1F]);
        break;
      case 3:
        if(BLDMOD & top)
          color = gfxDecreaseBrightness(color, coeff[COLY & 0x1F]);
        break;
      }
    } else {
      // semi-transparent OBJ
      u32 back = backdrop;
      u8 top2 = 0x20; 

	  //--DCN	  
	  // This is pretty much the exact same result:
	  //	line1[x] < (back & 0xFF000000)
	  //	
	  //	(u8)(line0[x]>>24) < (u8)(back >> 24) 
	  //
	  // The only difference is that the first is stored in a u32,
	  // and the second is stored in a u8
	  //*
		u8 li0 = (u8)(line0[x]>>24);
		u8 li1 = (u8)(line1[x]>>24);
		u8 li2 = (u8)(line2[x]>>24);
		u8 li3 = (u8)(line3[x]>>24);	
		
		u8 r = 	(li1 < li0) ? (li1) : (li0);
		
		if(li2 < r) {
			r = (li3 < li2) ? (li3) : (li2);
		}else if(li3 < r){
			r = 	(li3);
		}
		
		if(r < (u8)(color >> 24)) {
			if(r == li0){
				back = line0[x];
				top2 = 0x01;
			}else if(r == li1){
				back = line1[x];
				top2 = 0x02;
			}else if(r == li2){
				back = line2[x];
				top2 = 0x04;
			}else if(r == li3){
				back = line3[x];
				top2 = 0x08;
			}
		}

      if(top2 & (BLDMOD>>8))
        color = gfxAlphaBlend(color, back,
                              coeff[COLEV & 0x1F],
                              coeff[(COLEV >> 8) & 0x1F]);
      else {
        switch((BLDMOD >> 6) & 3) {
        case 2:
          if(BLDMOD & top)
            color = gfxIncreaseBrightness(color, coeff[COLY & 0x1F]);
          break;
        case 3:
          if(BLDMOD & top)
            color = gfxDecreaseBrightness(color, coeff[COLY & 0x1F]);
          break;
        }
      }
    }

    lineMix[x] = color;
  }
}

void mode0RenderLineAll()
{
  u16 *palette = (u16 *)paletteRAM;

  if(DISPCNT & 0x80) {

	int x = 232;	//240 - 8  
	do{
		lineMix[x  ] =
		lineMix[x+1] =
		lineMix[x+2] =
		lineMix[x+3] =
		lineMix[x+4] =
		lineMix[x+5] =
		lineMix[x+6] =
		lineMix[x+7] = 0x7fff;
		x-=8;
	}while(x>=0);

    return;
  }

  bool inWindow0 = false;
  bool inWindow1 = false;

  if(layerEnable & 0x2000) {
    u8 v0 = WIN0V >> 8;
    u8 v1 = WIN0V & 255;
    inWindow0 = ((v0 == v1) && (v0 >= 0xe8));
    if(v1 >= v0)
      inWindow0 |= (VCOUNT >= v0 && VCOUNT < v1);
    else
      inWindow0 |= (VCOUNT >= v0 || VCOUNT < v1);
  }
  if(layerEnable & 0x4000) {
    u8 v0 = WIN1V >> 8;
    u8 v1 = WIN1V & 255;
    inWindow1 = ((v0 == v1) && (v0 >= 0xe8));
    if(v1 >= v0)
      inWindow1 |= (VCOUNT >= v0 && VCOUNT < v1);
    else
      inWindow1 |= (VCOUNT >= v0 || VCOUNT < v1);
  }

  if((layerEnable & 0x0100)) {
    gfxDrawTextScreen(BG0CNT, BG0HOFS, BG0VOFS, line0);
  }

  if((layerEnable & 0x0200)) {
    gfxDrawTextScreen(BG1CNT, BG1HOFS, BG1VOFS, line1);
  }

  if((layerEnable & 0x0400)) {
    gfxDrawTextScreen(BG2CNT, BG2HOFS, BG2VOFS, line2);
  }

  if((layerEnable & 0x0800)) {
    gfxDrawTextScreen(BG3CNT, BG3HOFS, BG3VOFS, line3);
  }

  gfxDrawSprites(lineOBJ);
  gfxDrawOBJWin(lineOBJWin);

  u32 backdrop;
  if(customBackdropColor == -1) {
    backdrop = (READ16LE(&palette[0]) | 0x30000000);
  } else {
    backdrop = ((customBackdropColor & 0x7FFF) | 0x30000000);
  }

  u8 inWin0Mask = WININ & 0xFF;
  u8 inWin1Mask = WININ >> 8;
  u8 outMask = WINOUT & 0xFF;

  for(int x = 0; x < 240; x++) {
    u32 color = backdrop;
    u8 top = 0x20;
    u8 mask = outMask;

    if(!(lineOBJWin[x] & 0x80000000)) {
      mask = WINOUT >> 8;
    }

    if(inWindow1) {
      if(gfxInWin1[x])
        mask = inWin1Mask;
    }

    if(inWindow0) {
      if(gfxInWin0[x]) {
        mask = inWin0Mask;
      }
    }

	if((mask & 1) && (line0[x] < color)) {
      color = line0[x];
      top = 0x01;
    }

    if((mask & 2) && ((u8)(line1[x]>>24) < (u8)(color >> 24))) {
      color = line1[x];
      top = 0x02;
    }

    if((mask & 4) && ((u8)(line2[x]>>24) < (u8)(color >> 24))) {
      color = line2[x];
      top = 0x04;
    }

    if((mask & 8) && ((u8)(line3[x]>>24) < (u8)(color >> 24))) {
      color = line3[x];
      top = 0x08;
    }

    if((mask & 16) && ((u8)(lineOBJ[x]>>24) < (u8)(color >> 24))) {
      color = lineOBJ[x];
      top = 0x10;
    }

    if(color & 0x00010000) {
      // semi-transparent OBJ
      u32 back = backdrop;
      u8 top2 = 0x20;

      if((mask & 1) && ((u8)(line0[x]>>24) < (u8)(back >> 24))) {
        back = line0[x];
        top2 = 0x01;
      }

      if((mask & 2) && ((u8)(line1[x]>>24) < (u8)(back >> 24))) {
        back = line1[x];
        top2 = 0x02;
      }

      if((mask & 4) && ((u8)(line2[x]>>24) < (u8)(back >> 24))) {
        back = line2[x];
        top2 = 0x04;
      }

      if((mask & 8) && ((u8)(line3[x]>>24) < (u8)(back >> 24))) {
        back = line3[x];
        top2 = 0x08;
      }

      if(top2 & (BLDMOD>>8))
        color = gfxAlphaBlend(color, back,
                              coeff[COLEV & 0x1F],
                              coeff[(COLEV >> 8) & 0x1F]);
      else {
        switch((BLDMOD >> 6) & 3) {
        case 2:
          if(BLDMOD & top)
            color = gfxIncreaseBrightness(color, coeff[COLY & 0x1F]);
          break;
        case 3:
          if(BLDMOD & top)
            color = gfxDecreaseBrightness(color, coeff[COLY & 0x1F]);
          break;
        }
      }
    } else if(mask & 32) {
      // special FX on in the window
      switch((BLDMOD >> 6) & 3) {
      case 0:
        break;
      case 1:
        {
          if(top & BLDMOD) {
            u32 back = backdrop;
            u8 top2 = 0x20;

			if((mask & 1) && (top != 0x01) && (u8)(line0[x]>>24) < (u8)(back >> 24)) {
                back = line0[x];
                top2 = 0x01;
            }

            if((mask & 2) && (top != 0x02) && (u8)(line1[x]>>24) < (u8)(back >> 24)) {
                back = line1[x];
                top2 = 0x02;
            }

            if((mask & 4) && (top != 0x04) && (u8)(line2[x]>>24) < (u8)(back >> 24)) {
                back = line2[x];
                top2 = 0x04;
            }

            if((mask & 8) && (top != 0x08) && (u8)(line3[x]>>24) < (u8)(back >> 24)) {
                back = line3[x];
                top2 = 0x08;
            }

            if((mask & 16) && (top != 0x10) && (u8)(lineOBJ[x]>>24) < (u8)(back >> 24)) {
                back = lineOBJ[x];
                top2 = 0x10;
            }

            if(top2 & (BLDMOD>>8))
              color = gfxAlphaBlend(color, back,
                                    coeff[COLEV & 0x1F],
                                    coeff[(COLEV >> 8) & 0x1F]);
          }
        }
        break;
      case 2:
        if(BLDMOD & top)
          color = gfxIncreaseBrightness(color, coeff[COLY & 0x1F]);
        break;
      case 3:
        if(BLDMOD & top)
          color = gfxDecreaseBrightness(color, coeff[COLY & 0x1F]);
        break;
      }
    }

    lineMix[x] = color;
  }
}
