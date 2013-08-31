/*
 * inline.c --- Includes the inlined functions defined in the header
 * 	files as standalone functions, in case the application program
 * 	is compiled with inlining turned off.
 *
 * Copyright (C) 1993, 1994 Theodore Ts'o.
 *
 * %Begin-Header%
 * This file may be redistributed under the terms of the GNU Library
 * General Public License, version 2.
 * %End-Header%
 */

#ifndef _XOPEN_SOURCE
#define _XOPEN_SOURCE 600	/* for posix_memalign() */
#endif

#include "config.h"
#include <stdio.h>
#include <string.h>
#if HAVE_UNISTD_H
#include <unistd.h>
#endif
#include <fcntl.h>
#include <time.h>
#if HAVE_SYS_STAT_H
#include <sys/stat.h>
#endif
#if HAVE_SYS_TYPES_H
#include <sys/types.h>
#endif
#if HAVE_MALLOC_H
#include <malloc.h>
#endif

#include "ext2_fs.h"
#define INCLUDE_INLINE_FUNCS
#include "ext2fs.h"

#include "mem_allocate.h"

/*
 * We used to define this as an inline, but since we are now using
 * autoconf-defined #ifdef's, we need to export this as a
 * library-provided function exclusively.
 */
errcode_t ext2fs_get_memalign(unsigned long size,
			      unsigned long align, void *ptr)
{
	void **p = ptr;

	if (align < 8)
		align = 8;
#ifdef HAVE_POSIX_MEMALIGN
	errcode_t retval;
	retval = posix_memalign(p, align, size);
	if (retval) {
		if (retval == ENOMEM)
			return EXT2_ET_NO_MEMORY;
		return retval;
	}
#else
#ifdef HAVE_MEMALIGN
	*p = memalign(align, size);
	if (*p == NULL) {
		if (errno)
			return errno;
		else
			return EXT2_ET_NO_MEMORY;
	}
#else
#ifdef HAVE_VALLOC
	if (align > sizeof(long long))
		*p = valloc(size);
	else
#endif
		*p = malloc(size);
	if ((unsigned long) *p & (align - 1)) {
		free(*p);
		*p = 0;
	}
	if (*p == 0)
		return EXT2_ET_NO_MEMORY;
#endif
#endif
	return 0;
}

