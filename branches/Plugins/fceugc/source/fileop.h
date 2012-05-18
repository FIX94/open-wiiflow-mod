/****************************************************************************
 * FCE Ultra
 * Nintendo Wii/Gamecube Port
 *
 * Tantric 2008-2009
 *
 * fileop.h
 *
 * File operations
 ****************************************************************************/

#ifndef _FILEOP_H_
#define _FILEOP_H_

#include <stdio.h>
#include <string.h>
#include <unistd.h>

#define SAVEBUFFERSIZE (1024 * 512)

void InitDeviceThread();
void ResumeDeviceThread();
void HaltDeviceThread();
void HaltParseThread();
void MountAllFAT();
void UnmountAllFAT();
bool FindDevice(char * filepath, int * device);
char * StripDevice(char * path);
bool ChangeInterface(int device, bool silent);
bool ChangeInterface(char * filepath, bool silent);
void CreateAppPath(char * origpath);
bool GetFileSize(int i);
int ParseDirectory(bool waitParse = false, bool filter = true);
void AllocSaveBuffer();
void FreeSaveBuffer();
size_t LoadFile(char * rbuffer, char *filepath, size_t length, bool silent);
size_t LoadFile(char * filepath, bool silent);
size_t LoadSzFile(char * filepath, unsigned char * rbuffer);
size_t SaveFile(char * buffer, char *filepath, size_t datasize, bool silent);
size_t SaveFile(char * filepath, size_t datasize, bool silent);

extern unsigned char savebuffer[];
extern FILE * file;
extern bool unmountRequired[];
extern bool isMounted[];
extern int selectLoadedFile;

#endif
