/**
 * mem_allocate.h - Memory allocation and destruction calls.
 *
 * Copyright (c) 2009 Rhys "Shareese" Koedijk
 * Copyright (c) 2006 Michael "Chishm" Chisholm
 *
 * This program/include file is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as published
 * by the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program/include file is distributed in the hope that it will be
 * useful, but WITHOUT ANY WARRANTY; without even the implied warranty
 * of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software Foundation,
 * Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

#ifndef _MEM_ALLOCATE_H
#define _MEM_ALLOCATE_H

#include "mem2.hpp"

static inline void* ntfs_alloc (unsigned int size) {
    return MEM2_alloc(size);
}

static inline void* ntfs_align (unsigned int size) {
    #ifdef __wii__
    return MEM2_memalign(32, size);
    #else
    return MEM2_alloc(size);
    #endif
}

static inline void* ntfs_realloc (void *p, unsigned int size) {
    return MEM2_realloc(p, size);
}

static inline void ntfs_free (void* mem) {
    MEM2_free(mem);
}

#endif /* _MEM_ALLOCATE_H */
