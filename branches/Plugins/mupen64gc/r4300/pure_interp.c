/**
 * Mupen64 - pure_interp.c
 * Copyright (C) 2002 Hacktarux
 *
 * Mupen64 homepage: http://mupen64.emulation64.com
 * email address: hacktarux@yahoo.fr
 *
 * If you want to contribute to the project please contact
 * me first (maybe someone is already making what you are
 * planning to do).
 *
 *
 * This program is free software; you can redistribute it and/
 * or modify it under the terms of the GNU General Public Li-
 * cence as published by the Free Software Foundation; either
 * version 2 of the Licence, or any later version.
 *
 * This program is distributed in the hope that it will be use-
 * ful, but WITHOUT ANY WARRANTY; without even the implied war-
 * ranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public Licence for more details.
 *
 * You should have received a copy of the GNU General Public
 * Licence along with this program; if not, write to the Free
 * Software Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139,
 * USA.
 *
**/

#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include "r4300.h"
#include "exception.h"
#include "../gc_memory/memory.h"
#include "../gc_memory/TLB-Cache.h"
#include "macros.h"
#include "interupt.h"

#ifdef __PPC__
#include "../main/ROM-Cache.h"
#endif
#include "../gui/DEBUG.h"

#ifdef DBG
extern int debugger_mode;
extern void update_debugger();
#endif

#ifdef PPC_DYNAREC
#include "Invalid_Code.h"
#include "ARAM-blocks.h"

static void invalidate_func(unsigned int addr){
  PowerPC_block* block = blocks_get(address>>12);
	PowerPC_func* func = find_func(&block->funcs, addr);
	if(func)
		RecompCache_Free(func->start_addr);
}

#define check_memory() \
	if(dynacore && !invalid_code_get(address>>12)/* && \
	   blocks[address>>12]->code_addr[(address&0xfff)>>2]*/) \
		invalidate_func(address); //invalid_code_set(address>>12, 1);
#else
#define check_memory()
#endif

unsigned long interp_addr;
unsigned long op;
static long skip;

void prefetch();

/*static*/ void (*interp_ops[64])(void);

extern unsigned long next_vi;

static void NI()
{
   printf("NI:%x\n", (unsigned int)op);
   stop=1; 
#ifdef DEBUGON
  _break();
#endif     
}

static void SLL()
{
   rrd32 = (unsigned long)(rrt32) << rsa;
   sign_extended(rrd);
   interp_addr+=4;
}

static void SRL()
{
   rrd32 = (unsigned long)rrt32 >> rsa;
   sign_extended(rrd);
   interp_addr+=4;
}

static void SRA()
{
   rrd32 = (signed long)rrt32 >> rsa;
   sign_extended(rrd);
   interp_addr+=4;
}

static void SLLV()
{
   rrd32 = (unsigned long)(rrt32) << (rrs32&0x1F);
   sign_extended(rrd);
   interp_addr+=4;
}

static void SRLV()
{
   rrd32 = (unsigned long)rrt32 >> (rrs32 & 0x1F);
   sign_extended(rrd);
   interp_addr+=4;
}

static void SRAV()
{
   rrd32 = (signed long)rrt32 >> (rrs32 & 0x1F);
   sign_extended(rrd);
   interp_addr+=4;
}

static void JR()
{
   local_rs32 = irs32;
   interp_addr+=4;
   delay_slot=1;
   prefetch();
   interp_ops[((op >> 26) & 0x3F)]();
   update_count();
   delay_slot=0;
   interp_addr = local_rs32;
   last_addr = interp_addr;
   if (next_interupt <= Count) gen_interupt();
}

static void JALR()
{
   unsigned long long int *dest = PC->f.r.rd;
   local_rs32 = rrs32;
   interp_addr+=4;
   delay_slot=1;
   prefetch();
   interp_ops[((op >> 26) & 0x3F)]();
   update_count();
   delay_slot=0;
   if (!skip_jump)
     {
	*dest = interp_addr;
	sign_extended(*dest);

	interp_addr = local_rs32;
     }
   last_addr = interp_addr;
   if (next_interupt <= Count) gen_interupt();
}

static void SYSCALL()
{
   Cause = 8 << 2;
   exception_general();
}

static void SYNC()
{
   interp_addr+=4;
}

#define DUMP_ON_BREAK
#ifdef DUMP_ON_BREAK
#include <ogc/pad.h>
#endif
static void BREAK(){
#ifdef DUMP_ON_BREAK
#ifdef DEBUGON
	_break(); return;
#endif
	printf("-- BREAK @ %08x: DUMPING N64 REGISTERS --\n", interp_addr);
	int i;
	for(i=0; i<32; i+=4)
		printf("r%2d: %08x  r%2d: %08x  r%2d: %08x  r%2d: %08x\n",
		       i, (unsigned int)reg[i], i+1, (unsigned int)reg[i+1],
		       i+2, (unsigned int)reg[i+2], i+3, (unsigned int)reg[i+3]);
	printf("Press A to continue execution\n");
	while(!(PAD_ButtonsHeld(0) & PAD_BUTTON_A));
	while( (PAD_ButtonsHeld(0) & PAD_BUTTON_A));
#endif
}

static void MFHI()
{
   rrd = hi;
   interp_addr+=4;
}

static void MTHI()
{
   hi = rrs;
   interp_addr+=4;
}

static void MFLO()
{
   rrd = lo;
   interp_addr+=4;
}

static void MTLO()
{
   lo = rrs;
   interp_addr+=4;
}

static void DSLLV()
{
   rrd = rrt << (rrs32&0x3F);
   interp_addr+=4;
}

static void DSRLV()
{
   rrd = (unsigned long long)rrt >> (rrs32 & 0x3F);
   interp_addr+=4;
}

static void DSRAV()
{
   rrd = (long long)rrt >> (rrs32 & 0x3F);
   interp_addr+=4;
}

static void MULT()
{
   long long int temp;
   temp = rrs * rrt;
   hi = temp >> 32;
   lo = temp;
   sign_extended(lo);
   interp_addr+=4;
}

static void MULTU()
{
   unsigned long long int temp;
   temp = (unsigned long)rrs * (unsigned long long)((unsigned long)rrt);
   hi = (long long)temp >> 32;
   lo = temp;
   sign_extended(lo);
   interp_addr+=4;
}

static void DIV()
{
   if (rrt32)
     {
	lo = rrs32 / rrt32;
	hi = rrs32 % rrt32;
	sign_extended(lo);
	sign_extended(hi);
     }
   else printf("div\n");
   interp_addr+=4;
}

static void DIVU()
{
   if (rrt32)
     {
	lo = (unsigned long)rrs32 / (unsigned long)rrt32;
	hi = (unsigned long)rrs32 % (unsigned long)rrt32;
	sign_extended(lo);
	sign_extended(hi);
     }
   else printf("divu\n");
   interp_addr+=4;
}

static void DMULT()
{
   unsigned long long int op1, op2, op3, op4;
   unsigned long long int result1, result2, result3, result4;
   unsigned long long int temp1, temp2, temp3, temp4;
   int sign = 0;

   if (rrs < 0)
     {
	op2 = -rrs;
	sign = 1 - sign;
     }
   else op2 = rrs;
   if (rrt < 0)
     {
	op4 = -rrt;
	sign = 1 - sign;
     }
   else op4 = rrt;

   op1 = op2 & 0xFFFFFFFF;
   op2 = (op2 >> 32) & 0xFFFFFFFF;
   op3 = op4 & 0xFFFFFFFF;
   op4 = (op4 >> 32) & 0xFFFFFFFF;

   temp1 = op1 * op3;
   temp2 = (temp1 >> 32) + op1 * op4;
   temp3 = op2 * op3;
   temp4 = (temp3 >> 32) + op2 * op4;

   result1 = temp1 & 0xFFFFFFFF;
   result2 = temp2 + (temp3 & 0xFFFFFFFF);
   result3 = (result2 >> 32) + temp4;
   result4 = (result3 >> 32);

   lo = result1 | (result2 << 32);
   hi = (result3 & 0xFFFFFFFF) | (result4 << 32);
   if (sign)
     {
	hi = ~hi;
	if (!lo) hi++;
	else lo = ~lo + 1;
     }
   interp_addr+=4;
}

static void DMULTU()
{
   unsigned long long int op1, op2, op3, op4;
   unsigned long long int result1, result2, result3, result4;
   unsigned long long int temp1, temp2, temp3, temp4;

   op1 = rrs & 0xFFFFFFFF;
   op2 = (rrs >> 32) & 0xFFFFFFFF;
   op3 = rrt & 0xFFFFFFFF;
   op4 = (rrt >> 32) & 0xFFFFFFFF;

   temp1 = op1 * op3;
   temp2 = (temp1 >> 32) + op1 * op4;
   temp3 = op2 * op3;
   temp4 = (temp3 >> 32) + op2 * op4;

   result1 = temp1 & 0xFFFFFFFF;
   result2 = temp2 + (temp3 & 0xFFFFFFFF);
   result3 = (result2 >> 32) + temp4;
   result4 = (result3 >> 32);

   lo = result1 | (result2 << 32);
   hi = (result3 & 0xFFFFFFFF) | (result4 << 32);

   interp_addr+=4;
}

static void DDIV()
{
   if (rrt)
     {
	lo = (long long int)rrs / (long long int)rrt;
	hi = (long long int)rrs % (long long int)rrt;
     }
   else printf("ddiv\n");
   interp_addr+=4;
}

static void DDIVU()
{
   if (rrt)
     {
	lo = (unsigned long long int)rrs / (unsigned long long int)rrt;
	hi = (unsigned long long int)rrs % (unsigned long long int)rrt;
     }
   else printf("ddivu\n");
   interp_addr+=4;
}

static void ADD()
{
   rrd32 = rrs32 + rrt32;
   sign_extended(rrd);
   interp_addr+=4;
}

static void ADDU()
{
   rrd32 = rrs32 + rrt32;
   sign_extended(rrd);
   interp_addr+=4;
}

static void SUB()
{
   rrd32 = rrs32 - rrt32;
   sign_extended(rrd);
   interp_addr+=4;
}

static void SUBU()
{
   rrd32 = rrs32 - rrt32;
   sign_extended(rrd);
   interp_addr+=4;
}

static void AND()
{
   rrd = rrs & rrt;
   interp_addr+=4;
}

static void OR()
{
   rrd = rrs | rrt;
   interp_addr+=4;
}

static void XOR()
{
   rrd = rrs ^ rrt;
   interp_addr+=4;
}

static void NOR()
{
   rrd = ~(rrs | rrt);
   interp_addr+=4;
}

static void SLT()
{
   if (rrs < rrt) rrd = 1;
   else rrd = 0;
   interp_addr+=4;
}

static void SLTU()
{
   if ((unsigned long long)rrs < (unsigned long long)rrt)
     rrd = 1;
   else rrd = 0;
   interp_addr+=4;
}

static void DADD()
{
   rrd = rrs + rrt;
   interp_addr+=4;
}

static void DADDU()
{
   rrd = rrs + rrt;
   interp_addr+=4;
}

static void DSUB()
{
   rrd = rrs - rrt;
   interp_addr+=4;
}

static void DSUBU()
{
   rrd = rrs - rrt;
   interp_addr+=4;
}

static void TEQ()
{
   if (rrs == rrt)
     {
	printf("trap exception in teq\n");
	stop=1;
#ifdef DEBUGON
  _break();
#endif     
     }
   interp_addr+=4;
}

static void DSLL()
{
   rrd = rrt << rsa;
   interp_addr+=4;
}

static void DSRL()
{
   rrd = (unsigned long long)rrt >> rsa;
   interp_addr+=4;
}

static void DSRA()
{
   rrd = rrt >> rsa;
   interp_addr+=4;
}

static void DSLL32()
{
   rrd = rrt << (32+rsa);
   interp_addr+=4;
}

static void DSRL32()
{
   rrd = (unsigned long long int)rrt >> (32+rsa);
   interp_addr+=4;
}

static void DSRA32()
{
   rrd = (signed long long int)rrt >> (32+rsa);
   interp_addr+=4;
}

static void (*interp_special[64])(void) =
{
   SLL , NI   , SRL , SRA , SLLV   , NI    , SRLV  , SRAV  ,
   JR  , JALR , NI  , NI  , SYSCALL, BREAK , NI    , SYNC  ,
   MFHI, MTHI , MFLO, MTLO, DSLLV  , NI    , DSRLV , DSRAV ,
   MULT, MULTU, DIV , DIVU, DMULT  , DMULTU, DDIV  , DDIVU ,
   ADD , ADDU , SUB , SUBU, AND    , OR    , XOR   , NOR   ,
   NI  , NI   , SLT , SLTU, DADD   , DADDU , DSUB  , DSUBU ,
   NI  , NI   , NI  , NI  , TEQ    , NI    , NI    , NI    ,
   DSLL, NI   , DSRL, DSRA, DSLL32 , NI    , DSRL32, DSRA32
};

static void BLTZ()
{
   short local_immediate = iimmediate;
   local_rs = irs;
   if ((interp_addr + (local_immediate+1)*4) == interp_addr)
     if (local_rs < 0)
       {
	  if (probe_nop(interp_addr+4))
	    {
	       update_count();
	       skip = next_interupt - Count;
	       if (skip > 3)
		 {
		    Count += (skip & 0xFFFFFFFC);
		    return;
		 }
	    }
       }
   interp_addr+=4;
   delay_slot=1;
   prefetch();
   interp_ops[((op >> 26) & 0x3F)]();
   update_count();
   delay_slot=0;
   if (local_rs < 0)
     interp_addr += (local_immediate-1)*4;
   last_addr = interp_addr;
   if (next_interupt <= Count) gen_interupt();
}

static void BGEZ()
{
   short local_immediate = iimmediate;
   local_rs = irs;
   if ((interp_addr + (local_immediate+1)*4) == interp_addr)
     if (local_rs >= 0)
       {
	  if (probe_nop(interp_addr+4))
	    {
	       update_count();
	       skip = next_interupt - Count;
	       if (skip > 3)
		 {
		    Count += (skip & 0xFFFFFFFC);
		    return;
		 }
	    }
       }
   interp_addr+=4;
   delay_slot=1;
   prefetch();
   interp_ops[((op >> 26) & 0x3F)]();
   update_count();
   delay_slot=0;
   if (local_rs >= 0)
     interp_addr += (local_immediate-1)*4;
   last_addr = interp_addr;
   if (next_interupt <= Count) gen_interupt();
}

static void BLTZL()
{
   short local_immediate = iimmediate;
   local_rs = irs;
   if ((interp_addr + (local_immediate+1)*4) == interp_addr)
     if (irs < 0)
       {
	  if (probe_nop(interp_addr+4))
	    {
	       update_count();
	       skip = next_interupt - Count;
	       if (skip > 3)
		 {
		    Count += (skip & 0xFFFFFFFC);
		    return;
		 }
	    }
       }
   if (irs < 0)
     {
	interp_addr+=4;
	delay_slot=1;
	prefetch();
	interp_ops[((op >> 26) & 0x3F)]();
	update_count();
	delay_slot=0;
	interp_addr += (local_immediate-1)*4;
     }
   else interp_addr+=8;
   last_addr = interp_addr;
   if (next_interupt <= Count) gen_interupt();
}

static void BGEZL()
{
   short local_immediate = iimmediate;
   local_rs = irs;
   if ((interp_addr + (local_immediate+1)*4) == interp_addr)
     if (irs >= 0)
       {
	  if (probe_nop(interp_addr+4))
	    {
	       update_count();
	       skip = next_interupt - Count;
	       if (skip > 3)
		 {
		    Count += (skip & 0xFFFFFFFC);
		    return;
		 }
	    }
       }
   if (irs >= 0)
     {
	interp_addr+=4;
	delay_slot=1;
	prefetch();
	interp_ops[((op >> 26) & 0x3F)]();
	update_count();
	delay_slot=0;
	interp_addr += (local_immediate-1)*4;
     }
   else interp_addr+=8;
   last_addr = interp_addr;
   if (next_interupt <= Count) gen_interupt();
}

static void BLTZAL()
{
   short local_immediate = iimmediate;
   local_rs = irs;
   reg[31]=interp_addr+8;
   if((&irs)!=(reg+31))
     {
	if ((interp_addr + (local_immediate+1)*4) == interp_addr)
	  if (local_rs < 0)
	    {
	       if (probe_nop(interp_addr+4))
		 {
		    update_count();
		    skip = next_interupt - Count;
		    if (skip > 3)
		      {
			 Count += (skip & 0xFFFFFFFC);
			 return;
		      }
		 }
	    }
	interp_addr+=4;
	delay_slot=1;
	prefetch();
	interp_ops[((op >> 26) & 0x3F)]();
	update_count();
	delay_slot=0;
	if(local_rs < 0)
	  interp_addr += (local_immediate-1)*4;
     }
   else printf("erreur dans bltzal\n");
   last_addr = interp_addr;
   if (next_interupt <= Count) gen_interupt();
}

static void BGEZAL()
{
   short local_immediate = iimmediate;
   local_rs = irs;
   reg[31]=interp_addr+8;
   if((&irs)!=(reg+31))
     {
	if ((interp_addr + (local_immediate+1)*4) == interp_addr)
	  if (local_rs >= 0)
	    {
	       if (probe_nop(interp_addr+4))
		 {
		    update_count();
		    skip = next_interupt - Count;
		    if (skip > 3)
		      {
			 Count += (skip & 0xFFFFFFFC);
			 return;
		      }
		 }
	    }
	interp_addr+=4;
	delay_slot=1;
	prefetch();
	interp_ops[((op >> 26) & 0x3F)]();
	update_count();
	delay_slot=0;
	if(local_rs >= 0)
	  interp_addr += (local_immediate-1)*4;
     }
   else printf("erreur dans bgezal\n");
   last_addr = interp_addr;
   if (next_interupt <= Count) gen_interupt();
}

static void BLTZALL()
{
   short local_immediate = iimmediate;
   local_rs = irs;
   reg[31]=interp_addr+8;
   if((&irs)!=(reg+31))
     {
	if ((interp_addr + (local_immediate+1)*4) == interp_addr)
	  if (local_rs < 0)
	    {
	       if (probe_nop(interp_addr+4))
		 {
		    update_count();
		    skip = next_interupt - Count;
		    if (skip > 3)
		      {
			 Count += (skip & 0xFFFFFFFC);
			 return;
		      }
		 }
	    }
	if (local_rs < 0)
	  {
	     interp_addr+=4;
	     delay_slot=1;
	     prefetch();
	     interp_ops[((op >> 26) & 0x3F)]();
	     update_count();
	     delay_slot=0;
	     interp_addr += (local_immediate-1)*4;
	  }
	else interp_addr+=8;
     }
   else printf("erreur dans bltzall\n");
   last_addr = interp_addr;
   if (next_interupt <= Count) gen_interupt();
}

static void BGEZALL()
{
   short local_immediate = iimmediate;
   local_rs = irs;
   reg[31]=interp_addr+8;
   if((&irs)!=(reg+31))
     {
	if ((interp_addr + (local_immediate+1)*4) == interp_addr)
	  if (local_rs >= 0)
	    {
	       if (probe_nop(interp_addr+4))
		 {
		    update_count();
		    skip = next_interupt - Count;
		    if (skip > 3)
		      {
			 Count += (skip & 0xFFFFFFFC);
			 return;
		      }
		 }
	    }
	if (local_rs >= 0)
	  {
	     interp_addr+=4;
	     delay_slot=1;
	     prefetch();
	     interp_ops[((op >> 26) & 0x3F)]();
	     update_count();
	     delay_slot=0;
	     interp_addr += (local_immediate-1)*4;
	  }
	else interp_addr+=8;
     }
   else printf("erreur dans bgezall\n");
   last_addr = interp_addr;
   if (next_interupt <= Count) gen_interupt();
}

static void (*interp_regimm[32])(void) =
{
   BLTZ  , BGEZ  , BLTZL  , BGEZL  , NI, NI, NI, NI,
   NI    , NI    , NI     , NI     , NI, NI, NI, NI,
   BLTZAL, BGEZAL, BLTZALL, BGEZALL, NI, NI, NI, NI,
   NI    , NI    , NI     , NI     , NI, NI, NI, NI
};

static void TLBR()
{
   int index;
   index = Index & 0x1F;
   PageMask = tlb_e[index].mask << 13;
   EntryHi = ((tlb_e[index].vpn2 << 13) | tlb_e[index].asid);
   EntryLo0 = (tlb_e[index].pfn_even << 6) | (tlb_e[index].c_even << 3)
     | (tlb_e[index].d_even << 2) | (tlb_e[index].v_even << 1)
       | tlb_e[index].g;
   EntryLo1 = (tlb_e[index].pfn_odd << 6) | (tlb_e[index].c_odd << 3)
     | (tlb_e[index].d_odd << 2) | (tlb_e[index].v_odd << 1)
       | tlb_e[index].g;
   interp_addr+=4;
}

static void TLBWI()
{
   unsigned int i;

   if (tlb_e[Index&0x3F].v_even)
     {
	for (i=tlb_e[Index&0x3F].start_even; i<tlb_e[Index&0x3F].end_even; i+=0x1000)
#ifdef USE_TLB_CACHE
	  TLBCache_set_r(i>>12, 0);
#else
	  tlb_LUT_r[i>>12] = 0;
#endif
	if (tlb_e[Index&0x3F].d_even)
	  for (i=tlb_e[Index&0x3F].start_even; i<tlb_e[Index&0x3F].end_even; i+=0x1000)
#ifdef USE_TLB_CACHE
	    TLBCache_set_w(i>>12, 0);
#else
	    tlb_LUT_w[i>>12] = 0;
#endif
     }
   if (tlb_e[Index&0x3F].v_odd)
     {
	for (i=tlb_e[Index&0x3F].start_odd; i<tlb_e[Index&0x3F].end_odd; i+=0x1000)
#ifdef USE_TLB_CACHE
	  TLBCache_set_r(i>>12, 0);
#else
	  tlb_LUT_r[i>>12] = 0;
#endif
	if (tlb_e[Index&0x3F].d_odd)
	  for (i=tlb_e[Index&0x3F].start_odd; i<tlb_e[Index&0x3F].end_odd; i+=0x1000)
#ifdef USE_TLB_CACHE
	    TLBCache_set_w(i>>12, 0);
#else
	    tlb_LUT_w[i>>12] = 0;
#endif
     }
   tlb_e[Index&0x3F].g = (EntryLo0 & EntryLo1 & 1);
   tlb_e[Index&0x3F].pfn_even = (EntryLo0 & 0x3FFFFFC0) >> 6;
   tlb_e[Index&0x3F].pfn_odd = (EntryLo1 & 0x3FFFFFC0) >> 6;
   tlb_e[Index&0x3F].c_even = (EntryLo0 & 0x38) >> 3;
   tlb_e[Index&0x3F].c_odd = (EntryLo1 & 0x38) >> 3;
   tlb_e[Index&0x3F].d_even = (EntryLo0 & 0x4) >> 2;
   tlb_e[Index&0x3F].d_odd = (EntryLo1 & 0x4) >> 2;
   tlb_e[Index&0x3F].v_even = (EntryLo0 & 0x2) >> 1;
   tlb_e[Index&0x3F].v_odd = (EntryLo1 & 0x2) >> 1;
   tlb_e[Index&0x3F].asid = (EntryHi & 0xFF);
   tlb_e[Index&0x3F].vpn2 = (EntryHi & 0xFFFFE000) >> 13;
   //tlb_e[Index&0x3F].r = (EntryHi & 0xC000000000000000LL) >> 62;
   tlb_e[Index&0x3F].mask = (PageMask & 0x1FFE000) >> 13;

   tlb_e[Index&0x3F].start_even = tlb_e[Index&0x3F].vpn2 << 13;
   tlb_e[Index&0x3F].end_even = tlb_e[Index&0x3F].start_even+
     (tlb_e[Index&0x3F].mask << 12) + 0xFFF;
   tlb_e[Index&0x3F].phys_even = tlb_e[Index&0x3F].pfn_even << 12;

   if (tlb_e[Index&0x3F].v_even)
     {
	if (tlb_e[Index&0x3F].start_even < tlb_e[Index&0x3F].end_even &&
	    !(tlb_e[Index&0x3F].start_even >= 0x80000000 &&
	    tlb_e[Index&0x3F].end_even < 0xC0000000) &&
	    tlb_e[Index&0x3F].phys_even < 0x20000000)
	  {
	     for (i=tlb_e[Index&0x3F].start_even;i<tlb_e[Index&0x3F].end_even;i+=0x1000)
#ifdef USE_TLB_CACHE
		TLBCache_set_r(i>>12, 0x80000000 |
	       (tlb_e[Index&0x3F].phys_even + (i - tlb_e[Index&0x3F].start_even + 0xFFF)));
#else
	       tlb_LUT_r[i>>12] = 0x80000000 |
	       (tlb_e[Index&0x3F].phys_even + (i - tlb_e[Index&0x3F].start_even + 0xFFF));
#endif
	     if (tlb_e[Index&0x3F].d_even)
	       for (i=tlb_e[Index&0x3F].start_even;i<tlb_e[Index&0x3F].end_even;i+=0x1000)
#ifdef USE_TLB_CACHE
		TLBCache_set_w(i>>12, 0x80000000 |
	       (tlb_e[Index&0x3F].phys_even + (i - tlb_e[Index&0x3F].start_even + 0xFFF)));
#else
		 tlb_LUT_w[i>>12] = 0x80000000 |
	       (tlb_e[Index&0x3F].phys_even + (i - tlb_e[Index&0x3F].start_even + 0xFFF));
#endif
	  }
     }

   tlb_e[Index&0x3F].start_odd = tlb_e[Index&0x3F].end_even+1;
   tlb_e[Index&0x3F].end_odd = tlb_e[Index&0x3F].start_odd+
     (tlb_e[Index&0x3F].mask << 12) + 0xFFF;
   tlb_e[Index&0x3F].phys_odd = tlb_e[Index&0x3F].pfn_odd << 12;

   if (tlb_e[Index&0x3F].v_odd)
     {
	if (tlb_e[Index&0x3F].start_odd < tlb_e[Index&0x3F].end_odd &&
	    !(tlb_e[Index&0x3F].start_odd >= 0x80000000 &&
	    tlb_e[Index&0x3F].end_odd < 0xC0000000) &&
	    tlb_e[Index&0x3F].phys_odd < 0x20000000)
	  {
	     for (i=tlb_e[Index&0x3F].start_odd;i<tlb_e[Index&0x3F].end_odd;i+=0x1000)
#ifdef USE_TLB_CACHE
		TLBCache_set_r(i>>12, 0x80000000 |
	       (tlb_e[Index&0x3F].phys_odd + (i - tlb_e[Index&0x3F].start_odd + 0xFFF)));
#else
	       tlb_LUT_r[i>>12] = 0x80000000 |
	       (tlb_e[Index&0x3F].phys_odd + (i - tlb_e[Index&0x3F].start_odd + 0xFFF));
#endif
	     if (tlb_e[Index&0x3F].d_odd)
	       for (i=tlb_e[Index&0x3F].start_odd;i<tlb_e[Index&0x3F].end_odd;i+=0x1000)
#ifdef USE_TLB_CACHE
		TLBCache_set_w(i>>12, 0x80000000 |
	       (tlb_e[Index&0x3F].phys_odd + (i - tlb_e[Index&0x3F].start_odd + 0xFFF)));
#else
		 tlb_LUT_w[i>>12] = 0x80000000 |
	       (tlb_e[Index&0x3F].phys_odd + (i - tlb_e[Index&0x3F].start_odd + 0xFFF));
#endif
	  }
     }
   interp_addr+=4;
}

static void TLBWR()
{
	unsigned int i;
	update_count();
	Random = (Count/2 % (32 - Wired)) + Wired;

	if (tlb_e[Random].v_even){
		for (i=tlb_e[Random].start_even; i<tlb_e[Random].end_even; i+=0x1000)
#ifdef USE_TLB_CACHE
			TLBCache_set_r(i>>12, 0);
#else
			tlb_LUT_r[i>>12] = 0;
#endif
		if (tlb_e[Random].d_even)
			for (i=tlb_e[Random].start_even; i<tlb_e[Random].end_even; i+=0x1000)
#ifdef USE_TLB_CACHE
				TLBCache_set_w(i>>12, 0);
#else
				tlb_LUT_w[i>>12] = 0;
#endif
	}
	if (tlb_e[Random].v_odd){
		for (i=tlb_e[Random].start_odd; i<tlb_e[Random].end_odd; i+=0x1000)
#ifdef USE_TLB_CACHE
			TLBCache_set_r(i>>12, 0);
#else
			tlb_LUT_r[i>>12] = 0;
#endif
	if (tlb_e[Random].d_odd)
		for (i=tlb_e[Random].start_odd; i<tlb_e[Random].end_odd; i+=0x1000)
#ifdef USE_TLB_CACHE
			TLBCache_set_w(i>>12, 0);
#else
			tlb_LUT_w[i>>12] = 0;
#endif
	}

	tlb_e[Random].g = (EntryLo0 & EntryLo1 & 1);
	tlb_e[Random].pfn_even = (EntryLo0 & 0x3FFFFFC0) >> 6;
	tlb_e[Random].pfn_odd = (EntryLo1 & 0x3FFFFFC0) >> 6;
	tlb_e[Random].c_even = (EntryLo0 & 0x38) >> 3;
	tlb_e[Random].c_odd = (EntryLo1 & 0x38) >> 3;
	tlb_e[Random].d_even = (EntryLo0 & 0x4) >> 2;
	tlb_e[Random].d_odd = (EntryLo1 & 0x4) >> 2;
	tlb_e[Random].v_even = (EntryLo0 & 0x2) >> 1;
	tlb_e[Random].v_odd = (EntryLo1 & 0x2) >> 1;
	tlb_e[Random].asid = (EntryHi & 0xFF);
	tlb_e[Random].vpn2 = (EntryHi & 0xFFFFE000) >> 13;
	//tlb_e[Random].r = (EntryHi & 0xC000000000000000LL) >> 62;
	tlb_e[Random].mask = (PageMask & 0x1FFE000) >> 13;

	tlb_e[Random].start_even = tlb_e[Random].vpn2 << 13;
	tlb_e[Random].end_even = tlb_e[Random].start_even+
		(tlb_e[Random].mask << 12) + 0xFFF;
	tlb_e[Random].phys_even = tlb_e[Random].pfn_even << 12;

	if (tlb_e[Random].v_even){
		if(tlb_e[Random].start_even < tlb_e[Random].end_even &&
		   !(tlb_e[Random].start_even >= 0x80000000 &&
		   tlb_e[Random].end_even < 0xC0000000) &&
		   tlb_e[Random].phys_even < 0x20000000){
			for (i=tlb_e[Random].start_even;i<tlb_e[Random].end_even;i+=0x1000)
#ifdef USE_TLB_CACHE
				TLBCache_set_r(i>>12, 0x80000000 |
					(tlb_e[Random].phys_even + (i - tlb_e[Random].start_even + 0xFFF)));
#else
				tlb_LUT_r[i>>12] = 0x80000000 |
					(tlb_e[Random].phys_even + (i - tlb_e[Random].start_even + 0xFFF));
#endif
			if (tlb_e[Random].d_even)
				for (i=tlb_e[Random].start_even;i<tlb_e[Random].end_even;i+=0x1000)
#ifdef USE_TLB_CACHE
					TLBCache_set_w(i>>12, 0x80000000 |
						(tlb_e[Random].phys_even + (i - tlb_e[Random].start_even + 0xFFF)));
#else
					tlb_LUT_w[i>>12] = 0x80000000 |
						(tlb_e[Random].phys_even + (i - tlb_e[Random].start_even + 0xFFF));
#endif
		}
	}
	tlb_e[Random].start_odd = tlb_e[Random].end_even+1;
	tlb_e[Random].end_odd = tlb_e[Random].start_odd+
		(tlb_e[Random].mask << 12) + 0xFFF;
	tlb_e[Random].phys_odd = tlb_e[Random].pfn_odd << 12;

	if (tlb_e[Random].v_odd){
		if(tlb_e[Random].start_odd < tlb_e[Random].end_odd &&
		   !(tlb_e[Random].start_odd >= 0x80000000 &&
		   tlb_e[Random].end_odd < 0xC0000000) &&
		   tlb_e[Random].phys_odd < 0x20000000){
			for (i=tlb_e[Random].start_odd;i<tlb_e[Random].end_odd;i+=0x1000)
#ifdef USE_TLB_CACHE
				TLBCache_set_r(i>>12, 0x80000000 |
					(tlb_e[Random].phys_odd + (i - tlb_e[Random].start_odd + 0xFFF)));
#else
				tlb_LUT_r[i>>12] = 0x80000000 |
					(tlb_e[Random].phys_odd + (i - tlb_e[Random].start_odd + 0xFFF));
#endif
			if (tlb_e[Random].d_odd)
				for (i=tlb_e[Random].start_odd;i<tlb_e[Random].end_odd;i+=0x1000)
#ifdef USE_TLB_CACHE
					TLBCache_set_w(i>>2, 0x80000000 |
						(tlb_e[Random].phys_odd + (i - tlb_e[Random].start_odd + 0xFFF)));
#else
					tlb_LUT_w[i>>12] = 0x80000000 |
						(tlb_e[Random].phys_odd + (i - tlb_e[Random].start_odd + 0xFFF));
#endif
		}
	}
	interp_addr+=4;
}

static void TLBP()
{
   int i;
   Index |= 0x80000000;
   for (i=0; i<32; i++)
     {
	if (((tlb_e[i].vpn2 & (~tlb_e[i].mask)) ==
	     (((EntryHi & 0xFFFFE000) >> 13) & (~tlb_e[i].mask))) &&
	    ((tlb_e[i].g) ||
	     (tlb_e[i].asid == (EntryHi & 0xFF))))
	  {
	     Index = i;
	     break;
	  }
     }
   interp_addr+=4;
}

static void ERET()
{
//	DEBUG_print("ERET\n", DBG_USBGECKO);
   update_count();
   if (Status & 0x4)
     {
	printf("erreur dans ERET\n");
	stop=1;
#ifdef DEBUGON
  _break();
#endif     
     }
   else
     {
	Status &= 0xFFFFFFFD;
	interp_addr = EPC;
     }
   llbit = 0;
   check_interupt();
   last_addr = interp_addr;
   if (next_interupt <= Count) gen_interupt();
}

static void (*interp_tlb[64])(void) =
{
   NI  , TLBR, TLBWI, NI, NI, NI, TLBWR, NI,
   TLBP, NI  , NI   , NI, NI, NI, NI   , NI,
   NI  , NI  , NI   , NI, NI, NI, NI   , NI,
   ERET, NI  , NI   , NI, NI, NI, NI   , NI,
   NI  , NI  , NI   , NI, NI, NI, NI   , NI,
   NI  , NI  , NI   , NI, NI, NI, NI   , NI,
   NI  , NI  , NI   , NI, NI, NI, NI   , NI,
   NI  , NI  , NI   , NI, NI, NI, NI   , NI
};

static void MFC0()
{
   switch(PC->f.r.nrd)
     {
      case 1:
	printf("lecture de Random\n");
	stop=1;
#ifdef DEBUGON
  _break();
#endif     
      default:
	rrt32 = reg_cop0[PC->f.r.nrd];
	sign_extended(rrt);
     }
   interp_addr+=4;
}

static void MTC0()
{
   switch(PC->f.r.nrd)
     {
      case 0:    // Index
	Index = rrt & 0x8000003F;
	if ((Index & 0x3F) > 31)
	  {
	     printf ("il y a plus de 32 TLB\n");
	     stop=1;
#ifdef DEBUGON
       _break();
#endif     
	  }
	break;
      case 1:    // Random
	break;
      case 2:    // EntryLo0
	EntryLo0 = rrt & 0x3FFFFFFF;
	break;
      case 3:    // EntryLo1
 	EntryLo1 = rrt & 0x3FFFFFFF;
	break;
      case 4:    // Context
	Context = (rrt & 0xFF800000) | (Context & 0x007FFFF0);
	break;
      case 5:    // PageMask
	PageMask = rrt & 0x01FFE000;
	break;
      case 6:    // Wired
	Wired = rrt;
	Random = 31;
	break;
      case 8:    // BadVAddr
	break;
      case 9:    // Count
	update_count();
	if (next_interupt <= Count) gen_interupt();
	debug_count += Count;
	translate_event_queue(rrt & 0xFFFFFFFF);
	Count = rrt & 0xFFFFFFFF;
	debug_count -= Count;
	break;
      case 10:   // EntryHi
	EntryHi = rrt & 0xFFFFE0FF;
	break;
      case 11:   // Compare
	update_count();
	remove_event(COMPARE_INT);
	add_interupt_event_count(COMPARE_INT, (unsigned long)rrt);
	Compare = rrt;
	Cause = Cause & 0xFFFF7FFF; //Timer interupt is clear
	break;
      case 12:   // Status
	if((rrt & 0x04000000) != (Status & 0x04000000))
	  {
	     if (rrt & 0x04000000)
	       {
		  int i;
		  for (i=0; i<32; i++)
		    {
		       //reg_cop1_fgr_64[i]=reg_cop1_fgr_32[i];
		       reg_cop1_double[i]=(double*)&reg_cop1_fgr_64[i];
		       reg_cop1_simple[i]=(float*)&reg_cop1_fgr_64[i];
		    }
	       }
	     else
	       {
		  int i;
		  for (i=0; i<32; i++)
		    {
		       //reg_cop1_fgr_32[i]=reg_cop1_fgr_64[i]&0xFFFFFFFF;
		       //if (i<16) reg_cop1_double[i*2]=(double*)&reg_cop1_fgr_32[i*2];
		       //reg_cop1_double[i]=(double*)&reg_cop1_fgr_64[i & 0xFFFE];
		       if(!(i&1))
			 reg_cop1_double[i]=(double*)&reg_cop1_fgr_64[i>>1];
		       //reg_cop1_double[i]=(double*)&reg_cop1_fgr_64[i];
		       //reg_cop1_simple[i]=(float*)&reg_cop1_fgr_32[i];
		       //reg_cop1_simple[i]=(float*)&reg_cop1_fgr_64[i & 0xFFFE]+(i&1);
#ifndef _BIG_ENDIAN
		       reg_cop1_simple[i]=(float*)&reg_cop1_fgr_64[i>>1]+(i&1);
#else
		       reg_cop1_simple[i]=(float*)&reg_cop1_fgr_64[i>>1]+(1-(i&1));
#endif
		    }
	       }
	  }
	Status = rrt;
	interp_addr+=4;
	check_interupt();
	update_count();
	if (next_interupt <= Count) gen_interupt();
	interp_addr-=4;
	break;
      case 13:   // Cause
	if (rrt!=0)
	  {
	     printf("�criture dans Cause\n");
	     stop = 1;
#ifdef DEBUGON
       _break();
#endif     
	  }
	else Cause = rrt;
	break;
      case 14:   // EPC
	EPC = rrt;
	break;
      case 15:  // PRevID
	break;
      case 16:  // Config
	Config = rrt;
	break;
      case 18:  // WatchLo
	WatchLo = rrt & 0xFFFFFFFF;
	break;
      case 19:  // WatchHi
	WatchHi = rrt & 0xFFFFFFFF;
	break;
      case 27: // CacheErr
	break;
      case 28: // TagLo
	TagLo = rrt & 0x0FFFFFC0;
	break;
      case 29: // TagHi
	TagHi =0;
	break;
      default:
	printf("unknown mtc0 write : %d\n", PC->f.r.nrd);
	stop=1;
#ifdef DEBUGON
  _break();
#endif     
     }
   interp_addr+=4;
}

static void TLB()
{
   start_section(TLB_SECTION);
   interp_tlb[(op & 0x3F)]();
   end_section(TLB_SECTION);
}

static void (*interp_cop0[32])(void) =
{
   MFC0, NI, NI, NI, MTC0, NI, NI, NI,
   NI  , NI, NI, NI, NI  , NI, NI, NI,
   TLB , NI, NI, NI, NI  , NI, NI, NI,
   NI  , NI, NI, NI, NI  , NI, NI, NI
};

static void BC1F()
{
   short local_immediate = iimmediate;
   if ((interp_addr + (local_immediate+1)*4) == interp_addr)
     if ((FCR31 & 0x800000)==0)
       {
	  if (probe_nop(interp_addr+4))
	    {
	       update_count();
	       skip = next_interupt - Count;
	       if (skip > 3)
		 {
		    Count += (skip & 0xFFFFFFFC);
		    return;
		 }
	    }
       }
   interp_addr+=4;
   delay_slot=1;
   prefetch();
   interp_ops[((op >> 26) & 0x3F)]();
   update_count();
   delay_slot=0;
   if ((FCR31 & 0x800000)==0)
     interp_addr += (local_immediate-1)*4;
   last_addr = interp_addr;
   if (next_interupt <= Count) gen_interupt();
}

static void BC1T()
{
   short local_immediate = iimmediate;
   if ((interp_addr + (local_immediate+1)*4) == interp_addr)
     if ((FCR31 & 0x800000)!=0)
       {
	  if (probe_nop(interp_addr+4))
	    {
	       update_count();
	       skip = next_interupt - Count;
	       if (skip > 3)
		 {
		    Count += (skip & 0xFFFFFFFC);
		    return;
		 }
	    }
       }
   interp_addr+=4;
   delay_slot=1;
   prefetch();
   interp_ops[((op >> 26) & 0x3F)]();
   update_count();
   delay_slot=0;
   if ((FCR31 & 0x800000)!=0)
     interp_addr += (local_immediate-1)*4;
   last_addr = interp_addr;
   if (next_interupt <= Count) gen_interupt();
}

static void BC1FL()
{
   short local_immediate = iimmediate;
   if ((interp_addr + (local_immediate+1)*4) == interp_addr)
     if ((FCR31 & 0x800000)==0)
       {
	  if (probe_nop(interp_addr+4))
	    {
	       update_count();
	       skip = next_interupt - Count;
	       if (skip > 3)
		 {
		    Count += (skip & 0xFFFFFFFC);
		    return;
		 }
	    }
       }
   if ((FCR31 & 0x800000)==0)
     {
	interp_addr+=4;
	delay_slot=1;
	prefetch();
	interp_ops[((op >> 26) & 0x3F)]();
	update_count();
	delay_slot=0;
	interp_addr += (local_immediate-1)*4;
     }
   else
     interp_addr+=8;
   last_addr = interp_addr;
   if (next_interupt <= Count) gen_interupt();
}

static void BC1TL()
{
   short local_immediate = iimmediate;
   if ((interp_addr + (local_immediate+1)*4) == interp_addr)
     if ((FCR31 & 0x800000)!=0)
       {
	  if (probe_nop(interp_addr+4))
	    {
	       update_count();
	       skip = next_interupt - Count;
	       if (skip > 3)
		 {
		    Count += (skip & 0xFFFFFFFC);
		    return;
		 }
	    }
       }
   if ((FCR31 & 0x800000)!=0)
     {
	interp_addr+=4;
	delay_slot=1;
	prefetch();
	interp_ops[((op >> 26) & 0x3F)]();
	update_count();
	delay_slot=0;
	interp_addr += (local_immediate-1)*4;
     }
   else
     interp_addr+=8;
   last_addr = interp_addr;
   if (next_interupt <= Count) gen_interupt();
}

static void (*interp_cop1_bc[4])(void) =
{
   BC1F , BC1T,
   BC1FL, BC1TL
};

static void ADD_S()
{
   set_rounding();
   *reg_cop1_simple[cffd] = *reg_cop1_simple[cffs] +
     *reg_cop1_simple[cfft];
   interp_addr+=4;
}

static void SUB_S()
{
   set_rounding();
   *reg_cop1_simple[cffd] = *reg_cop1_simple[cffs] -
     *reg_cop1_simple[cfft];
   interp_addr+=4;
}

static void MUL_S()
{
   set_rounding();
   *reg_cop1_simple[cffd] = *reg_cop1_simple[cffs] *
     *reg_cop1_simple[cfft];
   interp_addr+=4;
}

static void DIV_S()
{
   if((FCR31 & 0x400) && *reg_cop1_simple[cfft] == 0)
     {
	printf("div_s by 0\n");
     }
   set_rounding();
   *reg_cop1_simple[cffd] = *reg_cop1_simple[cffs] /
     *reg_cop1_simple[cfft];
   interp_addr+=4;
}

static void SQRT_S()
{
   set_rounding();
   *reg_cop1_simple[cffd] = sqrt(*reg_cop1_simple[cffs]);
   interp_addr+=4;
}

static void ABS_S()
{
   set_rounding();
   *reg_cop1_simple[cffd] = fabs(*reg_cop1_simple[cffs]);
   interp_addr+=4;
}

static void MOV_S()
{
   set_rounding();
   *reg_cop1_simple[cffd] = *reg_cop1_simple[cffs];
   interp_addr+=4;
}

static void NEG_S()
{
   set_rounding();
   *reg_cop1_simple[cffd] = -(*reg_cop1_simple[cffs]);
   interp_addr+=4;
}

static void ROUND_L_S()
{
   set_round();
   *((long long*)(reg_cop1_double[cffd])) = *reg_cop1_simple[cffs];
   interp_addr+=4;
}

static void TRUNC_L_S()
{
   set_trunc();
   *((long long*)(reg_cop1_double[cffd])) = *reg_cop1_simple[cffs];
   interp_addr+=4;
}

static void CEIL_L_S()
{
   set_ceil();
   *((long long*)(reg_cop1_double[cffd])) = *reg_cop1_simple[cffs];
   interp_addr+=4;
}

static void FLOOR_L_S()
{
   set_floor();
   *((long long*)(reg_cop1_double[cffd])) = *reg_cop1_simple[cffs];
   interp_addr+=4;
}

static void ROUND_W_S()
{
   set_round();
   *((long*)reg_cop1_simple[cffd]) = *reg_cop1_simple[cffs];
   interp_addr+=4;
}

static void TRUNC_W_S()
{
   set_trunc();
   *((long*)reg_cop1_simple[cffd]) = *reg_cop1_simple[cffs];
   interp_addr+=4;
}

static void CEIL_W_S()
{
   set_ceil();
   *((long*)reg_cop1_simple[cffd]) = *reg_cop1_simple[cffs];
   interp_addr+=4;
}

static void FLOOR_W_S()
{
   set_floor();
   *((long*)reg_cop1_simple[cffd]) = *reg_cop1_simple[cffs];
   interp_addr+=4;
}

static void CVT_D_S()
{
   set_rounding();
   *reg_cop1_double[cffd] = *reg_cop1_simple[cffs];
   interp_addr+=4;
}

static void CVT_W_S()
{
   set_rounding();
   *((long*)reg_cop1_simple[cffd]) = *reg_cop1_simple[cffs];
   interp_addr+=4;
}

static void CVT_L_S()
{
   set_rounding();
   *((long long*)(reg_cop1_double[cffd])) = *reg_cop1_simple[cffs];
   interp_addr+=4;
}

static void C_F_S()
{
   FCR31 &= ~0x800000;
   interp_addr+=4;
}

static void C_UN_S()
{
   if (isnan(*reg_cop1_simple[cffs]) || isnan(*reg_cop1_simple[cfft]))
     FCR31 |= 0x800000;
   else FCR31 &= ~0x800000;
   interp_addr+=4;
}

static void C_EQ_S()
{
   if (!isnan(*reg_cop1_simple[cffs]) && !isnan(*reg_cop1_simple[cfft]) &&
       *reg_cop1_simple[cffs] == *reg_cop1_simple[cfft])
     FCR31 |= 0x800000;
   else FCR31 &= ~0x800000;
   interp_addr+=4;
}

static void C_UEQ_S()
{
   if (isnan(*reg_cop1_simple[cffs]) || isnan(*reg_cop1_simple[cfft]) ||
       *reg_cop1_simple[cffs] == *reg_cop1_simple[cfft])
     FCR31 |= 0x800000;
   else FCR31 &= ~0x800000;
   interp_addr+=4;
}

static void C_OLT_S()
{
   if (!isnan(*reg_cop1_simple[cffs]) && !isnan(*reg_cop1_simple[cfft]) &&
       *reg_cop1_simple[cffs] < *reg_cop1_simple[cfft])
     FCR31 |= 0x800000;
   else FCR31 &= ~0x800000;
   interp_addr+=4;
}

static void C_ULT_S()
{
   if (isnan(*reg_cop1_simple[cffs]) || isnan(*reg_cop1_simple[cfft]) ||
       *reg_cop1_simple[cffs] < *reg_cop1_simple[cfft])
     FCR31 |= 0x800000;
   else FCR31 &= ~0x800000;
   interp_addr+=4;
}

static void C_OLE_S()
{
   if (!isnan(*reg_cop1_simple[cffs]) && !isnan(*reg_cop1_simple[cfft]) &&
       *reg_cop1_simple[cffs] <= *reg_cop1_simple[cfft])
     FCR31 |= 0x800000;
   else FCR31 &= ~0x800000;
   interp_addr+=4;
}

static void C_ULE_S()
{
   if (isnan(*reg_cop1_simple[cffs]) || isnan(*reg_cop1_simple[cfft]) ||
       *reg_cop1_simple[cffs] <= *reg_cop1_simple[cfft])
     FCR31 |= 0x800000;
   else FCR31 &= ~0x800000;
   interp_addr+=4;
}

static void C_SF_S()
{
   if (isnan(*reg_cop1_simple[cffs]) || isnan(*reg_cop1_simple[cfft]))
     {
	printf("Invalid operation exception in C opcode\n");
	stop=1;
#ifdef DEBUGON
  _break();
#endif     
     }
   FCR31 &= ~0x800000;
   interp_addr+=4;
}

static void C_NGLE_S()
{
   if (isnan(*reg_cop1_simple[cffs]) || isnan(*reg_cop1_simple[cfft]))
     {
	printf("Invalid operation exception in C opcode\n");
	stop=1;
#ifdef DEBUGON
  _break();
#endif     
     }
   FCR31 &= ~0x800000;
   interp_addr+=4;
}

static void C_SEQ_S()
{
   if (isnan(*reg_cop1_simple[cffs]) || isnan(*reg_cop1_simple[cfft]))
     {
	printf("Invalid operation exception in C opcode\n");
	stop=1;
#ifdef DEBUGON
  _break();
#endif     
     }
   if (*reg_cop1_simple[cffs] == *reg_cop1_simple[cfft])
     FCR31 |= 0x800000;
   else FCR31 &= ~0x800000;
   interp_addr+=4;
}

static void C_NGL_S()
{
   if (isnan(*reg_cop1_simple[cffs]) || isnan(*reg_cop1_simple[cfft]))
     {
	printf("Invalid operation exception in C opcode\n");
	stop=1;
#ifdef DEBUGON
  _break();
#endif     
     }
   if (*reg_cop1_simple[cffs] == *reg_cop1_simple[cfft])
     FCR31 |= 0x800000;
   else FCR31 &= ~0x800000;
   interp_addr+=4;
}

static void C_LT_S()
{
   if (isnan(*reg_cop1_simple[cffs]) || isnan(*reg_cop1_simple[cfft]))
     {
	printf("Invalid operation exception in C opcode\n");
	stop=1;
#ifdef DEBUGON
  _break();
#endif     
     }
   if (*reg_cop1_simple[cffs] < *reg_cop1_simple[cfft])
     FCR31 |= 0x800000;
   else FCR31 &= ~0x800000;
   interp_addr+=4;
}

static void C_NGE_S()
{
   if (isnan(*reg_cop1_simple[cffs]) || isnan(*reg_cop1_simple[cfft]))
     {
	printf("Invalid operation exception in C opcode\n");
	stop=1;
#ifdef DEBUGON
  _break();
#endif     
     }
   if (*reg_cop1_simple[cffs] < *reg_cop1_simple[cfft])
     FCR31 |= 0x800000;
   else FCR31 &= ~0x800000;
   interp_addr+=4;
}

static void C_LE_S()
{
   if (isnan(*reg_cop1_simple[cffs]) || isnan(*reg_cop1_simple[cfft]))
     {
	printf("Invalid operation exception in C opcode\n");
	stop=1;
#ifdef DEBUGON
  _break();
#endif     
     }
   if (*reg_cop1_simple[cffs] <= *reg_cop1_simple[cfft])
     FCR31 |= 0x800000;
   else FCR31 &= ~0x800000;
   interp_addr+=4;
}

static void C_NGT_S()
{
   if (isnan(*reg_cop1_simple[cffs]) || isnan(*reg_cop1_simple[cfft]))
     {
	printf("Invalid operation exception in C opcode\n");
	stop=1;
#ifdef DEBUGON
  _break();
#endif     
     }
   if (*reg_cop1_simple[cffs] <= *reg_cop1_simple[cfft])
     FCR31 |= 0x800000;
   else FCR31 &= ~0x800000;
   interp_addr+=4;
}

static void (*interp_cop1_s[64])(void) =
{
ADD_S    ,SUB_S    ,MUL_S   ,DIV_S    ,SQRT_S   ,ABS_S    ,MOV_S   ,NEG_S    ,
ROUND_L_S,TRUNC_L_S,CEIL_L_S,FLOOR_L_S,ROUND_W_S,TRUNC_W_S,CEIL_W_S,FLOOR_W_S,
NI       ,NI       ,NI      ,NI       ,NI       ,NI       ,NI      ,NI       ,
NI       ,NI       ,NI      ,NI       ,NI       ,NI       ,NI      ,NI       ,
NI       ,CVT_D_S  ,NI      ,NI       ,CVT_W_S  ,CVT_L_S  ,NI      ,NI       ,
NI       ,NI       ,NI      ,NI       ,NI       ,NI       ,NI      ,NI       ,
C_F_S    ,C_UN_S   ,C_EQ_S  ,C_UEQ_S  ,C_OLT_S  ,C_ULT_S  ,C_OLE_S ,C_ULE_S  ,
C_SF_S   ,C_NGLE_S ,C_SEQ_S ,C_NGL_S  ,C_LT_S   ,C_NGE_S  ,C_LE_S  ,C_NGT_S
};

static void ADD_D()
{
   set_rounding();
   *reg_cop1_double[cffd] = *reg_cop1_double[cffs] +
     *reg_cop1_double[cfft];
   interp_addr+=4;
}

static void SUB_D()
{
   set_rounding();
   *reg_cop1_double[cffd] = *reg_cop1_double[cffs] -
     *reg_cop1_double[cfft];
   interp_addr+=4;
}

static void MUL_D()
{
   set_rounding();
   *reg_cop1_double[cffd] = *reg_cop1_double[cffs] *
     *reg_cop1_double[cfft];
   interp_addr+=4;
}

static void DIV_D()
{
   if((FCR31 & 0x400) && *reg_cop1_double[cfft] == 0)
     {
	//FCR31 |= 0x8020;
	/*FCR31 |= 0x8000;
	Cause = 15 << 2;
	exception_general();*/
	printf("div_d by 0\n");
	//return;
     }
   set_rounding();
   *reg_cop1_double[cffd] = *reg_cop1_double[cffs] /
     *reg_cop1_double[cfft];
   interp_addr+=4;
}

static void SQRT_D()
{
   set_rounding();
   *reg_cop1_double[cffd] = sqrt(*reg_cop1_double[cffs]);
   interp_addr+=4;
}

static void ABS_D()
{
   set_rounding();
   *reg_cop1_double[cffd] = fabs(*reg_cop1_double[cffs]);
   interp_addr+=4;
}

static void MOV_D()
{
   set_rounding();
   *reg_cop1_double[cffd] = *reg_cop1_double[cffs];
   interp_addr+=4;
}

static void NEG_D()
{
   set_rounding();
   *reg_cop1_double[cffd] = -(*reg_cop1_double[cffs]);
   interp_addr+=4;
}

static void ROUND_L_D()
{
   set_round();
   *((long long*)(reg_cop1_double[cffd])) = *reg_cop1_double[cffs];
   interp_addr+=4;
}

static void TRUNC_L_D()
{
   set_trunc();
   *((long long*)(reg_cop1_double[cffd])) = *reg_cop1_double[cffs];
   interp_addr+=4;
}

static void CEIL_L_D()
{
   set_ceil();
   *((long long*)(reg_cop1_double[cffd])) = *reg_cop1_double[cffs];
   interp_addr+=4;
}

static void FLOOR_L_D()
{
   set_floor();
   *((long long*)(reg_cop1_double[cffd])) = *reg_cop1_double[cffs];
   interp_addr+=4;
}

static void ROUND_W_D()
{
   set_round();
   *((long*)reg_cop1_simple[cffd]) = *reg_cop1_double[cffs];
   interp_addr+=4;
}

static void TRUNC_W_D()
{
   set_trunc();
   *((long*)reg_cop1_simple[cffd]) = *reg_cop1_double[cffs];
   interp_addr+=4;
}

static void CEIL_W_D()
{
   set_ceil();
   *((long*)reg_cop1_simple[cffd]) = *reg_cop1_double[cffs];
   interp_addr+=4;
}

static void FLOOR_W_D()
{
   set_floor();
   *((long*)reg_cop1_simple[cffd]) = *reg_cop1_double[cffs];
   interp_addr+=4;
}

static void CVT_S_D()
{
   set_rounding();
   *reg_cop1_simple[cffd] = *reg_cop1_double[cffs];
   interp_addr+=4;
}

static void CVT_W_D()
{
   set_rounding();
   *((long*)reg_cop1_simple[cffd]) = *reg_cop1_double[cffs];
   interp_addr+=4;
}

static void CVT_L_D()
{
   set_rounding();
   *((long long*)(reg_cop1_double[cffd])) = *reg_cop1_double[cffs];
   interp_addr+=4;
}

static void C_F_D()
{
   FCR31 &= ~0x800000;
   interp_addr+=4;
}

static void C_UN_D()
{
   if (isnan(*reg_cop1_double[cffs]) || isnan(*reg_cop1_double[cfft]))
     FCR31 |= 0x800000;
   else FCR31 &= ~0x800000;
   interp_addr+=4;
}

static void C_EQ_D()
{
   if (!isnan(*reg_cop1_double[cffs]) && !isnan(*reg_cop1_double[cfft]) &&
       *reg_cop1_double[cffs] == *reg_cop1_double[cfft])
     FCR31 |= 0x800000;
   else FCR31 &= ~0x800000;
   interp_addr+=4;
}

static void C_UEQ_D()
{
   if (isnan(*reg_cop1_double[cffs]) || isnan(*reg_cop1_double[cfft]) ||
       *reg_cop1_double[cffs] == *reg_cop1_double[cfft])
     FCR31 |= 0x800000;
   else FCR31 &= ~0x800000;
   interp_addr+=4;
}

static void C_OLT_D()
{
   if (!isnan(*reg_cop1_double[cffs]) && !isnan(*reg_cop1_double[cfft]) &&
       *reg_cop1_double[cffs] < *reg_cop1_double[cfft])
     FCR31 |= 0x800000;
   else FCR31 &= ~0x800000;
   interp_addr+=4;
}

static void C_ULT_D()
{
   if (isnan(*reg_cop1_double[cffs]) || isnan(*reg_cop1_double[cfft]) ||
       *reg_cop1_double[cffs] < *reg_cop1_double[cfft])
     FCR31 |= 0x800000;
   else FCR31 &= ~0x800000;
   interp_addr+=4;
}

static void C_OLE_D()
{
   if (!isnan(*reg_cop1_double[cffs]) && !isnan(*reg_cop1_double[cfft]) &&
       *reg_cop1_double[cffs] <= *reg_cop1_double[cfft])
     FCR31 |= 0x800000;
   else FCR31 &= ~0x800000;
   interp_addr+=4;
}

static void C_ULE_D()
{
   if (isnan(*reg_cop1_double[cffs]) || isnan(*reg_cop1_double[cfft]) ||
       *reg_cop1_double[cffs] <= *reg_cop1_double[cfft])
     FCR31 |= 0x800000;
   else FCR31 &= ~0x800000;
   interp_addr+=4;
}

static void C_SF_D()
{
   if (isnan(*reg_cop1_double[cffs]) || isnan(*reg_cop1_double[cfft]))
     {
	printf("Invalid operation exception in C opcode\n");
	stop=1;
#ifdef DEBUGON
  _break();
#endif     
     }
   FCR31 &= ~0x800000;
   interp_addr+=4;
}

static void C_NGLE_D()
{
   if (isnan(*reg_cop1_double[cffs]) || isnan(*reg_cop1_double[cfft]))
     {
	printf("Invalid operation exception in C opcode\n");
	stop=1;
#ifdef DEBUGON
  _break();
#endif     
     }
   FCR31 &= ~0x800000;
   interp_addr+=4;
}

static void C_SEQ_D()
{
   if (isnan(*reg_cop1_double[cffs]) || isnan(*reg_cop1_double[cfft]))
     {
	printf("Invalid operation exception in C opcode\n");
	stop=1;
#ifdef DEBUGON
  _break();
#endif     
     }
   if (*reg_cop1_double[cffs] == *reg_cop1_double[cfft])
     FCR31 |= 0x800000;
   else FCR31 &= ~0x800000;
   interp_addr+=4;
}

static void C_NGL_D()
{
   if (isnan(*reg_cop1_double[cffs]) || isnan(*reg_cop1_double[cfft]))
     {
	printf("Invalid operation exception in C opcode\n");
	stop=1;
#ifdef DEBUGON
  _break();
#endif     
     }
   if (*reg_cop1_double[cffs] == *reg_cop1_double[cfft])
     FCR31 |= 0x800000;
   else FCR31 &= ~0x800000;
   interp_addr+=4;
}

static void C_LT_D()
{
   if (isnan(*reg_cop1_double[cffs]) || isnan(*reg_cop1_double[cfft]))
     {
	printf("Invalid operation exception in C opcode\n");
	stop=1;
#ifdef DEBUGON
  _break();
#endif     
     }
   if (*reg_cop1_double[cffs] < *reg_cop1_double[cfft])
     FCR31 |= 0x800000;
   else FCR31 &= ~0x800000;
   interp_addr+=4;
}

static void C_NGE_D()
{
   if (isnan(*reg_cop1_double[cffs]) || isnan(*reg_cop1_double[cfft]))
     {
	printf("Invalid operation exception in C opcode\n");
	stop=1;
#ifdef DEBUGON
  _break();
#endif     
     }
   if (*reg_cop1_double[cffs] < *reg_cop1_double[cfft])
     FCR31 |= 0x800000;
   else FCR31 &= ~0x800000;
   interp_addr+=4;
}

static void C_LE_D()
{
   if (isnan(*reg_cop1_double[cffs]) || isnan(*reg_cop1_double[cfft]))
     {
	printf("Invalid operation exception in C opcode\n");
	stop=1;
#ifdef DEBUGON
  _break();
#endif     
     }
   if (*reg_cop1_double[cffs] <= *reg_cop1_double[cfft])
     FCR31 |= 0x800000;
   else FCR31 &= ~0x800000;
   interp_addr+=4;
}

static void C_NGT_D()
{
   if (isnan(*reg_cop1_double[cffs]) || isnan(*reg_cop1_double[cfft]))
     {
	printf("Invalid operation exception in C opcode\n");
	stop=1;
#ifdef DEBUGON
  _break();
#endif     
     }
   if (*reg_cop1_double[cffs] <= *reg_cop1_double[cfft])
     FCR31 |= 0x800000;
   else FCR31 &= ~0x800000;
   interp_addr+=4;
}

static void (*interp_cop1_d[64])(void) =
{
ADD_D    ,SUB_D    ,MUL_D   ,DIV_D    ,SQRT_D   ,ABS_D    ,MOV_D   ,NEG_D    ,
ROUND_L_D,TRUNC_L_D,CEIL_L_D,FLOOR_L_D,ROUND_W_D,TRUNC_W_D,CEIL_W_D,FLOOR_W_D,
NI       ,NI       ,NI      ,NI       ,NI       ,NI       ,NI      ,NI       ,
NI       ,NI       ,NI      ,NI       ,NI       ,NI       ,NI      ,NI       ,
CVT_S_D  ,NI       ,NI      ,NI       ,CVT_W_D  ,CVT_L_D  ,NI      ,NI       ,
NI       ,NI       ,NI      ,NI       ,NI       ,NI       ,NI      ,NI       ,
C_F_D    ,C_UN_D   ,C_EQ_D  ,C_UEQ_D  ,C_OLT_D  ,C_ULT_D  ,C_OLE_D ,C_ULE_D  ,
C_SF_D   ,C_NGLE_D ,C_SEQ_D ,C_NGL_D  ,C_LT_D   ,C_NGE_D  ,C_LE_D  ,C_NGT_D
};

static void CVT_S_W()
{
   set_rounding();
   *reg_cop1_simple[cffd] = *((long*)reg_cop1_simple[cffs]);
   interp_addr+=4;
}

static void CVT_D_W()
{
   set_rounding();
   *reg_cop1_double[cffd] = *((long*)reg_cop1_simple[cffs]);
   interp_addr+=4;
}

static void (*interp_cop1_w[64])(void) =
{
   NI     , NI     , NI, NI, NI, NI, NI, NI,
   NI     , NI     , NI, NI, NI, NI, NI, NI,
   NI     , NI     , NI, NI, NI, NI, NI, NI,
   NI     , NI     , NI, NI, NI, NI, NI, NI,
   CVT_S_W, CVT_D_W, NI, NI, NI, NI, NI, NI,
   NI     , NI     , NI, NI, NI, NI, NI, NI,
   NI     , NI     , NI, NI, NI, NI, NI, NI,
   NI     , NI     , NI, NI, NI, NI, NI, NI
};

static void CVT_S_L()
{
   set_rounding();
   *reg_cop1_simple[cffd] = *((long long*)(reg_cop1_double[cffs]));
   interp_addr+=4;
}

static void CVT_D_L()
{
   set_rounding();
   *reg_cop1_double[cffd] = *((long long*)(reg_cop1_double[cffs]));
   interp_addr+=4;
}

static void (*interp_cop1_l[64])(void) =
{
   NI     , NI     , NI, NI, NI, NI, NI, NI,
   NI     , NI     , NI, NI, NI, NI, NI, NI,
   NI     , NI     , NI, NI, NI, NI, NI, NI,
   NI     , NI     , NI, NI, NI, NI, NI, NI,
   CVT_S_L, CVT_D_L, NI, NI, NI, NI, NI, NI,
   NI     , NI     , NI, NI, NI, NI, NI, NI,
   NI     , NI     , NI, NI, NI, NI, NI, NI,
   NI     , NI     , NI, NI, NI, NI, NI, NI
};

static void MFC1()
{
   rrt32 = *((long*)reg_cop1_simple[rfs]);
   sign_extended(rrt);
   interp_addr+=4;
}

static void DMFC1()
{
   rrt = *((long long*)(reg_cop1_double[rfs]));
   interp_addr+=4;
}

static void CFC1()
{
   if (rfs==31)
     {
	rrt32 = FCR31;
	sign_extended(rrt);
     }
   if (rfs==0)
     {
	rrt32 = FCR0;
	sign_extended(rrt);
     }
   interp_addr+=4;
}

static void MTC1()
{
   *((long*)reg_cop1_simple[rfs]) = rrt32;
   interp_addr+=4;
}

static void DMTC1()
{
   *((long long*)reg_cop1_double[rfs]) = rrt;
   interp_addr+=4;
}

static void CTC1()
{
   if (rfs==31)
     FCR31 = rrt32;
   switch((FCR31 & 3))
     {
      case 0:
	rounding_mode = 0x33F;
	break;
      case 1:
	rounding_mode = 0xF3F;
	break;
      case 2:
	rounding_mode = 0xB3F;
	break;
      case 3:
	rounding_mode = 0x73F;
	break;
     }
   //if ((FCR31 >> 7) & 0x1F) printf("FPU Exception enabled : %x\n",
//				   (int)((FCR31 >> 7) & 0x1F));
   interp_addr+=4;
}

static void BC()
{
   interp_cop1_bc[(op >> 16) & 3]();
}

static void S()
{
   interp_cop1_s[(op & 0x3F)]();
}

static void D()
{
   interp_cop1_d[(op & 0x3F)]();
}

static void W()
{
   interp_cop1_w[(op & 0x3F)]();
}

static void L()
{
   interp_cop1_l[(op & 0x3F)]();
}

static void (*interp_cop1[32])(void) =
{
   MFC1, DMFC1, CFC1, NI, MTC1, DMTC1, CTC1, NI,
   BC  , NI   , NI  , NI, NI  , NI   , NI  , NI,
   S   , D    , NI  , NI, W   , L    , NI  , NI,
   NI  , NI   , NI  , NI, NI  , NI   , NI  , NI
};

static void SPECIAL()
{
   interp_special[(op & 0x3F)]();
}

static void REGIMM()
{
   interp_regimm[((op >> 16) & 0x1F)]();
}

static void J()
{
   unsigned long naddr = (PC->f.j.inst_index<<2) | (interp_addr & 0xF0000000);
   if (naddr == interp_addr)
     {
	if (probe_nop(interp_addr+4))
	  {
	     update_count();
	     skip = next_interupt - Count;
	     if (skip > 3)
	       {
		  Count += (skip & 0xFFFFFFFC);
		  return;
	       }
	  }
     }
   interp_addr+=4;
   delay_slot=1;
   prefetch();
   interp_ops[((op >> 26) & 0x3F)]();
   update_count();
   delay_slot=0;
   interp_addr = naddr;
   last_addr = interp_addr;
   if (next_interupt <= Count) gen_interupt();
}

static void JAL()
{
   unsigned long naddr = (PC->f.j.inst_index<<2) | (interp_addr & 0xF0000000);
   if (naddr == interp_addr)
     {
	if (probe_nop(interp_addr+4))
	  {
	     update_count();
	     skip = next_interupt - Count;
	     if (skip > 3)
	       {
		  Count += (skip & 0xFFFFFFFC);
		  return;
	       }
	  }
     }
   interp_addr+=4;
   delay_slot=1;
   prefetch();
   interp_ops[((op >> 26) & 0x3F)]();
   update_count();
   delay_slot=0;
   if (!skip_jump)
     {
	reg[31]=interp_addr;
	sign_extended(reg[31]);

	interp_addr = naddr;
     }
   last_addr = interp_addr;
   if (next_interupt <= Count) gen_interupt();
}

static void BEQ()
{
   short local_immediate = iimmediate;
   local_rs = irs;
   local_rt = irt;
   if ((interp_addr + (local_immediate+1)*4) == interp_addr)
     if (local_rs == local_rt)
       {
	  if (probe_nop(interp_addr+4))
	    {
	       update_count();
	       skip = next_interupt - Count;
	       if (skip > 3)
		 {
		    Count += (skip & 0xFFFFFFFC);
		    return;
		 }
	    }
       }
   interp_addr+=4;
   delay_slot=1;
   prefetch();
   interp_ops[((op >> 26) & 0x3F)]();
   update_count();
   delay_slot=0;
   if (local_rs == local_rt)
     interp_addr += (local_immediate-1)*4;
   last_addr = interp_addr;
   if (next_interupt <= Count) gen_interupt();
}

static void BNE()
{
   short local_immediate = iimmediate;
   local_rs = irs;
   local_rt = irt;
   if ((interp_addr + (local_immediate+1)*4) == interp_addr)
     if (local_rs != local_rt)
       {
	  if (probe_nop(interp_addr+4))
	    {
	       update_count();
	       skip = next_interupt - Count;
	       if (skip > 3)
		 {
		    Count += (skip & 0xFFFFFFFC);
		    return;
		 }
	    }
       }
   interp_addr+=4;
   delay_slot=1;
   prefetch();
   interp_ops[((op >> 26) & 0x3F)]();
   update_count();
   delay_slot=0;
   if (local_rs != local_rt)
     interp_addr += (local_immediate-1)*4;
   last_addr = interp_addr;
   if (next_interupt <= Count) gen_interupt();
}

static void BLEZ()
{
   short local_immediate = iimmediate;
   local_rs = irs;
   if ((interp_addr + (local_immediate+1)*4) == interp_addr)
     if (local_rs <= 0)
       {
	  if (probe_nop(interp_addr+4))
	    {
	       update_count();
	       skip = next_interupt - Count;
	       if (skip > 3)
		 {
		    Count += (skip & 0xFFFFFFFC);
		    return;
		 }
	    }
       }
   interp_addr+=4;
   delay_slot=1;
   prefetch();
   interp_ops[((op >> 26) & 0x3F)]();
   update_count();
   delay_slot=0;
   if (local_rs <= 0)
     interp_addr += (local_immediate-1)*4;
   last_addr = interp_addr;
   if (next_interupt <= Count) gen_interupt();
}

static void BGTZ()
{
   short local_immediate = iimmediate;
   local_rs = irs;
   if ((interp_addr + (local_immediate+1)*4) == interp_addr)
     if (local_rs > 0)
       {
	  if (probe_nop(interp_addr+4))
	    {
	       update_count();
	       skip = next_interupt - Count;
	       if (skip > 3)
		 {
		    Count += (skip & 0xFFFFFFFC);
		    return;
		 }
	    }
       }
   interp_addr+=4;
   delay_slot=1;
   prefetch();
   interp_ops[((op >> 26) & 0x3F)]();
   update_count();
   delay_slot=0;
   if (local_rs > 0)
     interp_addr += (local_immediate-1)*4;
   last_addr = interp_addr;
   if (next_interupt <= Count) gen_interupt();
}

static void ADDI()
{
   irt32 = irs32 + iimmediate;
   sign_extended(irt);
   interp_addr+=4;
}

static void ADDIU()
{
   irt32 = irs32 + iimmediate;
   sign_extended(irt);
   interp_addr+=4;
}

static void SLTI()
{
   if (irs < iimmediate) irt = 1;
   else irt = 0;
   interp_addr+=4;
}

static void SLTIU()
{
   if ((unsigned long long)irs < (unsigned long long)((long long)iimmediate))
     irt = 1;
   else irt = 0;
   interp_addr+=4;
}

static void ANDI()
{
   irt = irs & (unsigned short)iimmediate;
   interp_addr+=4;
}

static void ORI()
{
   irt = irs | (unsigned short)iimmediate;
   interp_addr+=4;
}

static void XORI()
{
   irt = irs ^ (unsigned short)iimmediate;
   interp_addr+=4;
}

static void LUI()
{
   irt32 = iimmediate << 16;
   sign_extended(irt);
   interp_addr+=4;
}

static void COP0()
{
   interp_cop0[((op >> 21) & 0x1F)]();
}

static void COP1()
{
   if (check_cop1_unusable()) return;
   start_section(FP_SECTION);
   interp_cop1[((op >> 21) & 0x1F)]();
   end_section(FP_SECTION);
}

static void BEQL()
{
   short local_immediate = iimmediate;
   local_rs = irs;
   local_rt = irt;
   if ((interp_addr + (local_immediate+1)*4) == interp_addr)
     if (irs == irt)
       {
	  if (probe_nop(interp_addr+4))
	    {
	       update_count();
	       skip = next_interupt - Count;
	       if (skip > 3)
		 {
		    Count += (skip & 0xFFFFFFFC);
		    return;
		 }
	    }
       }
   if (local_rs == local_rt)
     {
	interp_addr+=4;
	delay_slot=1;
	prefetch();
	interp_ops[((op >> 26) & 0x3F)]();
	update_count();
	delay_slot=0;
	interp_addr += (local_immediate-1)*4;
     }
   else
     {
	interp_addr+=8;
	update_count();
     }
   last_addr = interp_addr;
   if (next_interupt <= Count) gen_interupt();
}

static void BNEL()
{
   short local_immediate = iimmediate;
   local_rs = irs;
   local_rt = irt;
   if ((interp_addr + (local_immediate+1)*4) == interp_addr)
     if (irs != irt)
       {
	  if (probe_nop(interp_addr+4))
	    {
	       update_count();
	       skip = next_interupt - Count;
	       if (skip > 3)
		 {
		    Count += (skip & 0xFFFFFFFC);
		    return;
		 }
	    }
       }
   if (local_rs != local_rt)
     {
	interp_addr+=4;
	delay_slot=1;
	prefetch();
	interp_ops[((op >> 26) & 0x3F)]();
	update_count();
	delay_slot=0;
	interp_addr += (local_immediate-1)*4;
     }
   else
     {
	interp_addr+=8;
	update_count();
     }
   last_addr = interp_addr;
   if (next_interupt <= Count) gen_interupt();
}

static void BLEZL()
{
   short local_immediate = iimmediate;
   local_rs = irs;
   if ((interp_addr + (local_immediate+1)*4) == interp_addr)
     if (irs <= 0)
       {
	  if (probe_nop(interp_addr+4))
	    {
	       update_count();
	       skip = next_interupt - Count;
	       if (skip > 3)
		 {
		    Count += (skip & 0xFFFFFFFC);
		    return;
		 }
	    }
       }
   if (local_rs <= 0)
     {
	interp_addr+=4;
	delay_slot=1;
	prefetch();
	interp_ops[((op >> 26) & 0x3F)]();
	update_count();
	delay_slot=0;
	interp_addr += (local_immediate-1)*4;
     }
   else
     {
	interp_addr+=8;
	update_count();
     }
   last_addr = interp_addr;
   if (next_interupt <= Count) gen_interupt();
}

static void BGTZL()
{
   short local_immediate = iimmediate;
   local_rs = irs;
   if ((interp_addr + (local_immediate+1)*4) == interp_addr)
     if (irs > 0)
       {
	  if (probe_nop(interp_addr+4))
	    {
	       update_count();
	       skip = next_interupt - Count;
	       if (skip > 3)
		 {
		    Count += (skip & 0xFFFFFFFC);
		    return;
		 }
	    }
       }
   if (local_rs > 0)
     {
	interp_addr+=4;
	delay_slot=1;
	prefetch();
	interp_ops[((op >> 26) & 0x3F)]();
	update_count();
	delay_slot=0;
	interp_addr += (local_immediate-1)*4;
     }
   else
     {
	interp_addr+=8;
	update_count();
     }
   last_addr = interp_addr;
   if (next_interupt <= Count) gen_interupt();
}

static void DADDI()
{
   irt = irs + iimmediate;
   interp_addr+=4;
}

static void DADDIU()
{
   irt = irs + iimmediate;
   interp_addr+=4;
}

static void LDL()
{
   unsigned long long int word = 0;
   interp_addr+=4;
   switch ((iimmediate + irs32) & 7)
     {
      case 0:
	address = iimmediate + irs32;
	rdword = &irt;
	read_dword_in_memory();
	break;
      case 1:
	address = (iimmediate + irs32) & 0xFFFFFFF8;
	rdword = &word;
	read_dword_in_memory();
	irt = (irt & 0xFF) | (word << 8);
	break;
      case 2:
	address = (iimmediate + irs32) & 0xFFFFFFF8;
	rdword = &word;
	read_dword_in_memory();
	irt = (irt & 0xFFFF) | (word << 16);
	break;
      case 3:
	address = (iimmediate + irs32) & 0xFFFFFFF8;
	rdword = &word;
	read_dword_in_memory();
	irt = (irt & 0xFFFFFF) | (word << 24);
	break;
      case 4:
	address = (iimmediate + irs32) & 0xFFFFFFF8;
	rdword = &word;
	read_dword_in_memory();
	irt = (irt & 0xFFFFFFFF) | (word << 32);
	break;
      case 5:
	address = (iimmediate + irs32) & 0xFFFFFFF8;
	rdword = &word;
	read_dword_in_memory();
	irt = (irt & 0xFFFFFFFFFFLL) | (word << 40);
	break;
      case 6:
	address = (iimmediate + irs32) & 0xFFFFFFF8;
	rdword = &word;
	read_dword_in_memory();
	irt = (irt & 0xFFFFFFFFFFFFLL) | (word << 48);
	break;
      case 7:
	address = (iimmediate + irs32) & 0xFFFFFFF8;
	rdword = &word;
	read_dword_in_memory();
	irt = (irt & 0xFFFFFFFFFFFFFFLL) | (word << 56);
	break;
     }
}

static void LDR()
{
   unsigned long long int word = 0;
   interp_addr+=4;
   switch ((iimmediate + irs32) & 7)
     {
      case 0:
	address = (iimmediate + irs32) & 0xFFFFFFF8;
	rdword = &word;
	read_dword_in_memory();
	irt = (irt & 0xFFFFFFFFFFFFFF00LL) | (word >> 56);
	break;
      case 1:
	address = (iimmediate + irs32) & 0xFFFFFFF8;
	rdword = &word;
	read_dword_in_memory();
	irt = (irt & 0xFFFFFFFFFFFF0000LL) | (word >> 48);
	break;
      case 2:
	address = (iimmediate + irs32) & 0xFFFFFFF8;
	rdword = &word;
	read_dword_in_memory();
	irt = (irt & 0xFFFFFFFFFF000000LL) | (word >> 40);
	break;
      case 3:
	address = (iimmediate + irs32) & 0xFFFFFFF8;
	rdword = &word;
	read_dword_in_memory();
	irt = (irt & 0xFFFFFFFF00000000LL) | (word >> 32);
	break;
      case 4:
	address = (iimmediate + irs32) & 0xFFFFFFF8;
	rdword = &word;
	read_dword_in_memory();
	irt = (irt & 0xFFFFFF0000000000LL) | (word >> 24);
	break;
      case 5:
	address = (iimmediate + irs32) & 0xFFFFFFF8;
	rdword = &word;
	read_dword_in_memory();
	irt = (irt & 0xFFFF000000000000LL) | (word >> 16);
	break;
      case 6:
	address = (iimmediate + irs32) & 0xFFFFFFF8;
	rdword = &word;
	read_dword_in_memory();
	irt = (irt & 0xFF00000000000000LL) | (word >> 8);
	break;
      case 7:
	address = (iimmediate + irs32) & 0xFFFFFFF8;
	rdword = &irt;
	read_dword_in_memory();
	break;
     }
}

static void LB()
{
   interp_addr+=4;
   address = iimmediate + irs32;
   rdword = &irt;
   read_byte_in_memory();
   sign_extendedb(irt);

}

static void LH()
{
   interp_addr+=4;
   address = iimmediate + irs32;
   rdword = &irt;
   read_hword_in_memory();
   sign_extendedh(irt);
}

static void LWL()
{
   unsigned long long int word = 0;
   interp_addr+=4;
   switch ((iimmediate + irs32) & 3)
     {
      case 0:
	address = iimmediate + irs32;
	rdword = &irt;
	read_word_in_memory();
	break;
      case 1:
	address = (iimmediate + irs32) & 0xFFFFFFFC;
	rdword = &word;
	read_word_in_memory();
	irt = (irt & 0xFF) | (word << 8);
	break;
      case 2:
	address = (iimmediate + irs32) & 0xFFFFFFFC;
	rdword = &word;
	read_word_in_memory();
	irt = (irt & 0xFFFF) | (word << 16);
	break;
      case 3:
	address = (iimmediate + irs32) & 0xFFFFFFFC;
	rdword = &word;
	read_word_in_memory();
	irt = (irt & 0xFFFFFF) | (word << 24);
	break;
     }
   sign_extended(irt);
}

static void LW()
{
   address = iimmediate + irs32;
   rdword = &irt;
   interp_addr+=4;
   read_word_in_memory();
   sign_extended(irt);
}

static void LBU()
{
   interp_addr+=4;
   address = iimmediate + irs32;
   rdword = &irt;
   read_byte_in_memory();
}

static void LHU()
{
   interp_addr+=4;
   address = iimmediate + irs32;
   rdword = &irt;
   read_hword_in_memory();
}

static void LWR()
{
   unsigned long long int word = 0;
   interp_addr+=4;
   switch ((iimmediate + irs32) & 3)
     {
      case 0:
	address = (iimmediate + irs32) & 0xFFFFFFFC;
	rdword = &word;
	read_word_in_memory();
	irt = (irt & 0xFFFFFFFFFFFFFF00LL) | ((word >> 24) & 0xFF);
	break;
      case 1:
	address = (iimmediate + irs32) & 0xFFFFFFFC;
	rdword = &word;
	read_word_in_memory();
	irt = (irt & 0xFFFFFFFFFFFF0000LL) | ((word >> 16) & 0xFFFF);
	break;
      case 2:
	address = (iimmediate + irs32) & 0xFFFFFFFC;
	rdword = &word;
	read_word_in_memory();
	irt = (irt & 0xFFFFFFFFFF000000LL) | ((word >> 8) & 0xFFFFFF);
	break;
      case 3:
	address = (iimmediate + irs32) & 0xFFFFFFFC;
	rdword = &irt;
	read_word_in_memory();
	sign_extended(irt);
     }
}

static void LWU()
{
   address = iimmediate + irs32;
   rdword = &irt;
   interp_addr+=4;
   read_word_in_memory();
}

static void SB()
{
   interp_addr+=4;
   address = iimmediate + irs32;
   byte = (unsigned char)(irt & 0xFF);
   write_byte_in_memory();
   check_memory();
}

static void SH()
{
   interp_addr+=4;
   address = iimmediate + irs32;
   hword = (unsigned short)(irt & 0xFFFF);
   write_hword_in_memory();
   check_memory();
}
static void SWL()
{
   unsigned long long int old_word = 0;
   interp_addr+=4;
   switch ((iimmediate + irs32) & 3)
     {
      case 0:
	address = (iimmediate + irs32) & 0xFFFFFFFC;
	word = (unsigned long)irt;
	write_word_in_memory();
	check_memory();
	break;
      case 1:
	address = (iimmediate + irs32) & 0xFFFFFFFC;
	rdword = &old_word;
	read_word_in_memory();
	word = ((unsigned long)irt >> 8) | (old_word & 0xFF000000);
	write_word_in_memory();
	check_memory();
	break;
      case 2:
	address = (iimmediate + irs32) & 0xFFFFFFFC;
	rdword = &old_word;
	read_word_in_memory();
	word = ((unsigned long)irt >> 16) | (old_word & 0xFFFF0000);
	write_word_in_memory();
	check_memory();
	break;
      case 3:
	address = iimmediate + irs32;
	byte = (unsigned char)(irt >> 24);
	write_byte_in_memory();
	check_memory();
	break;
     }
}

static void SW()
{
   interp_addr+=4;
   address = iimmediate + irs32;
   word = (unsigned long)(irt & 0xFFFFFFFF);
   write_word_in_memory();
   check_memory();
}

static void SDL()
{
   unsigned long long int old_word = 0;
   interp_addr+=4;
   switch ((iimmediate + irs32) & 7)
     {
      case 0:
	address = (iimmediate + irs32) & 0xFFFFFFF8;
	dword = irt;
	write_dword_in_memory();
	check_memory();
	break;
      case 1:
	address = (iimmediate + irs32) & 0xFFFFFFF8;
	rdword = &old_word;
	read_dword_in_memory();
	dword = ((unsigned long long)irt >> 8)|(old_word & 0xFF00000000000000LL);
	write_dword_in_memory();
	check_memory();
	break;
      case 2:
	address = (iimmediate + irs32) & 0xFFFFFFF8;
	rdword = &old_word;
	read_dword_in_memory();
	dword = ((unsigned long long)irt >> 16)|(old_word & 0xFFFF000000000000LL);
	write_dword_in_memory();
	check_memory();
	break;
      case 3:
	address = (iimmediate + irs32) & 0xFFFFFFF8;
	rdword = &old_word;
	read_dword_in_memory();
	dword = ((unsigned long long)irt >> 24)|(old_word & 0xFFFFFF0000000000LL);
	write_dword_in_memory();
	check_memory();
	break;
      case 4:
	address = (iimmediate + irs32) & 0xFFFFFFF8;
	rdword = &old_word;
	read_dword_in_memory();
	dword = ((unsigned long long)irt >> 32)|(old_word & 0xFFFFFFFF00000000LL);
	write_dword_in_memory();
	check_memory();
	break;
      case 5:
	address = (iimmediate + irs32) & 0xFFFFFFF8;
	rdword = &old_word;
	read_dword_in_memory();
	dword = ((unsigned long long)irt >> 40)|(old_word & 0xFFFFFFFFFF000000LL);
	write_dword_in_memory();
	check_memory();
	break;
      case 6:
	address = (iimmediate + irs32) & 0xFFFFFFF8;
	rdword = &old_word;
	read_dword_in_memory();
	dword = ((unsigned long long)irt >> 48)|(old_word & 0xFFFFFFFFFFFF0000LL);
	write_dword_in_memory();
	check_memory();
	break;
      case 7:
	address = (iimmediate + irs32) & 0xFFFFFFF8;
	rdword = &old_word;
	read_dword_in_memory();
	dword = ((unsigned long long)irt >> 56)|(old_word & 0xFFFFFFFFFFFFFF00LL);
	write_dword_in_memory();
	check_memory();
	break;
     }
}

static void SDR()
{
   unsigned long long int old_word = 0;
   interp_addr+=4;
   switch ((iimmediate + irs32) & 7)
     {
      case 0:
	address = iimmediate + irs32;
	rdword = &old_word;
	read_dword_in_memory();
	dword = (irt << 56) | (old_word & 0x00FFFFFFFFFFFFFFLL);
	write_dword_in_memory();
	check_memory();
	break;
      case 1:
	address = (iimmediate + irs32) & 0xFFFFFFF8;
	rdword = &old_word;
	read_dword_in_memory();
	dword = (irt << 48) | (old_word & 0x0000FFFFFFFFFFFFLL);
	write_dword_in_memory();
	check_memory();
	break;
      case 2:
	address = (iimmediate + irs32) & 0xFFFFFFF8;
	rdword = &old_word;
	read_dword_in_memory();
	dword = (irt << 40) | (old_word & 0x000000FFFFFFFFFFLL);
	write_dword_in_memory();
	check_memory();
	break;
      case 3:
	address = (iimmediate + irs32) & 0xFFFFFFF8;
	rdword = &old_word;
	read_dword_in_memory();
	dword = (irt << 32) | (old_word & 0x00000000FFFFFFFFLL);
	write_dword_in_memory();
	check_memory();
	break;
      case 4:
	address = (iimmediate + irs32) & 0xFFFFFFF8;
	rdword = &old_word;
	read_dword_in_memory();
	dword = (irt << 24) | (old_word & 0x0000000000FFFFFFLL);
	write_dword_in_memory();
	check_memory();
	break;
      case 5:
	address = (iimmediate + irs32) & 0xFFFFFFF8;
	rdword = &old_word;
	read_dword_in_memory();
	dword = (irt << 16) | (old_word & 0x000000000000FFFFLL);
	write_dword_in_memory();
	check_memory();
	break;
      case 6:
	address = (iimmediate + irs32) & 0xFFFFFFF8;
	rdword = &old_word;
	read_dword_in_memory();
	dword = (irt << 8) | (old_word & 0x00000000000000FFLL);
	write_dword_in_memory();
	check_memory();
	break;
      case 7:
	address = (iimmediate + irs32) & 0xFFFFFFF8;
	dword = irt;
	write_dword_in_memory();
	check_memory();
	break;
     }
}

static void SWR()
{
   unsigned long long int old_word = 0;
   interp_addr+=4;
   switch ((iimmediate + irs32) & 3)
     {
      case 0:
	address = iimmediate + irs32;
	rdword = &old_word;
	read_word_in_memory();
	word = ((unsigned long)irt << 24) | (old_word & 0x00FFFFFF);
	write_word_in_memory();
	check_memory();
	break;
      case 1:
	address = (iimmediate + irs32) & 0xFFFFFFFC;
	rdword = &old_word;
	read_word_in_memory();
	word = ((unsigned long)irt << 16) | (old_word & 0x0000FFFF);
	write_word_in_memory();
	check_memory();
	break;
      case 2:
	address = (iimmediate + irs32) & 0xFFFFFFFC;
	rdword = &old_word;
	read_word_in_memory();
	word = ((unsigned long)irt << 8) | (old_word & 0x000000FF);
	write_word_in_memory();
	check_memory();
	break;
      case 3:
	address = (iimmediate + irs32) & 0xFFFFFFFC;
	word = (unsigned long)irt;
	write_word_in_memory();
	check_memory();
	break;
     }
}

static void CACHE()
{
   interp_addr+=4;
}

static void LL()
{
   address = iimmediate + irs32;
   rdword = &irt;
   interp_addr+=4;
   read_word_in_memory();
   sign_extended(irt);
   llbit = 1;
}

static void LWC1()
{
   unsigned long long int temp;
   if (check_cop1_unusable()) return;
   interp_addr+=4;
   address = lfoffset+reg[lfbase];
   rdword = &temp;
   read_word_in_memory();
   *((long*)reg_cop1_simple[lfft]) = *rdword;
}

static void LDC1()
{
   if (check_cop1_unusable()) return;
   interp_addr+=4;
   address = lfoffset+reg[lfbase];
   rdword = (long long*)reg_cop1_double[lfft];
   read_dword_in_memory();
}

static void LD()
{
   interp_addr+=4;
   address = iimmediate + irs32;
   rdword = &irt;
   read_dword_in_memory();
}

static void SC()
{
   interp_addr+=4;
   if(llbit)
     {
	address = iimmediate + irs32;
	word = (unsigned long)(irt & 0xFFFFFFFF);
	write_word_in_memory();
	check_memory();
	llbit = 0;
	irt = 1;
     }
   else
     {
	irt = 0;
     }
}

static void SWC1()
{
   if (check_cop1_unusable()) return;
   interp_addr+=4;
   address = lfoffset+reg[lfbase];
   word = *((long*)reg_cop1_simple[lfft]);
   write_word_in_memory();
   check_memory();
}

static void SDC1()
{
   if (check_cop1_unusable()) return;
   interp_addr+=4;
   address = lfoffset+reg[lfbase];
   dword = *((unsigned long long*)reg_cop1_double[lfft]);
   write_dword_in_memory();
   check_memory();
}

static void SD()
{
   interp_addr+=4;
   address = iimmediate + irs32;
   dword = irt;
   write_dword_in_memory();
   check_memory();
}

/*static*/ void (*interp_ops[64])(void) =
{
   SPECIAL, REGIMM, J   , JAL  , BEQ , BNE , BLEZ , BGTZ ,
   ADDI   , ADDIU , SLTI, SLTIU, ANDI, ORI , XORI , LUI  ,
   COP0   , COP1  , NI  , NI   , BEQL, BNEL, BLEZL, BGTZL,
   DADDI  , DADDIU, LDL , LDR  , NI  , NI  , NI   , NI   ,
   LB     , LH    , LWL , LW   , LBU , LHU , LWR  , LWU  ,
   SB     , SH    , SWL , SW   , SDL , SDR , SWR  , CACHE,
   LL     , LWC1  , NI  , NI   , NI  , LDC1, NI   , LD   ,
   SC     , SWC1  , NI  , NI   , NI  , SDC1, NI   , SD
};

void prefetch()
{
   //static FILE *f = NULL;
   //static int line=1;
   //static int tlb_used = 0;
   //unsigned int comp;
   //if (!tlb_used)
     //{
	/*if (f==NULL) f = fopen("/mnt/windows/pcdeb.txt", "rb");
	fscanf(f, "%x", &comp);
	if (comp != interp_addr)
	  {
	     printf("diff@%x, line:%d\n", interp_addr, line);
	     stop=1;
	  }*/
	//line++;
	//if ((debug_count+Count) > 0x50fe000) printf("line:%d\n", line);
	/*if ((debug_count+Count) > 0xb70000)
	  printf("count:%x, add:%x, op:%x, l%d\n", (int)(Count+debug_count),
		 interp_addr, op, line);*/
     //}
   //printf("addr:%x\n", interp_addr);

   // --- Trying to track down a bug ---
   /*static disturbed = 0;
   if(reg[31] == 0x100000 && !disturbed){
   	disturbed = 1;
   	printf("LR disturbed at last address %08x\n", (int)last_addr);
	int i;
	for(i=-8; i<4; ++i)
		printf(" %08x%s",
	   	       rdram[(last_addr&0xFFFFFF)/4+i],
	   	       ((i+8)%4 == 3) ? "\n" : "");
   }
   static pi_reg_value;
   if(pi_register.pi_dram_addr_reg != pi_reg_value){
   	pi_reg_value = pi_register.pi_dram_addr_reg;
   	printf("PI DRAM addr reg changed to %08x\n at last_addr %08x, op: %08x, count: %u\n",
   	       pi_reg_value, (int)last_addr, rdram[(last_addr&0xFFFFFF)/4], (unsigned int)debug_count+Count);
   }

   if(!((debug_count + Count) % 100000)) printf("%u instructions executed\n",
                                             (unsigned int)(debug_count + Count));*/
   // --- OK ---

if ((interp_addr >= 0x80000000) && (interp_addr < 0xc0000000))
     {
	if ((interp_addr >= 0x80000000) && (interp_addr < 0x80800000))
	  {
	     op = rdram[(interp_addr&0xFFFFFF)/4];
	     /*if ((debug_count+Count) > 234588)
	       printf("count:%x, addr:%x, op:%x\n", (int)(Count+debug_count),
		      interp_addr, op);*/
	     prefetch_opcode(op);
	  }
	else if ((interp_addr >= 0xa4000000) && (interp_addr < 0xa4001000))
	  {
	     op = SP_DMEM[(interp_addr&0xFFF)/4];
	     prefetch_opcode(op);
	  }
	else if ((interp_addr > 0xb0000000))
	  {
#ifdef __PPC__
		ROMCache_read(&op, (interp_addr & 0xFFFFFFF), 4);
#else
	     op = ((unsigned long*)rom)[(interp_addr & 0xFFFFFFF)/4];
#endif
	     prefetch_opcode(op);
	  }
	else
	  {
	     printf("execution &#65533; l'addresse :%x\n", (int)interp_addr);
	     stop=1;
#ifdef DEBUGON
       _break();
#endif     
	  }
     }
   else
     {
	unsigned long addr = interp_addr, phys;
	phys = virtual_to_physical_address(interp_addr, 2);
	if (phys != 0x00000000) interp_addr = phys;
	else
	  {
	     prefetch();
	     //tlb_used = 0;
	     return;
	  }
	//tlb_used = 1;
	prefetch();
	//tlb_used = 0;
	interp_addr = addr;
     }
}

void pure_interpreter()
{
   //interp_addr = 0xa4000040;
   stop=0;
   // FIXME: Do I have to adjust this now?
   //PC = malloc(sizeof(precomp_instr));
   last_addr = interp_addr;
   while (!stop)
     {
	prefetch();
#ifdef COMPARE_CORE
	compare_core();
#endif
	//if(interp_addr == 0x80000194) _break();
	//if (Count > 0x2000000) printf("inter:%x,%x\n", interp_addr,op);
	//if ((Count+debug_count) > 0xabaa2c) stop=1;
	interp_ops[((op >> 26) & 0x3F)]();

	//Count = (unsigned long)Count + 2;
	//if (interp_addr == 0x80000180) last_addr = interp_addr;
#ifdef DBG
	PC->addr = interp_addr;
	if (debugger_mode) update_debugger();
#endif
     }
   PC->addr = interp_addr;
}

void interprete_section(unsigned long addr)
{
   interp_addr = addr;
   PC = malloc(sizeof(precomp_instr));
   last_addr = interp_addr;
   while (!stop && (addr >> 12) == (interp_addr >> 12))
     {
	prefetch();
#ifdef COMPARE_CORE
	compare_core();
#endif
	PC->addr = interp_addr;
	interp_ops[((op >> 26) & 0x3F)]();
#ifdef DBG
	PC->addr = interp_addr;
	if (debugger_mode) update_debugger();
#endif
     }
   PC->addr = interp_addr;
}
