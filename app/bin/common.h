/** \file common.h
 * Defnitions of basic types 
 */

/*  XTrkCad - Model Railroad CAD
 *  Copyright (C) 2005 Dave Bullis
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 */

#ifndef COMMON_H
#define COMMON_H

#include <stdlib.h>
#include <stdint.h>

#ifndef TRUE
#define TRUE	(1)
#define FALSE	(0)
#endif

#define NUM_LAYERS		(99)

typedef double FLOAT_T;
typedef double POS_T;
typedef double DIST_T;
typedef double ANGLE_T;
#define SCANF_FLOAT_FORMAT "%lf"

typedef double DOUBLE_T;
typedef double WDOUBLE_T;
typedef double FONTSIZE_T;

typedef struct {
		POS_T x,y;
		} coOrd;

typedef struct {
	coOrd pt;
	int pt_type;
} pts_t;

typedef int INT_T;

typedef int BOOL_T;
typedef int EPINX_T;
typedef int CSIZE_T;
#ifndef WIN32
typedef int SIZE_T;
#endif
typedef int STATE_T;
typedef int STATUS_T;
typedef signed char TRKTYP_T;
typedef int TRKINX_T;
typedef long DEBUGF_T;
typedef int REGION_T;

enum paramFileState { PARAMFILE_UNLOADED = 0, PARAMFILE_NOTUSABLE, PARAMFILE_COMPATIBLE, PARAMFILE_FIT, PARAMFILE_MAXSTATE };

#define SCALE_ANY	(-2)
#define SCALE_DEMO	(-1)

typedef struct {
		int cnt;
		int max;
		void * ptr;
		} dynArr_t;

#if defined(WINDOWS) && ! defined(WIN32)
#define CHECK_SIZE(T,DA) \
		if ( (long)((DA).max) * (long)(sizeof *(T*)NULL) > 65500L ) \
			AbortProg( "Dynamic array too large at %s:%d", __FILE__, __LINE__ );
#else
#define CHECK_SIZE(T,DA)
#endif

#define DYNARR_APPEND(T,DA,INCR) \
		{ if ((DA).cnt >= (DA).max) { \
			(DA).max += INCR; \
			CHECK_SIZE(T,DA) \
			(DA).ptr = MyRealloc( (DA).ptr, (DA).max * sizeof *(T*)NULL ); \
			if ( (DA).ptr == NULL ) \
				abort(); \
		} \
		(DA).cnt++; }
#define DYNARR_ADD(T,DA,INCR) DYNARR_APPEND(T,DA,INCR)

#define DYNARR_LAST(T,DA) \
		(((T*)(DA).ptr)[(DA).cnt-1])
#define DYNARR_N(T,DA,N) \
		(((T*)(DA).ptr)[N])
#define DYNARR_RESET(T,DA) \
		(DA).cnt=0
#define DYNARR_SET(T,DA,N) \
		{ if ((DA).max < N) { \
			(DA).max = N; \
			CHECK_SIZE(T,DA) \
			(DA).ptr = MyRealloc( (DA).ptr, (DA).max * sizeof *(T*)NULL ); \
			if ( (DA).ptr == NULL ) \
				abort(); \
		} \
		(DA).cnt = N; }
#define DYNARR_FREE(T,DA) \
		{ if ((DA).ptr) { \
			MyFree( (DA).ptr); \
			(DA).ptr = NULL; \
		} \
		(DA).max = 0; \
		(DA).cnt = 0; }
#define DYNARR_REMOVE(T,DA,I) \
		{ \
		 { if ((DA).cnt-1 > I) { \
				for (int i=I;i<(DA).cnt-1;i++) { \
				(((T*)(DA).ptr)[i])= (((T*)(DA).ptr)[i+1]); \
				} \
			} \
		 } \
		if ((DA.cnt)>=I) (DA).cnt--; \
		}


#ifdef WINDOWS
#define M_PI 3.14159
#define strcasecmp _stricmp
#define strncasecmp _strnicmp
#endif

#if _MSC_VER >1300
	#define strdup _strdup
#endif

#endif

