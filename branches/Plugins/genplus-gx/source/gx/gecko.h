
#ifndef _GECKO_H_
#define _GECKO_H_

#ifdef __cplusplus
extern "C" {
#endif

#include <gccore.h>

//use this just like printf();
void gprintf(const char *format, ...);
bool InitGecko();

#ifdef __cplusplus
}
#endif

#endif
