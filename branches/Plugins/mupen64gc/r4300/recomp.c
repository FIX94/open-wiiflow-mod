/**
 * Mupen64 - recomp.c
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

#include <malloc.h>

#include "recomp.h"
#include "macros.h"
#include "r4300.h"
#include "ops.h"
#include "../gc_memory/memory.h"
#include "Invalid_Code.h"
#include "Recomp-Cache.h"
#include "recomph.h"

#include "../gui/DEBUG.h"

// global variables :
precomp_instr *dst; // destination structure for the recompiled instruction
int code_length; // current real recompiled code length
int max_code_length; // current recompiled code's buffer length
unsigned char **inst_pointer; // output buffer for recompiled code
precomp_block *dst_block; // the current block that we are recompiling
long src; // the current recompiled instruction
int fast_memory;

unsigned long *return_address; // that's where the dynarec will restart when
                               // going back from a C function

static long *SRC; // currently recompiled instruction in the input stream
static int check_nop; // next instruction is nop ?
static int delay_slot_compiled = 0;


static void RSV()
{
   dst->ops = RESERVED;
//if(dynacore)
	//if(dynacore) genreserved();

}

static void RFIN_BLOCK()
{
   dst->ops = FIN_BLOCK;
//if(dynacore)
	//if(dynacore) genfin_block();

}

static void RNOTCOMPILED()
{
   dst->ops = NOTCOMPILED;
//if(dynacore)
	//if(dynacore) gennotcompiled();

}

static void recompile_standard_i_type()
{
   dst->f.i.rs = reg + ((src >> 21) & 0x1F);
   dst->f.i.rt = reg + ((src >> 16) & 0x1F);
   dst->f.i.immediate = src & 0xFFFF;
}

static void recompile_standard_j_type()
{
   dst->f.j.inst_index = src & 0x3FFFFFF;
}

static void recompile_standard_r_type()
{
   dst->f.r.rs = reg + ((src >> 21) & 0x1F);
   dst->f.r.rt = reg + ((src >> 16) & 0x1F);
   dst->f.r.rd = reg + ((src >> 11) & 0x1F);
   dst->f.r.sa = (src >>  6) & 0x1F;
}

static void recompile_standard_lf_type()
{
   dst->f.lf.base = (src >> 21) & 0x1F;
   dst->f.lf.ft = (src >> 16) & 0x1F;
   dst->f.lf.offset = src & 0xFFFF;
}

static void recompile_standard_cf_type()
{
   dst->f.cf.ft = (src >> 16) & 0x1F;
   dst->f.cf.fs = (src >> 11) & 0x1F;
   dst->f.cf.fd = (src >>  6) & 0x1F;
}

//-------------------------------------------------------------------------
//                                  SPECIAL                                
//-------------------------------------------------------------------------

static void RNOP()
{
   dst->ops = NOP;
//if(dynacore)
	//if(dynacore) gennop();

}

static void RSLL()
{
   dst->ops = SLL;
   recompile_standard_r_type();
   if (dst->f.r.rd == reg) RNOP();
   //else 
//if(dynacore)
	//if(dynacore) gensll();

}

static void RSRL()
{
   dst->ops = SRL;
   recompile_standard_r_type();
   if (dst->f.r.rd == reg) RNOP();
   //else 
//if(dynacore)
	//if(dynacore) gensrl();

}

static void RSRA()
{
   dst->ops = SRA;
   recompile_standard_r_type();
   if (dst->f.r.rd == reg) RNOP();
   //else 
//if(dynacore)
	//if(dynacore) gensra();

}

static void RSLLV()
{
   dst->ops = SLLV;
   recompile_standard_r_type();
   if (dst->f.r.rd == reg) RNOP();
   //else
//if(dynacore)
	//if(dynacore) gensllv();

}

static void RSRLV()
{
   dst->ops = SRLV;
   recompile_standard_r_type();
   if (dst->f.r.rd == reg) RNOP();
   //else
//if(dynacore)
	//if(dynacore) gensrlv();

}

static void RSRAV()
{
   dst->ops = SRAV;
   recompile_standard_r_type();
   if (dst->f.r.rd == reg) RNOP();
    
//if(dynacore)
	//if(dynacore) gensrav();

}

static void RJR()
{
   dst->ops = JR;
   recompile_standard_i_type();
//if(dynacore)
	//if(dynacore) genjr();

}

static void RJALR()
{
   dst->ops = JALR;
   recompile_standard_r_type();
//if(dynacore)
	//if(dynacore) genjalr();

}

static void RSYSCALL()
{
   dst->ops = SYSCALL;
//if(dynacore)
	//if(dynacore) gensyscall();

}

static void RBREAK()
{
   dst->ops = NI;
//if(dynacore)
	//if(dynacore) genni();

}

static void RSYNC()
{
   dst->ops = SYNC;
//if(dynacore)
	//if(dynacore) gensync();

}

static void RMFHI()
{
   dst->ops = MFHI;
   recompile_standard_r_type();
   if (dst->f.r.rd == reg) RNOP();
   //else
//if(dynacore)
	//if(dynacore) genmfhi();

}

static void RMTHI()
{
   dst->ops = MTHI;
   recompile_standard_r_type();
//if(dynacore)
	//if(dynacore) genmthi();

}

static void RMFLO()
{
   dst->ops = MFLO;
   recompile_standard_r_type();
   if (dst->f.r.rd == reg) RNOP();
   //else
//if(dynacore)
	//if(dynacore) genmflo();

}

static void RMTLO()
{
   dst->ops = MTLO;
   recompile_standard_r_type();
//if(dynacore)
	//if(dynacore) genmtlo();
}

static void RDSLLV()
{
   dst->ops = DSLLV;
   recompile_standard_r_type();
   if (dst->f.r.rd == reg) RNOP();
   //else //if(dynacore)
	//if(dynacore) gendsllv();
}

static void RDSRLV()
{
   dst->ops = DSRLV;
   recompile_standard_r_type();
   if (dst->f.r.rd == reg) RNOP();
   //else //if(dynacore)
	//if(dynacore) gendsrlv();
}

static void RDSRAV()
{
   dst->ops = DSRAV;
   recompile_standard_r_type();
   if (dst->f.r.rd == reg) RNOP();
   //else //if(dynacore)
	//if(dynacore) gendsrav();
}

static void RMULT()
{
   dst->ops = MULT;
   recompile_standard_r_type();
//if(dynacore)
	//if(dynacore) genmult();
}

static void RMULTU()
{
   dst->ops = MULTU;
   recompile_standard_r_type();
//if(dynacore)
	//if(dynacore) genmultu();
}

static void RDIV()
{
   dst->ops = DIV;
   recompile_standard_r_type();
//if(dynacore)
	//if(dynacore) gendiv();
}

static void RDIVU()
{
   dst->ops = DIVU;
   recompile_standard_r_type();
//if(dynacore)
	//if(dynacore) gendivu();
}

static void RDMULT()
{
   dst->ops = DMULT;
   recompile_standard_r_type();
//if(dynacore)
	//if(dynacore) gendmult();
}

static void RDMULTU()
{
   dst->ops = DMULTU;
   recompile_standard_r_type();
//if(dynacore)
	//if(dynacore) gendmultu();
}

static void RDDIV()
{
   dst->ops = DDIV;
   recompile_standard_r_type();
//if(dynacore)
	//if(dynacore) genddiv();
}

static void RDDIVU()
{
   dst->ops = DDIVU;
   recompile_standard_r_type();
//if(dynacore)
	//if(dynacore) genddivu();
}

static void RADD()
{
   dst->ops = ADD;
   recompile_standard_r_type();
   if (dst->f.r.rd == reg) RNOP();
   //else //if(dynacore)
	//if(dynacore) genadd();
}

static void RADDU()
{
   dst->ops = ADDU;
   recompile_standard_r_type();
   if (dst->f.r.rd == reg) RNOP();
   //else //if(dynacore)
	//if(dynacore) genaddu();
}

static void RSUB()
{
   dst->ops = SUB;
   recompile_standard_r_type();
   if (dst->f.r.rd == reg) RNOP();
//if(dynacore)
	//if(dynacore) gensub();
}

static void RSUBU()
{
   dst->ops = SUBU;
   recompile_standard_r_type();
   if (dst->f.r.rd == reg) RNOP();
  // else //if(dynacore)
	//if(dynacore) gensubu();
}

static void RAND()
{
   dst->ops = AND;
   recompile_standard_r_type();
   if(dst->f.r.rd == reg) RNOP();
   //else //if(dynacore)
	//if(dynacore) genand();
}

static void ROR()
{
   dst->ops = OR;
   recompile_standard_r_type();
   if(dst->f.r.rd == reg) RNOP();
   //else //if(dynacore)
	//if(dynacore) genor();
}

static void RXOR()
{
   dst->ops = XOR;
   recompile_standard_r_type();
   if(dst->f.r.rd == reg) RNOP();
   //else //if(dynacore)
	//if(dynacore) genxor();
}

static void RNOR()
{
   dst->ops = NOR;
   recompile_standard_r_type();
   if(dst->f.r.rd == reg) RNOP();
//if(dynacore)
	//if(dynacore) gennor();
}

static void RSLT()
{
   dst->ops = SLT;
   recompile_standard_r_type();
   if(dst->f.r.rd == reg) RNOP();
   //else //if(dynacore)
	//if(dynacore) genslt();
}

static void RSLTU()
{
   dst->ops = SLTU;
   recompile_standard_r_type();
   if(dst->f.r.rd == reg) RNOP();
   //else //if(dynacore)
	//if(dynacore) gensltu();
}

static void RDADD()
{
   dst->ops = DADD;
   recompile_standard_r_type();
   if (dst->f.r.rd == reg) RNOP();
   //else //if(dynacore)
	//if(dynacore) gendadd();
}

static void RDADDU()
{
   dst->ops = DADDU;
   recompile_standard_r_type();
   if (dst->f.r.rd == reg) RNOP();
   //else //if(dynacore)
	//if(dynacore) gendaddu();
}

static void RDSUB()
{
   dst->ops = DSUB;
   recompile_standard_r_type();
   if (dst->f.r.rd == reg) RNOP();
   //else //if(dynacore)
	//if(dynacore) gendsub();
}

static void RDSUBU()
{
   dst->ops = DSUBU;
   recompile_standard_r_type();
   if (dst->f.r.rd == reg) RNOP();
   //else //if(dynacore)
	//if(dynacore) gendsubu();
}

static void RTGE()
{
   dst->ops = NI;
//if(dynacore)
	//if(dynacore) genni();
}

static void RTGEU()
{
   dst->ops = NI;
//if(dynacore)
	//if(dynacore) genni();
}

static void RTLT()
{
   dst->ops = NI;
//if(dynacore)
	//if(dynacore) genni();
}

static void RTLTU()
{
   dst->ops = NI;
//if(dynacore)
	//if(dynacore) genni();
}

static void RTEQ()
{
   dst->ops = TEQ;
   recompile_standard_r_type();
//if(dynacore)
	//if(dynacore) genteq();
}

static void RTNE()
{
   dst->ops = NI;
//if(dynacore)
	//if(dynacore) genni();
}

static void RDSLL()
{
   dst->ops = DSLL;
   recompile_standard_r_type();
   if (dst->f.r.rd == reg) RNOP();
   //else //if(dynacore)
	//if(dynacore) gendsll();
}

static void RDSRL()
{
   dst->ops = DSRL;
   recompile_standard_r_type();
   if (dst->f.r.rd == reg) RNOP();
   //else //if(dynacore)
	//if(dynacore) gendsrl();
}

static void RDSRA()
{
   dst->ops = DSRA;
   recompile_standard_r_type();
   if (dst->f.r.rd == reg) RNOP();
   //else //if(dynacore)
	//if(dynacore) gendsra();
}

static void RDSLL32()
{
   dst->ops = DSLL32;
   recompile_standard_r_type();
   if (dst->f.r.rd == reg) RNOP();
   //else //if(dynacore)
	//if(dynacore) gendsll32();
}

static void RDSRL32()
{
   dst->ops = DSRL32;
   recompile_standard_r_type();
   if (dst->f.r.rd == reg) RNOP();
   //else //if(dynacore)
	//if(dynacore) gendsrl32();
}

static void RDSRA32()
{
   dst->ops = DSRA32;
   recompile_standard_r_type();
   if (dst->f.r.rd == reg) RNOP();
   //else //if(dynacore)
	//if(dynacore) gendsra32();
}

static void (*recomp_special[64])(void) =
{
   RSLL , RSV   , RSRL , RSRA , RSLLV   , RSV    , RSRLV  , RSRAV  ,
   RJR  , RJALR , RSV  , RSV  , RSYSCALL, RBREAK , RSV    , RSYNC  ,
   RMFHI, RMTHI , RMFLO, RMTLO, RDSLLV  , RSV    , RDSRLV , RDSRAV ,
   RMULT, RMULTU, RDIV , RDIVU, RDMULT  , RDMULTU, RDDIV  , RDDIVU ,
   RADD , RADDU , RSUB , RSUBU, RAND    , ROR    , RXOR   , RNOR   ,
   RSV  , RSV   , RSLT , RSLTU, RDADD   , RDADDU , RDSUB  , RDSUBU ,
   RTGE , RTGEU , RTLT , RTLTU, RTEQ    , RSV    , RTNE   , RSV    ,
   RDSLL, RSV   , RDSRL, RDSRA, RDSLL32 , RSV    , RDSRL32, RDSRA32
};

//-------------------------------------------------------------------------
//                                   REGIMM                                
//-------------------------------------------------------------------------

static void RBLTZ()
{
   unsigned long target;
   dst->ops = BLTZ;
   recompile_standard_i_type();
   target = dst->addr + dst->f.i.immediate*4 + 4;
   if (target == dst->addr)
     {
	if (check_nop)
	  {
	     dst->ops = BLTZ_IDLE;
	  //if(dynacore)
	//if(dynacore) genbltz_idle();
	  }
	//else //if(dynacore)
	//if(dynacore) genbltz();
     }
   else if ((!interpcore && !dynacore) && (target < dst_block->start || target >= dst_block->end || dst->addr == (dst_block->end-4)))
     {
	dst->ops = BLTZ_OUT;
	//if(dynacore)
	//if(dynacore) genbltz_out();
     }
   //else //if(dynacore)
	//if(dynacore) genbltz();
}

static void RBGEZ()
{
   unsigned long target;
   dst->ops = BGEZ;
   recompile_standard_i_type();
   target = dst->addr + dst->f.i.immediate*4 + 4;
   if (target == dst->addr)
     {
	if (check_nop)
	  {
	     dst->ops = BGEZ_IDLE;
	  //if(dynacore)
	//if(dynacore) genbgez_idle();
	  }
	//else //if(dynacore)
	//if(dynacore) genbgez();
     }
   else if ((!interpcore && !dynacore) && (target < dst_block->start || target >= dst_block->end || dst->addr == (dst_block->end-4)))
     {
	dst->ops = BGEZ_OUT;
	//if(dynacore)
	//if(dynacore) genbgez_out();
     }
   //else //if(dynacore)
	//if(dynacore) genbgez();
}

static void RBLTZL()
{
   unsigned long target;
   dst->ops = BLTZL;
   recompile_standard_i_type();
   target = dst->addr + dst->f.i.immediate*4 + 4;
   if (target == dst->addr)
     {
	if (check_nop)
	  {
	     dst->ops = BLTZL_IDLE;
	  //if(dynacore)
	//if(dynacore) genbltzl_idle();
	  }
	//else //if(dynacore)
	//if(dynacore) genbltzl();
     }
   else if ((!interpcore && !dynacore) && (target < dst_block->start || target >= dst_block->end || dst->addr == (dst_block->end-4)))
     {
	dst->ops = BLTZL_OUT;
	//if(dynacore)
	//if(dynacore) genbltzl_out();
     }
   //else //if(dynacore)
	//if(dynacore) genbltzl();
}

static void RBGEZL()
{
   unsigned long target;
   dst->ops = BGEZL;
   recompile_standard_i_type();
   target = dst->addr + dst->f.i.immediate*4 + 4;
   if (target == dst->addr)
     {
	if (check_nop)
	  {
	     dst->ops = BGEZL_IDLE;
	  //if(dynacore)
	//if(dynacore) genbgezl_idle();
	  }
	//else //if(dynacore)
	//if(dynacore) genbgezl();
     }
   else if ((!interpcore && !dynacore) && (target < dst_block->start || target >= dst_block->end || dst->addr == (dst_block->end-4)))
     {
	dst->ops = BGEZL_OUT;
	//if(dynacore)
	//if(dynacore) genbgezl_out();
     }
   //else //if(dynacore)
	//if(dynacore) genbgezl();
}

static void RTGEI()
{
   dst->ops = NI;
//if(dynacore)
	//if(dynacore) genni();
}

static void RTGEIU()
{
   dst->ops = NI;
//if(dynacore)
	//if(dynacore) genni();
}

static void RTLTI()
{
   dst->ops = NI;
//if(dynacore)
	//if(dynacore) genni();
}

static void RTLTIU()
{
   dst->ops = NI;
//if(dynacore)
	//if(dynacore) genni();
}

static void RTEQI()
{
   dst->ops = NI;
//if(dynacore)
	//if(dynacore) genni();
}

static void RTNEI()
{
   dst->ops = NI;
//if(dynacore)
	//if(dynacore) genni();
}

static void RBLTZAL()
{
   unsigned long target;
   dst->ops = BLTZAL;
   recompile_standard_i_type();
   target = dst->addr + dst->f.i.immediate*4 + 4;
   if (target == dst->addr)
     {
	if (check_nop)
	  {
	     dst->ops = BLTZAL_IDLE;
	  //if(dynacore)
	//if(dynacore) genbltzal_idle();
	  }
	//else //if(dynacore)
	//if(dynacore) genbltzal();
     }
   else if ((!interpcore && !dynacore) && (target < dst_block->start || target >= dst_block->end || dst->addr == (dst_block->end-4)))
     {
	dst->ops = BLTZAL_OUT;
	//if(dynacore)
	//if(dynacore) genbltzal_out();
     }
   //else //if(dynacore)
	//if(dynacore) genbltzal();
}

static void RBGEZAL()
{
   unsigned long target;
   dst->ops = BGEZAL;
   recompile_standard_i_type();
   target = dst->addr + dst->f.i.immediate*4 + 4;
   if (target == dst->addr)
     {
	if (check_nop)
	  {
	     dst->ops = BGEZAL_IDLE;
	  //if(dynacore)
	//if(dynacore) genbgezal_idle();
	  }
	//else //if(dynacore)
	//if(dynacore) genbgezal();
     }
   else if ((!interpcore && !dynacore) && (target < dst_block->start || target >= dst_block->end || dst->addr == (dst_block->end-4)))
     {
	dst->ops = BGEZAL_OUT;
	//if(dynacore)
	//if(dynacore) genbgezal_out();
     }
   //else //if(dynacore)
	//if(dynacore) genbgezal();
}

static void RBLTZALL()
{
   unsigned long target;
   dst->ops = BLTZALL;
   recompile_standard_i_type();
   target = dst->addr + dst->f.i.immediate*4 + 4;
   if (target == dst->addr)
     {
	if (check_nop)
	  {
	     dst->ops = BLTZALL_IDLE;
	  //if(dynacore)
	//if(dynacore) genbltzall_idle();
	  }
	//else //if(dynacore)
	//if(dynacore) genbltzall();
     }
   else if ((!interpcore && !dynacore) && (target < dst_block->start || target >= dst_block->end || dst->addr == (dst_block->end-4)))
     {
	dst->ops = BLTZALL_OUT;
	//if(dynacore)
	//if(dynacore) genbltzall_out();
     }
   //else //if(dynacore)
	//if(dynacore) genbltzall();
}

static void RBGEZALL()
{
   unsigned long target;
   dst->ops = BGEZALL;
   recompile_standard_i_type();
   target = dst->addr + dst->f.i.immediate*4 + 4;
   if (target == dst->addr)
     {
	if (check_nop)
	  {
	     dst->ops = BGEZALL_IDLE;
	  //if(dynacore)
	//if(dynacore) genbgezall_idle();
	  }
	//else //if(dynacore)
	//if(dynacore) genbgezall();
     }
   else if ((!interpcore && !dynacore) && (target < dst_block->start || target >= dst_block->end || dst->addr == (dst_block->end-4)))
     {
	dst->ops = BGEZALL_OUT;
	//if(dynacore)
	//if(dynacore) genbgezall_out();
     }
   //else //if(dynacore)
	//if(dynacore) genbgezall();
}

static void (*recomp_regimm[32])(void) =
{
   RBLTZ  , RBGEZ  , RBLTZL  , RBGEZL  , RSV  , RSV, RSV  , RSV,
   RTGEI  , RTGEIU , RTLTI   , RTLTIU  , RTEQI, RSV, RTNEI, RSV,
   RBLTZAL, RBGEZAL, RBLTZALL, RBGEZALL, RSV  , RSV, RSV  , RSV,
   RSV    , RSV    , RSV     , RSV     , RSV  , RSV, RSV  , RSV
};

//-------------------------------------------------------------------------
//                                     TLB                                 
//-------------------------------------------------------------------------

static void RTLBR()
{
   dst->ops = TLBR;
//if(dynacore)
	//if(dynacore) gentlbr();
}

static void RTLBWI()
{
   dst->ops = TLBWI;
//if(dynacore)
	//if(dynacore) gentlbwi();
}

static void RTLBWR()
{
   dst->ops = TLBWR;
//if(dynacore)
	//if(dynacore) gentlbwr();
}

static void RTLBP()
{
   dst->ops = TLBP;
//if(dynacore)
	//if(dynacore) gentlbp();
}

static void RERET()
{
   dst->ops = ERET;
//if(dynacore)
	//if(dynacore) generet();
}

static void (*recomp_tlb[64])(void) =
{
   RSV  , RTLBR, RTLBWI, RSV, RSV, RSV, RTLBWR, RSV, 
   RTLBP, RSV  , RSV   , RSV, RSV, RSV, RSV   , RSV, 
   RSV  , RSV  , RSV   , RSV, RSV, RSV, RSV   , RSV, 
   RERET, RSV  , RSV   , RSV, RSV, RSV, RSV   , RSV, 
   RSV  , RSV  , RSV   , RSV, RSV, RSV, RSV   , RSV, 
   RSV  , RSV  , RSV   , RSV, RSV, RSV, RSV   , RSV, 
   RSV  , RSV  , RSV   , RSV, RSV, RSV, RSV   , RSV, 
   RSV  , RSV  , RSV   , RSV, RSV, RSV, RSV   , RSV
};

//-------------------------------------------------------------------------
//                                    COP0                                 
//-------------------------------------------------------------------------

static void RMFC0()
{
   dst->ops = MFC0;
   recompile_standard_r_type();
   dst->f.r.rd = (long long*)(reg_cop0 + ((src >> 11) & 0x1F));
   dst->f.r.nrd = (src >> 11) & 0x1F;
   if (dst->f.r.rt == reg) RNOP();
   //else //if(dynacore)
	//if(dynacore) genmfc0();
}

static void RMTC0()
{
   dst->ops = MTC0;
   recompile_standard_r_type();
   dst->f.r.nrd = (src >> 11) & 0x1F;
//if(dynacore)
	//if(dynacore) genmtc0();
}

static void RTLB()
{
   recomp_tlb[(src & 0x3F)]();
}

static void (*recomp_cop0[32])(void) =
{
   RMFC0, RSV, RSV, RSV, RMTC0, RSV, RSV, RSV,
   RSV  , RSV, RSV, RSV, RSV  , RSV, RSV, RSV,
   RTLB , RSV, RSV, RSV, RSV  , RSV, RSV, RSV,
   RSV  , RSV, RSV, RSV, RSV  , RSV, RSV, RSV
};

//-------------------------------------------------------------------------
//                                     BC                                  
//-------------------------------------------------------------------------

static void RBC1F()
{
   unsigned long target;
   dst->ops = BC1F;
   recompile_standard_i_type();
   target = dst->addr + dst->f.i.immediate*4 + 4;
   if (target == dst->addr)
     {
	if (check_nop)
	  {
	     dst->ops = BC1F_IDLE;
	  //if(dynacore)
	//if(dynacore) genbc1f_idle();
	  }
	//else //if(dynacore)
	//if(dynacore) genbc1f();
     }
   else if ((!interpcore && !dynacore) && (target < dst_block->start || target >= dst_block->end || dst->addr == (dst_block->end-4)))
     {
	dst->ops = BC1F_OUT;
	//if(dynacore)
	//if(dynacore) genbc1f_out();
     }
   //else //if(dynacore)
	//if(dynacore) genbc1f();
}

static void RBC1T()
{
   unsigned long target;
   dst->ops = BC1T;
   recompile_standard_i_type();
   target = dst->addr + dst->f.i.immediate*4 + 4;
   if (target == dst->addr)
     {
	if (check_nop)
	  {
	     dst->ops = BC1T_IDLE;
	  //if(dynacore)
	//if(dynacore) genbc1t_idle();
	  }
	//else //if(dynacore)
	//if(dynacore) genbc1t();
     }
   else if ((!interpcore && !dynacore) && (target < dst_block->start || target >= dst_block->end || dst->addr == (dst_block->end-4)))
     {
	dst->ops = BC1T_OUT;
	//if(dynacore)
	//if(dynacore) genbc1t_out();
     }
   //else //if(dynacore)
	//if(dynacore) genbc1t();
}

static void RBC1FL()
{
   unsigned long target;
   dst->ops = BC1FL;
   recompile_standard_i_type();
   target = dst->addr + dst->f.i.immediate*4 + 4;
   if (target == dst->addr)
     {
	if (check_nop)
	  {
	     dst->ops = BC1FL_IDLE;
	  //if(dynacore)
	//if(dynacore) genbc1fl_idle();
	  }
	//else //if(dynacore)
	//if(dynacore) genbc1fl();
     }
   else if ((!interpcore && !dynacore) && (target < dst_block->start || target >= dst_block->end || dst->addr == (dst_block->end-4)))
     {
	dst->ops = BC1FL_OUT;
	//if(dynacore)
	//if(dynacore) genbc1fl_out();
     }
   //else //if(dynacore)
	//if(dynacore) genbc1fl();
}

static void RBC1TL()
{
   unsigned long target;
   dst->ops = BC1TL;
   recompile_standard_i_type();
   target = dst->addr + dst->f.i.immediate*4 + 4;
   if (target == dst->addr)
     {
	if (check_nop)
	  {
	     dst->ops = BC1TL_IDLE;
	  //if(dynacore)
	//if(dynacore) genbc1tl_idle();
	  }
	//else //if(dynacore)
	//if(dynacore) genbc1tl();
     }
   else if ((!interpcore && !dynacore) && (target < dst_block->start || target >= dst_block->end || dst->addr == (dst_block->end-4)))
     {
	dst->ops = BC1TL_OUT;
	//if(dynacore)
	//if(dynacore) genbc1tl_out();
     }
   //else //if(dynacore)
	//if(dynacore) genbc1tl();
}

static void (*recomp_bc[4])(void) =
{
   RBC1F , RBC1T ,
   RBC1FL, RBC1TL
};

//-------------------------------------------------------------------------
//                                     S                                   
//-------------------------------------------------------------------------

static void RADD_S()
{
   dst->ops = ADD_S;
   recompile_standard_cf_type();
//if(dynacore)
	//if(dynacore) genadd_s();
}

static void RSUB_S()
{
   dst->ops = SUB_S;
   recompile_standard_cf_type();
//if(dynacore)
	//if(dynacore) gensub_s();
}

static void RMUL_S()
{
   dst->ops = MUL_S;
   recompile_standard_cf_type();
//if(dynacore)
	//if(dynacore) genmul_s();
}

static void RDIV_S()
{
   dst->ops = DIV_S;
   recompile_standard_cf_type();
//if(dynacore)
	//if(dynacore) gendiv_s();
}

static void RSQRT_S()
{
   dst->ops = SQRT_S;
   recompile_standard_cf_type();
//if(dynacore)
	//if(dynacore) gensqrt_s();
}

static void RABS_S()
{
   dst->ops = ABS_S;
   recompile_standard_cf_type();
//if(dynacore)
	//if(dynacore) genabs_s();
}

static void RMOV_S()
{
   dst->ops = MOV_S;
   recompile_standard_cf_type();
//if(dynacore)
	//if(dynacore) genmov_s();
}

static void RNEG_S()
{
   dst->ops = NEG_S;
   recompile_standard_cf_type();
//if(dynacore)
	//if(dynacore) genneg_s();
}

static void RROUND_L_S()
{
   dst->ops = ROUND_L_S;
   recompile_standard_cf_type();
//if(dynacore)
	//if(dynacore) genround_l_s();
}

static void RTRUNC_L_S()
{
   dst->ops = TRUNC_L_S;
   recompile_standard_cf_type();
//if(dynacore)
	//if(dynacore) gentrunc_l_s();
}

static void RCEIL_L_S()
{
   dst->ops = CEIL_L_S;
   recompile_standard_cf_type();
//if(dynacore)
	//if(dynacore) genceil_l_s();
}

static void RFLOOR_L_S()
{
   dst->ops = FLOOR_L_S;
   recompile_standard_cf_type();
//if(dynacore)
	//if(dynacore) genfloor_l_s();
}

static void RROUND_W_S()
{
   dst->ops = ROUND_W_S;
   recompile_standard_cf_type();
//if(dynacore)
	//if(dynacore) genround_w_s();
}

static void RTRUNC_W_S()
{
   dst->ops = TRUNC_W_S;
   recompile_standard_cf_type();
//if(dynacore)
	//if(dynacore) gentrunc_w_s();
}

static void RCEIL_W_S()
{
   dst->ops = CEIL_W_S;
   recompile_standard_cf_type();
//if(dynacore)
	//if(dynacore) genceil_w_s();
}

static void RFLOOR_W_S()
{
   dst->ops = FLOOR_W_S;
   recompile_standard_cf_type();
//if(dynacore)
	//if(dynacore) genfloor_w_s();
}

static void RCVT_D_S()
{
   dst->ops = CVT_D_S;
   recompile_standard_cf_type();
//if(dynacore)
	//if(dynacore) gencvt_d_s();
}

static void RCVT_W_S()
{
   dst->ops = CVT_W_S;
   recompile_standard_cf_type();
//if(dynacore)
	//if(dynacore) gencvt_w_s();
}

static void RCVT_L_S()
{
   dst->ops = CVT_L_S;
   recompile_standard_cf_type();
//if(dynacore)
	//if(dynacore) gencvt_l_s();
}

static void RC_F_S()
{
   dst->ops = C_F_S;
   recompile_standard_cf_type();
//if(dynacore)
	//if(dynacore) genc_f_s();
}

static void RC_UN_S()
{
   dst->ops = C_UN_S;
   recompile_standard_cf_type();
//if(dynacore)
	//if(dynacore) genc_un_s();
}

static void RC_EQ_S()
{
   dst->ops = C_EQ_S;
   recompile_standard_cf_type();
//if(dynacore)
	//if(dynacore) genc_eq_s();
}

static void RC_UEQ_S()
{
   dst->ops = C_UEQ_S;
   recompile_standard_cf_type();
//if(dynacore)
	//if(dynacore) genc_ueq_s();
}

static void RC_OLT_S()
{
   dst->ops = C_OLT_S;
   recompile_standard_cf_type();
//if(dynacore)
	//if(dynacore) genc_olt_s();
}

static void RC_ULT_S()
{
   dst->ops = C_ULT_S;
   recompile_standard_cf_type();
//if(dynacore)
	//if(dynacore) genc_ult_s();
}

static void RC_OLE_S()
{
   dst->ops = C_OLE_S;
   recompile_standard_cf_type();
//if(dynacore)
	//if(dynacore) genc_ole_s();
}

static void RC_ULE_S()
{
   dst->ops = C_ULE_S;
   recompile_standard_cf_type();
//if(dynacore)
	//if(dynacore) genc_ule_s();
}

static void RC_SF_S()
{
   dst->ops = C_SF_S;
   recompile_standard_cf_type();
//if(dynacore)
	//if(dynacore) genc_sf_s();
}

static void RC_NGLE_S()
{
   dst->ops = C_NGLE_S;
   recompile_standard_cf_type();
//if(dynacore)
	//if(dynacore) genc_ngle_s();
}

static void RC_SEQ_S()
{
   dst->ops = C_SEQ_S;
   recompile_standard_cf_type();
//if(dynacore)
	//if(dynacore) genc_seq_s();
}

static void RC_NGL_S()
{
   dst->ops = C_NGL_S;
   recompile_standard_cf_type();
//if(dynacore)
	//if(dynacore) genc_ngl_s();
}

static void RC_LT_S()
{
   dst->ops = C_LT_S;
   recompile_standard_cf_type();
//if(dynacore)
	//if(dynacore) genc_lt_s();
}

static void RC_NGE_S()
{
   dst->ops = C_NGE_S;
   recompile_standard_cf_type();
//if(dynacore)
	//if(dynacore) genc_nge_s();
}

static void RC_LE_S()
{
   dst->ops = C_LE_S;
   recompile_standard_cf_type();
//if(dynacore)
	//if(dynacore) genc_le_s();
}

static void RC_NGT_S()
{
   dst->ops = C_NGT_S;
   recompile_standard_cf_type();
//if(dynacore)
	//if(dynacore) genc_ngt_s();
}

static void (*recomp_s[64])(void) =
{
   RADD_S    , RSUB_S    , RMUL_S   , RDIV_S    , RSQRT_S   , RABS_S    , RMOV_S   , RNEG_S    , 
   RROUND_L_S, RTRUNC_L_S, RCEIL_L_S, RFLOOR_L_S, RROUND_W_S, RTRUNC_W_S, RCEIL_W_S, RFLOOR_W_S, 
   RSV       , RSV       , RSV      , RSV       , RSV       , RSV       , RSV      , RSV       , 
   RSV       , RSV       , RSV      , RSV       , RSV       , RSV       , RSV      , RSV       , 
   RSV       , RCVT_D_S  , RSV      , RSV       , RCVT_W_S  , RCVT_L_S  , RSV      , RSV       , 
   RSV       , RSV       , RSV      , RSV       , RSV       , RSV       , RSV      , RSV       , 
   RC_F_S    , RC_UN_S   , RC_EQ_S  , RC_UEQ_S  , RC_OLT_S  , RC_ULT_S  , RC_OLE_S , RC_ULE_S  , 
   RC_SF_S   , RC_NGLE_S , RC_SEQ_S , RC_NGL_S  , RC_LT_S   , RC_NGE_S  , RC_LE_S  , RC_NGT_S
};

//-------------------------------------------------------------------------
//                                     D                                   
//-------------------------------------------------------------------------

static void RADD_D()
{
   dst->ops = ADD_D;
   recompile_standard_cf_type();
//if(dynacore)
	//if(dynacore) genadd_d();
}

static void RSUB_D()
{
   dst->ops = SUB_D;
   recompile_standard_cf_type();
//if(dynacore)
	//if(dynacore) gensub_d();
}

static void RMUL_D()
{
   dst->ops = MUL_D;
   recompile_standard_cf_type();
//if(dynacore)
	//if(dynacore) genmul_d();
}

static void RDIV_D()
{
   dst->ops = DIV_D;
   recompile_standard_cf_type();
//if(dynacore)
	//if(dynacore) gendiv_d();
}

static void RSQRT_D()
{
   dst->ops = SQRT_D;
   recompile_standard_cf_type();
//if(dynacore)
	//if(dynacore) gensqrt_d();
}

static void RABS_D()
{
   dst->ops = ABS_D;
   recompile_standard_cf_type();
//if(dynacore)
	//if(dynacore) genabs_d();
}

static void RMOV_D()
{
   dst->ops = MOV_D;
   recompile_standard_cf_type();
//if(dynacore)
	//if(dynacore) genmov_d();
}

static void RNEG_D()
{
   dst->ops = NEG_D;
   recompile_standard_cf_type();
//if(dynacore)
	//if(dynacore) genneg_d();
}

static void RROUND_L_D()
{
   dst->ops = ROUND_L_D;
   recompile_standard_cf_type();
//if(dynacore)
	//if(dynacore) genround_l_d();
}

static void RTRUNC_L_D()
{
   dst->ops = TRUNC_L_D;
   recompile_standard_cf_type();
//if(dynacore)
	//if(dynacore) gentrunc_l_d();
}

static void RCEIL_L_D()
{
   dst->ops = CEIL_L_D;
   recompile_standard_cf_type();
//if(dynacore)
	//if(dynacore) genceil_l_d();
}

static void RFLOOR_L_D()
{
   dst->ops = FLOOR_L_D;
   recompile_standard_cf_type();
//if(dynacore)
	//if(dynacore) genfloor_l_d();
}

static void RROUND_W_D()
{
   dst->ops = ROUND_W_D;
   recompile_standard_cf_type();
//if(dynacore)
	//if(dynacore) genround_w_d();
}

static void RTRUNC_W_D()
{
   dst->ops = TRUNC_W_D;
   recompile_standard_cf_type();
//if(dynacore)
	//if(dynacore) gentrunc_w_d();
}

static void RCEIL_W_D()
{
   dst->ops = CEIL_W_D;
   recompile_standard_cf_type();
//if(dynacore)
	//if(dynacore) genceil_w_d();
}

static void RFLOOR_W_D()
{
   dst->ops = FLOOR_W_D;
   recompile_standard_cf_type();
//if(dynacore)
	//if(dynacore) genfloor_w_d();
}

static void RCVT_S_D()
{
   dst->ops = CVT_S_D;
   recompile_standard_cf_type();
//if(dynacore)
	//if(dynacore) gencvt_s_d();
}

static void RCVT_W_D()
{
   dst->ops = CVT_W_D;
   recompile_standard_cf_type();
//if(dynacore)
	//if(dynacore) gencvt_w_d();
}

static void RCVT_L_D()
{
   dst->ops = CVT_L_D;
   recompile_standard_cf_type();
//if(dynacore)
	//if(dynacore) gencvt_l_d();
}

static void RC_F_D()
{
   dst->ops = C_F_D;
   recompile_standard_cf_type();
//if(dynacore)
	//if(dynacore) genc_f_d();
}

static void RC_UN_D()
{
   dst->ops = C_UN_D;
   recompile_standard_cf_type();
//if(dynacore)
	//if(dynacore) genc_un_d();
}

static void RC_EQ_D()
{
   dst->ops = C_EQ_D;
   recompile_standard_cf_type();
//if(dynacore)
	//if(dynacore) genc_eq_d();
}

static void RC_UEQ_D()
{
   dst->ops = C_UEQ_D;
   recompile_standard_cf_type();
//if(dynacore)
	//if(dynacore) genc_ueq_d();
}

static void RC_OLT_D()
{
   dst->ops = C_OLT_D;
   recompile_standard_cf_type();
//if(dynacore)
	//if(dynacore) genc_olt_d();
}

static void RC_ULT_D()
{
   dst->ops = C_ULT_D;
   recompile_standard_cf_type();
//if(dynacore)
	//if(dynacore) genc_ult_d();
}

static void RC_OLE_D()
{
   dst->ops = C_OLE_D;
   recompile_standard_cf_type();
//if(dynacore)
	//if(dynacore) genc_ole_d();
}

static void RC_ULE_D()
{
   dst->ops = C_ULE_D;
   recompile_standard_cf_type();
//if(dynacore)
	//if(dynacore) genc_ule_d();
}

static void RC_SF_D()
{
   dst->ops = C_SF_D;
   recompile_standard_cf_type();
//if(dynacore)
	//if(dynacore) genc_sf_d();
}

static void RC_NGLE_D()
{
   dst->ops = C_NGLE_D;
   recompile_standard_cf_type();
//if(dynacore)
	//if(dynacore) genc_ngle_d();
}

static void RC_SEQ_D()
{
   dst->ops = C_SEQ_D;
   recompile_standard_cf_type();
//if(dynacore)
	//if(dynacore) genc_seq_d();
}

static void RC_NGL_D()
{
   dst->ops = C_NGL_D;
   recompile_standard_cf_type();
//if(dynacore)
	//if(dynacore) genc_ngl_d();
}

static void RC_LT_D()
{
   dst->ops = C_LT_D;
   recompile_standard_cf_type();
//if(dynacore)
	//if(dynacore) genc_lt_d();
}

static void RC_NGE_D()
{
   dst->ops = C_NGE_D;
   recompile_standard_cf_type();
//if(dynacore)
	//if(dynacore) genc_nge_d();
}

static void RC_LE_D()
{
   dst->ops = C_LE_D;
   recompile_standard_cf_type();
//if(dynacore)
	//if(dynacore) genc_le_d();
}

static void RC_NGT_D()
{
   dst->ops = C_NGT_D;
   recompile_standard_cf_type();
//if(dynacore)
	//if(dynacore) genc_ngt_d();
}

static void (*recomp_d[64])(void) =
{
   RADD_D    , RSUB_D    , RMUL_D   , RDIV_D    , RSQRT_D   , RABS_D    , RMOV_D   , RNEG_D    ,
   RROUND_L_D, RTRUNC_L_D, RCEIL_L_D, RFLOOR_L_D, RROUND_W_D, RTRUNC_W_D, RCEIL_W_D, RFLOOR_W_D,
   RSV       , RSV       , RSV      , RSV       , RSV       , RSV       , RSV      , RSV       ,
   RSV       , RSV       , RSV      , RSV       , RSV       , RSV       , RSV      , RSV       ,
   RCVT_S_D  , RSV       , RSV      , RSV       , RCVT_W_D  , RCVT_L_D  , RSV      , RSV       ,
   RSV       , RSV       , RSV      , RSV       , RSV       , RSV       , RSV      , RSV       ,
   RC_F_D    , RC_UN_D   , RC_EQ_D  , RC_UEQ_D  , RC_OLT_D  , RC_ULT_D  , RC_OLE_D , RC_ULE_D  ,
   RC_SF_D   , RC_NGLE_D , RC_SEQ_D , RC_NGL_D  , RC_LT_D   , RC_NGE_D  , RC_LE_D  , RC_NGT_D
};

//-------------------------------------------------------------------------
//                                     W                                   
//-------------------------------------------------------------------------

static void RCVT_S_W()
{
   dst->ops = CVT_S_W;
   recompile_standard_cf_type();
//if(dynacore)
	//if(dynacore) gencvt_s_w();
}

static void RCVT_D_W()
{
   dst->ops = CVT_D_W;
   recompile_standard_cf_type();
//if(dynacore)
	//if(dynacore) gencvt_d_w();
}

static void (*recomp_w[64])(void) =
{
   RSV     , RSV     , RSV, RSV, RSV, RSV, RSV, RSV, 
   RSV     , RSV     , RSV, RSV, RSV, RSV, RSV, RSV, 
   RSV     , RSV     , RSV, RSV, RSV, RSV, RSV, RSV, 
   RSV     , RSV     , RSV, RSV, RSV, RSV, RSV, RSV, 
   RCVT_S_W, RCVT_D_W, RSV, RSV, RSV, RSV, RSV, RSV, 
   RSV     , RSV     , RSV, RSV, RSV, RSV, RSV, RSV, 
   RSV     , RSV     , RSV, RSV, RSV, RSV, RSV, RSV, 
   RSV     , RSV     , RSV, RSV, RSV, RSV, RSV, RSV
};

//-------------------------------------------------------------------------
//                                     L                                   
//-------------------------------------------------------------------------

static void RCVT_S_L()
{
   dst->ops = CVT_S_L;
   recompile_standard_cf_type();
//if(dynacore)
	//if(dynacore) gencvt_s_l();
}

static void RCVT_D_L()
{
   dst->ops = CVT_D_L;
   recompile_standard_cf_type();
//if(dynacore)
	//if(dynacore) gencvt_d_l();
}

static void (*recomp_l[64])(void) =
{
   RSV     , RSV     , RSV, RSV, RSV, RSV, RSV, RSV, 
   RSV     , RSV     , RSV, RSV, RSV, RSV, RSV, RSV, 
   RSV     , RSV     , RSV, RSV, RSV, RSV, RSV, RSV, 
   RSV     , RSV     , RSV, RSV, RSV, RSV, RSV, RSV,
   RCVT_S_L, RCVT_D_L, RSV, RSV, RSV, RSV, RSV, RSV, 
   RSV     , RSV     , RSV, RSV, RSV, RSV, RSV, RSV, 
   RSV     , RSV     , RSV, RSV, RSV, RSV, RSV, RSV, 
   RSV     , RSV     , RSV, RSV, RSV, RSV, RSV, RSV, 
};

//-------------------------------------------------------------------------
//                                    COP1                                 
//-------------------------------------------------------------------------

static void RMFC1()
{
   dst->ops = MFC1;
   recompile_standard_r_type();
   dst->f.r.nrd = (src >> 11) & 0x1F;
   if (dst->f.r.rt == reg) RNOP();
   //else //if(dynacore)
	//if(dynacore) genmfc1();
}

static void RDMFC1()
{
   dst->ops = DMFC1;
   recompile_standard_r_type();
   dst->f.r.nrd = (src >> 11) & 0x1F;
   if (dst->f.r.rt == reg) RNOP();
   //else //if(dynacore)
	//if(dynacore) gendmfc1();
}

static void RCFC1()
{
   dst->ops = CFC1;
   recompile_standard_r_type();
   dst->f.r.nrd = (src >> 11) & 0x1F;
   if (dst->f.r.rt == reg) RNOP();
   //else //if(dynacore)
	//if(dynacore) gencfc1();
}

static void RMTC1()
{
   dst->ops = MTC1;
   recompile_standard_r_type();
   dst->f.r.nrd = (src >> 11) & 0x1F;
//if(dynacore)
	//if(dynacore) genmtc1();
}

static void RDMTC1()
{
   dst->ops = DMTC1;
   recompile_standard_r_type();
   dst->f.r.nrd = (src >> 11) & 0x1F;
//if(dynacore)
	//if(dynacore) gendmtc1();
}

static void RCTC1()
{
   dst->ops = CTC1;
   recompile_standard_r_type();
   dst->f.r.nrd = (src >> 11) & 0x1F;
//if(dynacore)
	//if(dynacore) genctc1();
}

static void RBC()
{
   recomp_bc[((src >> 16) & 3)]();
}

static void RS()
{
   recomp_s[(src & 0x3F)]();
}

static void RD()
{
   recomp_d[(src & 0x3F)]();
}

static void RW()
{
   recomp_w[(src & 0x3F)]();
}

static void RL()
{
   recomp_l[(src & 0x3F)]();
}

static void (*recomp_cop1[32])(void) =
{
   RMFC1, RDMFC1, RCFC1, RSV, RMTC1, RDMTC1, RCTC1, RSV,
   RBC  , RSV   , RSV  , RSV, RSV  , RSV   , RSV  , RSV,
   RS   , RD    , RSV  , RSV, RW   , RL    , RSV  , RSV,
   RSV  , RSV   , RSV  , RSV, RSV  , RSV   , RSV  , RSV
};

//-------------------------------------------------------------------------
//                                   R4300                                 
//-------------------------------------------------------------------------

static void RSPECIAL()
{
   recomp_special[(src & 0x3F)]();
}

static void RREGIMM()
{
   recomp_regimm[((src >> 16) & 0x1F)]();
}

static void RJ()
{
   unsigned long target;
   dst->ops = J_OUT;
   recompile_standard_j_type();
   target = (dst->f.j.inst_index<<2) | (dst->addr & 0xF0000000);
   if (target == dst->addr)
     {
	if (check_nop)
	  {
	     dst->ops = J_IDLE;
	  //if(dynacore)
	//if(dynacore) genj_idle();
	  }
	//else //if(dynacore)
	//if(dynacore) genj();
     }
   else if ((!interpcore && !dynacore) && (target < dst_block->start || target >= dst_block->end || dst->addr == (dst_block->end-4)))
     {
	dst->ops = J_OUT;
	//if(dynacore)
	//if(dynacore) genj_out();
     }
   //else //if(dynacore)
	//if(dynacore) genj();
}

static void RJAL()
{
   unsigned long target;
   dst->ops = JAL_OUT;
   recompile_standard_j_type();
   target = (dst->f.j.inst_index<<2) | (dst->addr & 0xF0000000);
   if (target == dst->addr)
     {
	if (check_nop)
	  {
	     dst->ops = JAL_IDLE;
	  //if(dynacore)
	//if(dynacore) genjal_idle();
	  }
	//else //if(dynacore)
	//if(dynacore) genjal();
     }
   else if ((!interpcore && !dynacore) && (target < dst_block->start || target >= dst_block->end || dst->addr == (dst_block->end-4)))
     {
	dst->ops = JAL_OUT;
	//if(dynacore)
	//if(dynacore) genjal_out();
     }
   //else //if(dynacore)
	//if(dynacore) genjal();
}

static void RBEQ()
{
   unsigned long target;
   dst->ops = BEQ;
   recompile_standard_i_type();
   target = dst->addr + dst->f.i.immediate*4 + 4;
   if (target == dst->addr)
     {
	if (check_nop)
	  {
	     dst->ops = BEQ_IDLE;
	  //if(dynacore)
	//if(dynacore) genbeq_idle();
	  }
	//else //if(dynacore)
	//if(dynacore) genbeq();
     }
   else if ((!interpcore && !dynacore) && (target < dst_block->start || target >= dst_block->end || dst->addr == (dst_block->end-4)))
     {
	dst->ops = BEQ_OUT;
	//if(dynacore)
	//if(dynacore) genbeq_out();
     }
   //else //if(dynacore)
	//if(dynacore) genbeq();
}

static void RBNE()
{
   unsigned long target;
   dst->ops = BNE;
   recompile_standard_i_type();
   target = dst->addr + dst->f.i.immediate*4 + 4;
   if (target == dst->addr)
     {
	if (check_nop)
	  {
	     dst->ops = BNE_IDLE;
	  //if(dynacore)
	//if(dynacore) genbne_idle();
	  }
	//else //if(dynacore)
	//if(dynacore) genbne();
     }
   else if ((!interpcore && !dynacore) && (target < dst_block->start || target >= dst_block->end || dst->addr == (dst_block->end-4)))
     {
	dst->ops = BNE_OUT;
	//if(dynacore)
	//if(dynacore) genbne_out();
     }
   //else //if(dynacore)
	//if(dynacore) genbne();
}

static void RBLEZ()
{
   unsigned long target;
   dst->ops = BLEZ;
   recompile_standard_i_type();
   target = dst->addr + dst->f.i.immediate*4 + 4;
   if (target == dst->addr)
     {
	if (check_nop)
	  {
	     dst->ops = BLEZ_IDLE;
	  //if(dynacore)
	//if(dynacore) genblez_idle();
	  }
	//else //if(dynacore)
	//if(dynacore) genblez();
     }
   else if ((!interpcore && !dynacore) && (target < dst_block->start || target >= dst_block->end || dst->addr == (dst_block->end-4)))
     {
	dst->ops = BLEZ_OUT;
	//if(dynacore)
	//if(dynacore) genblez_out();
     }
   //else //if(dynacore)
	//if(dynacore) genblez();
}

static void RBGTZ()
{
   unsigned long target;
   dst->ops = BGTZ;
   recompile_standard_i_type();
   target = dst->addr + dst->f.i.immediate*4 + 4;
   if (target == dst->addr)
     {
	if (check_nop)
	  {
	     dst->ops = BGTZ_IDLE;
	  //if(dynacore)
	//if(dynacore) genbgtz_idle();
	  }
	//else //if(dynacore)
	//if(dynacore) genbgtz();
     }
   else if ((!interpcore && !dynacore) && (target < dst_block->start || target >= dst_block->end || dst->addr == (dst_block->end-4)))
     {
	dst->ops = BGTZ_OUT;
	//if(dynacore)
	//if(dynacore) genbgtz_out();
     }
   //else //if(dynacore)
	//if(dynacore) genbgtz();
}

static void RADDI()
{
   dst->ops = ADDI;
   recompile_standard_i_type();
   if(dst->f.i.rt == reg) RNOP();
   //else //if(dynacore)
	//if(dynacore) genaddi();
}

static void RADDIU()
{
   dst->ops = ADDIU;
   recompile_standard_i_type();
   if(dst->f.i.rt == reg) RNOP();
   //else //if(dynacore)
	//if(dynacore) genaddiu();
}

static void RSLTI()
{
   dst->ops = SLTI;
   recompile_standard_i_type();
   if(dst->f.i.rt == reg) RNOP();
   //else //if(dynacore)
	//if(dynacore) genslti();
}

static void RSLTIU()
{
   dst->ops = SLTIU;
   recompile_standard_i_type();
   if(dst->f.i.rt == reg) RNOP();
  // else //if(dynacore)
	//if(dynacore) gensltiu();
}

static void RANDI()
{
   dst->ops = ANDI;
   recompile_standard_i_type();
   if(dst->f.i.rt == reg) RNOP();
   //else //if(dynacore)
	//if(dynacore) genandi();
}

static void RORI()
{
   dst->ops = ORI;
   recompile_standard_i_type();
   if (dst->f.i.rt == reg) RNOP();
   //else //if(dynacore)
	//if(dynacore) genori();
}

static void RXORI()
{
   dst->ops = XORI;
   recompile_standard_i_type();
   if (dst->f.i.rt == reg) RNOP();
   //else //if(dynacore)
	//if(dynacore) genxori();
}

static void RLUI()
{
   dst->ops = LUI;
   recompile_standard_i_type();
   if (dst->f.i.rt == reg) RNOP();
   //else //if(dynacore)
	//if(dynacore) genlui();
}

static void RCOP0()
{
   recomp_cop0[((src >> 21) & 0x1F)]();
}

static void RCOP1()
{
   recomp_cop1[((src >> 21) & 0x1F)]();
}

static void RBEQL()
{
   unsigned long target;
   dst->ops = BEQL;
   recompile_standard_i_type();
   target = dst->addr + dst->f.i.immediate*4 + 4;
   if (target == dst->addr)
     {
	if (check_nop)
	  {
	     dst->ops = BEQL_IDLE;
	  //if(dynacore)
	//if(dynacore) genbeql_idle();
	  }
	//else //if(dynacore)
	//if(dynacore) genbeql();
     }
   else if ((!interpcore && !dynacore) && (target < dst_block->start || target >= dst_block->end || dst->addr == (dst_block->end-4)))
     {
	dst->ops = BEQL_OUT;
	//if(dynacore)
	//if(dynacore) genbeql_out();
     }
   //else //if(dynacore)
	//if(dynacore) genbeql();
}

static void RBNEL()
{
   unsigned long target;
   dst->ops = BNEL;
   recompile_standard_i_type();
   target = dst->addr + dst->f.i.immediate*4 + 4;
   if (target == dst->addr)
     {
	if (check_nop)
	  {
	     dst->ops = BNEL_IDLE;
	  //if(dynacore)
	//if(dynacore) genbnel_idle();
	  }
	//else //if(dynacore)
	//if(dynacore) genbnel();
     }
   else if ((!interpcore && !dynacore) && (target < dst_block->start || target >= dst_block->end || dst->addr == (dst_block->end-4)))
     {
	dst->ops = BNEL_OUT;
	//if(dynacore)
	//if(dynacore) genbnel_out();
     }
   //else //if(dynacore)
	//if(dynacore) genbnel();
}

static void RBLEZL()
{
   unsigned long target;
   dst->ops = BLEZL;
   recompile_standard_i_type();
   target = dst->addr + dst->f.i.immediate*4 + 4;
   if (target == dst->addr)
     {
	if (check_nop)
	  {
	     dst->ops = BLEZL_IDLE;
	  //if(dynacore)
	//if(dynacore) genblezl_idle();
	  }
	//else //if(dynacore)
	//if(dynacore) genblezl();
     }
   else if ((!interpcore && !dynacore) && (target < dst_block->start || target >= dst_block->end || dst->addr == (dst_block->end-4)))
     {
	dst->ops = BLEZL_OUT;
	//if(dynacore)
	//if(dynacore) genblezl_out();
     }
   //else //if(dynacore)
	//if(dynacore) genblezl();
}

static void RBGTZL()
{
   unsigned long target;
   dst->ops = BGTZL;
   recompile_standard_i_type();
   target = dst->addr + dst->f.i.immediate*4 + 4;
   if (target == dst->addr)
     {
	if (check_nop)
	  {
	     dst->ops = BGTZL_IDLE;
	  //if(dynacore)
	//if(dynacore) genbgtzl_idle();
	  }
	//else //if(dynacore)
	//if(dynacore) genbgtzl();
     }
   else if ((!interpcore && !dynacore) && (target < dst_block->start || target >= dst_block->end || dst->addr == (dst_block->end-4)))
     {
	dst->ops = BGTZL_OUT;
	//if(dynacore)
	//if(dynacore) genbgtzl_out();
     }
   //else //if(dynacore)
	//if(dynacore) genbgtzl();
}

static void RDADDI()
{
   dst->ops = DADDI;
   recompile_standard_i_type();
   if(dst->f.i.rt == reg) RNOP();
   //else //if(dynacore)
	//if(dynacore) gendaddi();
}

static void RDADDIU()
{
   dst->ops = DADDIU;
   recompile_standard_i_type();
   if(dst->f.i.rt == reg) RNOP();
   //else //if(dynacore)
	//if(dynacore) gendaddiu();
}

static void RLDL()
{
   dst->ops = LDL;
   recompile_standard_i_type();
   if(dst->f.i.rt == reg) RNOP();
   //else //if(dynacore)
	//if(dynacore) genldl();
}

static void RLDR()
{
   dst->ops = LDR;
   recompile_standard_i_type();
   if(dst->f.i.rt == reg) RNOP();
   //else //if(dynacore)
	//if(dynacore) genldr();
}

static void RLB()
{
   dst->ops = LB;
   recompile_standard_i_type();
   if (dst->f.i.rt == reg) RNOP();
   //else //if(dynacore)
	//if(dynacore) genlb();
}

static void RLH()
{
   dst->ops = LH;
   recompile_standard_i_type();
   if (dst->f.i.rt == reg) RNOP();
   //else //if(dynacore)
	//if(dynacore) genlh();
}

static void RLWL()
{
   dst->ops = LWL;
   recompile_standard_i_type();
   if (dst->f.i.rt == reg) RNOP();
   //else //if(dynacore)
	//if(dynacore) genlwl();
}

static void RLW()
{
   dst->ops = LW;
   recompile_standard_i_type();
   if (dst->f.i.rt == reg) RNOP();
   //else //if(dynacore)
	//if(dynacore) genlw();
}

static void RLBU()
{
   dst->ops = LBU;
   recompile_standard_i_type();
   if(dst->f.i.rt == reg) RNOP();
   //else //if(dynacore)
	//if(dynacore) genlbu();
}

static void RLHU()
{
   dst->ops = LHU;
   recompile_standard_i_type();
   if(dst->f.i.rt == reg) RNOP();
   //else //if(dynacore)
	//if(dynacore) genlhu();
}

static void RLWR()
{
   dst->ops = LWR;
   recompile_standard_i_type();
   if(dst->f.i.rt == reg) RNOP();
  // else //if(dynacore)
	//if(dynacore) genlwr();
}

static void RLWU()
{
   dst->ops = LWU;
   recompile_standard_i_type();
   if(dst->f.i.rt == reg) RNOP();
   //else //if(dynacore)
	//if(dynacore) genlwu();
}

static void RSB()
{
   dst->ops = SB;
   recompile_standard_i_type();
//if(dynacore)
	//if(dynacore) gensb();
}

static void RSH()
{
   dst->ops = SH;
   recompile_standard_i_type();
//if(dynacore)
	//if(dynacore) gensh();
}

static void RSWL()
{
   dst->ops = SWL;
   recompile_standard_i_type();
//if(dynacore)
	//if(dynacore) genswl();
}

static void RSW()
{
   dst->ops = SW;
   recompile_standard_i_type();
//if(dynacore)
	//if(dynacore) gensw();
}

static void RSDL()
{
   dst->ops = SDL;
   recompile_standard_i_type();
//if(dynacore)
	//if(dynacore) gensdl();
}

static void RSDR()
{
   dst->ops = SDR;
   recompile_standard_i_type();
//if(dynacore)
	//if(dynacore) gensdr();
}

static void RSWR()
{
   dst->ops = SWR;
   recompile_standard_i_type();
//if(dynacore)
	//if(dynacore) genswr();
}

static void RCACHE()
{
   dst->ops = CACHE;
//if(dynacore)
	//if(dynacore) gencache();
}

static void RLL()
{
   dst->ops = LL;
   recompile_standard_i_type();
   if(dst->f.i.rt == reg) RNOP();
   //else //if(dynacore)
	//if(dynacore) genll();
}

static void RLWC1()
{
   dst->ops = LWC1;
   recompile_standard_lf_type();
//if(dynacore)
	//if(dynacore) genlwc1();
}

static void RLLD()
{
   dst->ops = NI;
   recompile_standard_i_type();
//if(dynacore)
	//if(dynacore) genni();
}

static void RLDC1()
{
   dst->ops = LDC1;
   recompile_standard_lf_type();
//if(dynacore)
	//if(dynacore) genldc1();
}

static void RLD()
{
   dst->ops = LD;
   recompile_standard_i_type();
   if (dst->f.i.rt == reg) RNOP();
   //else //if(dynacore)
	//if(dynacore) genld();
}

static void RSC()
{
   dst->ops = SC;
   recompile_standard_i_type();
   if (dst->f.i.rt == reg) RNOP();
   //else //if(dynacore)
	//if(dynacore) gensc();
}

static void RSWC1()
{
   dst->ops = SWC1;
   recompile_standard_lf_type();
//if(dynacore)
	//if(dynacore) genswc1();
}

static void RSCD()
{
   dst->ops = NI;
   recompile_standard_i_type();
//if(dynacore)
	//if(dynacore) genni();
}

static void RSDC1()
{
   dst->ops = SDC1;
   recompile_standard_lf_type();
//if(dynacore)
	//if(dynacore) gensdc1();
}

static void RSD()
{
   dst->ops = SD;
   recompile_standard_i_type();
//if(dynacore)
	//if(dynacore) gensd();
}

static void (*recomp_ops[64])(void) =
{
   RSPECIAL, RREGIMM, RJ   , RJAL  , RBEQ , RBNE , RBLEZ , RBGTZ ,
   RADDI   , RADDIU , RSLTI, RSLTIU, RANDI, RORI , RXORI , RLUI  ,
   RCOP0   , RCOP1  , RSV  , RSV   , RBEQL, RBNEL, RBLEZL, RBGTZL,
   RDADDI  , RDADDIU, RLDL , RLDR  , RSV  , RSV  , RSV   , RSV   ,
   RLB     , RLH    , RLWL , RLW   , RLBU , RLHU , RLWR  , RLWU  ,
   RSB     , RSH    , RSWL , RSW   , RSDL , RSDR , RSWR  , RCACHE,
   RLL     , RLWC1  , RSV  , RSV   , RLLD , RLDC1, RSV   , RLD   ,
   RSC     , RSWC1  , RSV  , RSV   , RSCD , RSDC1, RSV   , RSD
};

/**********************************************************************
 ******************** initialize an empty block ***********************
 **********************************************************************/
#ifndef PPC_DYNAREC
void init_block(long *source, precomp_block *block)
{
   int i, length, already_exist = 1;
   static int init_length;
   start_section(COMPILER_SECTION);
   //printf("init block recompiled %x\n", (int)block->start);
   
   length = (block->end-block->start)/4;
   
   if (!block->block)
     {
    sprintf(txtbuffer, "Allocating block @ %08x of %dkB\n",
            block->start, (((length+1)+(length>>2))*sizeof(precomp_instr))/1024);
    DEBUG_print(txtbuffer, DBG_USBGECKO);
#ifdef USE_RECOMP_CACHE
	block->block = RecompCache_Alloc(((length+1)+(length>>2)) * sizeof(precomp_instr),
	                                 block->start>>12);
#else
	block->block = malloc(((length+1)+(length>>2)) * sizeof(precomp_instr));
#endif
	already_exist = 0;
	// Update corresponding blocks if they've already been created
	if(block->end < 0x80000000 || block->start >= 0xc0000000){	
		unsigned long paddr;
		paddr = virtual_to_physical_address(block->start, 2);
		if(blocks[paddr>>12])
			blocks[paddr>>12]->block = block->block;
		paddr += block->end - block->start - 4;
		if(blocks[paddr>>12])
			blocks[paddr>>12]->block = block->block;
	} else {
		if(block->start >= 0x80000000 && block->end < 0xa0000000
		   && blocks[(block->start+0x20000000)>>12])
			blocks[(block->start+0x20000000)>>12]->block = block->block;
		if(block->start >= 0xa0000000 && block->end < 0xc0000000
		   && blocks[(block->start-0x20000000)>>12])
			blocks[(block->start-0x20000000)>>12]->block = block->block;
	}
		
     }
#if 0 //if(dynacore)
     {
	if (!block->code)
	  {
	     block->code = malloc(5000);
	     max_code_length = 5000;
	  }
	else
	  max_code_length = block->max_code_length;
	code_length = 0;
	inst_pointer = &block->code;
	
	if (block->jumps_table)
	  {
	     free(block->jumps_table);
	     block->jumps_table = NULL;
	  }
	init_assembler(NULL, 0);
	init_cache(block->block);
     }
#endif   
   if (!already_exist)
     {
	for (i=0; i<length; i++)
	  {
	     dst = block->block + i;
	     dst->addr = block->start + i*4;
	     dst->reg_cache_infos.need_map = 0;
	     dst->local_addr = code_length;
#ifdef EMU64_DEBUG
	//if(dynacore) gendebug();
#endif
	     RNOTCOMPILED();
	  }
	init_length = code_length;
     }
   else
     {
	code_length = init_length;
	for (i=0; i<length; i++)
	  {
	     dst = block->block + i;
	     dst->reg_cache_infos.need_map = 0;
	     dst->local_addr = i * (code_length / length);
	     dst->ops = NOTCOMPILED;
	  }
     }
   
#if 0
	//if(dynacore)
     {
	free_all_registers();
	//passe2(block->block, 0, i, block);
	block->code_length = code_length;
	block->max_code_length = max_code_length;
	free_assembler(&block->jumps_table, &block->jumps_number);
     }
#endif
   
   /* here we're marking the block as a valid code even if it's not compiled
    * yet as the game should have already set up the code correctly.
    */
   invalid_code_set(block->start>>12, 0);
   if (block->end < 0x80000000 || block->start >= 0xc0000000)
     {	
	unsigned long paddr;
	
	paddr = virtual_to_physical_address(block->start, 2);
	invalid_code_set(paddr>>12, 0);
	if (!blocks[paddr>>12])
	  {
	     blocks[paddr>>12] = malloc(sizeof(precomp_block));
	     blocks[paddr>>12]->code = NULL;
	     blocks[paddr>>12]->block = block->block;
	     blocks[paddr>>12]->jumps_table = NULL;
	     blocks[paddr>>12]->start = paddr & ~0xFFF;
	     blocks[paddr>>12]->end = (paddr & ~0xFFF) + 0x1000;
	  }
	init_block(NULL, blocks[paddr>>12]);
	
	paddr += block->end - block->start - 4;
	invalid_code_set(paddr>>12, 0);
	if (!blocks[paddr>>12])
	  {
	     blocks[paddr>>12] = malloc(sizeof(precomp_block));
	     blocks[paddr>>12]->code = NULL;
	     blocks[paddr>>12]->block = block->block;
	     blocks[paddr>>12]->jumps_table = NULL;
	     blocks[paddr>>12]->start = paddr & ~0xFFF;
	     blocks[paddr>>12]->end = (paddr & ~0xFFF) + 0x1000;
	  }
	init_block(NULL, blocks[paddr>>12]);
     }
   else
     {
	if (block->start >= 0x80000000 && block->end < 0xa0000000 &&
	    invalid_code_get((block->start+0x20000000)>>12))
	  {
	     if (!blocks[(block->start+0x20000000)>>12])
	       {
		  blocks[(block->start+0x20000000)>>12] = malloc(sizeof(precomp_block));
		  blocks[(block->start+0x20000000)>>12]->code = NULL;
		  blocks[(block->start+0x20000000)>>12]->block = block->block;
		  blocks[(block->start+0x20000000)>>12]->jumps_table = NULL;
		  blocks[(block->start+0x20000000)>>12]->start = (block->start+0x20000000) & ~0xFFF;
		  blocks[(block->start+0x20000000)>>12]->end = ((block->start+0x20000000) & ~0xFFF) + 0x1000;
	       }
	     init_block(NULL, blocks[(block->start+0x20000000)>>12]);
	  }
	if (block->start >= 0xa0000000 && block->end < 0xc0000000 &&
	    invalid_code_get((block->start-0x20000000)>>12))
	  {
	     if (!blocks[(block->start-0x20000000)>>12])
	       {
		  blocks[(block->start-0x20000000)>>12] = malloc(sizeof(precomp_block));
		  blocks[(block->start-0x20000000)>>12]->code = NULL;
		  blocks[(block->start-0x20000000)>>12]->block = block->block;
		  blocks[(block->start-0x20000000)>>12]->jumps_table = NULL;
		  blocks[(block->start-0x20000000)>>12]->start = (block->start-0x20000000) & ~0xFFF;
		  blocks[(block->start-0x20000000)>>12]->end = ((block->start-0x20000000) & ~0xFFF) + 0x1000;
	       }
	     init_block(NULL, blocks[(block->start-0x20000000)>>12]);
	  }
     }
   end_section(COMPILER_SECTION);
}

/**********************************************************************
 ********************* recompile a block of code **********************
 **********************************************************************/
void recompile_block(long *source, precomp_block *block, unsigned long func)
{
   int i, length, finished=0;
   start_section(COMPILER_SECTION);
   length = (block->end-block->start)/4;
   dst_block = block;
   
   //for (i=0; i<16; i++) block->md5[i] = 0;
   block->adler32 = 0;
   
#if 0
	//if(dynacore)
     {
	code_length = block->code_length;
	max_code_length = block->max_code_length;
	inst_pointer = &block->code;
	init_assembler(block->jumps_table, block->jumps_number);
	init_cache(block->block + (func&0xFFF)/4);
     }
#endif
   
   for (i=(func&0xFFF)/4; /*i<length &&*/finished!=2; i++)
     {
	if(block->start < 0x80000000 || block->start >= 0xc0000000)
	  {
	     unsigned long address2 = 
	       virtual_to_physical_address(block->start + i*4, 0);
	     if(blocks[address2>>12]->block[(address2&0xFFF)/4].ops == NOTCOMPILED)
	       blocks[address2>>12]->block[(address2&0xFFF)/4].ops = NOTCOMPILED2;
	  }
	
	SRC = source + i;
	src = source[i];
	if (!source[i+1]) check_nop = 1; else check_nop = 0;
	dst = block->block + i;
	dst->addr = block->start + i*4;
	dst->reg_cache_infos.need_map = 0;
	dst->local_addr = code_length;
#ifdef EMU64_DEBUG
	//if(dynacore) gendebug();
#endif
	recomp_ops[((src >> 26) & 0x3F)]();
	dst = block->block + i;
	/*if ((dst+1)->ops != NOTCOMPILED && !delay_slot_compiled &&
	    i < length)
	  {

	//if(dynacore) genlink_subblock();
	     finished = 2;
	  }*/
	if (delay_slot_compiled) 
	  {
	     delay_slot_compiled--;
	     //free_all_registers();
	  }
	
	if (i >= length-2+(length>>2)) finished = 2;
	if (i >= (length-1) && (block->start == 0xa4000000 ||
				block->start >= 0xc0000000 ||
				block->end   <  0x80000000)) finished = 2;
	if (dst->ops == ERET || finished == 1) finished = 2;
	if (/*i >= length &&*/ 
	    (dst->ops == J || dst->ops == J_OUT || dst->ops == JR) &&
	    !(i >= (length-1) && (block->start >= 0xc0000000 ||
				  block->end   <  0x80000000)))
	  finished = 1;
     }
   if (i >= length)
     {
	dst = block->block + i;
	dst->addr = block->start + i*4;
	dst->reg_cache_infos.need_map = 0;
	dst->local_addr = code_length;
#ifdef EMU64_DEBUG
	//if(dynacore) gendebug();
#endif
	RFIN_BLOCK();
	i++;
	if (i < length-1+(length>>2)) // useful when last opcode is a jump
	  {
	     dst = block->block + i;
	     dst->addr = block->start + i*4;
	     dst->reg_cache_infos.need_map = 0;
	     dst->local_addr = code_length;
#ifdef EMU64_DEBUG
	//if(dynacore) gendebug();
#endif
	     RFIN_BLOCK();
	     i++;
	  }
     }
   //else
	//if(dynacore) genlink_subblock();
#if 0
	//if(dynacore)
     {
	free_all_registers();
	passe2(block->block, (func&0xFFF)/4, i, block);
	block->code_length = code_length;
	block->max_code_length = max_code_length;
	free_assembler(&block->jumps_table, &block->jumps_number);
     }
#endif
   //printf("block recompiled (%x-%x)\n", (int)func, (int)(block->start+i*4));
   //getchar();
   end_section(COMPILER_SECTION);
}
#endif

int is_jump()
{
   int dyn=0;
   int jump=0;
   //if(dynacore) dyn=1;
   if(dyn) dynacore = 0;
   recomp_ops[((src >> 26) & 0x3F)]();
   if(dst->ops == J ||
      dst->ops == J_OUT ||
      dst->ops == J_IDLE ||
      dst->ops == JAL ||
      dst->ops == JAL_OUT ||
      dst->ops == JAL_IDLE ||
      dst->ops == BEQ ||
      dst->ops == BEQ_OUT ||
      dst->ops == BEQ_IDLE ||
      dst->ops == BNE ||
      dst->ops == BNE_OUT ||
      dst->ops == BNE_IDLE ||
      dst->ops == BLEZ ||
      dst->ops == BLEZ_OUT ||
      dst->ops == BLEZ_IDLE ||
      dst->ops == BGTZ ||
      dst->ops == BGTZ_OUT ||
      dst->ops == BGTZ_IDLE ||
      dst->ops == BEQL ||
      dst->ops == BEQL_OUT ||
      dst->ops == BEQL_IDLE ||
      dst->ops == BNEL ||
      dst->ops == BNEL_OUT ||
      dst->ops == BNEL_IDLE ||
      dst->ops == BLEZL ||
      dst->ops == BLEZL_OUT ||
      dst->ops == BLEZL_IDLE ||
      dst->ops == BGTZL ||
      dst->ops == BGTZL_OUT ||
      dst->ops == BGTZL_IDLE ||
      dst->ops == JR ||
      dst->ops == JALR ||
      dst->ops == BLTZ ||
      dst->ops == BLTZ_OUT ||
      dst->ops == BLTZ_IDLE ||
      dst->ops == BGEZ ||
      dst->ops == BGEZ_OUT ||
      dst->ops == BGEZ_IDLE ||
      dst->ops == BLTZL ||
      dst->ops == BLTZL_OUT ||
      dst->ops == BLTZL_IDLE ||
      dst->ops == BGEZL ||
      dst->ops == BGEZL_OUT ||
      dst->ops == BGEZL_IDLE ||
      dst->ops == BLTZAL ||
      dst->ops == BLTZAL_OUT ||
      dst->ops == BLTZAL_IDLE ||
      dst->ops == BGEZAL ||
      dst->ops == BGEZAL_OUT ||
      dst->ops == BGEZAL_IDLE ||
      dst->ops == BLTZALL ||
      dst->ops == BLTZALL_OUT ||
      dst->ops == BLTZALL_IDLE ||
      dst->ops == BGEZALL ||
      dst->ops == BGEZALL_OUT ||
      dst->ops == BGEZALL_IDLE ||
      dst->ops == BC1F ||
      dst->ops == BC1F_OUT ||
      dst->ops == BC1F_IDLE ||
      dst->ops == BC1T ||
      dst->ops == BC1T_OUT ||
      dst->ops == BC1T_IDLE ||
      dst->ops == BC1FL ||
      dst->ops == BC1FL_OUT ||
      dst->ops == BC1FL_IDLE ||
      dst->ops == BC1TL ||
      dst->ops == BC1TL_OUT ||
      dst->ops == BC1TL_IDLE)
     jump = 1;
   if(dyn) dynacore = 1;
   return jump;
}

/**********************************************************************
 ************ recompile only one opcode (use for delay slot) **********
 **********************************************************************/
void recompile_opcode()
{
   SRC++;
   src = *SRC;
   dst++;
   dst->addr = (dst-1)->addr + 4;
   dst->reg_cache_infos.need_map = 0;
   if(!is_jump())
     recomp_ops[((src >> 26) & 0x3F)]();
   else
     RNOP();
   delay_slot_compiled = 2;
}

/**********************************************************************
 ************** decode one opcode (for the interpreter) ***************
 **********************************************************************/
extern unsigned long op;
void prefetch_opcode(unsigned long instr)
{
   dst = PC;
   src = instr;
   op = instr;
   //printf("Interpreting %08x\n",op);
   recomp_ops[((src >> 26) & 0x3F)]();
}
