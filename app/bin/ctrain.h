/** \file ctrain.h
 * Definitions and prototypes for train operations
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

#ifndef HAVE_CTRAIN_H
#define HAVE_CTRAIN_H

#include "common.h"
#include "include/paramfile.h"
#include "track.h"

extern wIndex_t trainCmdInx;

struct carItem_t;
typedef struct carItem_t carItem_t;
typedef carItem_t * carItem_p;
typedef struct {
		coOrd pos;
		ANGLE_T angle;
		} vector_t;

extern carItem_p currCarItemPtr;
extern wControl_p newCarControls[2];
void DoCarDlg( void );
BOOL_T CarItemRead( char * );
track_p NewCar( wIndex_t, carItem_p, coOrd, ANGLE_T );
void CarGetPos( track_p, coOrd *, ANGLE_T * );
void CarSetVisible( track_p );
void CarItemUpdate( carItem_p );
void CarItemLoadList( void * );
char * CarItemDescribe( carItem_p, long, long * );
void CarItemFindCouplerMountPoint( carItem_p, traverseTrack_t, coOrd[2] );
void CarItemSize( carItem_p, coOrd * );
char * CarItemNumber( carItem_p );
DIST_T CarItemCoupledLength( carItem_p );
BOOL_T CarItemIsLoco( carItem_p );
BOOL_T CarItemIsLocoMaster( carItem_p );
void CarItemSetLocoMaster( carItem_p, BOOL_T );
void CarItemSetTrack( carItem_p, track_p );
void CarItemPlace( carItem_p, traverseTrack_p, DIST_T * );
void CarItemDraw( drawCmd_p, carItem_p, wDrawColor, int, BOOL_T, vector_t *, BOOL_T, track_p );
BOOL_T StoreCarItem (carItem_p item, void **data,long *len);
BOOL_T ReplayCarItem(carItem_p item, void *data,long len);
enum paramFileState	GetCarPartCompatibility(int paramFileIndex, SCALEINX_T scaleIndex);
enum paramFileState	GetCarProtoCompatibility(int paramFileIndex, SCALEINX_T scaleIndex);
int CarAvailableCount( void );
BOOL_T TraverseTrack2( traverseTrack_p, DIST_T );
void FlipTraverseTrack( traverseTrack_p );
void CheckCarTraverse( track_p trk);
void DeleteCarProto(int fileIndex);
void DeleteCarPart(int fileIndex);

#endif // !HAVE_CTRAIN_H
