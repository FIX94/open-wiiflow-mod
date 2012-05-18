#include "../System.h"
#include "GBA.h"
#include "Globals.h"
#include "../common/Port.h"
#include "../Util.h"
#include "../NLS.h"
#include "vmmem.h"

#include <time.h>
#include <string.h>
#include <stdio.h>

u8 systemGetSensorDarkness();
int systemGetSensorZ();

enum RTCSTATE { IDLE, COMMAND, DATA, READDATA };

typedef struct {
  u8 byte0;
  u8 byte1;
  u8 byte2;
  u8 command;
  int dataLen;
  int bits;
  RTCSTATE state;
  u8 data[12];
  // reserved variables for future
  u8 reserved[12];
  bool reserved2;
  u32 reserved3;
} RTCCLOCKDATA;

static RTCCLOCKDATA rtcClockData;
static bool rtcEnabled = false;
static bool rtcWarioRumbleEnabled = false;

void rtcEnable(bool e)
{
  rtcEnabled = e;
}

bool rtcIsEnabled()
{
  return rtcEnabled;
}

void rtcEnableWarioRumble(bool e)
{
  if (e) rtcEnable(true);
  rtcWarioRumbleEnabled = e;
}

u16 rtcRead(u32 address)
{
  if(rtcEnabled) {
    switch(address){
    case 0x80000c8:
      return rtcClockData.byte2;
      break;
    case 0x80000c6:
      return rtcClockData.byte1;
      break;
    case 0x80000c4:
	  
	  // Boktai Solar Sensor
	  if (rtcClockData.byte1 == 7) {
		if (rtcClockData.reserved[11] >= systemGetSensorDarkness()) {
			rtcClockData.reserved[10] = 0;
			rtcClockData.reserved[11] = 0;
			return 8;
		} else return 0;

	  // WarioWare Twisted Tilt Sensor
	  } else if (rtcClockData.byte1 == 0x0b) {
		//sprintf(DebugStr, "Reading Twisted Sensor bit %d", rtcClockData.reserved[11]);
		u16 v = systemGetSensorZ();
		return ((v >> rtcClockData.reserved[11]) & 1) << 2;
		
	  // Real Time Clock
	  } else {
		//sprintf(DebugStr, "Reading RTC %02x, %02x, %02x", rtcClockData.byte0, rtcClockData.byte1, rtcClockData.byte2);
	    return rtcClockData.byte0;
	  }
      break;
    }
  }
#ifdef USE_VM
  return VMRead16( address & 0x1FFFFFE );
#else
  return READ16LE((&rom[address & 0x1FFFFFE]));
#endif
}

static u8 toBCD(u8 value)
{
  value = value % 100;
  int l = value % 10;
  int h = value / 10;
  return h * 16 + l;
}

bool rtcWrite(u32 address, u16 value)
{
  if(!rtcEnabled)
    return false;

  if(address == 0x80000c8) {
    rtcClockData.byte2 = (u8)value; // bit 0 = enable reading from 0x80000c4 c6 and c8
  } else if(address == 0x80000c6) {
    rtcClockData.byte1 = (u8)value; // 0=read/1=write (for each of 4 low bits)
	// rumble is off when not writing to that pin
	if (rtcWarioRumbleEnabled && !(value & 8)) systemCartridgeRumble(false);
  } else if(address == 0x80000c4) { // 4 bits of I/O Port Data (upper bits not used)
	
	// WarioWare Twisted  rumble
	if(rtcWarioRumbleEnabled && (rtcClockData.byte1 & 8)) {
		systemCartridgeRumble(value & 8);
	}

	// Boktai solar sensor
	if (rtcClockData.byte1 == 7) {
		if (value & 2) {
			// reset counter to 0
			rtcClockData.reserved[11]=0;
			rtcClockData.reserved[10]=0;
		}
		if ((value & 1) && (!(rtcClockData.reserved[10] & 1))) {
			// increase counter, ready to do another read
			if (rtcClockData.reserved[11]<255) rtcClockData.reserved[11]++;
		}
		rtcClockData.reserved[10] = value & rtcClockData.byte1;
	}
	
	// WarioWare Twisted rotation sensor
	if (rtcClockData.byte1 == 0xb) {
		if (value & 2) {
			// clock goes high in preperation for reading a bit
			rtcClockData.reserved[11]--;
		}
		if (value & 1) {
			// start ADC conversion
			rtcClockData.reserved[11] = 15;
		}
			
		rtcClockData.byte0 = value & rtcClockData.byte1;
		
	// Real Time Clock
	} 
	/**/

	if(rtcClockData.byte2 & 1) { // if reading is enabled
	  if(rtcClockData.state == IDLE && rtcClockData.byte0 == 1 && value == 5) {
          rtcClockData.state = COMMAND;
          rtcClockData.bits = 0;
          rtcClockData.command = 0;
      } else if(!(rtcClockData.byte0 & 1) && (value & 1)) { // bit transfer
        rtcClockData.byte0 = (u8)value;
        switch(rtcClockData.state) {
        case COMMAND:
          rtcClockData.command |= ((value & 2) >> 1) << (7-rtcClockData.bits);
          rtcClockData.bits++;
          if(rtcClockData.bits == 8) {
            rtcClockData.bits = 0;
            switch(rtcClockData.command) {
            case 0x60:
              // not sure what this command does but it doesn't take parameters
              // maybe it is a reset or stop
              rtcClockData.state = IDLE;
              rtcClockData.bits = 0;
              break;
            case 0x62:
              // this sets the control state but not sure what those values are
              rtcClockData.state = READDATA;
              rtcClockData.dataLen = 1;
              break;
            case 0x63:
              rtcClockData.dataLen = 1;
              rtcClockData.data[0] = 0x40;
              rtcClockData.state = DATA;
              break;
            case 0x64:
              break;
            case 0x65:
              {
                struct tm *newtime;
                time_t long_time;

                time( &long_time );                /* Get time as long integer. */
                newtime = localtime( &long_time ); /* Convert to local time. */

                rtcClockData.dataLen = 7;
                rtcClockData.data[0] = toBCD(newtime->tm_year);
                rtcClockData.data[1] = toBCD(newtime->tm_mon+1);
                rtcClockData.data[2] = toBCD(newtime->tm_mday);
                rtcClockData.data[3] = toBCD(newtime->tm_wday);
                rtcClockData.data[4] = toBCD(newtime->tm_hour);
                rtcClockData.data[5] = toBCD(newtime->tm_min);
                rtcClockData.data[6] = toBCD(newtime->tm_sec);
                rtcClockData.state = DATA;
              }
              break;
            case 0x67:
              {
                struct tm *newtime;
                time_t long_time;

                time( &long_time );                /* Get time as long integer. */
                newtime = localtime( &long_time ); /* Convert to local time. */

                rtcClockData.dataLen = 3;
                rtcClockData.data[0] = toBCD(newtime->tm_hour);
                rtcClockData.data[1] = toBCD(newtime->tm_min);
                rtcClockData.data[2] = toBCD(newtime->tm_sec);
                rtcClockData.state = DATA;
              }
              break;
            default:
              systemMessage(0, N_("Unknown RTC command %02x"), rtcClockData.command);
              rtcClockData.state = IDLE;
              break;
            }
          }
          break;
        case DATA:
          if(rtcClockData.byte1 & 2) {
          } else {
            rtcClockData.byte0 = (rtcClockData.byte0 & ~2) |
              ((rtcClockData.data[rtcClockData.bits >> 3] >>
                (rtcClockData.bits & 7)) & 1)*2;
            rtcClockData.bits++;
            if(rtcClockData.bits == 8*rtcClockData.dataLen) {
              rtcClockData.bits = 0;
              rtcClockData.state = IDLE;
            }
          }
          break;
        case READDATA:
          if(!(rtcClockData.byte1 & 2)) {
          } else {
            rtcClockData.data[rtcClockData.bits >> 3] =
              (rtcClockData.data[rtcClockData.bits >> 3] >> 1) |
              ((value << 6) & 128);
            rtcClockData.bits++;
            if(rtcClockData.bits == 8*rtcClockData.dataLen) {
              rtcClockData.bits = 0;
              rtcClockData.state = IDLE;
            }
          }
          break;
        default:
          break;
        }
      } else
        rtcClockData.byte0 = (u8)value;
    }
  }
  return true;
}

void rtcReset()
{
  memset(&rtcClockData, 0, sizeof(rtcClockData));

  rtcClockData.byte0 = 0;
  rtcClockData.byte1 = 0;
  rtcClockData.byte2 = 0;
  rtcClockData.command = 0;
  rtcClockData.dataLen = 0;
  rtcClockData.bits = 0;
  rtcClockData.state = IDLE;
  rtcClockData.reserved[11] = 0;
}

void rtcSaveGame(gzFile gzFile)
{
  utilGzWrite(gzFile, &rtcClockData, sizeof(rtcClockData));
}

void rtcReadGame(gzFile gzFile)
{
  utilGzRead(gzFile, &rtcClockData, sizeof(rtcClockData));
}
