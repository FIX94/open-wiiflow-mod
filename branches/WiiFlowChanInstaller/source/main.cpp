
#include <ogc/system.h>
#include <unistd.h>
#include <errno.h>
#include <network.h>
#include "const_str.hpp"
#include "channel/nand.hpp"
#include "gecko/gecko.hpp"
#include "gui/video.hpp"
#include "gui/text.hpp"
#include "loader/wdvd.h"
#include "loader/sys.h"
#include "loader/cios.h"
#include "loader/nk.h"
#include "menu/menu.hpp"
#include "memory/memory.h"
#include "memory/mem2.hpp"

CMenu mainMenu;
bool iosOK = false;

int main()
{
	__exception_setreload(10);
	Gecko_Init(); //USB Gecko and SD/WiFi buffer
	gprintf(" \nWelcome to the %s!\nThis is the debug output.\n", VERSION_STRING.c_str());

	m_vid.init(); // Init video
	MEM_init(); //Inits both mem1lo and mem2
	m_vid.waitMessage(0.15f);

	NandHandle.Init();
	if(AHBRPOT_Patched())
		iosOK = NandHandle.LoadDefaultIOS();
	NandHandle.Init_ISFS(iosOK);

	Sys_Init();
	Open_Inputs();
	mainMenu.init();
	if(iosOK)
		mainMenu.main();
	mainMenu.cleanup();
	NandHandle.DeInit_ISFS();
	WDVD_Close();
	Sys_Exit();
	return 0;
}
