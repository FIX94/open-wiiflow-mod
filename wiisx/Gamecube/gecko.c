
#include <gccore.h>
#include <malloc.h>
#include <stdio.h>
#include <string.h>
#include <sys/iosupport.h>
#include <stdarg.h>
#include "gecko.h"

/* init-globals */
bool geckoinit = false;

static ssize_t __out_write(struct _reent *r __attribute__((unused)), int fd __attribute__((unused)), const char *ptr, size_t len)
{
	if(geckoinit && ptr)
	{
		u32 level;
		level = IRQ_Disable();
		usb_sendbuffer_safe(1, ptr, len);
		IRQ_Restore(level);
	}
	return len;
}

static const devoptab_t gecko_out = {
	"stdout",		// device name
	0,				// size of file structure
	NULL,			// device open
	NULL,			// device close
	__out_write,	// device write
	NULL,			// device read
	NULL,			// device seek
	NULL,			// device fstat
	NULL,			// device stat
	NULL,			// device link
	NULL,			// device unlink
	NULL,			// device chdir
	NULL,			// device rename
	NULL,			// device mkdir
	0,				// dirStateSize
	NULL,			// device diropen_r
	NULL,			// device dirreset_r
	NULL,			// device dirnext_r
	NULL,			// device dirclose_r
	NULL,			// device statvfs_r
	NULL,			// device ftruncate_r
	NULL,			// device fsync_r
	NULL,			// device deviceData
	NULL,			// device chmod_r
	NULL,			// device fchmod_r
};

static void USBGeckoOutput()
{
	devoptab_list[STD_OUT] = &gecko_out;
	devoptab_list[STD_ERR] = &gecko_out;
}

#define GPRINTF_SIZE	256
static char gprintfBuffer[GPRINTF_SIZE];
void gprintf(const char *format, ...)
{
	va_list va;
	va_start(va, format);
	size_t len = vsnprintf(gprintfBuffer, GPRINTF_SIZE - 1, format, va);
	va_end(va);

	gprintfBuffer[GPRINTF_SIZE - 1] = '\0';
	__out_write(NULL, 0, gprintfBuffer, len);
}

char ascii(char s)
{
	if(s < 0x20)
		return '.';
	if(s > 0x7E)
		return '.';
	return s;
}

static const char *initstr = "USB Gecko inited.\n";
bool InitGecko()
{
	if(geckoinit)
		return geckoinit;

	USBGeckoOutput();
	memset(gprintfBuffer, 0, GPRINTF_SIZE);

	u32 geckoattached = usb_isgeckoalive(EXI_CHANNEL_1);
	if(geckoattached)
	{
		geckoinit = true;
		usb_flush(EXI_CHANNEL_1);
		__out_write(NULL, 0, initstr, strlen(initstr));
	}
	return geckoinit;
}
