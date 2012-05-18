/// \file
/// \brief Contains methods related to the build configuration

#include <string.h>
#include <stdio.h>
#include <stdlib.h>

#include "types.h"
#include "version.h"
#include "fceu.h"
#include "driver.h"
#include "utils/memory.h"

static char *aboutString = 0;

///returns a string suitable for use in an aboutbox
char *FCEUI_GetAboutString() {
	const char *aboutTemplate = 
	FCEU_NAME_AND_VERSION "\n\n\
Administrators:\n\
zeromus, adelikat\n\n\
Current Contributors:\n\
punkrockguy318 (Lukas Sabota)\n\
Plombo (Bryan Cain)\n\
qeed, QFox, Shinydoofy, ugetab\n\
CaH4e3, gocha, Acmlm, DWEdit, AnS\n\
\n\
FCEUX 2.0:\n\
mz, nitsujrehtona, Lukas Sabota,\n\
SP, Ugly Joe\n\
\n\
Previous versions:\n\
FCE - Bero\n\
FCEU - Xodnizel\n\
FCEU XD - Bbitmaster & Parasyte\n\
FCEU XD SP - Sebastian Porst\n\
FCEU MM - CaH4e3\n\
FCEU TAS - blip & nitsuja\n\
FCEU TAS+ - Luke Gustafson\n\
\n\
FCEUX is dedicated to the fallen heroes\n\
of NES emulation. In Memoriam --\n\
ugetab\n\
\n\
"__TIME__" "__DATE__"\n";

	if(aboutString) return aboutString;

	const char *compilerString = FCEUD_GetCompilerString();

	//allocate the string and concatenate the template with the compiler string
	if (!(aboutString = (char*)FCEU_dmalloc(strlen(aboutTemplate) + strlen(compilerString) + 1)))
        return NULL;
	
    sprintf(aboutString,"%s%s",aboutTemplate,compilerString);
	return aboutString;
}
