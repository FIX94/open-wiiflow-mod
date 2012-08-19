/****************************************************************************
 * Visual Boy Advance GX
 *
 * Tantric September 2008
 *
 * vbasupport.cpp
 *
 * VBA support code
 ***************************************************************************/

#include <gccore.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <wiiuse/wpad.h>
#include <malloc.h>
#include <ogc/lwp_watchdog.h>

#include "vbagx.h"
#include "fileop.h"
#include "filebrowser.h"
#include "audio.h"
#include "vmmem.h"
#include "input.h"
#include "gameinput.h"
#include "video.h"
#include "menu.h"
#include "gcunzip.h"
#include "gamesettings.h"
#include "preferences.h"
#include "fastmath.h"
#include "utils/pngu.h"
#include "utils/unzip/unzip.h"

#include "vba/Util.h"
#include "vba/common/Port.h"
#include "vba/common/Patch.h"
#include "vba/gba/Flash.h"
#include "vba/gba/RTC.h"
#include "vba/gba/Sound.h"
#include "vba/gba/Cheats.h"
#include "vba/gba/GBA.h"
#include "vba/gba/agbprint.h"
#include "vba/gb/gb.h"
#include "vba/gb/gbGlobals.h"
#include "vba/gb/gbCheats.h"
#include "vba/gb/gbSound.h"

static u32 start;
int cartridgeType = 0;
u32 RomIdCode;
char RomTitle[17];

int SunBars = 3;
bool TiltSideways = false;

/****************************************************************************
 * VBA Globals
 ***************************************************************************/

int systemSaveUpdateCounter = SYSTEM_SAVE_NOT_UPDATED;

int systemDebug = 0;
int emulating = 0;

int systemFrameSkip = 0;
int systemVerbose = 0;

int systemRedShift = 0;
int systemBlueShift = 0;
int systemGreenShift = 0;
int systemColorDepth = 0;
u16 systemGbPalette[24];
u16 systemColorMap16[0x10000];
u32 *systemColorMap32 = NULL;

void gbSetPalette(u32 RRGGBB[]);
bool StartColorizing();
void StopColorizing();
extern bool ColorizeGameboy;
extern u16 systemMonoPalette[14];
void gbSetBgPal(u8 WhichPal, u32 bright, u32 medium, u32 dark, u32 black=0x000000);
void gbSetSpritePal(u8 WhichPal, u32 bright, u32 medium, u32 dark);

struct EmulatedSystem emulator =
{
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
	false,
	0
};

/****************************************************************************
* systemGetClock
*
* Returns number of milliseconds since program start
****************************************************************************/
u32 systemGetClock( void )
{
	u32 now = gettime();
	return diff_usec(start, now) / 1000;
}

void systemFrame() {}
void systemScreenCapture(int a) {}
void systemShowSpeed(int speed) {}
void systemGbBorderOn() {}

bool systemPauseOnFrame()
{
	return false;
}

static u32 lastTime = 0;
#define RATE60HZ 166666.67 // 1/6 second or 166666.67 usec

void system10Frames(int rate)
{
	u32 time = gettime();
	u32 diff = diff_usec(lastTime, time);

	// expected diff - actual diff
	u32 timeOff = RATE60HZ - diff;

	if(timeOff > 0 && timeOff < 100000) // we're running ahead!
		usleep(timeOff); // let's take a nap
	else
		timeOff = 0; // timeoff was not valid

	int speed = (RATE60HZ/diff)*100;

	if (cartridgeType == 2) // GBA games require frameskipping
	{
		// consider increasing skip
		if(speed < 60)
			systemFrameSkip += 4;
		else if(speed < 70)
			systemFrameSkip += 3;
		else if(speed < 80)
			systemFrameSkip += 2;
		else if(speed < 98)
			++systemFrameSkip;

		// consider decreasing skip
		else if(speed > 185)
			systemFrameSkip -= 3;
		else if(speed > 145)
			systemFrameSkip -= 2;
		else if(speed > 125)
			systemFrameSkip -= 1;

		// correct invalid frame skip values
		if(systemFrameSkip > 20)
			systemFrameSkip = 20;
		else if(systemFrameSkip < 0)
			systemFrameSkip = 0;
	}
	lastTime = gettime();
}

/****************************************************************************
* System
****************************************************************************/

void systemGbPrint(u8 *data,int pages,int feed,int palette, int contrast) {}
void debuggerOutput(const char *s, u32 addr) {}
void (*dbgOutput)(const char *s, u32 addr) = debuggerOutput;
void systemMessage(int num, const char *msg, ...) {}

bool MemCPUReadBatteryFile(char * membuffer, int size)
{
	systemSaveUpdateCounter = SYSTEM_SAVE_NOT_UPDATED;

	if(size == 512 || size == 0x2000)
	{
		memcpy(eepromData, membuffer, size);
	}
	else
	{
		if(size == 0x20000)
		{
			memcpy(flashSaveMemory, membuffer, 0x20000);
			flashSetSize(0x20000);
		}
		else
		{
			memcpy(flashSaveMemory, membuffer, 0x10000);
			flashSetSize(0x10000);
		}
	}
	return true;
}

extern int gbaSaveType;

int MemCPUWriteBatteryFile(char * membuffer)
{
	int result = 0;
	if(gbaSaveType == 0)
	{
		if(eepromInUse)
			gbaSaveType = 3;
		else
			switch(saveType)
			{
			case 1:
				gbaSaveType = 1;
				break;
			case 2:
				gbaSaveType = 2;
				break;
			}
	}

	if((gbaSaveType) && (gbaSaveType!=5))
	{
		// only save if Flash/Sram in use or EEprom in use
		if(gbaSaveType != 3)
		{
			if(gbaSaveType == 2)
			{
				memcpy(membuffer, flashSaveMemory, flashSize);
				result = flashSize;
			}
			else
			{
				memcpy(membuffer, flashSaveMemory, 0x10000);
				result = 0x10000;
			}
		}
		else
		{
			memcpy(membuffer, eepromData, eepromSize);
			result = eepromSize;
		}
	}
	return result;
}

/****************************************************************************
* LoadBatteryOrState
* Load Battery/State file into memory
* action = FILE_SRAM - Load battery
* action = FILE_SNAPSHOT - Load state
****************************************************************************/

bool LoadBatteryOrState(char * filepath, int action, bool silent)
{
	bool result = false;
	int offset = 0;
	int device;
		
	if(!FindDevice(filepath, &device))
		return 0;

	AllocSaveBuffer();

	// load the file into savebuffer
	offset = LoadFile(filepath, silent);

	// load savebuffer into VBA memory
	if (offset > 0)
	{
		if(action == FILE_SRAM)
		{
			if(cartridgeType == 1)
				result = MemgbReadBatteryFile((char *)savebuffer, offset);
			else
				result = MemCPUReadBatteryFile((char *)savebuffer, offset);
		}
		else
		{
			result = emulator.emuReadMemState((char *)savebuffer, offset);
		}
	}

	FreeSaveBuffer();

	if(!silent && !result)
	{
		if(offset == 0)
		{
			if(action == FILE_SRAM)
				ErrorPrompt ("Save file not found");
			else
				ErrorPrompt ("State file not found");
		}
		else
		{
			if(action == FILE_SRAM)
				ErrorPrompt ("Invalid save file");
			else
				ErrorPrompt ("Invalid state file");
		}
	}
	return result;
}

bool LoadBatteryOrStateAuto(int action, bool silent)
{
	char filepath[MAXPATHLEN];
	char filepath2[MAXPATHLEN];

	if(!MakeFilePath(filepath, action, ROMFilename, 0))
		return false;

	if (action==FILE_SRAM)
	{
		if (LoadBatteryOrState(filepath, action, SILENT))
			return true;

		// look for file with no number or Auto appended
		if(!MakeFilePath(filepath2, action, ROMFilename, -1))
			return false;

		if(LoadBatteryOrState(filepath2, action, silent))
		{
			// rename this file - append Auto
			rename(filepath2, filepath); // rename file (to avoid duplicates)
			return true;
		}
		return false;
	}
	else
	{
		return LoadBatteryOrState(filepath, action, silent);
	}
}

/****************************************************************************
* SaveBatteryOrState
* Save Battery/State file into memory
* action = 0 - Save battery
* action = 1 - Save state
****************************************************************************/

bool SaveBatteryOrState(char * filepath, int action, bool silent)
{
	bool result = false;
	int offset = 0;
	int datasize = 0; // we need the actual size of the data written
	int device;
	
	if(!FindDevice(filepath, &device))
		return 0;

	if(action == FILE_SNAPSHOT && gameScreenPngSize > 0)
	{
		char screenpath[1024];
		strncpy(screenpath, filepath, 1024);
		screenpath[strlen(screenpath)-4] = 0;
		sprintf(screenpath, "%s.png", screenpath);
		SaveFile((char *)gameScreenPng, screenpath, gameScreenPngSize, silent);
	}

	AllocSaveBuffer();

	// put VBA memory into savebuffer, sets datasize to size of memory written
	if(action == FILE_SRAM)
	{
		if(cartridgeType == 1)
			datasize = MemgbWriteBatteryFile((char *)savebuffer);
		else
			datasize = MemCPUWriteBatteryFile((char *)savebuffer);
	}
	else
	{
		if(emulator.emuWriteMemState((char *)savebuffer, SAVEBUFFERSIZE))
			datasize = *((int *)(savebuffer+4)) + 8;
	}

	// write savebuffer into file
	if(datasize > 0)
	{
		offset = SaveFile(filepath, datasize, silent);

		if(offset > 0)
		{
			if(!silent)
				InfoPrompt ("Save successful");
			result = true;
		}
	}
	else
	{
		if(!silent)
			InfoPrompt("No data to save!");
	}

	FreeSaveBuffer();

	return result;
}

bool SaveBatteryOrStateAuto(int action, bool silent)
{
	char filepath[1024];

	if(!MakeFilePath(filepath, action, ROMFilename, 0))
		return false;

	return SaveBatteryOrState(filepath, action, silent);
}

/****************************************************************************
* Sound
****************************************************************************/

SoundDriver * systemSoundInit()
{
	soundShutdown();
	return new SoundWii();
}

bool systemCanChangeSoundQuality()
{
	return true;
}

void systemOnWriteDataToSoundBuffer(const u16 * finalWave, int length)
{
}

void systemOnSoundShutdown()
{
}

/****************************************************************************
* systemReadJoypads
****************************************************************************/
bool systemReadJoypads()
{
	UpdatePads();
	return true;
}

u32 systemReadJoypad(int which)
{
	if(which == -1) which = 0; // default joypad
	return GetJoy(which);
}

/****************************************************************************
* Motion/Tilt sensor
* Used for games like:
* - Yoshi's Universal Gravitation
* - Kirby's Tilt-N-Tumble
* - Wario Ware Twisted!
*
****************************************************************************/
static int sensorX = 2047;
static int sensorY = 2047;
static int sensorWario = 0x6C0;
static u8 sensorDarkness = 0xE8; // total darkness (including daylight on rainy days)
bool CalibrateWario = false;

int systemGetSensorX()
{
	return sensorX;
}

int systemGetSensorY()
{
	return sensorY;
}

int systemGetSensorZ()
{
	if (CalibrateWario) return 0x6C0;
	else return sensorWario;
}

u8 systemGetSensorDarkness()
{
	return sensorDarkness;
}

void systemUpdateSolarSensor()
{
	u8 sun = 0x0; //sun = 0xE8 - 0xE8 (case 0 and default)

	switch (SunBars)
	{
		case 1:
			sun = 0xE8 -  0xE0;
			break;
		case 2:
			sun = 0xE8 -  0xDA;
			break;
		case 3:
			sun = 0xE8 -  0xD0;
			break;
		case 4:
			sun = 0xE8 -  0xC8;
			break;
		case 5:
			sun = 0xE8 -  0xC0;
			break;
		case 6:
			sun = 0xE8 -  0xB0;
			break;
		case 7:
			sun = 0xE8 -  0xA0;
			break;
		case 8:
			sun = 0xE8 -  0x88;
			break;
		case 9:
			sun = 0xE8 -  0x70;
			break;
		case 10:
			sun = 0xE8 -  0x50;
			break;
		default:
			break;
	}

	struct tm *newtime;
	time_t long_time;

	// regardless of the weather, there should be no sun at night time!
	time(&long_time); // Get time as long integer.
	newtime = localtime(&long_time); // Convert to local time.
	if (newtime->tm_hour > 21 || newtime->tm_hour < 5)
	{
		sun = 0; // total darkness, 9pm - 5am
	}
	else if (newtime->tm_hour > 20 || newtime->tm_hour < 6)
	{
		sun /= 9; // almost total darkness 8pm-9pm, 5am-6am
	}
	else if (newtime->tm_hour > 18 || newtime->tm_hour < 7)
	{
		sun >>= 1;
	}

#ifdef HW_RVL
	// pointing the Gun Del Sol at the ground blocks the sun light,
	// because sometimes you need the shade.
	WPADData *Data = WPAD_Data(0);// first wiimote
	WPADData data = *Data;
	float f = 1.0f;
	if (data.orient.pitch > 0)
	{
		f = 1.0f - (data.orient.pitch/85.0f);
		if (f < 0)
			f = 0;
	}
	sun = int(float(int(sun)) * f);
#endif
	sensorDarkness = 0xE8 - sun;
}

void systemUpdateMotionSensor()
{
#ifdef HW_RVL
	WPADData *Data = WPAD_Data(0); // first wiimote
	WPADData data = *Data;
	static float OldTiltAngle, OldAvg;
	static bool WasFlat = false;
	float DeltaAngle = 0;

	if (TiltSideways)
	{
		sensorY = 2047+(data.gforce.x*50);
		sensorX = 2047+(data.gforce.y*50);
		TiltAngle = ((-data.orient.pitch) + OldTiltAngle)*0.5f;
		OldTiltAngle = -data.orient.pitch;
	}
	else
	{
		sensorX = 2047-(data.gforce.x*50);
		sensorY = 2047+(data.gforce.y*50);
		TiltAngle = ((data.orient.roll) + OldTiltAngle)*0.5f;
		OldTiltAngle = data.orient.roll;
	}
	DeltaAngle = TiltAngle - OldAvg;
	if (DeltaAngle > 180.0f)
		DeltaAngle -= 360.0f;
	else if (DeltaAngle < -180.0f)
		DeltaAngle += 360.0f;
	OldAvg = TiltAngle;

	if(absf(TiltAngle) < 3.0f)
	{
		WasFlat = true;
		TiltAngle = 0;
	}
	else
	{
		if (WasFlat) TiltAngle *= 0.5f;
		WasFlat = false;
	}

	sensorWario = 0x6C0+DeltaAngle*11;

#endif

	systemUpdateSolarSensor();
}

/****************************************************************************
* systemDrawScreen
****************************************************************************/
static int srcWidth = 0;
static int srcHeight = 0;
static int srcPitch = 0;

void systemDrawScreen()
{
	GX_Render( srcWidth, srcHeight, pix, srcPitch );
}

static bool ValidGameId(u32 id)
{
	if (id == 0)
		return false;
	for (unsigned i = 1u; i <= 4u; ++i)
	{
		u8 b = id & 0xFF;
		id >>= 8;
		if (!(b >= 'A' && b <= 'Z') && !(b >= '0' && b <= '9'))
			return false;
	}
	return true;
}

bool IsGameboyGame()
{
	if(cartridgeType == 1 && !gbCgbMode && !gbSgbMode)
		return true;
	return false;
}

bool IsGBAGame()
{
	if(cartridgeType == 2)
		return true;
	return false;
}

static void gbApplyPerImagePreferences()
{
	// Only works for some GB Colour roms
	u8 Colour = gbRom[0x143];
	if (Colour == 0x80 || Colour == 0xC0)
	{
		RomIdCode = gbRom[0x13f] | (gbRom[0x140] << 8) | (gbRom[0x141] << 16)
				| (gbRom[0x142] << 24);
		if (!ValidGameId(RomIdCode))
			RomIdCode = 0;
	}
	else
		RomIdCode = 0;
	// Otherwise we need to make up our own code
	RomTitle[15] = '\0';
	RomTitle[16] = '\0';
	if (gbRom[0x143] < 0x7F && gbRom[0x143] > 0x20)
		strncpy(RomTitle, (const char *) &gbRom[0x134], 16);
	else
		strncpy(RomTitle, (const char *) &gbRom[0x134], 15);

	if (RomIdCode == 0)
	{
		if (strcmp(RomTitle, "ZELDA") == 0)
			RomIdCode = LINKSAWAKENING;
		else if (strcmp(RomTitle, "MORTAL KOMBAT") == 0)
			RomIdCode = MK1;
		else if (strcmp(RomTitle, "MORTALKOMBATI&II") == 0)
			RomIdCode = MK12;
		else if (strcmp(RomTitle, "MORTAL KOMBAT II") == 0)
			RomIdCode = MK2;
		else if (strcmp(RomTitle, "MORTAL KOMBAT 3") == 0)
			RomIdCode = MK3;
		else if (strcmp(RomTitle, "MORTAL KOMBAT 4") == 0)
			RomIdCode = MK4;
		else if (strcmp(RomTitle, "SUPER MARIOLAND") == 0)
			RomIdCode = MARIOLAND1;
		else if (strcmp(RomTitle, "MARIOLAND2") == 0)
			RomIdCode = MARIOLAND2;
		else if (strcmp(RomTitle, "METROID2") == 0)
			RomIdCode = METROID2;
		else if (strcmp(RomTitle, "MARBLE MADNESS") == 0)
			RomIdCode = MARBLEMADNESS;
		else if (strcmp(RomTitle, "TMNT FOOT CLAN") == 0)
			RomIdCode = TMNT1;
		else if (strcmp(RomTitle, "TMNT BACK FROM") == 0 || strcmp(RomTitle, "TMNT 2") == 0)
			RomIdCode = TMNT2;
		else if (strcmp(RomTitle, "TMNT3") == 0)
			RomIdCode = TMNT3;
		else if (strcmp(RomTitle, "CASTLEVANIA ADVE") == 0)
			RomIdCode = CVADVENTURE;
		else if (strcmp(RomTitle, "CASTLEVANIA2 BEL") == 0)
			RomIdCode = CVBELMONT;
		else if (strcmp(RomTitle, "CASTLEVANIA") == 0 || strcmp(RomTitle, "CV3 GER") == 0)
			RomIdCode = CVLEGENDS;
		else if (strcmp(RomTitle, "STAR WARS") == 0)
			RomIdCode = SWEP4;
		else if (strcmp(RomTitle, "EMPIRE STRIKES") == 0)
			RomIdCode = SWEP5;
		else if (strcmp(RomTitle, "SRJ DMG") == 0)
			RomIdCode = SWEP6;
	}
	// look for matching palettes if a monochrome gameboy game
	// (or if a Super Gameboy game, but the palette will be ignored later in that case)
	if ((Colour != 0x80) && (Colour != 0xC0))
	{
		if (GCSettings.colorize && strcmp(RomTitle, "MEGAMAN") != 0)
			SetPalette(RomTitle);
		else
			StopColorizing();
	}
}

/****************************************************************************
 * ApplyPerImagePreferences
 * Apply game specific settings, originally from vba-over.ini
 ***************************************************************************/

static void ApplyPerImagePreferences()
{
	// look for matching game setting
	int snum = -1;
	RomIdCode = rom[0xac] | (rom[0xad] << 8) | (rom[0xae] << 16) | (rom[0xaf] << 24);
	RomTitle[0] = '\0';

	for(int i=0; i < gameSettingsCount; ++i)
	{
		if(gameSettings[i].gameID[0] == rom[0xac] &&
			gameSettings[i].gameID[1] == rom[0xad] &&
			gameSettings[i].gameID[2] == rom[0xae] &&
			gameSettings[i].gameID[3] == rom[0xaf])
		{
			snum = i;
			break;
		}
	}

	// match found!
	if(snum >= 0)
	{
		if(gameSettings[snum].rtcEnabled >= 0)
			rtcEnable(gameSettings[snum].rtcEnabled);
		if(gameSettings[snum].flashSize > 0)
			flashSetSize(gameSettings[snum].flashSize);
		if(gameSettings[snum].saveType >= 0)
			cpuSaveType = gameSettings[snum].saveType;
		if(gameSettings[snum].mirroringEnabled >= 0)
			mirroringEnable = gameSettings[snum].mirroringEnabled;
	}
	// In most cases this is already handled in GameSettings, but just to make sure:
	switch (rom[0xac])
	{
		case 'F': // Classic NES
			cpuSaveType = 1; // EEPROM
			mirroringEnable = 1;
			break;
		case 'K': // Accelerometers
			cpuSaveType = 4; // EEPROM + sensor
			break;
		case 'R': // WarioWare Twisted style sensors
		case 'V': // Drill Dozer
			rtcEnableWarioRumble(true);
			break;
		case 'U': // Boktai solar sensor and clock
			rtcEnable(true);
			break;
	}
}

void LoadPatch()
{
	int patchsize = 0;
	int patchtype = 0;

	AllocSaveBuffer ();

	char patchpath[2][512];
	memset(patchpath, 0, sizeof(patchpath));
	sprintf(patchpath[0], "%s%s.ips",browser.dir,ROMFilename);
	sprintf(patchpath[1], "%s%s.ups",browser.dir,ROMFilename);

	for(; patchtype<2; patchtype++)
	{
		patchsize = LoadFile(patchpath[patchtype], SILENT);

		if(patchsize)
			break;
	}

	if(patchsize > 0)
	{
		ShowAction("Loading patch...");
		// create memory file
		MFILE * mf = memfopen((char *)savebuffer, patchsize);

		if(cartridgeType == 1)
		{
			if(patchtype == 0)
				patchApplyIPS(mf, &gbRom, &gbRomSize);
			else
				patchApplyUPS(mf, &gbRom, &gbRomSize);
		}
		else
		{
			if(patchtype == 0)
				patchApplyIPS(mf, &rom, &GBAROMSize);
			else
				patchApplyUPS(mf, &rom, &GBAROMSize);
		}

		memfclose(mf); // close memory file
	}

	FreeSaveBuffer ();
}

extern bool gbUpdateSizes();

bool LoadGBROM()
{
	gbRom = (u8 *)malloc(1024*1024*4); // allocate 4 MB to GB ROM
	bios = (u8 *)calloc(1,0x100);

	systemSaveUpdateCounter = SYSTEM_SAVE_NOT_UPDATED;

	if(!inSz)
	{
		char filepath[1024];

		if(!MakeFilePath(filepath, FILE_ROM))
			return false;

		gbRomSize = LoadFile ((char *)gbRom, filepath, browserList[browser.selIndex].length, NOTSILENT);
	}
	else
	{
		gbRomSize = LoadSzFile(szpath, (unsigned char *)gbRom);
	}

	if(gbRomSize <= 0)
		return false;

	return gbUpdateSizes();
}

bool LoadVBAROM()
{
	cartridgeType = 0;
	bool loaded = false;

	// image type (checks file extension)
	if(utilIsGBAImage(browserList[browser.selIndex].filename))
		cartridgeType = 2;
	else if(utilIsGBImage(browserList[browser.selIndex].filename))
		cartridgeType = 1;
	else if(utilIsZipFile(browserList[browser.selIndex].filename))
	{
		// we need to check the file extension of the first file in the archive
		char * zippedFilename = GetFirstZipFilename ();

		if(zippedFilename == NULL) // loading the file failed
		{
			ErrorPrompt("Empty or invalid ZIP file!");
			return false;
		}

		if(utilIsGBAImage(zippedFilename))
		{
			cartridgeType = 2;
		}
		else if(utilIsGBImage(zippedFilename))
		{
			cartridgeType = 1;
		}
		else
		{
			free(zippedFilename);
			return false;
		}
		free(zippedFilename);
	}

	// leave before we do anything
	if(cartridgeType != 1 && cartridgeType != 2)
	{
		// Not zip gba agb gbc cgb sgb gb mb bin elf or dmg!
		ErrorPrompt("Unrecognized file extension!");
		return false;
	}
	
	if(!GetFileSize(browser.selIndex))
	{
		ErrorPrompt("Error loading game!");
		return false;
	}

	srcWidth = 0;
	srcHeight = 0;
	srcPitch = 0;

	VMClose(); // cleanup GBA memory
	gbCleanUp(); // cleanup GB memory

	switch(cartridgeType)
	{
		case 2:
			emulator = GBASystem;
			srcWidth = 240;
			srcHeight = 160;
			loaded = VMCPULoadROM();
			srcPitch = 484;
			soundSetSampleRate(22050); //44100 / 2
			cpuSaveType = 0;
			break;

		case 1:
			emulator = GBSystem;

			gbBorderOn = 0; // GB borders always off

			if(gbBorderOn)
			{
				srcWidth = 256;
				srcHeight = 224;
				gbBorderLineSkip = 256;
				gbBorderColumnSkip = 48;
				gbBorderRowSkip = 40;
			}
			else
			{
				srcWidth = 160;
				srcHeight = 144;
				gbBorderLineSkip = 160;
				gbBorderColumnSkip = 0;
				gbBorderRowSkip = 0;
			}

			loaded = LoadGBROM();
			srcPitch = 324;
			soundSetSampleRate(44100);
			break;
	}

	if(!loaded)
	{
		ErrorPrompt("Error loading game!");
		return false;
	}
	else
	{
		// Setup GX
		GX_Render_Init(srcWidth, srcHeight);

		if (cartridgeType == 1)
		{
			gbGetHardwareType();

			// used for the handling of the gb Boot Rom
			//if (gbHardware & 5)
			//gbCPUInit(gbBiosFileName, useBios);

			LoadPatch();

			// Apply preferences specific to this game
			gbApplyPerImagePreferences();

			gbSoundReset();
			gbSoundSetDeclicking(true);
			gbReset();
		}
		else
		{
			// Set defaults
			cpuSaveType = 0; // automatic
			flashSetSize(0x10000); // 64K saves
			rtcEnable(false);
			agbPrintEnable(false);
			mirroringEnable = false;

			// Apply preferences specific to this game
			ApplyPerImagePreferences();
			doMirroring(mirroringEnable);

			soundReset();
			CPUInit(NULL, false);
			LoadPatch();
			CPUReset();
		}

		SetAudioRate(cartridgeType);
		soundInit();

		emulating = 1;

		// reset frameskip variables
		lastTime = systemFrameSkip = 0;

		// Start system clock
		start = gettime();

		return true;
	}
}

/****************************************************************************
* Palette
****************************************************************************/

void InitialisePalette()
{
	int i;
	// Build GBPalette
	for( i = 0; i < 24; )
	{
		systemGbPalette[i++] = (0x1f) | (0x1f << 5) | (0x1f << 10);
		systemGbPalette[i++] = (0x15) | (0x15 << 5) | (0x15 << 10);
		systemGbPalette[i++] = (0x0c) | (0x0c << 5) | (0x0c << 10);
		systemGbPalette[i++] = 0;
	}
	// Set palette etc - Fixed to RGB565
	systemColorDepth = 16;
	systemRedShift = 11;
	systemGreenShift = 6;
	systemBlueShift = 0;
	for(i = 0; i < 0x10000; i++)
	{
		systemColorMap16[i] =
			((i & 0x1f) << systemRedShift) |
			(((i & 0x3e0) >> 5) << systemGreenShift) |
			(((i & 0x7c00) >> 10) << systemBlueShift);
	}
}
