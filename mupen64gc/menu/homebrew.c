#include <gccore.h>
#include <ogcsys.h>
#include <stdio.h>
#include <string.h>
#include <ogc/machine/processor.h>

#define EXECUTE_ADDR	((u8 *) 0x92000000)

extern void __exception_closeall();
typedef void (*entrypoint) (void);

void *homebrewbuffer = (void *)EXECUTE_ADDR;

static u8 position = 0;
static char *Arguments[2];

// this code was contributed by shagkur
typedef struct _dolheader {
	u32 text_pos[7];
	u32 data_pos[11];
	u32 text_start[7];
	u32 data_start[11];
	u32 text_size[7];
	u32 data_size[11];
	u32 bss_start;
	u32 bss_size;
	u32 entry_point;
} dolheader;

u32 load_dol_image(void *dolstart, struct __argv *argv)
{
	u32 i;
	dolheader *dolfile;

	if (dolstart)
	{
		dolfile = (dolheader *) dolstart;
		for (i = 0; i < 7; i++)
		{
			if ((!dolfile->text_size[i]) || (dolfile->text_start[i] < 0x100))
				continue;

			memmove((void *) dolfile->text_start[i], dolstart
				+ dolfile->text_pos[i], dolfile->text_size[i]);

			DCFlushRange ((void *) dolfile->text_start[i], dolfile->text_size[i]);
			ICInvalidateRange((void *) dolfile->text_start[i], dolfile->text_size[i]);
		}

		for (i = 0; i < 11; i++)
		{
			if ((!dolfile->data_size[i]) || (dolfile->data_start[i] < 0x100))
				continue;

			memmove((void *) dolfile->data_start[i], dolstart
					+ dolfile->data_pos[i], dolfile->data_size[i]);

			DCFlushRange((void *) dolfile->data_start[i],
					dolfile->data_size[i]);
		}

		if (argv && argv->argvMagic == ARGV_MAGIC)
		{
			void *new_argv = (void *) (dolfile->entry_point + 8);
			memmove(new_argv, argv, sizeof(*argv));
			DCFlushRange(new_argv, sizeof(*argv));
		}
		return dolfile->entry_point;
	}
	return 0;
}

bool IsDollZ (u8 *buff)
{
  u8 dollz_stamp[] = {0x3C};
  int dollz_offs = 0x100;

  int ret = memcmp (&buff[dollz_offs], dollz_stamp, sizeof(dollz_stamp));
  if (ret == 0) return true;
  
  return false;
}

int LoadHomebrew(const char * filepath)
{
	if(!filepath) 
		return -1;

	FILE *file = fopen(filepath, "rb");
	if(!file) 
		return -2;

	fseek(file, 0, SEEK_END);
	u32 filesize = ftell(file);
	fseek (file, 0, SEEK_SET);

	fread(homebrewbuffer, 1, filesize, file);
	fclose(file);

	return 1;
}

int BootHomebrew()
{
	struct __argv args;
	u32 exeEntryPointAddress = load_dol_image(homebrewbuffer, &args);
	entrypoint exeEntryPoint = (entrypoint) exeEntryPointAddress;

	VIDEO_WaitVSync();

	u32 level;
	SYS_ResetSystem(SYS_SHUTDOWN, 0, 0);
	_CPU_ISR_Disable(level);
	__exception_closeall();
	exeEntryPoint();
	_CPU_ISR_Restore(level);
	return 0;
}
