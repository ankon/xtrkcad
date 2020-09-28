/** \file genpath.c
 * Generate Turnout Paths using endpoints and segments
 *
 */

/*  XTrkCad - Model Railroad CAD
 *  Copyright (C) 2020 Dave Bullis
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

/*
 * Generate Paths fo Turnouts.
 *
 * Given a set of segments and end points:
 * - find the set of segments (SubPaths) connecting 2 end points
 * - group the SubPaths into non-overlapping groups (SubPathGroups)
 * - generate the Paths object from the groups
 *
 * Major data types and structures
 *
 * PathData:
 * Holds the input segment and end points and flags
 *
 * PATHELEM_T:
 * An 'int'.
 * If positive, the bottom bit inidicates which segment end point and
 * the upper bits are the index into the segPathMap
 * If negative, indicates a end point index
 *
 * segPathMap:
 * For each segment, contains the end points and angles and
 * the set of segments each end point is connected to (PATHELEM_T)
 *
 * epPtr 
 */
#include <ctype.h>
#include <math.h>
#include <string.h>


#include "common.h"
#include "compound.h"
#include "fileio.h"
#include "track.h"
#include "utility.h"
#include "messages.h"

#ifdef NEWPATH
int log_genpaths = -1;
long lEnablePathGeneration = 0;

typedef int PATHELEM_T;
typedef struct {
	coOrd pos;
	ANGLE_T angle;
	dynArr_t epPath_da;
	} epData_t, * epData_p;

typedef struct {
	coOrd pos[2];
	ANGLE_T ang[2];
	wBool_t checked;
	dynArr_t nextSeg_da[2];
} segPathMap_t, * segPathMap_p;
segPathMap_p segPathMap;

typedef struct {
	wIndex_t segCnt;
	trkSeg_p segPtr;
	wIndex_t epCnt;
	epData_p epPtr;
	wBool_t pathOverRide;
	wBool_t pathNoCombine;
	ANGLE_T pathAngle;
	} pathData_t, * pathData_p;



wBool_t bUseSavedPaths = FALSE;
wBool_t bSuppressNoCombine = FALSE;

/*****************************************************************************
 *
 * External i/f for Paths
 *
 */

static PATHPTR_T GenerateTrackPaths( track_p trk );
static PATHPTR_T GenerateParamPaths( turnoutInfo_t * to );
#endif


/* GetPaths()
 *
 * Return the paths for 'trk'.
 * If paths are not present yet, generate them.
 *
 * \param trk IN Get paths for track 'trk'
 */
EXPORT PATHPTR_T GetPaths( track_p trk )
{
	struct extraData * xx = GetTrkExtraData( trk );
#ifdef NEWPATH
	if ( xx->new_paths == NULL ) {
		xx->new_paths = GenerateTrackPaths( trk );
	}
	return xx->new_paths;
#else
	return xx->paths;
#endif
}


/* GetPathsLength()
 *
 * Return the length of the paths object
 *
 * \param paths IN paths object
 */
EXPORT wIndex_t GetPathsLength( PATHPTR_T paths )
{
	PATHPTR_T pp;
	ASSERT( paths != NULL );
	for ( pp = paths; pp[0]; pp+=2 )
		for ( pp += strlen( (char*)pp ); pp[0] || pp[1]; pp++ );
	return pp - paths + 1;
}


/* SetPaths()
 *
 * Set the .saved_paths for 'trk'.
 * Called when paths are read from a layout file, copied from a param def'n or
 * from a Spilt turnout.
 * .saved_paths are compared with the generated paths as a check
 * This will be removed in future when all paths are generated
 *
 * \param trk IN
 * \param paths IN
 */
EXPORT void * SetPaths( track_p trk, PATHPTR_T paths )
{
	struct extraData * xx = GetTrkExtraData( trk );
	if ( paths == NULL ) {
		// Structure, but just to be safe
		paths = "\0\0\0";
	}
	wIndex_t pathLen = GetPathsLength( paths );
#ifdef NEWPATH
	if ( xx->saved_paths ) {
		MyFree( xx->saved_paths );
	}
	xx->saved_paths = memdup( paths, pathLen * sizeof *xx->saved_paths );
	if ( xx->new_paths ) {
		MyFree( xx->new_paths );
		xx->new_paths = NULL;
	}
#else
	if ( xx->paths )
		MyFree( xx->paths );
	xx->paths = memdup( paths, pathLen * sizeof *xx->paths );
#endif
	xx->currPath = NULL;
	xx->currPathIndex = 0;
}


/* GetCurrPath()
 *
 * Return the current path for 'trk'.
 * Current path is the .currPathIndex'th path
 * If the .currPathIndex is greater then the number of paths, return the first
 *
 * \param trk IN
 */
EXPORT PATHPTR_T GetCurrPath( track_p trk )
{
	struct extraData * xx = GetTrkExtraData( trk );
	if ( xx->currPath )
		return xx->currPath;
	PATHPTR_T path = GetPaths( trk );
	for ( wIndex_t position = xx->currPathIndex;
		position > 0 && path[0];
		path+=2, position-- ) {
		for ( path += strlen( (char*)path ); path[0] || path[1]; path++ );
	}
	if ( !path[0] ) {
		xx->currPathIndex = 0;
		path = GetPaths( trk );
	}
	xx->currPath = path;
	return xx->currPath;
}


/* SetCurrPathIndex()
 *
 * Get the index of the current path
 *
 * \param trk IN
 */
EXPORT long GetCurrPathIndex( track_p trk )
{
	struct extraData * xx = GetTrkExtraData( trk );
	return xx->currPathIndex;
}


/* SetCurrPathIndex()
 *
 * Set the index of the current path
 * If position is greater then the number of paths, it will be reset on
 * the next call to GetCurrPath()
 *
 * \param trk IN
 * \param position IN
 */
EXPORT void SetCurrPathIndex( track_p trk, long position )
{
	struct extraData * xx = GetTrkExtraData( trk );
	xx->currPathIndex = position;
	xx->currPath = NULL;
}


/* GetParamPaths()
 *
 * Return the .saved_paths for turnout parameter 'to'.
 *
 * \param to IN
 */
PATHPTR_T GetParamPaths( turnoutInfo_t * to )
{
#ifdef NEWPATH
	return to->saved_paths;
#else
	return to->paths;
#endif
}


/* SetParamPaths()
 *
 * Sets the saved_paths for turnout parameter 'to'.
 * Used when creating a new turnout def'n
 * This will be removed in future when all paths are generated
 *
 * \param to IN
 * \param paths IN
 */
void SetParamPaths( turnoutInfo_t * to, PATHPTR_T paths )
{
	ASSERT( paths != NULL );
#ifdef NEWPATH
	if ( to->saved_paths )
		MyFree( to->saved_paths );
	wIndex_t pathsLen = GetPathsLength( paths );
	to->saved_paths = (PATHPTR_T)memdup( paths, pathsLen * sizeof *(PATHPTR_T*)NULL );
	// Check it the generated paths are the same as the old paths
	GenerateParamPaths( to );
#else
	if ( to->paths )
		MyFree( to->paths );
	wIndex_t pathsLen = GetPathsLength( paths );
	to->paths = (PATHPTR_T)memdup( paths, pathsLen * sizeof *(PATHPTR_T*)NULL );
#endif
}


/*****************************************************************************
 *
 * Forwards
 */

#ifdef NEWPATH
static PATHPTR_T GeneratePaths( pathData_p );
static wBool_t ComparePaths( PATHPTR_T, PATHPTR_T );
static void DumpPaths( PATHPTR_T pPath );

static void GeneratePathSegments( pathData_p );
static void GeneratePathEndPts( pathData_p );

static void DumpSegPathMap( pathData_p );

static void GenerateSubPaths( pathData_p );
static void CombineSubPaths( pathData_p );

static PATHPTR_T CreatePaths( pathData_p );

typedef struct WalkSetPathMap_s WalkSetPathMap_t, *WalkSetPathMap_p;
static void WalkSegPathMap( int depth, WalkSetPathMap_p tsP, int inx, int cnt );
static void AppendSubPath( int depth, WalkSetPathMap_p tsP );
static wBool_t CompareSubPathEndPts( PATHELEM_T * path1, PATHELEM_T * path2 );
static void DumpSubPath( wIndex_t inx );

static wIndex_t SubPathLength( PATHELEM_T * path, wBool_t bInclEP );

static void DecomposePath( dynArr_t * subPath_da, PATHPTR_T pPath );
static int CmpPath1Ptr( const void * ptr1, const void * ptr2 );
static int CmpPath2Ptr( const void * ptr1, const void * ptr2 );
static int CmpPath3Ptr( const void * ptr1, const void * ptr2 );

static void FlipSubPath( PATHELEM_T * path );
typedef struct WalkSubPaths_s WalkSubPaths_t, *WalkSubPaths_p;
static wBool_t ConflictingSubPaths( PATHELEM_T * path1, PATHELEM_T * path2 );
static void WalkSubPaths( pathData_p pdP, int subPathInx, WalkSubPaths_p wspP );
typedef int SUBPATHGROUP_T;
static wBool_t SubsumesSubPathGroup( SUBPATHGROUP_T * spg1, SUBPATHGROUP_T * spg2 );
int CmpSubPathGroups( const void * ptr1, const void * ptr2 );
static void SortSubPathGroups( pathData_p pdP );

static char indent[] = " . . . . . . . . . . . . . . . . . . .";


/*****************************************************************************
 *
 * High level 
 */

/* GeneratedTrackPaths()
 *
 * Populate global data structure (pathdata_t) from Turnout Track
 * Generate the paths
 * Compare the new paths with .saved_paths
 *
 * \param trk IN
 */
static PATHPTR_T GenerateTrackPaths( track_p trk )
{
	if ( log_genpaths < 0 ) {
		log_genpaths = LogFindIndex( "genpaths" );
		wPrefGetInteger( "misc", "EnablePathGeneration", &lEnablePathGeneration, 0 );
	}

	struct extraData * xx = GetTrkExtraData( trk );
	LOG( log_genpaths, 1, ("GenPaths T%d %s\n", GetTrkIndex( trk ), xx->title ) );
	if ( xx->pathOverRide || lEnablePathGeneration == 0 )
		return xx->saved_paths;
	pathData_t pathData;
	memset( &pathData, 0, sizeof pathData );
	pathData.segCnt = xx->segCnt;
	pathData.segPtr = xx->segs;
	pathData.epCnt = GetTrkEndPtCnt(trk);
	pathData.epPtr = (epData_p)MyMalloc( pathData.epCnt * sizeof *(epData_p)NULL );
	for ( wIndex_t epInx = 0; epInx < pathData.epCnt; epInx ++ ) {
		epData_p epPtr = &pathData.epPtr[epInx];
		epPtr->pos = GetTrkEndPos( trk, epInx );
		epPtr->pos.x -= xx->orig.x;
		epPtr->pos.y -= xx->orig.y;
		epPtr->angle = GetTrkEndAngle( trk, epInx );
		Rotate( &epPtr->pos, zero, -xx->angle );
		epPtr->angle = NormalizeAngle( epPtr->angle - xx->angle );
	}
	pathData.pathNoCombine = xx->pathNoCombine;
	PATHPTR_T pPath = GeneratePaths( &pathData );
	wBool_t bSame = ComparePaths( xx->saved_paths, pPath );
	if ( ! bSame )
		LogPrintf( "Path mismatch T%d: %s\n", GetTrkIndex( trk ), xx->title );
	if ( ( ! bSame ) ||
	     ( log_genpaths >= 1 && logTable(log_genpaths).level >= 1 ) ) {
		LogPrintf( "Old Path:\n" );
		DumpPaths( xx->saved_paths );
		LogPrintf( "New Path:\n" );
		DumpPaths( pPath );
	}
	if ( bUseSavedPaths )
		pPath = xx->saved_paths;

	return pPath;
}


/* GeneratedParamPaths()
 *
 * Populate global data structure (pathdata_t) from Turnout def'n
 * Generate the paths
 * Compare the new paths with .saved_paths
 *
 * \param to IN
 */
static PATHPTR_T GenerateParamPaths( turnoutInfo_t * to )
{
	if ( log_genpaths < 0 ) {
		log_genpaths = LogFindIndex( "genpaths" );
		wPrefGetInteger( "misc", "EnablePathGeneration", &lEnablePathGeneration, 0 );
	}
	LOG( log_genpaths, 1, ("GenPaths %s\n", to->title ) );
	if ( to->pathOverRide || lEnablePathGeneration == 0 )
		return to->saved_paths;
	pathData_t pathData;
	memset( &pathData, 0, sizeof pathData );
	pathData.segCnt = to->segCnt;
	pathData.segPtr = to->segs;
	pathData.epCnt = to->endCnt;
	pathData.epPtr = (epData_p)MyMalloc( pathData.epCnt * sizeof *(epData_p)NULL );
	for ( wIndex_t epInx = 0; epInx < pathData.epCnt; epInx ++ ) {
		epData_p epPtr = &pathData.epPtr[epInx];
		epPtr->pos = to->endPt[epInx].pos;
		epPtr->angle = to->endPt[epInx].angle;
	}
	pathData.pathNoCombine = to->pathNoCombine;
	pathData.pathOverRide = to->pathOverRide;
	PATHPTR_T pPath = GeneratePaths( &pathData );
	wBool_t bSame = ComparePaths( to->saved_paths, pPath );
	if ( ! bSame )
		LogPrintf( "Path mismatch %s:%d %s\n", paramFileName, paramLineNum, to->title );
	if ( ( ! bSame ) ||
	     ( log_genpaths >= 1 && logTable(log_genpaths).level >= 1 ) ) {
		LogPrintf( "Old Path:\n" );
		DumpPaths( to->saved_paths );
		LogPrintf( "New Path:\n" );
		DumpPaths( pPath );
	}

	if ( bUseSavedPaths )
		pPath = to->saved_paths;

	return pPath;
}


/* GeneratePaths()
 *
 *
 * \param pdP IN/OUT genpaths work area
 */
static PATHPTR_T GeneratePaths( pathData_p pdP )
{
	if ( pdP->pathAngle == 0 )
		pdP->pathAngle = 5.0;

	GeneratePathSegments( pdP );
	
	GeneratePathEndPts( pdP );

	DumpSegPathMap( pdP );

	GenerateSubPaths( pdP );

	CombineSubPaths( pdP );

	return CreatePaths( pdP );
}

/*****************************************************************************
 *
 * Get position and angle of each segment and endpoint
 *
 */


/* GeneratePathSegments()
 *
 * Build SegPathMap
 * - compute each segment's end point Position and Angle
 * - for each segment's end point, find any other close segments
 *
 * \param pdP IN/OUT genpaths work area
 */
static void GeneratePathSegments( pathData_p pdP )
{
	segPathMap = (segPathMap_p)MyMalloc( pdP->segCnt * sizeof *(segPathMap_p)NULL );
	LOG( log_genpaths, 3, ( "GeneratePathSegments - segPathMap\n For each segment, find its EndPt pos and angle\n" ) );
	for ( int segInx=0; segInx < pdP->segCnt; segInx++ ) {
		if ( !IsSegTrack( &pdP->segPtr[segInx] ) )
			continue;
		for ( int dir = 0; dir < 2; dir++ )
			segPathMap[segInx].pos[dir] = GetSegEndPt( &pdP->segPtr[segInx], dir, FALSE, &segPathMap[segInx].ang[dir] );
		LOG( log_genpaths, 3, ( " S%d [%0.3f %0.3f] A%0.3f - [%0.3f %0.3f] A%0.3f\n", segInx+1,
		segPathMap[segInx].pos[0].x,
		segPathMap[segInx].pos[0].y,
		segPathMap[segInx].ang[0],
		segPathMap[segInx].pos[1].x,
		segPathMap[segInx].pos[1].y,
		segPathMap[segInx].ang[1] ) );
	}

	// find connections
	LOG( log_genpaths, 3, ( "Connections - segPathMap\n For each segment, find the segments it attaches to\n" ) );
	for ( int segInx0 = 0; segInx0 < pdP->segCnt; segInx0++ ) {
		if ( ! IsSegTrack( &pdP->segPtr[segInx0] ) )
			continue;
		segPathMap_p segPathMapP0 = & segPathMap[segInx0];
		for ( int segInx1 = segInx0+1; segInx1 < pdP->segCnt; segInx1++ ) {
			if ( ! IsSegTrack( &pdP->segPtr[segInx1] ) )
				continue;
			segPathMap_p segPathMapP1 = & segPathMap[segInx1];
			for ( int dir0 = 0; dir0 < 2; dir0++ ) {
				for ( int dir1 = 0; dir1 < 2; dir1++ ) {
					ANGLE_T ang = 
						segPathMapP0->ang[dir0] -
						segPathMapP1->ang[dir1];
					ang = NormalizeAngle( ang + 180.0 + pdP->pathAngle/2.0 ); 
					if ( ang > pdP->pathAngle ) {
						continue;
					}
					DIST_T dist = FindDistance(
						segPathMapP0->pos[dir0],
						segPathMapP1->pos[dir1] );
					if ( dist > connectDistance ) {
						continue;
					}
					LOG( log_genpaths, 3, ( " S%d.%d = S%d.%d D:%0.3f A:%0.3f\n",
						segInx0+1, dir0, segInx1+1, dir1, dist, fabs( ang - pdP->pathAngle/2.0 ) ) );
					DYNARR_APPEND( PATHELEM_T, segPathMapP0->nextSeg_da[dir0], 10 );
					DYNARR_LAST( PATHELEM_T, segPathMapP0->nextSeg_da[dir0] ) = (segInx1+1)*2 + dir1;
					DYNARR_APPEND( PATHELEM_T, segPathMapP1->nextSeg_da[dir1], 10 );
					DYNARR_LAST( PATHELEM_T, segPathMapP1->nextSeg_da[dir1] ) = (segInx0+1)*2 + dir0;
				}
			}
		}
	}
}


/* GeneratePathEndPts()
 *
 * Add EndPoints to the SegPathMap
 *
 * \param pdP IN/OUT genpaths work area
 */
static void GeneratePathEndPts( pathData_p pdP )
{
	LOG( log_genpaths, 3, ( "GenerateEndPoints - .epPtr\n For each EndPt, find the segments it attaches to\n" ) );
	wIndex_t eCnt = pdP->epCnt;

	for ( wIndex_t eInx = 0; eInx < eCnt; eInx++ ) {
		LOG( log_genpaths, 3, ( " EP%d [%0.3f %0.3f] A%0.3f", eInx+1, pdP->epPtr[eInx].pos.x, pdP->epPtr[eInx].pos.y, pdP->epPtr[eInx].angle ) );
		for ( int segInx0 = 0; segInx0 < pdP->segCnt; segInx0++ ) {
			if ( ! IsSegTrack( &pdP->segPtr[segInx0] ) )
				continue;
			for ( int dir0 = 0; dir0 < 2; dir0++ ) {
				segPathMap_p segPathMapP = & segPathMap[segInx0];
				ANGLE_T ang = 
					segPathMapP->ang[dir0] -
					pdP->epPtr[eInx].angle;
				ang = NormalizeAngle( ang + 5.0/2.0 ); 
				if ( ang > 5.0 )
					continue;
				DIST_T dist = FindDistance(
					segPathMapP->pos[dir0],
					pdP->epPtr[eInx].pos );
				if ( dist > connectDistance )
					continue;
				LOG( log_genpaths, 3, ( " = S%d.%d", segInx0+1, dir0 ) );
				DYNARR_APPEND( PATHELEM_T, segPathMapP->nextSeg_da[dir0], 10 );
				DYNARR_LAST( PATHELEM_T, segPathMapP->nextSeg_da[dir0] ) = -(eInx+1);
				DYNARR_APPEND( PATHELEM_T, pdP->epPtr[eInx].epPath_da, 10 );
				DYNARR_LAST( PATHELEM_T, pdP->epPtr[eInx].epPath_da ) = (segInx0+1)*2+dir0;

			}
		}
		LOG( log_genpaths, 3, ( "\n" ) );
	}
}


static void DumpSegPathMap( pathData_p pdP )
{
	if ( log_genpaths >= 1 && logTable(log_genpaths).level >= 2 ) {
		wIndex_t eCnt = pdP->epCnt;
		LogPrintf( "SegPathMap:\n" );
		for ( int segInx0 = 0; segInx0 < pdP->segCnt; segInx0++ ) {
			for ( int dir0 = 0; dir0 < 2; dir0++ ) {
				for (int n = 0; n<segPathMap[segInx0].nextSeg_da[dir0].cnt; n++ ) {
					PATHELEM_T elem = DYNARR_N( PATHELEM_T, segPathMap[segInx0].nextSeg_da[dir0], n );
					if ( elem >= 0 )
						LogPrintf( " S%d.%d - S%ld.%ld\n", segInx0+1, dir0, elem>>1, elem&0x1 );
					else
						LogPrintf( " S%d.%d - E%ld\n", segInx0+1, dir0, -elem );
				}
			}
		}
	}
}

/*****************************************************************************
 *
 *
 *
 */

struct WalkSetPathMap_s {
	WalkSetPathMap_p prev;
	PATHELEM_T elem;
	};

dynArr_t subPaths_da;
#define subPaths(N) DYNARR_N( PATHELEM_T *, subPaths_da, N )
wIndex_t bumperEndPt;

/* GenerateSubPaths()
 *
 * Find the SubPaths that connection EndPoints
 *
 * \param pdP IN/OUT genpaths work area
 */
static void GenerateSubPaths( pathData_p pdP )
{
	LOG( log_genpaths, 3, ( "GenerateSubPaths - subPaths < segPathMap\n For each EndPt, walk its segment tree and add the branch to subPaths\n" ) );
	wIndex_t eCnt = pdP->epCnt;
	bumperEndPt = eCnt;
	DYNARR_RESET( PATHELEM_T*, subPaths_da );
	for ( wIndex_t eInx = 0; eInx < eCnt; eInx++ ) {
		for ( int segInx0 = 0; segInx0<pdP->segCnt; segInx0++ )
			segPathMap[segInx0].checked = FALSE;
		WalkSetPathMap_t ts0;
		ts0.prev = NULL;
		ts0.elem = -(eInx+1);
		int epCnt = pdP->epPtr[eInx].epPath_da.cnt;
		for (int epInx = 0; epInx<epCnt; epInx++ ) {
			PATHELEM_T elem = DYNARR_N( PATHELEM_T, pdP->epPtr[eInx].epPath_da, epInx );
			WalkSetPathMap_t ts1;
			ts1.prev = &ts0;
			ts1.elem = elem;
			LOG( log_genpaths, 5, ( ". [1] EP%d% d/%d\n", eInx+1, epInx+1, epCnt ) );
			WalkSegPathMap( 2, &ts1, epInx, epCnt );
		}
	}
}


/* WalkSegPathMap()
 *
 * Recursively walk the SetPathMap from each EndPoint to end of a branch
 *
 * 'tsP' is a linked list of segments to the root for the current path
 * If the current node is an EndPoint we are done with this branch of the tree
 * and can append the SubPath to the list (if not a Loop or Duplicate)
 * If the node has no nextSeg, then we append a fake EndPoint and append.
 * If the node is 'checked, then there is a loop and we stop.
 *
 * \param depth IN recursion depth
 * \param tsP IN current node on the linked list of Segments
 * \param inx IN debug
 * \param cnt IN debug
 */
static void WalkSegPathMap( int depth, WalkSetPathMap_p tsP, int inx, int cnt )
{
	int segInx, dir;
	PATHELEM_T elem = tsP->elem;
	if ( elem > 0 ) {
		segInx = (int)(elem>>1)-1;
		dir = (int)elem&0x1;
		LOG( log_genpaths, 5, ( "%.*s [%d] %d/%d S%d.%d\n", depth*2, indent, depth, inx+1, cnt, segInx+1, dir ) );
	} else {
		LOG( log_genpaths, 5, ( "%.*s [%d] %d/%d EP%d\n", depth*2, indent, depth, inx+1, cnt, -elem ) );
	}
	if ( elem < 0 ) {
		// Hit EndPt
		// Terminate path
		AppendSubPath( depth, tsP );
		return;
	}
	if ( segPathMap[segInx].nextSeg_da[1-dir].cnt == 0 ) {
		// Hit bumper - append a fake endPt
		WalkSetPathMap_t ts1;
		ts1.prev = tsP;
		ts1.elem =  -(++bumperEndPt);
		depth++;
		LOG( log_genpaths, 5, ( "%.*s [%d] %d/%d EP%d\n", depth*2, indent, depth, 1, 1, -ts1.elem ) );
		AppendSubPath( depth, &ts1 );
		return;
	}
	if ( segPathMap[segInx].checked ) {
		// hit a loop
		if ( log_genpaths >= 1 && logTable(log_genpaths).level >= 5 ) {
			LogPrintf( "%.*s = Loop: %d", depth*2, indent, segInx+1 );
			for ( ; tsP; tsP=tsP->prev ) {
				elem = tsP->elem;
				if ( elem < 0 )
					LogPrintf( " EP%d", - elem );
				else
					LogPrintf( " S%d.%d", elem>>1, elem&0x01 );
			}
			LogPrintf( "\n" );
		}
		return;
	}
	segPathMap[segInx].checked = TRUE;
	WalkSetPathMap_t ts;
	ts.prev = tsP;
	int segCnt = segPathMap[segInx].nextSeg_da[1-dir].cnt;
	for (int n = 0; n<segCnt; n++ ) {
		PATHELEM_T elem = DYNARR_N( PATHELEM_T, segPathMap[segInx].nextSeg_da[1-dir], n );
		ts.elem = elem;
		WalkSegPathMap( depth+1, &ts, n, segCnt );
	}
}


/* AppendSubPath()
 *
 * Build a SubPath from the linked list
 * Check if it a duplicate
 * Append it to SubPaths[]
 *
 * \param depth IN
 * \param tsP IN
 */

static void AppendSubPath( int depth, WalkSetPathMap_p tsP )
{
	PATHELEM_T *pSubPath = (PATHELEM_T*)MyMalloc( (depth+2) * sizeof *(PATHELEM_T*)NULL );
	int depth1 = depth;
	pSubPath[depth] = 0;
	pSubPath[depth+1] = 0;
	for ( depth--; depth >= 0; depth-- ) {
		pSubPath[depth] = tsP->elem;
		tsP = tsP->prev;
	}
	for ( wIndex_t inx = 0; inx < subPaths_da.cnt; inx++ ) {
		if ( CompareSubPathEndPts( pSubPath, subPaths(inx) ) ) {
			LOG( log_genpaths, 5, ("%.*s = Duplicate\n", depth1*2, indent ) );
			MyFree( pSubPath );
			return;
		}
	}
	DYNARR_APPEND( PATHELEM_T*, subPaths_da, 10 );
	DYNARR_LAST( PATHELEM_T*, subPaths_da ) =  pSubPath;

	LOG( log_genpaths, 5 , ( "%.*s = Appending\n", depth1*2, indent ) );
	if ( log_genpaths >= 1 && logTable(log_genpaths).level >= 3 ) {
		DumpSubPath( subPaths_da.cnt-1 );
	}
}


static wBool_t CompareSubPathEndPts( PATHELEM_T * path1, PATHELEM_T * path2 )
{
	PATHELEM_T e10, e11, e20, e21;
	e10 = *path1;
	e20 = *path2;
	for ( PATHELEM_T * p = path1; *p; e11=*p++ );
	for ( PATHELEM_T * p = path2; *p; e21=*p++ );
	if ( e10 >= 0 || e11 >= 0 || e20 >= 0 || e21 >= 0 )
		return FALSE;
	if ( (e10 == e20 && e11 == e21) ||
	     (e10 == e21 && e11 == e20) )
		return TRUE;
	return FALSE;
}


/*****************************************************************************
 *
 *
 *
 */


struct WalkSubPaths_s {
	WalkSubPaths_p prev;
	int subPathInx;
	};
wBool_t * pConflictMap = NULL;
dynArr_t subPathGroup_da;
#define SubPathGroup(N) DYNARR_N( SUBPATHGROUP_T *, subPathGroup_da, N )

/* CombineSubPaths()
 *
 * Combine distinct subpath (having no segments in common) from SubPaths[]
 * Append indices of subPath to SubPathGroup[]
 *
 * \param pdP IN/OUT genpaths work area
 */
static void CombineSubPaths( pathData_p pdP )
{
	// Make subpaths simplier
	for ( int inx0 = 0; inx0 < subPaths_da.cnt; inx0++ ) 
		FlipSubPath( subPaths(inx0) );

	DYNARR_RESET( SUBPATHGROUP_T*, subPathGroup_da );
	if ( pdP->pathNoCombine && !bSuppressNoCombine ) {
		// append each subPath as a separate subPathGroup member
		for ( int inx = 0; inx<subPaths_da.cnt; inx++ ) {
			SUBPATHGROUP_T * pSubPath = (SUBPATHGROUP_T*)MyMalloc( 2 * sizeof *(SUBPATHGROUP_T*)NULL );
			pSubPath[0] = inx+1;
			pSubPath[1] = 0;
			DYNARR_APPEND( SUBPATHGROUP_T *, subPathGroup_da, 10 );
			DYNARR_LAST( SUBPATHGROUP_T *, subPathGroup_da ) = pSubPath;
		}
		return;	
	}

	// Build a map of conflicting subPaths
	int nPaths = subPaths_da.cnt;
	pConflictMap = (wBool_t*)MyMalloc( nPaths * nPaths * sizeof *pConflictMap ); 
	for ( int inx1 = 0; inx1 < nPaths; inx1++ ) {
		pConflictMap[inx1+inx1*nPaths] = TRUE;
		for ( int inx2 = inx1+1; inx2 < nPaths; inx2++ ) {
			pConflictMap[inx2*nPaths+inx1] =
			pConflictMap[inx1*nPaths+inx2] =
				ConflictingSubPaths( subPaths(inx1), subPaths(inx2) );
		}
	}
	if ( log_genpaths >= 1 && logTable(log_genpaths).level >= 3 ) {
		LogPrintf( "CombineSubPaths - \n Generate a list of all distinct subpath combinations\n ConflictMap:\n" );
		printf( "    %.*s\n", nPaths*2, " 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0" );
		for ( int inx1 = 0; inx1 < nPaths; inx1++ ) {
			printf( "%3d ", inx1+1 );
			for ( int inx2 = 0; inx2 < nPaths; inx2++ )
				printf( " %s", inx1==inx2?"o":pConflictMap[inx1*nPaths+inx2]?"X":"+" );
			printf("\n");
		}
	}


	WalkSubPaths( pdP, 0, NULL );
	MyFree( pConflictMap );

	SortSubPathGroups( pdP );

	if ( log_genpaths >= 1 && logTable(log_genpaths).level >= 3 ) {
		LogPrintf( "SubPathGroups:\n" );
		for ( int inx=0; inx<subPathGroup_da.cnt; inx++ ) {
			SUBPATHGROUP_T * pSPG = SubPathGroup(inx);
			if ( pSPG != NULL ) {
				LogPrintf( " SPG%d=(", inx+1 );
				for ( ; *pSPG; pSPG++ )
					LogPrintf( " SP%d", *pSPG );
				LogPrintf( " )\n" );
			}
		}
	}

}


static void FlipSubPath( PATHELEM_T * path )
{
	if ( path == NULL )
		return;
	int nFlipped = 0;
	int nTotal = 0;
	int len;
	for ( len = 0; path[len] || path[len+1]; len++ ) {
		if ( path[len] <= 0 )
			continue;
		nTotal++;
		if ( path[len] & 0x01 )
			nFlipped++;
	}
	if ( nFlipped*2 <= nTotal ) 
		return;

	for ( int inx = 0; inx*2 < len; inx++ ) {
		PATHELEM_T elem1 = path[inx];
		PATHELEM_T elem2 = path[len-inx-1];
		// flip segments
		if ( elem1 > 0 )
			elem1 = (elem1&~0x1) | (1-elem1&0x1);
		if ( elem2 > 0 )
			elem2 = (elem2&~0x1) | (1-elem2&0x1);
		path[len-inx-1] = elem1;
		path[inx] = elem2;
	}

}


static wBool_t ConflictingSubPaths( PATHELEM_T * path1, PATHELEM_T * path2 )
{
	for ( int inx1 = 0; path1[inx1] || path1[inx1+1]; inx1++ ) {
		PATHELEM_T elem1 = path1[inx1];
		if ( elem1 == 0 )
			continue;
		if ( elem1 > 0 )
			elem1 /= 2;
		for ( int inx2 = 0; path2[inx2] || path2[inx2+1]; inx2++ ) {
			PATHELEM_T elem2 = path2[inx2];
			if ( elem2 == 0 )
				continue;
			if ( elem2 > 0 )
				elem2 /= 2;
			if ( elem1 == elem2 )
				return TRUE;
		}
	}
	return FALSE;
}

static void WalkSubPaths( pathData_p pdP, int subPathInx, WalkSubPaths_p wspP )
{
	if ( subPathGroup_da.cnt > 1000 ) {
		printf( "WalkSubPaths recursion limit\n" );
		return;
	}
	if ( log_genpaths >= 1 && logTable(log_genpaths).level >= 5 ) {
		LogPrintf( "%.*s ( SP%d", (subPathInx+1)*2, indent, subPathInx+1 );
		for ( WalkSubPaths_p p = wspP; p; p=p->prev )
			LogPrintf( " SP%d", p->subPathInx+1 );
		LogPrintf( " )\n");
	}

	// Does new SubPath conflict with any previous SubPath?
	wBool_t bConflict = FALSE;
	for ( WalkSubPaths_p p = wspP; p; p = p->prev ) {
		if ( p->subPathInx < 0 )
			continue;
		bConflict = pConflictMap[ p->subPathInx*subPaths_da.cnt+subPathInx ];
		if ( bConflict ) {
			LOG( log_genpaths, 4, (" SP%d SP%d: CONFLICT\n", subPathInx+1, p->subPathInx+1, bConflict?"Conflict":"OK" ) );
			break;
		}
	}
	subPathInx++; // subPathInx is 1-based
	if ( subPathInx >= subPaths_da.cnt ) {
		// Reached the bottom
		WalkSubPaths_t wsp0;
		if ( !bConflict ) {
			// Add current subPath to path
			wsp0.prev = wspP;
			wsp0.subPathInx = subPathInx-1;
			wspP = &wsp0;
		}
		// Transform SubPath from list to an array
		int nSubPaths = 1;
		for ( WalkSubPaths_p p = wspP; p; p=p->prev, nSubPaths++ );
		SUBPATHGROUP_T * pSubPaths = (SUBPATHGROUP_T*)MyMalloc( nSubPaths * sizeof *(SUBPATHGROUP_T*)NULL );
		pSubPaths[--nSubPaths] = 0;
		for ( WalkSubPaths_p p = wspP; p; p=p->prev, nSubPaths-- )
			pSubPaths[nSubPaths-1] = p->subPathInx+1;
		if ( log_genpaths >= 1 && logTable(log_genpaths).level >= 4 ) {
			LogPrintf( " SPG%d=(", subPathGroup_da.cnt );
			for ( int * pInx = pSubPaths; *pInx; pInx++ )
				LogPrintf( " SP%d", *pInx );
			LogPrintf( " )\n" );
		}

		// Check if the new group subsumes any previous groups
		for ( int inx=0; inx<subPathGroup_da.cnt; inx++ ) {
			SUBPATHGROUP_T * * ppSubPaths1 = &DYNARR_N( SUBPATHGROUP_T *, subPathGroup_da, inx );
			if ( *ppSubPaths1 == NULL )
				continue;
			if ( SubsumesSubPathGroup( *ppSubPaths1, pSubPaths ) ) {
				LOG( log_genpaths, 4, ( " - Subsumes SPG%d\n", inx ) );
				MyFree( *ppSubPaths1 );
				*ppSubPaths1 = NULL;
			}
		}

		// Append new SubPathGroup
		DYNARR_APPEND( SUBPATHGROUP_T *, subPathGroup_da, 10 );
		DYNARR_LAST( SUBPATHGROUP_T *, subPathGroup_da ) = pSubPaths;
		return;
	}

	// Keep going to next SubPath
	WalkSubPaths( pdP, subPathInx, wspP );
	if ( !bConflict ) {
		// Walk again, including the current SubPath
		WalkSubPaths_t wsp1;
		wsp1.prev = wspP;
		wsp1.subPathInx = subPathInx-1;
		WalkSubPaths( pdP, subPathInx, &wsp1 );
	}
}


/* SubsumesSupPathGroup()
 *
 * Is spg1 subsumed by spg2?
 * Does every element in spg1 also exist in spg2
 */

static wBool_t SubsumesSubPathGroup( SUBPATHGROUP_T * spg1, SUBPATHGROUP_T * spg2 )
{
	// Assume: subPathGroups are ordered
	for ( ; *spg1; spg1++ ) {
		//printf("SPG1:%p=%d\n", spg1, *spg1 );
		for ( ; *spg2 && *spg1 != *spg2; spg2++ ) {
			//printf( "SPG2:%p=%d\n", spg2, *spg2 );
		}
		if ( *spg2 == 0 )
			return FALSE;
	}
	return TRUE;
}


int CmpSubPaths( const void * ptr1, const void * ptr2 )
{
	PATHELEM_T * pSP1 = (PATHELEM_T*)ptr1;
	PATHELEM_T * pSP2 = (PATHELEM_T*)ptr2;
	for ( ; *pSP1 && *pSP2; pSP1++, pSP2++ ) {
		PATHELEM_T elem1 = *pSP1;
		PATHELEM_T elem2 = *pSP2;
		if ( elem1 < 0 )
			elem1 = 0;
		if ( elem2 < 0 )
			elem2 = 0;
		if ( elem1 != elem2 )
			return elem1 - elem2;
	}
	if ( *pSP1 == 0 && *pSP2 == 0 )
		return 0;
	if ( *pSP1 == 0 )
		return -1;
	else
		return 1;
}

int CmpSubPathGroups( const void * ptr1, const void * ptr2 )
{
	SUBPATHGROUP_T * pSPG1 = *(SUBPATHGROUP_T**)ptr1;
	SUBPATHGROUP_T * pSPG2 = *(SUBPATHGROUP_T**)ptr2;
	if ( pSPG1 == pSPG2 )
		return 0;
	if ( pSPG1 == NULL )
		return -1;
	if ( pSPG2 == NULL )
		return 1;
	for ( ; *pSPG1 && *pSPG2 ; pSPG1++,pSPG2++ ) {
		int rc = CmpSubPaths( pSPG1, pSPG2 );
		if ( rc != 0 )
			return rc;
	}
	return 0;
}

static void SortSubPathGroups( pathData_p pdP )
{
	qsort( subPathGroup_da.ptr, subPathGroup_da.cnt, sizeof *(SUBPATHGROUP_T**)NULL, CmpSubPathGroups );
}



/*****************************************************************************
 *
 * Create final Path object
 *
 */

/* CreatePaths()
 *
 *
 * \param pdP IN/OUT genpaths work area
 */
static PATHPTR_T CreatePaths( pathData_p pdP )
{
	int len = 0;
	PATHPTR_T pPath0 = NULL;
	PATHPTR_T pPath1 = NULL;
	for ( int inx=0; inx<subPathGroup_da.cnt; inx++ ) {
		SUBPATHGROUP_T * pSPG = SubPathGroup(inx);
		if ( SubPathGroup(inx) == NULL )
			continue;
		len += 1 + (inx>=100?3:inx>=10?2:1) + 1;
		for ( ; *pSPG; pSPG++ )
			len += SubPathLength( subPaths(*pSPG-1), FALSE );
	}
	len++;
	pPath0 = pPath1 = (PATHPTR_T)MyMalloc( len * sizeof *(PATHPTR_T)NULL );

	for ( int inx = 0; inx < subPathGroup_da.cnt; inx++ ) {
		SUBPATHGROUP_T * pSPG = SubPathGroup(inx);
		if ( pSPG == NULL )
			continue;
		sprintf( pPath1, "P%d", inx );
		pPath1 += strlen( pPath1 );
		for ( ; *pSPG; pSPG++ ) {
			*pPath1++ = '\0';
			for ( PATHELEM_T * peP = subPaths(*pSPG-1); peP[0]; peP++ ) {
				if ( *peP < 0 )
					continue;
				*pPath1 = *peP/2;
				if ( *peP&0x1 ) 
					*pPath1 = - *pPath1;
				pPath1++;
			}
		}
		*pPath1++ = '\0';
		*pPath1++ = '\0';
	}
	*pPath1++ = '\0';
	return pPath0;
}


static wIndex_t SubPathLength( PATHELEM_T * path, wBool_t bInclEP )
{
	wIndex_t len = 0;
	for ( wIndex_t inx = 0; path[inx] || path[inx+1]; inx++ )
		if ( bInclEP || path[inx] >= 0 )
			len++;

	return len+2;
}



/*****************************************************************************
 *
 * Testing: compare new and old paths
 *
 */


/* ComparePaths()
 *
 *
 * \param pPath1 IN
 * \param pPath2 IN
 */
static wBool_t ComparePaths( PATHPTR_T pPath1, PATHPTR_T pPath2 )
{
	wBool_t bMatch=FALSE;
	static dynArr_t subPaths1_da;
	DYNARR_RESET( PATHPTR_T, subPaths1_da );
	LOG( log_genpaths, 6, ( "Compare Paths\n Old Paths\n" ) );
	DecomposePath( &subPaths1_da, pPath1 );
	static dynArr_t subPaths2_da;
	DYNARR_RESET( PATHPTR_T, subPaths2_da );
	LOG( log_genpaths, 6, ( " New Paths\n" ) );
	DecomposePath( &subPaths2_da, pPath2 );

	if ( subPaths1_da.cnt != subPaths2_da.cnt )
		goto DONE;
	for ( int inx1 = 0; inx1 < subPaths1_da.cnt; inx1++ ) {
		if ( 0 != CmpPath3Ptr( 
				&DYNARR_N( dynArr_t, subPaths1_da, inx1 ),
				&DYNARR_N( dynArr_t, subPaths2_da, inx1 ) ) )
			goto DONE;
	}
					
	bMatch = TRUE;
DONE:
	DYNARR_FREE( PATHPTR_T, subPaths1_da );
	DYNARR_FREE( PATHPTR_T, subPaths2_da );
	return bMatch;
}



static void DecomposePath( dynArr_t * subPath_daP, PATHPTR_T pPath )
{
	memset( subPath_daP, 0, sizeof subPath_daP );
	while ( *pPath ) {
		pPath = pPath + strlen(pPath) + 1;
		DYNARR_APPEND( dynArr_t, *subPath_daP, 10 );
		dynArr_t * pPath_daP = &DYNARR_LAST( dynArr_t, *subPath_daP );
		memset( pPath_daP, 0, sizeof *(dynArr_t*)NULL );
		while ( *pPath ) {
			DYNARR_APPEND( PATHPTR_T, *pPath_daP, 10 );
			DYNARR_LAST( PATHPTR_T, *pPath_daP ) = pPath;
			for ( ; *pPath ; pPath++ );
			pPath++;
		}
		qsort( pPath_daP->ptr, pPath_daP->cnt, sizeof *(PATHPTR_T*)NULL, CmpPath1Ptr );
		pPath++;
	}
	qsort( subPath_daP->ptr, subPath_daP->cnt, sizeof *(dynArr_t*)NULL, CmpPath2Ptr );

	if ( log_genpaths >= 1 && logTable(log_genpaths).level >= 6 ) {
		for ( int inx = 0; inx < subPath_daP->cnt; inx++) {
			LogPrintf( "  Path[%d]:\n", inx );
			dynArr_t *daP = &DYNARR_N( dynArr_t, *subPath_daP, inx );
			for ( int inx2 = 0; inx2 < daP->cnt; inx2++ ) {
				LogPrintf( "   Sub[%d]:", inx2 );
				PATHPTR_T pp = DYNARR_N( PATHPTR_T, *daP, inx2 );
				for ( ; *pp; pp++ )
					LogPrintf( " %d", *pp );
				LogPrintf( "\n" );
			}
		}
	}
}

static int CmpPath1Ptr( const void * ptr1, const void * ptr2 )
{
	PATHPTR_T pp1 = *(PATHPTR_T*)ptr1;
	PATHPTR_T pp2 = *(PATHPTR_T*)ptr2;
	for ( ; *pp1 && *pp2 && *pp1 == *pp2; pp1++,pp2++ );
	return *pp1 - *pp2;
}


static int CmpPath2Ptr( const void * ptr1, const void * ptr2 )
{
	dynArr_t * daP1 = (dynArr_t*)ptr1;
	dynArr_t * daP2 = (dynArr_t*)ptr2;
	if ( daP1->cnt != daP2->cnt )
		return daP1->cnt - daP2->cnt;
	for ( int inx = 0; inx<daP1->cnt; inx++ ) {
		PATHPTR_T pp1 = DYNARR_N( PATHPTR_T, *daP1, inx );
		PATHPTR_T pp2 = DYNARR_N( PATHPTR_T, *daP2, inx );
		for ( ; ( pp1[0] || pp1[1] ) && ( pp2[0] || pp2[1] ) && ( *pp1 == *pp2 ); pp1++,pp2++ );
		if ( *pp1 != *pp2 )
			return *pp1 - *pp2;
	}
	return 0;
}


static int CmpPath3Ptr( const void * ptr1, const void * ptr2 )
{
	dynArr_t * daP1 = (dynArr_t*)ptr1;
	dynArr_t * daP2 = (dynArr_t*)ptr2;
	if ( daP1->cnt != daP2->cnt )
		return daP1->cnt - daP2->cnt;
	for ( int inx = 0; inx<daP1->cnt; inx++ ) {
		PATHPTR_T pp1 = DYNARR_N( PATHPTR_T, *daP1, inx );
		PATHPTR_T pp2 = DYNARR_N( PATHPTR_T, *daP2, inx );
		for ( ; pp1[0] && pp2[0] && ( *pp1 == *pp2 ); pp1++,pp2++ );
		if ( *pp1 != *pp2 )
			return *pp1 - *pp2;
	}
	return 0;
}



/*****************************************************************************
 *
 * Object dumps
 *
 */


static void DumpPaths( PATHPTR_T pPath )
{
	for ( ; *pPath; ) {
		LogPrintf( "	P \"%s\"", pPath );
		for ( pPath += strlen( pPath ) + 1; pPath[0] || pPath[1]; pPath++ )
			LogPrintf( " %d", pPath[0] );
		LogPrintf( "\n" );
		pPath += 2;
	}
}


static void DumpSubPath( wIndex_t inx )
{
	PATHELEM_T * path = subPaths( inx );
	if ( path == NULL )
		return;
	LogPrintf( "+ SP%d:", inx+1 );
	for ( ; path[0] || path[1]; path++ ) {
		PATHELEM_T elem = *path;
		if ( elem == 0 )
			LogPrintf( " ??0" );
		else if ( elem < 0 )
			LogPrintf( " E%ld", -elem );
		else 
			LogPrintf(" S%ld.%ld.%ld", elem&0x1, elem>>1, 1-(elem&0x1) );
	}
	LogPrintf( "\n" );
}

#endif
