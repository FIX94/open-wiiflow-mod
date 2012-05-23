#ifndef WII_UTIL_H
#define WII_UTIL_H

#ifdef __cplusplus
extern "C" {
#endif

#include <string.h>
#include <gccore.h>

#define DIR_SEP_CHAR '/'
#define DIR_SEP_STR  "/"

typedef struct RGBA
{
  unsigned char R;
  unsigned char G;
  unsigned char B;
  unsigned char A;
} RGBA;

extern void Util_chomp(char *s);
extern void Util_trim(char *s);
extern int Util_sscandec(const char *s);
extern u32 Util_sscandec_u32(const char *s);
extern char *Util_strlcpy(char *dest, const char *src, size_t size);
extern int Util_fileexists( char *filename );
extern void Util_splitpath(const char *path, char *dir_part, char *file_part);
extern void Util_getextension( char *filename, char *ext );
extern int Util_hextodec( const char* hex );
extern void Util_hextorgba( const char* hex, RGBA* rgba );
extern void Util_dectohex( int dec, char *hex, int fill );
extern void Util_rgbatohex( RGBA* rgba, char *hex );
extern void Util_ansitoUTF8( unsigned char* in, unsigned char* out );


/*
 * Converts the RGBA to an integer value
 *
 * rgba   The RGBA
 * includeAlpha Whether to include alpha in the value
 * return The RGBA as an integer value
 */
extern u32 Util_rgbatovalue( RGBA* rgba, BOOL includeAlpha );


#ifdef __cplusplus
}
#endif

#endif
