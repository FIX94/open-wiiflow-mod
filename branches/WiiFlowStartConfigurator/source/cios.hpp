
#include <gccore.h>
#include "types.h"

typedef struct _iosinfo_t {
	u32 magicword;					//0x1ee7c105
	u32 magicversion;				// 1
	u32 version;					// Example: 5
	u32 baseios;					// Example: 56
	char name[0x10];				// Example: d2x
	char versionstring[0x10];		// Example: beta2
} iosinfo_t;

void loadIOSlist();
void nextIOS(u8 *current);
void prevIOS(u8 *current);