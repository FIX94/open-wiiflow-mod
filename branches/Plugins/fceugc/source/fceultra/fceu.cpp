/* FCE Ultra - NES/Famicom Emulator
*
* Copyright notice for this file:
*  Copyright (C) 2003 Xodnizel
*
* This program is free software; you can redistribute it and/or modify
* it under the terms of the GNU General Public License as published by
* the Free Software Foundation; either version 2 of the License, or
* (at your option) any later version.
*
* This program is distributed in the hope that it will be useful,
* but WITHOUT ANY WARRANTY; without even the implied warranty of
* MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
* GNU General Public License for more details.
*
* You should have received a copy of the GNU General Public License
* along with this program; if not, write to the Free Software
* Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
*/

#include <string>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <time.h>
#include "types.h"
#include "x6502.h"
#include "fceu.h"
#include "ppu.h"
#include "sound.h"
#include "netplay.h"
#include "file.h"
#include "utils/endian.h"
#include "utils/memory.h"
#include "utils/crc32.h"

#include "cart.h"
#include "nsf.h"
#include "fds.h"
#include "ines.h"
#include "unif.h"
#include "cheat.h"
#include "palette.h"
#include "state.h"
#include "movie.h"
#include "video.h"
#include "input.h"
#include "file.h"
#include "vsuni.h"
#include "ines.h"

#ifdef WIN32
#include "drivers/win/pref.h"

#include "drivers/win/taseditor/greenzone.h"
extern GREENZONE greenzone;
#endif

#include <fstream>
#include <sstream>

#ifdef _S9XLUA_H
#include "fceulua.h"
#endif

//TODO - we really need some kind of global platform-specific options api
#ifdef WIN32
#include "drivers/win/main.h"
#include "drivers/win/cheat.h"
#include "drivers/win/texthook.h"
#include "drivers/win/ram_search.h"
#include "drivers/win/ramwatch.h"
#include "drivers/win/memwatch.h"
#include "drivers/win/tracer.h"
#else
#ifdef GEKKO
#include "driver.h"
#else
#include "drivers/sdl/sdl.h"
#endif
#endif

using namespace std;

int AFon = 1, AFoff = 1, AutoFireOffset = 0; //For keeping track of autofire settings
bool justLagged = false;
bool frameAdvanceLagSkip = false; //If this is true, frame advance will skip over lag frame (i.e. it will emulate 2 frames instead of 1)
bool AutoSS = false;		//Flagged true when the first auto-savestate is made while a game is loaded, flagged false on game close
bool movieSubtitles = true; //Toggle for displaying movie subtitles
bool DebuggerWasUpdated = false; //To prevent the debugger from updating things without being updated.

FCEUGI::FCEUGI()
: filename(0)
, archiveFilename(0)
{
	printf("%08x",opsize);
}

FCEUGI::~FCEUGI()
{
	if(filename) delete filename;
	if(archiveFilename) delete archiveFilename;
}

bool CheckFileExists(const char* filename)
{
	//This function simply checks to see if the given filename exists
	if (!filename) return false;
	fstream test;
	test.open(filename,fstream::in);
		
	if (test.fail())
	{
		test.close();
		return false; 
	}
	else
	{
		test.close();
		return true; 
	}
}

void FCEU_TogglePPU(void)
{
	newppu ^= 1;
	if (newppu)
	{
		FCEU_DispMessage("New PPU loaded", 0);
		FCEUI_printf("New PPU loaded");
	}
	else
	{
		FCEU_DispMessage("Old PPU loaded", 0);
		FCEUI_printf("Old PPU loaded");
	}
}

static void FCEU_CloseGame(void)
{
	if(GameInfo)
	{

#ifdef WIN32
//SP CODE
	extern char LoadedRomFName[2048];

	if (storePreferences(LoadedRomFName))
	{
		FCEUD_PrintError("Couldn't store debugging data");
	}
#endif

		if(FCEUnetplay)
		{
			FCEUD_NetworkClose();
		}

		if(GameInfo->name)
		{
			free(GameInfo->name);
			GameInfo->name=0;
		}

		if(GameInfo->type!=GIT_NSF)
		{
			FCEU_FlushGameCheats(0,0);
		}

		GameInterface(GI_CLOSE);

		FCEUI_StopMovie();

		ResetExState(0,0);

		//clear screen when game is closed
		extern uint8 *XBuf;
		if(XBuf)
			memset(XBuf,0,256*256);

		CloseGenie();

		delete GameInfo;
		GameInfo = 0;
				
		currFrameCounter = 0;

		//Reset flags for Undo/Redo/Auto Savestating //adelikat: TODO: maybe this stuff would be cleaner as a struct or class
		lastSavestateMade[0] = 0;
		undoSS = false;
		redoSS = false;
		lastLoadstateMade[0] = 0;
		undoLS = false;
		redoLS = false;
		AutoSS = false;
	}
}


uint64 timestampbase;


FCEUGI *GameInfo = 0;

void (*GameInterface)(GI h);
void (*GameStateRestore)(int version);

readfunc ARead[0x10000];
writefunc BWrite[0x10000];
static readfunc *AReadG;
static writefunc *BWriteG;
static int RWWrap=0;

//mbg merge 7/18/06 docs
//bit0 indicates whether emulation is paused
//bit1 indicates whether emulation is in frame step mode
int EmulationPaused=0;
bool frameAdvanceRequested=false;
int frameAdvanceDelay;

//indicates that the emulation core just frame advanced (consumed the frame advance state and paused)
bool JustFrameAdvanced=false;

static int *AutosaveStatus; //is it safe to load Auto-savestate
static int AutosaveIndex = 0; //which Auto-savestate we're on
int AutosaveQty = 4; // Number of Autosaves to store
int AutosaveFrequency = 256; // Number of frames between autosaves

// Flag that indicates whether the Auto-save option is enabled or not
int EnableAutosave = 0;

///a wrapper for unzip.c
extern "C" FILE *FCEUI_UTF8fopen_C(const char *n, const char *m) { return ::FCEUD_UTF8fopen(n,m); }

static DECLFW(BNull)
{

}

static DECLFR(ANull)
{
	return(X.DB);
}

int AllocGenieRW(void)
{
	if(!(AReadG=(readfunc *)FCEU_malloc(0x8000*sizeof(readfunc))))
		return 0;
	if(!(BWriteG=(writefunc *)FCEU_malloc(0x8000*sizeof(writefunc))))
		return 0;
	RWWrap=1;
	return 1;
}

void FlushGenieRW(void)
{
	int32 x;

	if(RWWrap)
	{
		for(x=0;x<0x8000;x++)
		{
			ARead[x+0x8000]=AReadG[x];
			BWrite[x+0x8000]=BWriteG[x];
		}
		free(AReadG);
		free(BWriteG);
		AReadG=0;
		BWriteG=0;
		RWWrap=0;
	}
}

readfunc GetReadHandler(int32 a)
{
	if(a>=0x8000 && RWWrap)
		return AReadG[a-0x8000];
	else
		return ARead[a];
}
void SetReadHandler(int32 start, int32 end, readfunc func)
{
	int32 x;
	if(!func)
		func=ANull;

	if(RWWrap)
		for(x=end;x>=start;x--)
		{
			if(x>=0x8000)
				AReadG[x-0x8000]=func;
			else
				ARead[x]=func;
		}
	else

		for(x=end;x>=start;x--)
			ARead[x]=func;
}

writefunc GetWriteHandler(int32 a)
{
	if(RWWrap && a>=0x8000)
		return BWriteG[a-0x8000];
	else
		return BWrite[a];
}

void SetWriteHandler(int32 start, int32 end, writefunc func)
{
	int32 x;

	if(!func)
		func=BNull;

	if(RWWrap)
		for(x=end;x>=start;x--)
		{
			if(x>=0x8000)
				BWriteG[x-0x8000]=func;
			else
				BWrite[x]=func;
		}
	else
		for(x=end;x>=start;x--)
			BWrite[x]=func;
}

uint8 *GameMemBlock;
uint8 *RAM;

//---------
//windows might need to allocate these differently, so we have some special code

static void AllocBuffers()
{
	GameMemBlock = (uint8*)FCEU_gmalloc(GAME_MEM_BLOCK_SIZE);
	RAM = (uint8*)FCEU_gmalloc(0x800);
}

static void FreeBuffers()
{
	FCEU_free(GameMemBlock);
	FCEU_free(RAM);
}
//------

uint8 PAL=0;

static DECLFW(BRAML)
{
	RAM[A]=V;
	#ifdef _S9XLUA_H
	CallRegisteredLuaMemHook(A, 1, V, LUAMEMHOOK_WRITE);
	#endif
}

static DECLFW(BRAMH)
{
	RAM[A&0x7FF]=V;
	#ifdef _S9XLUA_H
	CallRegisteredLuaMemHook(A&0x7FF, 1, V, LUAMEMHOOK_WRITE);
	#endif
}

static DECLFR(ARAML)
{
	return RAM[A];
}

static DECLFR(ARAMH)
{
	return RAM[A&0x7FF];
}


void ResetGameLoaded(void)
{
	if(GameInfo) FCEU_CloseGame();
	EmulationPaused = 0; //mbg 5/8/08 - loading games while paused was bad news. maybe this fixes it
	GameStateRestore=0;
	PPU_hook=0;
	GameHBIRQHook=0;
	FFCEUX_PPURead = 0;
	FFCEUX_PPUWrite = 0;
	if(GameExpSound.Kill)
		GameExpSound.Kill();
	memset(&GameExpSound,0,sizeof(GameExpSound));
	MapIRQHook=0;
	MMC5Hack=0;
	PAL&=1;
	pale=0;
}

int UNIFLoad(const char *name, FCEUFILE *fp);
int iNESLoad(const char *name, FCEUFILE *fp, int OverwriteVidMode);
int FDSLoad(const char *name, FCEUFILE *fp);
int NSFLoad(const char *name, FCEUFILE *fp);

//char lastLoadedGameName [2048] = {0,}; // hack for movie WRAM clearing on record from poweron


FCEUGI *FCEUI_LoadGameVirtual(const char *name, int OverwriteVidMode)
{
	//mbg merge 7/17/07 - why is this here
	//#ifdef WIN32
	//	StopSound();
	//#endif

	//----------
	//attempt to open the files
	FCEUFILE *fp;

	FCEU_printf("Loading %s...\n\n",name);

	const char* romextensions[] = {"nes","fds",0};
	fp=FCEU_fopen(name,0,"rb",0,-1,romextensions);

	if(!fp) {
		FCEU_PrintError("Error opening \"%s\"!",name);
		return 0;
	}

	GetFileBase(fp->filename.c_str());
	//---------

	//file opened ok. start loading.

	ResetGameLoaded();

	if (!AutosaveStatus)
		AutosaveStatus = (int*)FCEU_dmalloc(sizeof(int)*AutosaveQty);
	for (AutosaveIndex=0; AutosaveIndex<AutosaveQty; ++AutosaveIndex)
		AutosaveStatus[AutosaveIndex] = 0;

	FCEU_CloseGame();
	GameInfo = new FCEUGI();
	memset(GameInfo, 0, sizeof(FCEUGI));

	GameInfo->filename = strdup(fp->filename.c_str());
	if(fp->archiveFilename != "") GameInfo->archiveFilename = strdup(fp->archiveFilename.c_str());
	GameInfo->archiveCount = fp->archiveCount;

	GameInfo->soundchan = 0;
	GameInfo->soundrate = 0;
	GameInfo->name=0;
	GameInfo->type=GIT_CART;
	GameInfo->vidsys=GIV_USER;
	GameInfo->input[0]=GameInfo->input[1]=SI_UNSET;
	GameInfo->inputfc=SIFC_UNSET;
	GameInfo->cspecial=SIS_NONE;

	//try to load each different format
	bool FCEUXLoad(const char *name, FCEUFILE *fp);
	/*if(FCEUXLoad(name,fp))
		goto endlseq;*/
	if(iNESLoad(name,fp,OverwriteVidMode))
		goto endlseq;
	if(NSFLoad(name,fp))
		goto endlseq;
	if(UNIFLoad(name,fp))
		goto endlseq;
	if(FDSLoad(name,fp))
		goto endlseq;

	FCEU_PrintError("An error occurred while loading the file.");
	FCEU_fclose(fp);

	delete GameInfo;
	GameInfo = 0;

	return 0;

endlseq:

	FCEU_fclose(fp);

#ifdef WIN32
// ################################## Start of SP CODE ###########################
		extern char LoadedRomFName[2048];
		extern int loadDebugDataFailed;

		if ((loadDebugDataFailed = loadPreferences(LoadedRomFName)))
		{
			FCEUD_PrintError("Couldn't load debugging data");
		}

// ################################## End of SP CODE ###########################
#endif

	FCEU_ResetVidSys();

	if(GameInfo->type!=GIT_NSF)
		if(FSettings.GameGenie)
			OpenGenie();
	PowerNES();

	if(GameInfo->type!=GIT_NSF)
		FCEU_LoadGamePalette();

	FCEU_ResetPalette();
	FCEU_ResetMessages();	// Save state, status messages, etc.

	if(GameInfo->type!=GIT_NSF)
		FCEU_LoadGameCheats(0);

#if defined (WIN32) || defined (WIN64)
	DoDebuggerDataReload(); // Reloads data without reopening window
#endif

	return GameInfo;
}

FCEUGI *FCEUI_LoadGame(const char *name, int OverwriteVidMode)
{
	return FCEUI_LoadGameVirtual(name,OverwriteVidMode);
}


//Return: Flag that indicates whether the function was succesful or not.
bool FCEUI_Initialize()
{
	srand(time(0));

	if(!FCEU_InitVirtualVideo())
	{
		return false;
	}

	AllocBuffers();

	// Initialize some parts of the settings structure
	//mbg 5/7/08 - I changed the ntsc settings to match pal.
	//this is more for precision emulation, instead of entertainment, which is what fceux is all about nowadays
	memset(&FSettings,0,sizeof(FSettings));
	//FSettings.UsrFirstSLine[0]=8;
	FSettings.UsrFirstSLine[0]=0;
	FSettings.UsrFirstSLine[1]=0;
	//FSettings.UsrLastSLine[0]=231;
	FSettings.UsrLastSLine[0]=239;
	FSettings.UsrLastSLine[1]=239;
	FSettings.SoundVolume=150;		//0-150 scale
	FSettings.TriangleVolume=256;	//0-256 scale (256 is max volume)
	FSettings.Square1Volume=256;	//0-256 scale (256 is max volume)
	FSettings.Square2Volume=256;	//0-256 scale (256 is max volume)
	FSettings.NoiseVolume=256;		//0-256 scale (256 is max volume)
	FSettings.PCMVolume=256;		//0-256 scale (256 is max volume)

	FCEUPPU_Init();

	X6502_Init();

	return true;
}

void FCEUI_Kill(void)
{
	#ifdef _S9XLUA_H
	FCEU_LuaStop();
	#endif
	FCEU_KillVirtualVideo();
	FCEU_KillGenie();
	FreeBuffers();
}

int rapidAlternator = 0;
int AutoFirePattern[8] = {1,0,0,0,0,0,0,0};
int AutoFirePatternLength = 2;

void SetAutoFirePattern(int onframes, int offframes)
{
	int i;
	for(i = 0; i < onframes && i < 8; i++)
	{
		AutoFirePattern[i] = 1;
	}
	for(;i < 8; i++)
	{
		AutoFirePattern[i] = 0;
	}
	if(onframes + offframes < 2)
	{
		AutoFirePatternLength = 2;
	}
	else if(onframes + offframes > 8)
	{
		AutoFirePatternLength = 8;
	}
	else
	{
		AutoFirePatternLength = onframes + offframes;
	}
	AFon = onframes; AFoff = offframes;
}

void SetAutoFireOffset(int offset)
{
	if(offset < 0 || offset > 8) return;
	AutoFireOffset = offset;
}

void AutoFire(void)
{
	static int counter = 0;
	if (justLagged == false) 
		counter = (counter + 1) % (8*7*5*3);
	//If recording a movie, use the frame # for the autofire so the offset
	//doesn't get screwed up when loading.
	if(FCEUMOV_Mode(MOVIEMODE_RECORD | MOVIEMODE_PLAY))
	{
		rapidAlternator= AutoFirePattern[(AutoFireOffset + FCEUMOV_GetFrame())%AutoFirePatternLength]; //adelikat: TODO: Think through this, MOVIEMODE_FINISHED should not use movie data for auto-fire?
	}
	else
	{
		rapidAlternator= AutoFirePattern[(AutoFireOffset + counter)%AutoFirePatternLength];
	}
}

void UpdateAutosave(void);

///Emulates a single frame.

///Skip may be passed in, if FRAMESKIP is #defined, to cause this to emulate more than one frame
void FCEUI_Emulate(uint8 **pXBuf, int32 **SoundBuf, int32 *SoundBufSize, int skip)
{
	//skip initiates frame skip if 1, or frame skip and sound skip if 2
	int r,ssize;

	JustFrameAdvanced = false;

	if (frameAdvanceRequested)
	{
		if (frameAdvanceDelay==0 || frameAdvanceDelay>=10)
			EmulationPaused = 3;
		if (frameAdvanceDelay==0 || frameAdvanceDelay < 10)
			frameAdvanceDelay++;
	}

	if(EmulationPaused&2)
		EmulationPaused &= ~1;        // clear paused flag temporarily (frame advance)
	else if((EmulationPaused&1))
	{
		memcpy(XBuf, XBackBuf, 256*256);
		FCEU_PutImage();
		*pXBuf=XBuf;
		*SoundBuf=WaveFinal;
		*SoundBufSize=0;

		return;
	}

	AutoFire();
	UpdateAutosave();

#ifdef _S9XLUA_H
	FCEU_LuaFrameBoundary();
#endif

	FCEU_UpdateInput();
	lagFlag = 1;

#ifdef _S9XLUA_H
	CallRegisteredLuaFunctions(LUACALL_BEFOREEMULATION);
#endif

	if(geniestage!=1) FCEU_ApplyPeriodicCheats();
	r = FCEUPPU_Loop(skip);

	if (skip != 2) ssize=FlushEmulateSound(); //If skip = 2 we are skipping sound processing

#ifdef _S9XLUA_H
	CallRegisteredLuaFunctions(LUACALL_AFTEREMULATION);
#endif

#ifdef WIN32
	//These Windows only dialogs need to be updated only once per frame so they are included here
	UpdateCheatList(); // CaH4e3: can't see why, this is only cause problems with selection - adelikat: selection is only a problem when not paused, it shoudl be paused to select, we want to see the values update
	UpdateTextHooker();
	Update_RAM_Search(); // Update_RAM_Watch() is also called.
	RamChange();
	UpdateLogWindow();
	//FCEUI_AviVideoUpdate(XBuf);
	extern int KillFCEUXonFrame;
	if (KillFCEUXonFrame && (FCEUMOV_GetFrame() >= KillFCEUXonFrame))
		DoFCEUExit();
#endif

	timestampbase += timestamp;
	timestamp = 0;

	*pXBuf=skip?0:XBuf;
	if (skip == 2) //If skip = 2, then bypass sound
	{
		*SoundBuf=0;
		*SoundBufSize=0;
	}
	else
	{
		*SoundBuf=WaveFinal;
		*SoundBufSize=ssize;
	}

	if (EmulationPaused&2 && ( !frameAdvanceLagSkip || !lagFlag) )
	//Lots of conditions here.  EmulationPaused&2 must be true.  In addition frameAdvanceLagSkip or lagFlag must be false
	{
		EmulationPaused = 1;		   // restore paused flag
		JustFrameAdvanced = true;
		#ifdef WIN32
			if(soundoptions&SO_MUTEFA) //mute the frame advance if the user requested it
				*SoundBufSize=0;       //keep sound muted
		#endif

	}

	if (lagFlag)
	{
		lagCounter++;
		justLagged = true;
	}
	else justLagged = false;

	if (movieSubtitles)
		ProcessSubtitles();

#ifdef WIN32
	if(FCEUMOV_Mode(MOVIEMODE_TASEDITOR))
		greenzone.TryDumpIncremental(lagFlag != 0);
#endif
}

void FCEUI_CloseGame(void)
{
	if(!FCEU_IsValidUI(FCEUI_CLOSEGAME))
		return;

	FCEU_CloseGame();
}

void ResetNES(void)
{
	FCEUMOV_AddCommand(FCEUNPCMD_RESET);
	if(!GameInfo) return;
	GameInterface(GI_RESETM2);
	FCEUSND_Reset();
	FCEUPPU_Reset();
	X6502_Reset();

	// clear back baffer
	extern uint8 *XBackBuf;
	memset(XBackBuf,0,256*256);
}

void FCEU_MemoryRand(uint8 *ptr, uint32 size)
{
	int x=0;
	while(size)
	{
		*ptr=(x&4)?0xFF:0x00;
		x++;
		size--;
		ptr++;
	}
}

void hand(X6502 *X, int type, unsigned int A)
{

}

int suppressAddPowerCommand=0; // hack... yeah, I know...
void PowerNES(void)
{
	//void MapperInit();
	//MapperInit();

	if(!suppressAddPowerCommand)
		FCEUMOV_AddCommand(FCEUNPCMD_POWER);
	if(!GameInfo) return;

	FCEU_CheatResetRAM();
	FCEU_CheatAddRAM(2,0,RAM);

	GeniePower();

	FCEU_MemoryRand(RAM,0x800);
	//memset(RAM,0xFF,0x800);

	SetReadHandler(0x0000,0xFFFF,ANull);
	SetWriteHandler(0x0000,0xFFFF,BNull);

	SetReadHandler(0,0x7FF,ARAML);
	SetWriteHandler(0,0x7FF,BRAML);

	SetReadHandler(0x800,0x1FFF,ARAMH); // Part of a little
	SetWriteHandler(0x800,0x1FFF,BRAMH); //hack for a small speed boost.

	InitializeInput();
	FCEUSND_Power();
	FCEUPPU_Power();

	//Have the external game hardware "powered" after the internal NES stuff.  Needed for the NSF code and VS System code.
	GameInterface(GI_POWER);
	if(GameInfo->type==GIT_VSUNI)
		FCEU_VSUniPower();

	//if we are in a movie, then reset the saveram
	extern int disableBatteryLoading;
	if(disableBatteryLoading)
		GameInterface(GI_RESETSAVE);


	timestampbase=0;
	LagCounterReset();

	X6502_Power();
	FCEU_PowerCheats();
	// clear back baffer
	extern uint8 *XBackBuf;
	memset(XBackBuf,0,256*256);

#ifdef WIN32
	Update_RAM_Search(); // Update_RAM_Watch() is also called.
#endif
}

void FCEU_ResetVidSys(void)
{
	int w;

	if(GameInfo->vidsys==GIV_NTSC)
		w=0;
	else if(GameInfo->vidsys==GIV_PAL)
		w=1;
	else
		w=FSettings.PAL;

	PAL=w?1:0;
	FCEUPPU_SetVideoSystem(w);
	SetSoundVariables();
}

FCEUS FSettings;

void FCEU_printf(char *format, ...)
{
#ifndef GEKKO
	char temp[2048];

	va_list ap;

	va_start(ap,format);
	vsnprintf(temp,sizeof(temp),format,ap);
	FCEUD_Message(temp);

	va_end(ap);
#endif
}

void FCEU_PrintError(char *format, ...)
{
#ifndef GEKKO
	char temp[2048];

	va_list ap;

	va_start(ap,format);
	vsnprintf(temp,sizeof(temp),format,ap);
	FCEUD_PrintError(temp);

	va_end(ap);
#endif
}

void FCEUI_SetRenderedLines(int ntscf, int ntscl, int palf, int pall)
{
	FSettings.UsrFirstSLine[0]=ntscf;
	FSettings.UsrLastSLine[0]=ntscl;
	FSettings.UsrFirstSLine[1]=palf;
	FSettings.UsrLastSLine[1]=pall;
	if(PAL)
	{
		FSettings.FirstSLine=FSettings.UsrFirstSLine[1];
		FSettings.LastSLine=FSettings.UsrLastSLine[1];
	}
	else
	{
		FSettings.FirstSLine=FSettings.UsrFirstSLine[0];
		FSettings.LastSLine=FSettings.UsrLastSLine[0];
	}

}

void FCEUI_SetVidSystem(int a)
{
	FSettings.PAL=a?1:0;
	if(GameInfo)
	{
		FCEU_ResetVidSys();
		FCEU_ResetPalette();
		FCEUD_VideoChanged();
	}
}

int FCEUI_GetCurrentVidSystem(int *slstart, int *slend)
{
	if(slstart)
		*slstart=FSettings.FirstSLine;
	if(slend)
		*slend=FSettings.LastSLine;
	return(PAL);
}

//Enable or disable Game Genie option.
void FCEUI_SetGameGenie(bool a)
{
	FSettings.GameGenie = a;
}

//this variable isn't used at all, snap is always name-based
//void FCEUI_SetSnapName(bool a)
//{
//	FSettings.SnapName = a;
//}

int32 FCEUI_GetDesiredFPS(void)
{
	if(PAL)
		return(838977920); // ~50.007
	else
		return(1008307711);	// ~60.1
}

int FCEUI_EmulationPaused(void)
{
	return (EmulationPaused&1);
}

int FCEUI_EmulationFrameStepped()
{
	return (EmulationPaused&2);
}

void FCEUI_ClearEmulationFrameStepped()
{
	EmulationPaused &=~2;
}

//mbg merge 7/18/06 added
//ideally maybe we shouldnt be using this, but i need it for quick merging
void FCEUI_SetEmulationPaused(int val)
{
	EmulationPaused = val;
}

void FCEUI_ToggleEmulationPause(void)
{
	EmulationPaused = (EmulationPaused&1)^1;
	DebuggerWasUpdated = false;
}

void FCEUI_FrameAdvanceEnd(void)
{
	frameAdvanceRequested = false;
}

void FCEUI_FrameAdvance(void)
{
	frameAdvanceRequested = true;
	frameAdvanceDelay = 0;
}

static int AutosaveCounter = 0;

void UpdateAutosave(void)
{
	if(!EnableAutosave || turbo)
		return;

	char * f;
	if(++AutosaveCounter >= AutosaveFrequency)
	{
		AutosaveCounter = 0;
		AutosaveIndex = (AutosaveIndex + 1) % AutosaveQty;
		f = strdup(FCEU_MakeFName(FCEUMKF_AUTOSTATE,AutosaveIndex,0).c_str());
		FCEUSS_Save(f);
		AutoSS = true;	//Flag that an auto-savestate was made
		free(f);
		AutosaveStatus[AutosaveIndex] = 1;
	}
}

void FCEUI_Autosave(void)
{
	if(!EnableAutosave || !AutoSS || FCEUMOV_Mode(MOVIEMODE_TASEDITOR))
		return;

	if(AutosaveStatus[AutosaveIndex] == 1)
	{
		char * f;
		f = strdup(FCEU_MakeFName(FCEUMKF_AUTOSTATE,AutosaveIndex,0).c_str());
		FCEUSS_Load(f);
		free(f);

		//Set pointer to previous available slot
		if(AutosaveStatus[(AutosaveIndex + AutosaveQty-1)%AutosaveQty] == 1)
		{
			AutosaveIndex = (AutosaveIndex + AutosaveQty-1)%AutosaveQty;
		}

		//Reset time to next Auto-save
		AutosaveCounter = 0;
	}
}

int FCEU_TextScanlineOffset(int y)
{
	return FSettings.FirstSLine*256;
}
int FCEU_TextScanlineOffsetFromBottom(int y)
{
	return (FSettings.LastSLine-y)*256;
}

bool FCEU_IsValidUI(EFCEUI ui)
{
	switch(ui)
	{
	case FCEUI_OPENGAME:
	case FCEUI_CLOSEGAME:
		if(FCEUMOV_Mode(MOVIEMODE_TASEDITOR)) return false;
		break;
	case FCEUI_RECORDMOVIE:
	case FCEUI_PLAYMOVIE:
	case FCEUI_QUICKSAVE:
	case FCEUI_QUICKLOAD:
	case FCEUI_SAVESTATE:
	case FCEUI_LOADSTATE:
	case FCEUI_NEXTSAVESTATE:
	case FCEUI_PREVIOUSSAVESTATE:
	case FCEUI_VIEWSLOTS:
		if(!GameInfo) return false;
		if(FCEUMOV_Mode(MOVIEMODE_TASEDITOR)) return false;
		break;

	case FCEUI_STOPMOVIE:
	case FCEUI_PLAYFROMBEGINNING:
		return (FCEUMOV_Mode(MOVIEMODE_PLAY|MOVIEMODE_RECORD|MOVIEMODE_FINISHED));

	case FCEUI_STOPAVI:
		return FCEUI_AviIsRecording();

	case FCEUI_TASEDITOR:
		if(!GameInfo) return false;
		if(FCEUMOV_Mode(MOVIEMODE_TASEDITOR)) return false;
		break;

	case FCEUI_RESET:
		if(!GameInfo) return false;
		if(FCEUMOV_Mode(MOVIEMODE_FINISHED|MOVIEMODE_TASEDITOR|MOVIEMODE_PLAY)) return false;
		break;

	case FCEUI_POWER:
		if(!GameInfo) return false;
		if(FCEUMOV_Mode(MOVIEMODE_RECORD)) return true;
		if(!FCEUMOV_Mode(MOVIEMODE_INACTIVE)) return false;
		break;

	}
	return true;
}

//---------------------
//experimental new mapper and ppu system follows

class FCEUXCart {
public:
	int mirroring;
	int chrPages, prgPages;
	uint32 chrSize, prgSize;
	char* CHR, *PRG;

	FCEUXCart()
		: CHR(0)
		, PRG(0)
	{}

	~FCEUXCart() {
		if(CHR) delete[] CHR;
		if(PRG) delete[] PRG;
	}

	virtual void Power() {
	}

protected:
	//void SetReadHandler(int32 start, int32 end, readfunc func) {
};

FCEUXCart* cart = 0;

//uint8 Read_ByteFromRom(uint32 A) {
//	if(A>=cart->prgSize) return 0xFF;
//	return cart->PRG[A];
//}
//
//uint8 Read_Unmapped(uint32 A) {
//	return 0xFF;
//}



class NROM : FCEUXCart {
public:
	virtual void Power() {
		SetReadHandler(0x8000,0xFFFF,CartBR);
		setprg16(0x8000,0);
		setprg16(0xC000,~0);
		setchr8(0);

		vnapage[0] = NTARAM;
		vnapage[2] = NTARAM;
		vnapage[1] = NTARAM+0x400;
		vnapage[3] = NTARAM+0x400;
		PPUNTARAM=0xF;
	}
};

void FCEUXGameInterface(GI command) {
	switch(command) {
		case GI_POWER:
			cart->Power();
	}
}



bool FCEUXLoad(const char *name, FCEUFILE *fp)
{
	//read ines header
	iNES_HEADER head;
	if(FCEU_fread(&head,1,16,fp)!=16)
		return false;

	//validate header
	if(memcmp(&head,"NES\x1a",4))
		return 0;

	int mapper = (head.ROM_type>>4);
	mapper |= (head.ROM_type2&0xF0);

	//choose what kind of cart to use.
	cart = (FCEUXCart*)new NROM();

	//fceu ines loading code uses 256 here when the romsize is 0.
	cart->prgPages = head.ROM_size;
	if(cart->prgPages == 0) {
		printf("FCEUX: received zero prgpages\n");
		cart->prgPages = 256;
	}

	cart->chrPages = head.VROM_size;

	cart->mirroring = (head.ROM_type&1);
	if(head.ROM_type&8) cart->mirroring=2;

	//skip trainer
	bool hasTrainer = (head.ROM_type&4)!=0;
	if(hasTrainer) {
		FCEU_fseek(fp,512,SEEK_CUR);
	}

	//load data
	cart->prgSize = cart->prgPages*16*1024;
	cart->chrSize = cart->chrPages*8*1024;
	cart->PRG = new char[cart->prgSize];
	cart->CHR = new char[cart->chrSize];
	FCEU_fread(cart->PRG,1,cart->prgSize,fp);
	FCEU_fread(cart->CHR,1,cart->chrSize,fp);

	//setup the emulator
	GameInterface=FCEUXGameInterface;
	ResetCartMapping();
	SetupCartPRGMapping(0,(uint8*)cart->PRG,cart->prgSize,0);
	SetupCartCHRMapping(0,(uint8*)cart->CHR,cart->chrSize,0);

	return true;
}


uint8 FCEU_ReadRomByte(uint32 i) {
	extern iNES_HEADER head;
	if(i < 16) return *((unsigned char *)&head+i);
	if(i < 16+PRGsize[0])return PRGptr[0][i-16];
	if(i < 16+PRGsize[0]+CHRsize[0])return CHRptr[0][i-16-PRGsize[0]];
	return 0;
}
