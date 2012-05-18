
#ifndef _BOOTHOMEBREW_H_
#define _BOOTHOMEBREW_H_

#ifdef __cplusplus
extern "C"
{
#endif

	extern bool bootHB;
	int BootHomebrew();
	int CopyHomebrewMemory(u8 *temp, u32 pos, u32 len);
	void AddBootArgument(const char * arg);
	void FreeHomebrewBuffer();
	int LoadHomebrew(const char * filepath);

#ifdef __cplusplus
}
#endif

#endif
