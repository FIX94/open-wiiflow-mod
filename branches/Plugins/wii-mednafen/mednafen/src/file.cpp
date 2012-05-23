/* Mednafen - Multi-system Emulator
*
* This program is free software; you can redistribute it and/or modify
* it under the terms of the GNU General Public License as published by
* the Free Software Foundation; either version 2 of the License, or
* (at your option) any later version.
*
* This program is distributed in the hope that it will be useful,
* but WITHOUT ANY WARRANTY; without even the implied warranty of
* MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
* GNU General Public License for more details.
*
* You should have received a copy of the GNU General Public License
* along with this program; if not, write to the Free Software
* Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
*/

#include "mednafen.h"
#include <stdarg.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <errno.h>
#include <trio/trio.h>

#ifdef HAVE_MMAP
#include <sys/mman.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#endif


#include <zlib.h>
#include "compress/unzip.h"

#include "file.h"
#include "general.h"

#ifdef MEM2
#include "mem2.h"
#endif

#ifndef __GNUC__
#define strcasecmp strcmp
#endif

static const int64 MaxROMImageSize = (int64)1 << 26; // 2 ^ 26 = 64MiB

enum
{
  MDFN_FILETYPE_PLAIN = 0,
  MDFN_FILETYPE_GZIP = 1,
  MDFN_FILETYPE_ZIP = 2,
};

static const char *unzErrorString(int error_code)
{
  if(error_code == UNZ_OK)
    return("ZIP OK");
  else if(error_code == UNZ_END_OF_LIST_OF_FILE)
    return("ZIP End of file list");
  else if(error_code == UNZ_EOF)
    return("ZIP EOF");
  else if(error_code == UNZ_PARAMERROR)
    return("ZIP Parameter error");
  else if(error_code == UNZ_BADZIPFILE)
    return("ZIP file bad");
  else if(error_code == UNZ_INTERNALERROR)
    return("ZIP Internal error");
  else if(error_code == UNZ_CRCERROR)
    return("ZIP CRC error");
  else if(error_code == UNZ_ERRNO)
    return(strerror(errno));
  else
    return("ZIP Unknown");
}

bool MDFNFILE::ApplyIPS(FILE *ips)
{
  uint8 header[5];
  uint32 count = 0;

  //MDFN_printf(_("Applying IPS file \"%s\"...\n"), path);

  MDFN_indent(1);
  if(::fread(header, 1, 5, ips) != 5)
  {
    ErrnoHolder ene(errno);

    MDFN_PrintError(_("Error reading IPS file header: %s"), ene.StrError());
    MDFN_indent(-1);
    return(0);
  }

  if(memcmp(header, "PATCH", 5))
  {
    MDFN_PrintError(_("IPS file header is invalid."));
    MDFN_indent(-1);
    return(0);
  }

#ifdef HAVE_MMAP
  // If the file is mmap()'d, move it to malloc()'d RAM
  if(is_mmap)
  {
    void *tmp_ptr = MDFN_malloc(f_size, _("file read buffer"));
    if(!tmp_ptr)
    {
      //Close();
      //fclose(ipsfile);
      return(0);
    }
    memcpy(tmp_ptr, f_data, f_size);

    munmap(f_data, f_size);

    is_mmap = FALSE;
    f_data = (uint8 *)tmp_ptr;
  }
#endif

  while(::fread(header, 1, 3, ips) == 3)
  {
    uint32 offset = (header[0] << 16) | (header[1] << 8) | header[2];
    uint8 patch_size_raw[2];
    uint32 patch_size;
    bool rle = false;

    if(!memcmp(header, "EOF", 3))
    {
      MDFN_printf(_("IPS EOF:  Did %d patches\n\n"), count);
      MDFN_indent(-1);
      return(1);
    }

    if(::fread(patch_size_raw, 1, 2, ips) != 2)
    {
      ErrnoHolder ene(errno);
      MDFN_PrintError(_("Error reading IPS patch length: %s"), ene.StrError());
      return(0);
    }

    patch_size = MDFN_de16msb(patch_size_raw);

    if(!patch_size)	/* RLE */
    {
      if(::fread(patch_size_raw, 1, 2, ips) != 2)
      {
        ErrnoHolder ene(errno);
        MDFN_PrintError(_("Error reading IPS RLE patch length: %s"), ene.StrError());
        return(0);
      }

      patch_size = MDFN_de16msb(patch_size_raw);

      // Is this right?
      if(!patch_size)
        patch_size = 65536;

      rle = true;
      //MDFN_printf("  Offset: %8d  Size: %5d RLE\n",offset, patch_size);
    }

    if((offset + patch_size) > f_size)
    {
      uint8 *tmp;

      //printf("%d\n", offset + patch_size, f_size);

      if((offset + patch_size) > MaxROMImageSize)
      {
        MDFN_PrintError(_("ROM image will be too large after IPS patch; maximum size allowed is %llu bytes."), (unsigned long long)MaxROMImageSize);
        return(0);
      }

#ifdef MEM2
      if( isgame )
      {
        if(!(tmp = (uint8 *)Mem2ManagerAdjust(f_data, offset + patch_size, _("file read buffer"))))
          return(0);
      }
      else
      {
#endif
      if(!(tmp = (uint8 *)MDFN_realloc(f_data, offset + patch_size, _("file read buffer"))))
        return(0);
#ifdef MEM2
      }
#endif
      // Zero newly-allocated memory
      memset(tmp + f_size, 0, (offset + patch_size) - f_size);

      f_size = offset + patch_size;
      f_data = tmp;
    }


    if(rle)
    {
      const int b = ::fgetc(ips);
      uint8 *start = f_data + offset;

      if(EOF == b)
      {
        ErrnoHolder ene(errno);

        MDFN_PrintError(_("Error reading IPS RLE patch byte: %s"), ene.StrError());

        return(0);
      }

      while(patch_size--)
      {
        *start=b;
        start++;
      }

    }
    else		/* Normal patch */
    {
      //MDFN_printf("  Offset: %8d  Size: %5d\n", offset, patch_size);
      if(::fread(f_data + offset, 1, patch_size, ips) != patch_size)
      {
        ErrnoHolder ene(errno);

        MDFN_PrintError(_("Error reading IPS patch: %s"), ene.StrError());
        return(0);
      }
    }
    count++;
  }
  ErrnoHolder ene(errno);

  //MDFN_printf(_("Warning:  IPS ended without an EOF chunk.\n"));
  //MDFN_printf(_("IPS EOF:  Did %d patches\n\n"), count);
  MDFN_indent(-1);

  MDFN_PrintError(_("Error reading IPS patch header: %s"), ene.StrError());
  return(0);

  //return(1);
}

// This function should ALWAYS close the system file "descriptor"(gzip library, zip library, or FILE *) it's given,
// even if it errors out.
bool MDFNFILE::MakeMemWrap(void *tz, int type)
{
  bool ret = FALSE;

#ifdef HAVE_MMAP
  is_mmap = FALSE;
#endif
  location = 0;

  if(type == MDFN_FILETYPE_PLAIN)
  {
    ::fseek((FILE *)tz, 0, SEEK_END);
    f_size = ::ftell((FILE *)tz);
    ::fseek((FILE *)tz, 0, SEEK_SET);

    if(size > MaxROMImageSize)
    {
      MDFN_PrintError(_("ROM image is too large; maximum size allowed is %llu bytes."), (unsigned long long)MaxROMImageSize);
      goto doret;
    }

#ifdef HAVE_MMAP
    if((void *)-1 != (f_data = (uint8 *)mmap(NULL, size, PROT_READ, MAP_SHARED, fileno((FILE *)tz), 0)))
    {
      //puts("mmap'ed");
      is_mmap = TRUE;
#ifdef HAVE_MADVISE
      if(0 == madvise(f_data, size, MADV_SEQUENTIAL | MADV_WILLNEED))
      {
        //puts("madvised");
      }
#endif
    }
    else
    {
#endif

#ifdef MEM2
      if( isgame )
      {
        if(!(f_data = (uint8*)Mem2ManagerAlloc(size, _("file read buffer"))))
        {
          goto doret;
        }
      }
      else
      {
#endif
      if(!(f_data = (uint8*)MDFN_malloc(size, _("file read buffer"))))
      {
        goto doret;
      }
#ifdef MEM2
      }
#endif
      if((int64)::fread(f_data, 1, size, (FILE *)tz) != size)
      {
#ifdef MEM2
        if( !isgame )
        {
#endif
        free(f_data);
#ifdef MEM2
        }
#endif
        goto doret;
      }
#ifdef HAVE_MMAP
    }
#endif
  }
  else if(type == MDFN_FILETYPE_GZIP)
  {
    uint32_t cur_size = 0;
    uint32_t cur_alloced = 65536;
    int howmany;

#ifdef MEM2
    if( isgame )
    {
      if(!(f_data = (uint8*)Mem2ManagerAlloc(cur_alloced, _("file read buffer"))))
      {
        goto doret;
      }
    }
    else
    {
#endif
    if(!(f_data = (uint8*)MDFN_malloc(cur_alloced, _("file read buffer"))))
    {
      goto doret;
    }
#ifdef MEM2
    }
#endif

    while((howmany = gzread(tz, f_data + cur_size, cur_alloced - cur_size)) > 0)
    {
      cur_size += howmany;
      cur_alloced <<= 1;
#ifdef MEM2
      if( cur_alloced > 0x2000000 ) 
      {
        // 32megs is our max... deal with it.
        cur_alloced = 0x2000000;
      }

      if( isgame )
      {
        if(!(f_data = (uint8 *)Mem2ManagerAdjust(f_data, cur_alloced, _("file read buffer")))) 
        {
          goto doret;
        }
      }
      else
      {
#endif
      if(!(f_data = (uint8 *)MDFN_realloc(f_data, cur_alloced, _("file read buffer")))) 
      {
        goto doret;
      }
#ifdef MEM2
      }
#endif
    }

#ifdef MEM2
    if( isgame )
    {
      if(!(f_data = (uint8 *)Mem2ManagerAdjust(f_data, cur_size, _("file read buffer")))) 
      {
        goto doret;
      }
    }
    else
    {
#endif
    if(!(f_data = (uint8 *)MDFN_realloc(f_data, cur_size, _("file read buffer")))) 
    {
      goto doret;
    }
#ifdef MEM2
    }
#endif

    f_size = cur_size;
  }
  else if(type == MDFN_FILETYPE_ZIP)
  {
    unz_file_info ufo; 
    unzGetCurrentFileInfo(tz, &ufo, 0, 0, 0, 0, 0, 0);

    f_size = ufo.uncompressed_size;

    if(size > MaxROMImageSize)
    {
      MDFN_PrintError(_("ROM image is too large; maximum size allowed is %llu bytes."), (unsigned long long)MaxROMImageSize);
      goto doret;
    }

#ifdef MEM2
    if( isgame )
    {
      if(!(f_data = (uint8*)Mem2ManagerAlloc(ufo.uncompressed_size, _("file read buffer"))))
      {
        goto doret;
      }
    }
    else
    {
#endif
    if(!(f_data=(uint8 *)MDFN_malloc(ufo.uncompressed_size, _("file read buffer"))))
    {
      goto doret;
    }
#ifdef MEM2
    }
#endif

    unzReadCurrentFile(tz, f_data, ufo.uncompressed_size);
  }

  ret = TRUE;

doret:
  if(type == MDFN_FILETYPE_PLAIN)
  {
    fclose((FILE *)tz);
  }
  else if(type == MDFN_FILETYPE_GZIP)
  {
    gzclose(tz);
  }
  else if(type == MDFN_FILETYPE_ZIP)
  {
    unzCloseCurrentFile(tz);
    unzClose(tz);
  }

  return(ret);
}

MDFNFILE::MDFNFILE() : size(f_size), data((const uint8* const &)f_data), ext((const char * const &)f_ext), isgame(false)
{
  f_data = NULL;
  f_size = 0;
  f_ext = NULL;

  location = 0;

#ifdef HAVE_MMAP
  is_mmap = 0;
#endif
}

MDFNFILE::~MDFNFILE()
{
  Close();
}


bool MDFNFILE::Open(const char *path, const FileExtensionSpecStruct *known_ext, const char *purpose, const bool suppress_notfound_pe)
{
  if( purpose != NULL && !strcmp( purpose, "game" ) )
  {
    isgame = true;
  }

  void *t;
  unzFile tz;

  local_errno = 0;
  error_code = MDFNFILE_EC_OTHER;	// Set to 0 at the end if the function succeeds.

  //f_data = (uint8 *)0xDEADBEEF;

  // Try opening it as a zip file first
  if((tz = unzOpen(path)))
  {
    char tempu[1024];
    int errcode;

    if((errcode = unzGoToFirstFile(tz)) != UNZ_OK)
    {
      MDFN_PrintError(_("Could not seek to first file in ZIP archive: %s"), unzErrorString(errcode));
      unzClose(tz);

      return(NULL);
    }

    if(known_ext)
    {
      bool FileFound = FALSE;
      while(!FileFound)
      {
        size_t tempu_strlen;
        const FileExtensionSpecStruct *ext_search = known_ext;

        if((errcode = unzGetCurrentFileInfo(tz, 0, tempu, 1024, 0, 0, 0, 0)) != UNZ_OK)
        {
          MDFN_PrintError(_("Could not get file information in ZIP archive: %s"), unzErrorString(errcode));
          unzClose(tz);

          return(NULL);
        }

        tempu[1023] = 0;
        tempu_strlen = strlen(tempu);

        while(ext_search->extension && !FileFound)
        {
          size_t ttmeow = strlen(ext_search->extension);
          if(tempu_strlen >= ttmeow)
          {
            if(!strcasecmp(tempu + tempu_strlen - ttmeow, ext_search->extension))
              FileFound = TRUE;
          }
          ext_search++;
        }

        if(FileFound)
          break;

        if((errcode = unzGoToNextFile(tz)) != UNZ_OK)
        { 
          if(errcode != UNZ_END_OF_LIST_OF_FILE)
          {
            MDFN_PrintError(_("Error seeking to next file in ZIP archive: %s"), unzErrorString(errcode));
            unzClose(tz);
            return(NULL);
          }

          if((errcode = unzGoToFirstFile(tz)) != UNZ_OK)
          {
            MDFN_PrintError(_("Could not seek to first file in ZIP archive: %s"), unzErrorString(errcode));
            unzClose(tz);
            return(NULL);
          }
          break;     
        }

      } // end to while(!FileFound)
    } // end to if(ext)

    if((errcode = unzOpenCurrentFile(tz)) != UNZ_OK)
    {
      MDFN_PrintError(_("Could not open file in ZIP archive: %s"), unzErrorString(errcode));
      unzClose(tz);
      return(NULL);
    }

    if(!MakeMemWrap(tz, MDFN_FILETYPE_ZIP))
      return(0);

    char *ld = strrchr(tempu, '.');

    f_ext = strdup(ld ? ld + 1 : "");
  }
  else // If it's not a zip file, handle it as...another type of file!
  {
    if(!(t = fopen(path,"rb")))
    {
      ErrnoHolder ene(errno);
      local_errno = ene.Errno();

      if(ene.Errno() == ENOENT)
      {
        local_errno = ene.Errno();
        error_code = MDFNFILE_EC_NOTFOUND;
      }

      if(ene.Errno() != ENOENT || !suppress_notfound_pe)
        MDFN_PrintError(_("Error opening \"%s\": %s"), path, ene.StrError());

      return(0);
    }

    uint32 gzmagic;

    gzmagic = ::fgetc((FILE *)t);
    gzmagic |= ::fgetc((FILE *)t)<<8;
    gzmagic |= ::fgetc((FILE *)t)<<16;

    if(gzmagic!=0x088b1f)   /* Not gzip... */
    {
      ::fseek((FILE *)t,0,SEEK_SET);

      if(!MakeMemWrap(t, 0))
        return(0);

      const char *ld = strrchr(path, '.');
      f_ext = strdup(ld ? ld + 1 : "");
    }
    else                  /* Probably gzip */
    {
#ifdef WII
      lseek(fileno((FILE *)t), 0, SEEK_SET);
      void* tmpt;
      if(!(tmpt=gzdopen(fileno((FILE*)t), "rb")))
      {
        fclose((FILE*)t);
        return(0);
      }
      t = tmpt;
#else
      // Is this a bug? seems both handles need to be closed... -- Raz.
      int fd;
      fd = dup(fileno( (FILE *)t)); 
      lseek(fd, 0, SEEK_SET);

      if(!(t=gzdopen(fd, "rb")))
      {
        close(fd);
        return(0);
      }
#endif
      if(!MakeMemWrap(t, 1))
      {
        gzclose(t);
        return(0);
      }

      char *tmp_path = strdup(path);
      char *ld = strrchr(tmp_path, '.');

      if(ld && ld > tmp_path)
      {
        char *last_ld = ld;
        *ld = 0;
        ld = strrchr(tmp_path, '.');
        if(!ld) { *last_ld = '.'; ld = last_ld; }
      }
      f_ext = strdup(ld ? ld + 1 : "");
      free(tmp_path);
    } // End gzip handling
  } // End normal and gzip file handling else to zip

  // FIXME:  Handle extension fixing for cases where loaded filename is like "moo.moo/lalala"

  error_code = 0;

  return(TRUE);
}

bool MDFNFILE::Close(void)
{
  if(f_ext)
  {
    free(f_ext);
    f_ext = NULL;
  }

  if(f_data)
  {
#if HAVE_MMAP
    if(is_mmap) 
      munmap(f_data, size);
    else
#endif
#ifdef MEM2
    if( !isgame )
    {
#endif
      MDFN_free(f_data);
#ifdef MEM2
    }
#endif
    f_data = NULL;
  }

  return(1);
}

uint64 MDFNFILE::fread(void *ptr, size_t element_size, size_t nmemb)
{
  uint32 total = element_size * nmemb;

  if(location >= f_size)
    return 0;

  if((location + total) > f_size)
  {
    int64 ak = f_size - location;

    memcpy((uint8*)ptr, f_data + location, ak);

    location = f_size;

    return(ak / element_size);
  }
  else
  {
    memcpy((uint8*)ptr, f_data + location, total);

    location += total;

    return nmemb;
  }
}

int MDFNFILE::fseek(int64 offset, int whence)
{
  switch(whence)
  {
  case SEEK_SET:if(offset >= f_size)
                  return(-1);
    location = offset;
    break;

  case SEEK_CUR:if((offset + location) > f_size)
                  return(-1);

    location += offset;
    break;
  }    
  return 0;
}

int MDFNFILE::read16le(uint16 *val)
{
  if((location + 2) > size)
    return 0;

  *val = MDFN_de16lsb(data + location);

  location += 2;

  return(1);
}

int MDFNFILE::read32le(uint32 *val)
{
  if((location + 4) > size)
    return 0;

  *val = MDFN_de32lsb(data + location);

  location += 4;

  return(1);
}

char *MDFNFILE::fgets(char *s, int buffer_size)
{
  int pos = 0;

  if(!buffer_size)
    return(NULL);

  if(location >= buffer_size)
    return(NULL);

  while(pos < (buffer_size - 1) && location < buffer_size)
  {
    int v = data[location];
    s[pos] = v;
    location++;
    pos++;
    if(v == '\n') break;
  }

  if(buffer_size)
    s[pos] = 0;

  return(s);
}

static INLINE bool MDFN_DumpToFileReal(const char *filename, int compress, const std::vector<PtrLengthPair> &pearpairs)
{
  if(MDFN_GetSettingB("filesys.disablesavegz"))
    compress = 0;

  if(compress)
  {
    char mode[64];
    gzFile gp;

    trio_snprintf(mode, 64, "wb%d", compress);

    gp = gzopen(filename, mode);

    if(!gp)
    {
      ErrnoHolder ene(errno);

      MDFN_PrintError(_("Error opening \"%s\": %s"), filename, ene.StrError());
      return(0);
    }

    for(unsigned int i = 0; i < pearpairs.size(); i++)
    {
      const void *data = pearpairs[i].GetData();
      const int64 length = pearpairs[i].GetLength();

      if(gzwrite(gp, data, length) != length)
      {
        int errnum;

        MDFN_PrintError(_("Error writing to \"%s\": %s"), filename, gzerror(gp, &errnum));
        gzclose(gp);
        return(0);
      }
    }

    if(gzclose(gp) != Z_OK) // FIXME: Huhm, how should we handle this?
    {
      MDFN_PrintError(_("Error closing \"%s\""), filename);
      return(0);
    }
  }
  else
  {
    FILE *fp = fopen(filename, "wb");
    if(!fp)
    {
      ErrnoHolder ene(errno);

      MDFN_PrintError(_("Error opening \"%s\": %s"), filename, ene.StrError());
      return(0);
    }

    for(unsigned int i = 0; i < pearpairs.size(); i++)
    {
      const void *data = pearpairs[i].GetData();
      const uint64 length = pearpairs[i].GetLength();

      if(fwrite(data, 1, length, fp) != length)
      {
        ErrnoHolder ene(errno);

        MDFN_PrintError(_("Error writing to \"%s\": %s"), filename, ene.StrError());
        fclose(fp);
        return(0);
      }
    }

    if(fclose(fp) == EOF)
    {
      ErrnoHolder ene(errno);

      MDFN_PrintError(_("Error closing \"%s\": %s"), filename, ene.StrError());
      return(0);
    }
  }
  return(1);
}

bool MDFN_DumpToFile(const char *filename, int compress, const std::vector<PtrLengthPair> &pearpairs)
{
  return(MDFN_DumpToFileReal(filename, compress, pearpairs));
}

bool MDFN_DumpToFile(const char *filename, int compress, const void *data, uint64 length)
{
  std::vector<PtrLengthPair> tmp_pairs;
  tmp_pairs.push_back(PtrLengthPair(data, length));
  return(MDFN_DumpToFileReal(filename, compress, tmp_pairs));
}
