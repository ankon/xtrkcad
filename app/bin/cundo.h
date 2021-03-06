/** \file cundo.h
 * Function prototypes for undo  functionality
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

#ifndef HAVE_CUNDO_H
#define HAVE_CUNDO_H

#include "common.h"
#include "track.h"

int UndoUndo( void );
int UndoRedo( void );
void UndoResume( void );
void UndoSuspend( void );
void UndoStart( char *, char *, ... );
BOOL_T UndoModify( track_p );
BOOL_T UndoDelete( track_p );
BOOL_T UndoNew( track_p );
void UndoEnd( void );
void UndoClear( void );

#endif // !HAVE_CUNDO_H
