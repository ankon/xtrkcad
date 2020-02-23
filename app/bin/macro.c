/** \file macro.c
 * Macros
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

#include <assert.h>
#include <stdlib.h>
#include <stdio.h>
#ifndef WINDOWS
#include <unistd.h>
#include <dirent.h>
#include <errno.h>
#endif
#include <math.h>
#include <ctype.h>
#include <string.h>
#include <time.h>
#ifdef WINDOWS
#include <io.h>
#include <windows.h>
#else
#include <sys/stat.h>
#endif
#include <stdarg.h>
#ifndef WINDOWS
#include <sys/time.h>
#else
#include <sys/timeb.h>
#endif
#include <locale.h>

#include <stdint.h>

#include "common.h"
#include "compound.h"
#include "cundo.h"
#include "custom.h"
#include "draw.h"
#include "fileio.h"
#include "i18n.h"
#include "messages.h"
#include "misc.h"
#include "param.h" 
#include "paths.h"
#include "track.h"
#include "trackx.h"
#include "utility.h"
#include "version.h"

EXPORT long adjTimer;
static void DemoInitValues( void );

extern char *userLocale;
static int log_playbackCursor = 0;



/*****************************************************************************
 *
 * RECORD
 *
 */

EXPORT FILE * recordF;
static wWin_p recordW;
struct wFilSel_t * recordFile_fs;
static BOOL_T recordMouseMoves = TRUE;

static void DoRecordButton( void * context );
static paramTextData_t recordTextData = { 50, 16 };
static paramData_t recordPLs[] = {
#define I_RECSTOP		(0)
#define recStopB		((wButton_p)recordPLs[I_RECSTOP].control)
	{   PD_BUTTON, (void*)DoRecordButton, "stop", PDO_NORECORD, NULL, N_("Stop"), 0, (void*)0 },
#define I_RECMESSAGE	(1)
#define recMsgB ((wButton_p)recordPLs[I_RECMESSAGE].control)
	{   PD_BUTTON, (void*)DoRecordButton, "message", PDO_NORECORD|PDO_DLGHORZ, NULL, N_("Message"), 0, (void*)2 },
#define I_RECEND		(2)
#define recEndB ((wButton_p)recordPLs[I_RECEND].control)
	{   PD_BUTTON, (void*)DoRecordButton, "end", PDO_NORECORD|PDO_DLGHORZ, NULL, N_("End"), BO_DISABLED, (void*)4 },
#define I_RECTEXT		(3)
#define recordT			((wText_p)recordPLs[I_RECTEXT].control)
	{   PD_TEXT, NULL, "text", PDO_NORECORD|PDO_DLGRESIZE, &recordTextData, NULL, BT_CHARUNITS|BO_READONLY} };
static paramGroup_t recordPG = { "record", 0, recordPLs, sizeof recordPLs/sizeof recordPLs[0] };


#ifndef WINDOWS
static struct timeval lastTim = {0,0};
static void ComputePause( void )
{
	struct timeval tim;
	long secs;
	long msecs;
	gettimeofday( &tim, NULL );
	secs = tim.tv_sec-lastTim.tv_sec;
	if (secs > 10 || secs < 0)
		return;
	msecs = secs * 1000 + (tim.tv_usec - lastTim.tv_usec)/1000;
	if (msecs > 5000)
		msecs = 5000;
	if (msecs > 1)
		fprintf( recordF, "PAUSE %ld\n", msecs );
	lastTim = tim;
}
#else
static struct _timeb lastTim;
static void ComputePause( void )
{
	struct _timeb tim;
	long secs, msecs;
	_ftime( &tim );
	secs = (long)(tim.time - lastTim.time);
	if (secs > 10 || secs < 0)
		return;
	msecs = secs * 1000;
	if (tim.millitm >= lastTim.millitm) {
		msecs += (tim.millitm - lastTim.millitm);
	} else {
		msecs -= (lastTim.millitm - tim.millitm);
	}
	if (msecs > 5000)
		msecs = 5000;
	if (msecs > 1)
		fprintf( recordF, "PAUSE %ld\n", msecs );
	lastTim = tim;
}
#endif


EXPORT void RecordMouse( char * name, wAction_t action, POS_T px, POS_T py )
{
	int keyState;
	if ( action == C_MOVE || action == C_RMOVE || (action&0xFF) == C_TEXT )
		ComputePause();
	else if ( action == C_DOWN || action == C_RDOWN )
#ifndef WINDOWS
		gettimeofday( &lastTim, NULL );
#else
		_ftime( &lastTim );
#endif
	if (action == wActionMove && !recordMouseMoves)
		return;
	keyState = wGetKeyState();
	if (keyState)
		fprintf( recordF, "KEYSTATE %d\n", keyState );
	fprintf( recordF, "%s %d %0.3f %0.3f\n", name, (int)action, px, py );
	fflush( recordF );
}


static int StartRecord( int cnt, char ** pathName, void * context )
{
	time_t clock;

	assert( pathName != NULL );
	assert( cnt == 1 );

	SetCurrentPath( MACROPATHKEY, pathName[0] );
	recordF = fopen(pathName[0], "w");
	if (recordF==NULL) {
		NoticeMessage( MSG_OPEN_FAIL, _("Continue"), NULL, _("Recording"), pathName[0], strerror(errno) );
		return FALSE;
	}
	time(&clock);
	fprintf(recordF, "# %s Version: %s, Date: %s\n", sProdName, sVersion, ctime(&clock) );
	fprintf(recordF, "VERSION %d\n", iParamVersion );
	fprintf(recordF, "ROOMSIZE %0.1f x %0.1f\n", mapD.size.x, mapD.size.y );
	fprintf(recordF, "SCALE %s\n", curScaleName );
	fprintf(recordF, "ORIG %0.3f %0.3f %0.3f\n", mainD.scale, mainD.orig.x, mainD.orig.y );
	if ( logClock != 0 )
		fprintf( recordF, "# LOG CLOCK %s\n", ctime(&logClock) );
	if ( logTable_da.cnt > 11 )
		lprintf( "StartRecord( %s ) @ %s\n", pathName, ctime(&clock) );
	ParamStartRecord();
	WriteTracks( recordF, TRUE );
	WriteLayers( recordF );
	fprintf( recordF, "REDRAW\n" );
	fflush( recordF );
	wTextClear( recordT );
	wShow( recordW );
	Reset();
	wControlActive( (wControl_p)recEndB, FALSE );
	return TRUE;
}


static void DoRecordButton( void * context )
{
	static wBool_t recordingMessage = FALSE;
	char * cp;
	int len;

	switch( (int)(long)context ){
	case 0: /* Stop */
		fprintf( recordF, "CLEAR\nMESSAGE\n");
		fprintf( recordF, N_("End of Playback.  Hit Step to exit\n"));
		fprintf( recordF, "END\nSTEP\n" );
		fclose( recordF );
		recordF = NULL;
		wHide( recordW );
		break;

	case 1: /* Step */
		fprintf( recordF, "STEP\n" );
		break;

	case 4: /* End */
		if (recordingMessage) {
			len = wTextGetSize( recordT );
			if (len == 0)
				break;
			cp = (char*)MyMalloc( len+2 );
			wTextGetText( recordT, cp, len );
			if ( cp[len-1] == '\n' ) len--;
			cp[len] = '\0';
			fprintf( recordF, "%s\nEND\nSTEP\n", cp );
			MyFree( cp );
			recordingMessage = FALSE;
		}
		wTextSetReadonly( recordT, TRUE );
		fflush( recordF );
		wControlActive( (wControl_p)recStopB, TRUE );
		wControlActive( (wControl_p)recMsgB, TRUE );
		wControlActive( (wControl_p)recEndB, FALSE );
		wWinSetBusy( mainW, FALSE );
		break;

	case 2: /* Message */
		fprintf( recordF, "MESSAGE\n" );
		wTextSetReadonly( recordT, FALSE );
		wTextClear( recordT );
		recordingMessage = TRUE;
		wControlActive( (wControl_p)recStopB, FALSE );
		wControlActive( (wControl_p)recMsgB, FALSE );
		wControlActive( (wControl_p)recEndB, TRUE );
		wWinSetBusy( mainW, TRUE );
		break;

	case 3: /* Pause */
		fprintf( recordF, "BIGPAUSE\n" );
		fflush( recordF );
		break;

	case 5: /* CLEAR */
		fprintf( recordF, "CLEAR\n" );
		fflush( recordF );
		wTextClear( recordT );
		break;

	default:
		;
	}
}



EXPORT void DoRecord( void * context )
{
	if (recordW == NULL) {
		char * title = MakeWindowTitle(_("Record"));
		recordW = ParamCreateDialog( &recordPG, title, NULL, NULL, NULL, FALSE, NULL, F_RESIZE, NULL );
		recordFile_fs = wFilSelCreate( mainW, FS_SAVE, 0, title, sRecordFilePattern, StartRecord, NULL );
	}
	wTextClear( recordT );
	wFilSelect( recordFile_fs, GetCurrentPath(MACROPATHKEY ));
}

/*****************************************************************************
 *
 * PLAYBACK MOUSE
 *
 */

static wDrawBitMap_p playbackBm = NULL;
static wDrawColor playbackColor;
static drawCmd_p playbackD;
static wPos_t playbackX, playbackY;

#include "bitmaps/arrow0.xbm"
#include "bitmaps/arrow0_shift.xbm"
#include "bitmaps/arrow0_ctl.xbm"
#include "bitmaps/arrow3.xbm"
#include "bitmaps/arrow3_shift.xbm"
#include "bitmaps/arrow3_ctl.xbm"
#include "bitmaps/arrows.xbm"
#include "bitmaps/arrowr3.xbm"
#include "bitmaps/arrowr3_shift.xbm"
#include "bitmaps/arrowr3_ctl.xbm"
#include "bitmaps/flash.xbm"

static wDrawColor rightDragColor;
static wDrawColor leftDragColor;
static wDrawBitMap_p arrow0_bm;
static wDrawBitMap_p arrow0_shift_bm;
static wDrawBitMap_p arrow0_ctl_bm;
static wDrawBitMap_p arrow3_bm;
static wDrawBitMap_p arrow3_shift_bm;
static wDrawBitMap_p arrow3_ctl_bm;
static wDrawBitMap_p arrows_bm;
static wDrawBitMap_p arrowr3_bm;
static wDrawBitMap_p arrowr3_shift_bm;
static wDrawBitMap_p arrowr3_ctl_bm;
static wDrawBitMap_p flash_bm;

static long flashTO = 120;
static DIST_T PixelsPerStep = 5;
static long stepTO = 100;
EXPORT unsigned long playbackTimer;

static wBool_t didPause;
static wBool_t flashTwice = FALSE;

int DBMCount=0;

#define DRAWALL
typedef enum { FLASH_PLUS, FLASH_MINUS, REDRAW, CLEAR, DRAW, RESET, ORIG, MOVE_PLYBCK1, MOVE_PLYBCK2, MOVE_PLYBCK3, MOVE_PLYBCK4, QUIT } DrawBitMap_e;

char * DrawBitMapToString(DrawBitMap_e dbm) {
	switch(dbm) {
	case FLASH_PLUS:
		return "Flsh+";
	case FLASH_MINUS:
		return "Flsh-";
	case REDRAW:
		return "Redraw";
	case CLEAR:
		return "Clr";
	case DRAW:
		return "Draw";
	case RESET:
		return "RESET";
	case ORIG:
		return "ORIG";
	case MOVE_PLYBCK1:
		return "MPBC1";
	case MOVE_PLYBCK2:
		return "MPBC2";
	case MOVE_PLYBCK3:
		return "MPBC3";
	case MOVE_PLYBCK4:
		return "MPBC4";
	case QUIT:
		return "Quit";
	default:
		return "";
	}
}

static void MacroDrawBitMap(
		DrawBitMap_e dbm,
		drawCmd_p d,
		wDrawBitMap_p bm,
		wPos_t x,
		wPos_t y,
		wDrawColor color )
{
	wBool_t ret = wDrawSetTempMode( d->d, TRUE );
	wDrawBitMap( d->d, bm, x, y, color, wDrawOptTemp|wDrawOptNoClip );
	wDrawSetTempMode( d->d, ret );
	wFlush();

	LOG( log_playbackCursor, 1, ("%s %d DrawBitMap( %p %p %d %d %d %d )\n", DrawBitMapToString(dbm), DBMCount++, d->d, bm, x, y, color, wDrawOptTemp|wDrawOptNoClip ) );
}


static void Flash( drawCmd_p d, wPos_t x, wPos_t y, wDrawColor flashColor )
{
	if (playbackTimer != 0)
		return;

	MacroDrawBitMap( FLASH_PLUS, d, flash_bm, x, y, flashColor );
	wPause( flashTO*2 );
}


EXPORT long playbackDelay = 100;
static long playbackSpeed = 2;

static void SetPlaybackSpeed(
		wIndex_t inx )
{
	switch (inx) {
	case 0: playbackDelay = 500; break;
	case 1: playbackDelay = 200; break;
	default:
	case 2: playbackDelay = 100; break;
	case 3: playbackDelay = 50; break;
	case 4: playbackDelay = 15; break;
	case 5: playbackDelay = 0; break;
	}
	playbackSpeed = inx;
}

static void ClearPlaybackCursor( BOOL_T quit )
{
	playbackBm = NULL;
}

static void DrawPlaybackCursor(
		drawCmd_p d,
		wDrawBitMap_p bm,
		wPos_t xx,
		wPos_t yy,
		wDrawColor color )
{
	if ( playbackBm )
		printf( "DrawPlayBack: playbackBm not null\n" );
	MacroDrawBitMap( DRAW,  playbackD=d, playbackBm=bm, playbackX=xx, playbackY=yy, playbackColor=color );
}


EXPORT void RedrawPlaybackCursor() {
	if ( playbackBm && inPlayback)
		MacroDrawBitMap( DRAW,  playbackD, playbackBm, playbackX, playbackY, playbackColor );
}

static void MoveCursor(
		drawCmd_p d,
		playbackProc proc,
		wAction_t action,
		coOrd pos,
		wDrawBitMap_p bm,
		wDrawColor color )
{
	DIST_T dist, dx, dy;
	coOrd pos1, dpos;
	int i, steps;
	wPos_t x, y;
	wPos_t xx, yy;
	wPos_t x0=playbackX;
	wPos_t y0=playbackY;

	if (d == NULL)
		return;

	d->CoOrd2Pix( d, pos, &x, &y );

	if (playbackTimer == 0 && playbackD == d /*&& !didPause*/) {
		dx = (DIST_T)(x-x0);
		dy = (DIST_T)(y-y0);
		dist = sqrt( dx*dx + dy*dy );
		steps = (int)(dist / PixelsPerStep ) + 1;
		dx /= steps;
		dy /= steps;
		d->Pix2CoOrd( d, x0, y0, &pos1 );
		dpos.x = (pos.x-pos1.x)/steps;
		dpos.y = (pos.y-pos1.y)/steps;

		for ( i=1; i<=steps; i++ ) {

			ClearPlaybackCursor(FALSE);

			xx = x0+(wPos_t)(i*dx);
			yy = y0+(wPos_t)(i*dy);

			pos1.x += dpos.x;
			pos1.y += dpos.y;
			if ( proc != NULL ) {
				proc( action, pos1 );
			} else {
				TempRedraw();
			}
			DrawPlaybackCursor( d, bm, xx, yy, color );
			if ( d->d == mainD.d ) {
				InfoPos( pos1 );
				wFlush();
			}
			// Simple mouse moves happen twice as fast
			wPause( stepTO*playbackDelay/100/(action==wActionMove?2:1) );


			if (!inPlayback) {
				ClearPlaybackCursor(FALSE);
				return;
			}
		}
	} else {
		ClearPlaybackCursor(FALSE);
		DrawPlaybackCursor( d, bm, x, y, color );
	}
}


static void PlaybackCursor(
		drawCmd_p d,
		playbackProc proc,
		wAction_t action,
		coOrd pos,
		wDrawColor color )
{
	wDrawBitMap_p bm;
	wPos_t x, y;
	long time0, time1;

	time0 = wGetTimer();

	d->CoOrd2Pix( d, pos, &x, &y );



	switch( action&0xFF ) {

	case wActionMove:
		bm = ((MyGetKeyState()&WKEY_SHIFT)?arrow0_shift_bm:(MyGetKeyState()&WKEY_CTRL)?arrow0_ctl_bm:arrow0_bm); //0 is normal, shift, ctrl
		MoveCursor( d, proc, wActionMove, pos, bm, wDrawColorBlack );
		break;

	case C_DOWN:
		bm = ((MyGetKeyState()&WKEY_SHIFT)?arrow0_shift_bm:(MyGetKeyState()&WKEY_CTRL)?arrow0_ctl_bm:arrow0_bm);
		MoveCursor( d, proc, wActionMove, pos, bm, wDrawColorBlack );  //Go to spot
		bm = ((MyGetKeyState()&WKEY_SHIFT)?arrow3_shift_bm:(MyGetKeyState()&WKEY_CTRL)?arrow3_ctl_bm:arrow3_bm);
		if (flashTwice) Flash( d, x, y, rightDragColor );
		Flash( d, x, y, playbackColor=rightDragColor );
		proc( action, pos );
		/* no break */

	case C_MOVE:
		bm = ((MyGetKeyState()&WKEY_SHIFT)?arrow3_shift_bm:(MyGetKeyState()&WKEY_CTRL)?arrow3_ctl_bm:arrow3_bm);
		MoveCursor( d, proc, C_MOVE, pos, bm, rightDragColor );
		break;

	case C_UP:
		bm = ((MyGetKeyState()&WKEY_SHIFT)?arrow3_shift_bm:(MyGetKeyState()&WKEY_CTRL)?arrow3_ctl_bm:arrow0_bm);
		MoveCursor( d, proc, C_MOVE, pos, bm, rightDragColor );
		if (flashTwice) Flash( d, x, y, rightDragColor );
		Flash( d, x, y, rightDragColor );
		proc( action, pos );
		bm = ((MyGetKeyState()&WKEY_SHIFT)?arrow0_shift_bm:(MyGetKeyState()&WKEY_CTRL)?arrow0_ctl_bm:arrow0_bm);
		MoveCursor( d, NULL, 0, pos, bm, wDrawColorBlack );
		break;

	case C_RDOWN:
		bm = ((MyGetKeyState()&WKEY_SHIFT)?arrow0_shift_bm:(MyGetKeyState()&WKEY_CTRL)?arrow0_ctl_bm:arrow0_bm);
		MoveCursor( d, proc, wActionMove, pos, bm, wDrawColorBlack );  //Go to spot
		bm = ((MyGetKeyState()&WKEY_SHIFT)?arrowr3_shift_bm:(MyGetKeyState()&WKEY_CTRL)?arrowr3_ctl_bm:arrowr3_bm);
		if (flashTwice) Flash( d, x, y, leftDragColor );
		Flash( d, x, y, playbackColor=leftDragColor );
		proc( action, pos );
		/* no break */

	case C_RMOVE:
		bm = ((MyGetKeyState()&WKEY_SHIFT)?arrowr3_shift_bm:(MyGetKeyState()&WKEY_CTRL)?arrowr3_ctl_bm:arrowr3_bm);
		MoveCursor( d, proc, C_RMOVE, pos, bm, leftDragColor );
		break;

	case C_RUP:
		bm = ((MyGetKeyState()&WKEY_SHIFT)?arrowr3_shift_bm:(MyGetKeyState()&WKEY_CTRL)?arrowr3_ctl_bm:arrowr3_bm);
		MoveCursor( d, proc, C_RMOVE, pos, bm, leftDragColor );
		if (flashTwice) Flash( d, x, y, leftDragColor );
		Flash( d, x, y, leftDragColor );
		proc( action, pos );
		bm = ((MyGetKeyState()&WKEY_SHIFT)?arrow0_shift_bm:(MyGetKeyState()&WKEY_CTRL)?arrow0_ctl_bm:arrow0_bm);
		MoveCursor( d, NULL, 0, pos, bm, wDrawColorBlack );
		break;

	case C_REDRAW:
		proc( action, pos );																	//Send Redraw to functions
		MacroDrawBitMap( REDRAW, playbackD, playbackBm, playbackX, playbackY, playbackColor );
		break;

	case C_TEXT:
		proc( action, pos);
		char c = action>>8;
		bm = playbackBm;
		break;


	default:
		bm = playbackBm;
	}

	playbackBm = bm;
	time1 = wGetTimer();
	adjTimer += (time1-time0);
}


EXPORT void PlaybackMouse(
		playbackProc proc,
		drawCmd_p d,
		wAction_t action,
		coOrd pos,
		wDrawColor color )
{
	PlaybackCursor( d, proc, action, pos, wDrawColorBlack );
	didPause = FALSE;
}


EXPORT void MovePlaybackCursor(
		drawCmd_p d,
		wPos_t x,
		wPos_t y, wBool_t direct, wControl_p control)
{
	coOrd pos;
	d->Pix2CoOrd( d, x, y, &pos );
	d->CoOrd2Pix( d, pos, &x, &y );
	if (!direct)
		MoveCursor( d, NULL, wActionMove, pos, arrow0_bm, wDrawColorBlack );
	else
		ClearPlaybackCursor(FALSE);
	MacroDrawBitMap( MOVE_PLYBCK1, d, arrow0_bm, x, y, wDrawColorBlack );
	MacroDrawBitMap( MOVE_PLYBCK2, d, arrow3_bm, x, y, rightDragColor );

	Flash( d, x, y, rightDragColor );
	if (direct) {
		wControlHilite(control,TRUE);
	}
	MacroDrawBitMap( MOVE_PLYBCK3, d, arrow3_bm, x, y, rightDragColor );
	MacroDrawBitMap( MOVE_PLYBCK4, d, arrow0_bm, x, y, wDrawColorBlack );
	if (direct) {
		wPause(1000);
		wControlHilite(control,FALSE);
	}
}

/*****************************************************************************
 *
 * PLAYBACK
 *
 */

EXPORT wBool_t inPlayback;
EXPORT wBool_t inPlaybackQuit;
EXPORT wWin_p demoW;
EXPORT int curDemo = -1;

typedef struct {
		char * title;
		char * fileName;
		} demoList_t;
static dynArr_t demoList_da;
#define demoList(N) DYNARR_N( demoList_t, demoList_da, N )
static struct wFilSel_t * playbackFile_fs;

typedef struct {
		char * label;
		playbackProc_p proc;
		void * data;
		} playbackProc_t;
static dynArr_t playbackProc_da;
#define playbackProc(N) DYNARR_N( playbackProc_t, playbackProc_da, N )

static coOrd oldRoomSize;
static coOrd oldMainOrig;
static coOrd oldMainSize;
static DIST_T oldMainScale;
static char * oldScaleName;

static wBool_t pauseDemo = FALSE;
static long bigPause = 2000;
#ifdef DEMOPAUSE
static wButton_p demoPause;
#endif
static BOOL_T playbackNonStop = FALSE;

static BOOL_T showParamLineNum = FALSE;

static int playbackKeyState;

static void DoDemoButton( void * context );
static paramTextData_t demoTextData = { 50, 16 };
static paramData_t demoPLs[] = {
#define I_DEMOSTEP		(0)
#define demoStep		((wButton_p)demoPLs[I_DEMOSTEP].control)
	{   PD_BUTTON, (void*)DoDemoButton, "step", PDO_NORECORD, NULL, N_("Step"), BB_DEFAULT, (void*)0 },
#define I_DEMONEXT		(1)
#define demoNext		((wButton_p)demoPLs[I_DEMONEXT].control)
	{   PD_BUTTON, (void*)DoDemoButton, "next", PDO_NORECORD|PDO_DLGHORZ, NULL, N_("Next"), 0, (void*)1 },
#define I_DEMOQUIT		(2)
#define demoQuit		((wButton_p)demoPLs[I_DEMOQUIT].control)
	{   PD_BUTTON, (void*)DoDemoButton, "quit", PDO_NORECORD|PDO_DLGHORZ, NULL, N_("Quit"), BB_CANCEL, (void*)3 },
#define I_DEMOSPEED		(3)
#define demoSpeedL		((wList_p)demoPLs[I_DEMOSPEED].control)
	{   PD_DROPLIST, &playbackSpeed, "speed", PDO_NORECORD|PDO_LISTINDEX|PDO_DLGHORZ, (void*)80, N_("Speed") },
#define I_DEMOTEXT		(4)
#define demoT			((wText_p)demoPLs[I_DEMOTEXT].control)
	{   PD_TEXT, NULL, "text", PDO_NORECORD|PDO_DLGRESIZE, &demoTextData, NULL, BT_CHARUNITS|BO_READONLY} };
static paramGroup_t demoPG = { "demo", 0, demoPLs, sizeof demoPLs/sizeof demoPLs[0] };

EXPORT int MyGetKeyState( void )
{
	if (inPlayback)
		return playbackKeyState;
	else
		return wGetKeyState();
}


EXPORT void AddPlaybackProc( char * label, playbackProc_p proc, void * data )
{
	DYNARR_APPEND( playbackProc_t, playbackProc_da, 10 );
	playbackProc(playbackProc_da.cnt-1).label = MyStrdup(label);
	playbackProc(playbackProc_da.cnt-1).proc = proc;
	playbackProc(playbackProc_da.cnt-1).data = data;
}


static void PlaybackQuit( void )
{
	long playbackSpeed1 = playbackSpeed;
	if (paramFile)
		fclose( paramFile );
	paramFile = NULL;
	if (!inPlayback)
		return;
	inPlaybackQuit = TRUE;
	ClearPlaybackCursor(TRUE);
	wPrefReset();
	wHide( demoW );
	wWinSetBusy( mainW, FALSE );
	wWinSetBusy( mapW, FALSE );
	ParamRestoreAll();
	RestoreLayers();
	wEnableBalloonHelp( (int)enableBalloonHelp );
	mainD.scale = oldMainScale;
	mainD.size = oldMainSize;
	mainD.orig = oldMainOrig;
	SetRoomSize( oldRoomSize );
	tempD.orig = mainD.orig;
	tempD.size = mainD.size;
	tempD.scale = mainD.scale;
	Reset();
	ClearTracks();
	checkPtMark = changed = 0;
	RestoreTrackState();
	inPlaybackQuit = FALSE;
	DoSetScale( oldScaleName );
	DoChangeNotification( CHANGE_ALL );
	CloseDemoWindows();
	inPlayback = FALSE;
	curDemo = -1;
	wPrefSetInteger( "misc", "playbackspeed", playbackSpeed );
	playbackNonStop = FALSE;
	playbackSpeed = playbackSpeed1;
	UndoResume();
	wWinBlockEnable( TRUE );
}


static int documentEnable = 0;
static int documentAutoSnapshot = 0;

static drawCmd_t snapshot_d = {
		NULL,
		&screenDrawFuncs,
		0,
		16.0,
		0,
		{0.0, 0.0}, {1.0, 1.0},
		Pix2CoOrd, CoOrd2Pix };
static int documentSnapshotNum = 1;
static int documentCopy = 0;
static FILE * documentFile;
static BOOL_T snapshotMouse = FALSE;

EXPORT void TakeSnapshot( drawCmd_t * d )
{
	char * cp;
	wPos_t ix, iy;
	if (d->dpi < 0)
		d->dpi = mainD.dpi;
	if (d->scale < 0)
		d->scale = mainD.scale;
	if (d->orig.x < 0 || d->orig.y < 0)
		d->orig = mainD.orig;
	if (d->size.x < 0 || d->size.y < 0)
		d->size = mainD.size;
	ix = (wPos_t)(d->dpi*d->size.x/d->scale);
	iy = (wPos_t)(d->dpi*d->size.y/d->scale);
	d->d = wBitMapCreate( ix, iy, 8 );
	if (d->d == (wDraw_p)0) {
		return;
	}
	DrawTracks( d, d->scale, d->orig, d->size );
	if ( snapshotMouse && playbackBm )
		wDrawBitMap( d->d, playbackBm, playbackX, playbackY, playbackColor, 0 );
	wDrawLine( d->d, 0, 0, ix-1, 0, 0, wDrawLineSolid, wDrawColorBlack, 0 );
	wDrawLine( d->d, ix-1, 0, ix-1, iy-1, 0, wDrawLineSolid, wDrawColorBlack, 0 );
	wDrawLine( d->d, ix-1, iy-1, 0, iy-1, 0, wDrawLineSolid, wDrawColorBlack, 0 );
	wDrawLine( d->d, 0, iy-1, 0, 0, 0, wDrawLineSolid, wDrawColorBlack, 0 );
	strcpy( message, paramFileName );
	cp = message+strlen(message)-4;
	sprintf( cp, "-%4.4d.xpm", documentSnapshotNum );
	wBitMapWriteFile( d->d, message );
	wBitMapDelete( d->d );
	documentSnapshotNum++;
	if (documentCopy && documentFile) {
		cp = FindFilename(message);
		cp[strlen(cp)-4] = 0;
		fprintf( documentFile, "\n?G%s\n", cp );
	}
}

/*
* Regression test
*/
static int log_regression = 0;
wBool_t bWriteEndPtDirectIndex;

void DoRegression( char * sFileName )
{
	typedef enum { REGRESSION_NONE, REGRESSION_CHECK, REGRESSION_QUIET, REGRESSION_SAVE } E_REGRESSION;
	E_REGRESSION eRegression = REGRESSION_NONE;
	long oldParamVersion;
	long regressVersion;
	FILE * fRegression;
	char * sRegressionFile = "xtrkcad.regress";
	wBool_t bWroteActualTracks;
	eRegression = log_regression > 0 ? logTable(log_regression).level : 0;
	char * cp;
	regressVersion = strtol( paramLine+16, &cp, 10 );
	if (cp == paramLine+16 )
		regressVersion = PARAMVERSION;
	LOG( log_regression, 1, ("REGRESSION %s %d %s:%d %s\n",
				eRegression==REGRESSION_SAVE?"SAVE":"CHECK",
				regressVersion,
				sFileName, paramLineNum,
				cp ) );
	switch ( eRegression ){
	case REGRESSION_SAVE:
		fRegression = fopen( sRegressionFile, "a" );
		if ( fRegression == NULL ) {
			NoticeMessage( MSG_OPEN_FAIL, _("Continue"), NULL, _("Regression"), sFileName, strerror(errno) );
		} else {
			fprintf( fRegression, "REGRESSION START %d %s\n",
				PARAMVERSION, cp );
			fprintf( fRegression, "# %s - %d\n", sFileName, paramLineNum );
			WriteTracks( fRegression, FALSE );
			fclose( fRegression );
		}
		while ( fgets(paramLine, STR_LONG_SIZE, paramFile) != NULL ) {
			if ( strncmp( paramLine, "REGRESSION END", 14 ) == 0)
				break;
		}
		break;
	case REGRESSION_CHECK:
	case REGRESSION_QUIET:
		oldParamVersion = paramVersion;
		paramVersion = regressVersion;
		bWroteActualTracks = FALSE;
		track_p to_first_save = to_first;
		track_p* to_last_save = to_last;
		while ( GetNextLine() ) {
			// Read Expected track
			to_first = NULL;
			to_last = &to_first;
			paramVersion = regressVersion;
			if ( !ReadTrack( paramLine ) )
				break;
			track_cp tExpected = to_first;
			to_first = to_first_save;
			// Find corresponding Actual track
			track_cp tActual = FindTrack( GetTrkIndex( tExpected ) );
			strcat( message, "Regression " );
			if ( ! CompareTrack( tActual, tExpected ) ) {
				// Actual doesn't match Expected
				LOG( log_regression, 1, ("  FAIL: %s", message) );
				fRegression = fopen( sRegressionFile, "a" );
				if ( fRegression == NULL ) {
					NoticeMessage( MSG_OPEN_FAIL, _("Continue"), NULL, _("Regression"), sRegressionFile, strerror(errno) );
					break;
				}
				fprintf( fRegression, "REGRESSION FAIL %d\n",
					PARAMVERSION );
				fprintf( fRegression, "# %s - %d\n", sFileName, paramLineNum );
				fprintf( fRegression, "# %s", message );
				if ( !bWroteActualTracks ) {
					// Print Actual tracks
					fprintf( fRegression, "Actual Tracks\n" );
					paramVersion = PARAMVERSION;
					WriteTracks( fRegression, FALSE );
					bWroteActualTracks = TRUE;
				}
				// Print Expected track
				to_first = tExpected;
				fprintf( fRegression, "Expected Track\n" );
				WriteTracks( fRegression, FALSE );
				fclose( fRegression );
				strcat( message, "Continue test?" );
				if ( eRegression == REGRESSION_CHECK ) {
					int rc = wNoticeEx( NT_ERROR, message, _("Stop"), _("Continue") );
					if ( !rc ) {
						while ( GetNextLine() &&
							strncmp( paramLine, "REGRESSION END", 14 ) != 0 )
							;
						break;
					}
				}
			}
			// Delete Expected track
			to_first = tExpected;
			to_last = &to_first;
			FreeTrack( tExpected );
		}
		to_first = to_first_save;
		to_last = to_last_save;
		if ( strncmp( paramLine, "REGRESSION END", 14 ) != 0 )
			InputError( "Expected REGRESSION END", TRUE );
		paramVersion = oldParamVersion;
		break;
	case REGRESSION_NONE:
	default:
		while ( GetNextLine() ) {
			if ( strncmp( paramLine, "REGRESSION END", 14 ) == 0 )
				break;
		}
		break;
	}
}

static void EnableButtons(
		BOOL_T enable )
{
	wButtonSetBusy( demoStep, !enable );
	wButtonSetBusy( demoNext, !enable );
	wControlActive( (wControl_p)demoStep, enable );
	wControlActive( (wControl_p)demoNext, enable );
#ifdef DEMOPAUSE
	wButtonSetBusy( demoPause, enable );
#endif
}

EXPORT void PlaybackMessage(
		char * line )
{
	char * cp;
	wTextAppend( demoT, _(line) );
	if ( documentCopy && documentFile ) {
		if (strncmp(line, "__________", 10) != 0) {
			for (cp=line; *cp; cp++) {
				switch (*cp) {
				case '<':
					fprintf( documentFile, "$B" );
					break;
				case '>':
					fprintf( documentFile, "$" );
					break;
				default:
					fprintf( documentFile, "%c", *cp );
				}
			}
		}
	}
}


static void PlaybackSetup( void )
{
	SaveTrackState();
	EnableButtons( TRUE );
	SetPlaybackSpeed( (wIndex_t)playbackSpeed );
	wListSetIndex( demoSpeedL, (wIndex_t)playbackSpeed );
	wTextClear( demoT );
	wShow( demoW );
	wFlush();
	wPrefFlush();
	wWinSetBusy( mainW, TRUE );
	wWinSetBusy( mapW, TRUE );
	ParamSaveAll();
	paramLineNum = 0;
	oldRoomSize = mapD.size;
	oldMainOrig = mainD.orig;
	oldMainSize = mainD.size;
	oldMainScale = mainD.scale;
	oldScaleName = curScaleName;
	playbackX = 0;
	playbackY = 0;
	Reset();
	paramVersion = -1;
	playbackColor=wDrawColorBlack;
	paramTogglePlaybackHilite = FALSE;
	CompoundClearDemoDefns();
	SaveLayers();
}


static void Playback( void )
{
	POS_T x, y;
	POS_T zoom;
	wIndex_t inx;
	long timeout;
	static enum { pauseCmd, mouseCmd, otherCmd } thisCmd, lastCmd;
	int len;
	static wBool_t demoWinOnTop = FALSE;
	coOrd roomSize;
	char * cp, * cq;
	char *demoFileName = NULL;

	useCurrentLayer = FALSE;
	inPlayback = TRUE;
	EnableButtons( FALSE );
	lastCmd = otherCmd;
	playbackTimer = 0;
	if (demoWinOnTop) {
		wWinTop( mainW );
		demoWinOnTop = FALSE;
	}
	char * oldLocale = NULL;
	oldLocale = SaveLocale( "C" );
	while (TRUE) {
		if ( paramFile == NULL ||
			 fgets(paramLine, STR_LONG_SIZE, paramFile) == NULL ) {
			paramTogglePlaybackHilite = FALSE;
			ClearPlaybackCursor(FALSE);
			CloseDemoWindows();
			if (paramFile) {
				fclose( paramFile );
				paramFile = NULL;
			}
			if (documentFile) {
				fclose( documentFile );
				documentFile = NULL;
			}
			Reset();
			if (curDemo < 0 || curDemo >= demoList_da.cnt)
				break;
			demoFileName = strdup(demoList(curDemo).fileName );
			paramFile = fopen( demoFileName, "r" );
			if ( paramFile == NULL ) {
				NoticeMessage( MSG_OPEN_FAIL, _("Continue"), NULL, _("Demo"), demoFileName, strerror(errno) );
				RestoreLocale( oldLocale );
				return;
			}
			
			playbackColor=wDrawColorBlack;
			paramLineNum = 0;
			wWinSetTitle( demoW, demoList( curDemo ).title );
			curDemo++;
			ClearTracks();
			UndoSuspend();
			wWinBlockEnable( FALSE );
			checkPtMark = 0;
			DoChangeNotification( CHANGE_ALL );
			CompoundClearDemoDefns();
			if ( fgets(paramLine, STR_LONG_SIZE, paramFile) == NULL ) {
				NoticeMessage( MSG_CANT_READ_DEMO, _("Continue"), NULL, sProdName, demoFileName );
				fclose( paramFile );
				paramFile = NULL;
				RestoreLocale( oldLocale );
				return;
			}
			free(demoFileName);
			demoFileName = NULL;
		}
		if (paramLineNum == 0) {
			documentSnapshotNum = 1;
			if (documentEnable) {
				strcpy( message, paramFileName );
				cp = message+strlen(message)-4;
				strcpy( cp, ".hlpsrc" );
				documentFile = fopen( message, "w" );
				documentCopy = TRUE;
			}
		}
		thisCmd = otherCmd;
		paramLineNum++;
		if (showParamLineNum)
			InfoCount( paramLineNum );
		Stripcr( paramLine );
		if (paramLine[0] == '#') {
			/* comment */
		} else if (paramLine[0] == 0) {
			/* empty paramLine */
		} else if (ReadTrack( paramLine ) ) {
		} else if (strncmp( paramLine, "STEP", 5 ) == 0) {
			paramTogglePlaybackHilite = TRUE;
			wWinTop( demoW );
			demoWinOnTop = TRUE;
			didPause = FALSE;
			EnableButtons( TRUE );
			if (!demoWinOnTop) {
				wWinTop( demoW );
				demoWinOnTop = TRUE;
			}
			if ( documentAutoSnapshot ) {
				snapshot_d.dpi=snapshot_d.scale=snapshot_d.orig.x=snapshot_d.orig.y=snapshot_d.size.x=snapshot_d.size.y=-1;
				TakeSnapshot(&snapshot_d);
			}
			if (playbackNonStop) {
				wPause( 1000 );
				EnableButtons( FALSE );
			} else {
				RestoreLocale( oldLocale );
				return;
			}
		} else if (strncmp( paramLine, "CLEAR", 5 ) == 0) {
			wTextClear( demoT );
		} else if (strncmp( paramLine, "MESSAGE", 7 ) == 0) {
			didPause = FALSE;
			wWinTop( demoW );
			demoWinOnTop = TRUE;
			while ( ( fgets( paramLine, STR_LONG_SIZE, paramFile ) ) != NULL ) {
				paramLineNum++;
				if ( strncmp(paramLine, "END", 3) == 0 )
					break;
				if ( strncmp(paramLine, "STEP", 3) == 0 ) {
					wWinTop( demoW );
					demoWinOnTop = TRUE;
					EnableButtons( TRUE );
					RestoreLocale( oldLocale );
					return;
				}
				PlaybackMessage( paramLine );
			}
		} else if (strncmp( paramLine, "ROOMSIZE ", 9 ) == 0) {
			if (ParseRoomSize( paramLine+9, &roomSize ))
				SetRoomSize( roomSize );
		} else if (strncmp( paramLine, "SCALE ", 6 ) == 0) {
			DoSetScale( paramLine+6 );
		} else if (strncmp( paramLine, "REDRAW", 6 ) == 0) {
			ResolveIndex();
			RecomputeElevations();
			DoRedraw();
			/*DoChangeNotification( CHANGE_ALL );*/
			if (playbackD != NULL && playbackBm != NULL)
				MacroDrawBitMap( REDRAW, playbackD, playbackBm, playbackX, playbackY, playbackColor );
		} else if (strncmp( paramLine, "COMMAND ", 8 ) == 0) {
			paramTogglePlaybackHilite = FALSE;
			PlaybackCommand( paramLine, paramLineNum );
		} else if (strncmp( paramLine, "RESET", 5 ) == 0) {
			paramTogglePlaybackHilite = TRUE;
			InfoMessage("Esc Key Pressed");
			ConfirmReset(TRUE);
			if (playbackD != NULL && playbackBm != NULL)
				MacroDrawBitMap( RESET, playbackD, playbackBm, playbackX, playbackY, playbackColor );
		} else if (strncmp( paramLine, "VERSION", 7 ) == 0) {
			paramVersion = atol( paramLine+8 );
			if ( paramVersion > iParamVersion ) {
				NoticeMessage( MSG_PLAYBACK_VERSION_UPGRADE, _("Ok"), NULL, paramVersion, iParamVersion, sProdName );
				break;
			}
			if ( paramVersion < iMinParamVersion ) {
				NoticeMessage( MSG_PLAYBACK_VERSION_DOWNGRADE, _("Ok"), NULL, paramVersion, iMinParamVersion, sProdName );
				break;
			}
		} else if (strncmp( paramLine, "ORIG ", 5 ) == 0) {
			if ( !GetArgs( paramLine+5, "fff", &zoom, &x, &y ) )
				continue;
			if (zoom == 0.0) {
				double scale_x = mapD.size.x/(mainD.size.x/mainD.scale);
				double scale_y = mapD.size.y/(mainD.size.y/mainD.scale);
				if (scale_x<scale_y)
					scale_x = scale_y;
				scale_x = ceil(scale_x);
				if (scale_x < 1) scale_x = 1;
				if (scale_x > MAX_MAIN_SCALE) scale_x = MAX_MAIN_SCALE;
				zoom = scale_x;
			}
			mainD.scale = zoom;
			InfoMessage("Zoom Set to %.0f", zoom);
			mainD.orig.x = x;
			mainD.orig.y = y;
			SetMainSize();
			tempD.orig = mainD.orig;
			tempD.size = mainD.size;
			tempD.scale = mainD.scale;

			DoRedraw();
			if (playbackD != NULL && playbackBm != NULL)
				MacroDrawBitMap( ORIG, playbackD, playbackBm, playbackX, playbackY, playbackColor );

		} else if (strncmp( paramLine, "PAUSE ", 6 ) == 0) {
			paramTogglePlaybackHilite = TRUE;
			didPause = TRUE;

			if ( !GetArgs( paramLine+6, "l", &timeout ) )
				continue;
			if (timeout > 10000)
				timeout = 1000;
			if (playbackTimer == 0)
				wPause( timeout*playbackDelay/100 );
			wFlush();
			if (demoWinOnTop) {
				wWinTop( mainW );
				demoWinOnTop = FALSE;
			}
		} else if (strncmp( paramLine, "BIGPAUSE ", 6 ) == 0) {
			paramTogglePlaybackHilite = TRUE;
			didPause = FALSE;

			if (playbackTimer == 0) {
				timeout = bigPause*playbackDelay/100;
				if (timeout <= dragTimeout)
					timeout = dragTimeout+1;
				wPause( timeout );
			}
		} else if (strncmp( paramLine, "KEYSTATE ", 9 ) == 0 ) {
			playbackKeyState = atoi( paramLine+9 );
		} else if (strncmp( paramLine, "TIMESTART", 9 ) == 0 ) {
			playbackTimer = wGetTimer();
		} else if (strncmp( paramLine, "TIMEEND", 7 ) == 0 ) {
			if (playbackTimer == 0) {
				NoticeMessage( MSG_PLAYBACK_TIMEEND, _("Ok"), NULL );
			} else {
				playbackTimer = wGetTimer() - playbackTimer;
				sprintf( message, _("Elapsed time %lu\n"), playbackTimer );
				wTextAppend( demoT, message );
				playbackTimer = 0;
			}
		} else if (strncmp( paramLine, "MEMSTATS", 8 ) == 0 ) {
			wTextAppend( demoT, wMemStats() );
			wTextAppend( demoT, "\n" );
		} else if (strncmp( paramLine, "SNAPSHOT", 8 ) == 0 ) {
			if ( !documentEnable )
				continue;
			snapshot_d.dpi=snapshot_d.scale=snapshot_d.orig.x=snapshot_d.orig.y=snapshot_d.size.x=snapshot_d.size.y=-1;
			cp = paramLine+8;
			while (*cp && isspace((unsigned char)*cp)) cp++;
			if (snapshot_d.dpi = strtod( cp, &cq ), cp == cq)
				snapshot_d.dpi = -1;
			else if (snapshot_d.scale = strtod( cq, &cp ), cp == cq)
				snapshot_d.scale = -1;
			else if (snapshot_d.orig.x = strtod( cp, &cq ), cp == cq)
				snapshot_d.orig.x = -1;
			else if (snapshot_d.orig.y = strtod( cq, &cp ), cp == cq)
				snapshot_d.orig.y = -1;
			else if (snapshot_d.size.x = strtod( cp, &cq ), cp == cq)
				snapshot_d.size.x = -1;
			else if (snapshot_d.size.y = strtod( cq, &cp ), cp == cq)
				snapshot_d.size.y = -1;
			TakeSnapshot(&snapshot_d);
		} else if (strncmp( paramLine, "DOCUMENT ON", 11 ) == 0 ) {
			documentCopy = documentEnable;
		} else if (strncmp( paramLine, "DOCUMENT OFF", 12 ) == 0 ) {
			documentCopy = FALSE;
		} else if (strncmp( paramLine, "DOCUMENT COPY", 13 ) == 0 ) {
			while ( ( fgets( paramLine, STR_LONG_SIZE, paramFile ) ) != NULL ) {
				paramLineNum++;
				if ( strncmp(paramLine, "END", 3) == 0 )
					break;
				if ( documentCopy && documentFile )
					fprintf( documentFile, "%s", paramLine );
			}
		} else if ( strncmp( paramLine, "DEMOINIT", 8 ) == 0 ) {
			DemoInitValues();
		} else if ( strncmp( paramLine, "REGRESSION START", 16 ) == 0 ) {
			DoRegression( curDemo < 1 ? paramFileName :
					            demoList(curDemo-1).fileName );
		} else {
			if (strncmp( paramLine, "MOUSE ", 6 ) == 0) {
				thisCmd = mouseCmd;
			}
			if (strncmp( paramLine, "MAP ", 6 ) == 0) {
				thisCmd = mouseCmd;
			}
			for ( inx=0; inx<playbackProc_da.cnt; inx++ ) {
				len = strlen(playbackProc(inx).label);
				if (strncmp( paramLine, playbackProc(inx).label, len ) == 0) {
					if (playbackProc(inx).data == NULL) {
						while (paramLine[len] == ' ') len++;
						playbackProc(inx).proc( paramLine+len );
					} else
						playbackProc(inx).proc( (char*)playbackProc(inx).data );
					break;
				}
			}
			if ( thisCmd == mouseCmd ) {
				EnableButtons( FALSE );
				playbackKeyState = 0;
			}
			if (inx == playbackProc_da.cnt) {
				NoticeMessage( MSG_PLAYBACK_UNK_CMD, _("Ok"), NULL, paramLineNum, paramLine );
			}
		}
		lastCmd = thisCmd;
		wFlush();
		if (pauseDemo) {
			EnableButtons( TRUE );
			pauseDemo = FALSE;
			RestoreLocale( oldLocale );
			return;
		}
	}
	if (paramFile) {
		fclose( paramFile );
		paramFile = NULL;
	}
	if (documentFile) {
		fclose( documentFile );
		documentFile = NULL;
	}
	PlaybackQuit();
	RestoreLocale( oldLocale );
}


static int StartPlayback( int cnt, char **pathName, void * context )
{
	assert( pathName != NULL );
	assert( cnt ==1 );

	SetCurrentPath( MACROPATHKEY, pathName[0] );
	paramFile = fopen( pathName[0], "r" );
	if ( paramFile == NULL ) {
		NoticeMessage( MSG_OPEN_FAIL, _("Continue"), NULL, _("Playback"), pathName[0], strerror(errno) );
		return FALSE;
	}

	paramFileName = strdup( pathName[0] );

	PlaybackSetup();
	curDemo = -1;
	UndoSuspend();
	wWinBlockEnable( FALSE );
	Playback();
	free( paramFileName );
	paramFileName = NULL;

	return TRUE;
}


static void DoDemoButton( void * command )
{
	switch( (int)(long)command ) {
	case 0:
		/* step */
		playbackNonStop = (wGetKeyState() & WKEY_SHIFT) != 0;
		paramHiliteFast = (wGetKeyState() & WKEY_CTRL) != 0;
		Playback();
		break;
	case 1:
		if (curDemo == -1) {
			DoSaveAs( NULL );
		} else {
			/* next */
			if (paramFile)
				fclose(paramFile);
			paramFile = NULL;
			wTextClear( demoT );
			if ( (wGetKeyState()&WKEY_SHIFT)!=0 ) {
				if ( curDemo >= 2 )
					curDemo -= 2;
				else
					curDemo = 0;
			}
			Playback();
		}
		break;
	case 2:
		/* pause */
		pauseDemo = TRUE;
		break;
	case 3:
		/* quit */
		PlaybackQuit();
		break;
	default:
		;
	}
}




static void DemoDlgUpdate(
		paramGroup_p pg,
		int inx,
		void * valueP )
{
	if ( inx != I_DEMOSPEED ) return;
	SetPlaybackSpeed( (wIndex_t)*(long*)valueP );
}


static void CreateDemoW( void )
{
	char * title = MakeWindowTitle(_("Demo"));
	demoW = ParamCreateDialog( &demoPG, title, NULL, NULL, NULL, FALSE, NULL, F_RESIZE, DemoDlgUpdate );

	wListAddValue( demoSpeedL, _("Slowest"), NULL, (void*)0 );
	wListAddValue( demoSpeedL, _("Slow"), NULL, (void*)1 );
	wListAddValue( demoSpeedL, _("Normal"), NULL, (void*)2 );
	wListAddValue( demoSpeedL, _("Fast"), NULL, (void*)3 );
	wListAddValue( demoSpeedL, _("Faster"), NULL, (void*)4 );
	wListAddValue( demoSpeedL, _("Fastest"), NULL, (void*)5 );
	wListSetIndex( demoSpeedL, (wIndex_t)playbackSpeed );
	playbackFile_fs = wFilSelCreate( mainW, FS_LOAD, 0, title, sRecordFilePattern, StartPlayback, NULL );
}


EXPORT void DoPlayBack( void * context )
{
	if (demoW == NULL)
		CreateDemoW();
	wButtonSetLabel( demoNext, _("Save") );
	wFilSelect( playbackFile_fs, GetCurrentPath(MACROPATHKEY));
}



/*****************************************************************************
 *
 * DEMO
 *
 */

static char * demoInitParams[] = {
		"layout title1 XTrackCAD",
		"layout title2 Demo",
		"GROUP layout",
		"display tunnels 1",
		"display endpt 2",
		"display labelenable 0",
		"display description-fontsize 48",
		"display labelscale 8",
		"display layoutlabels 6",
		"display tworailscale 16",
		"display tiedraw 0",
		"pref mingridspacing 5",
		"pref balloonhelp 1",
		"display hotbarlabels 1",
		"display mapscale 64",
		"display livemap 0",
		"display carhotbarlabels 1",
		"display hideTrainsInTunnels 0",
		"GROUP display",
		"pref turntable-angle 15.00",
		"cmdopt preselect 1",
		"pref coupling-speed-max 100",
		"cmdopt rightclickmode 0",
		"GROUP cmdopt",
		"pref checkpoint 0",
		"pref units 0",
		"pref dstfmt 1",
		"pref anglesystem 0",
		"pref minlength 0.100",
		"pref connectdistance 0.100",
		"pref connectangle 1.000",
		"pref dragpixels 20",
		"pref dragtimeout 500",
		"display autoPan 0",
		"display listlabels 7",
		"layout mintrackradius 1.000",
		"layout maxtrackgrade 5.000",
		"display trainpause 300",
		"GROUP pref",
		"rgbcolor snapgrid 65280",
		"rgbcolor marker 16711680",
		"rgbcolor border 0",
		"rgbcolor crossmajor 16711680",
		"rgbcolor crossminor 255",
		"rgbcolor normal 0",
		"rgbcolor selected 16711680",
		"rgbcolor profile 16711935",
		"rgbcolor exception 16711808",
		"rgbcolor tie 16744448",
		"GROUP rgbcolor",
		"easement val 0.000",
		"easement r 0.000",
		"easement x 0.000",
		"easement l 0.000",
		"GROUP easement",
		"grid horzspacing 12.000",
		"grid horzdivision 12",
		"grid horzenable 0",
		"grid vertspacing 12.000",
		"grid vertdivision 12",
		"grid vertenable 0",
		"grid origx 0.000",
		"grid origy 0.000",
		"grid origa 0.000",
		"grid show 0",
		"GROUP grid",
		"misc toolbarset 65535",
		"GROUP misc",
		"sticky set 67108479", /* 0x3fffe7f - all but Helix and Turntable */
		"GROUP sticky",
		"turnout hide 0",
		"layer button-count 10",
		NULL };

static void DemoInitValues( void )
{
	int inx;
	char **cpp;
	static playbackProc_p paramPlaybackProc = NULL;
	static coOrd roomSize = { 96.0, 48.0 };
	char scaleName[10];
	if ( paramPlaybackProc == NULL ) {
		for ( inx=0; inx<playbackProc_da.cnt; inx++ ) {
			if (strncmp( "PARAMETER", playbackProc(inx).label, 9 ) == 0 ) {
				paramPlaybackProc = playbackProc(inx).proc;
				break;
			}
		}
	}
	SetRoomSize( roomSize );
	strcpy( scaleName, "DEMO" );
	DoSetScale( scaleName );
	if ( paramPlaybackProc == NULL ) {
		wNoticeEx( NT_INFORMATION, _("Can not find PARAMETER playback proc"), _("Ok"), NULL );
		return;
	}
	for ( cpp = demoInitParams; *cpp; cpp++ )
		paramPlaybackProc( *cpp );
}


static void DoDemo( void * demoNumber )
{

	if (demoW == NULL)
		CreateDemoW();
	wButtonSetLabel( demoNext, _("Next") );
	curDemo = (int)(long)demoNumber;
	if ( curDemo < 0 || curDemo >= demoList_da.cnt ) {
		NoticeMessage( MSG_DEMO_BAD_NUM, _("Ok"), NULL, curDemo );
		return;
	}
	PlaybackSetup();
	playbackNonStop = (wGetKeyState() & WKEY_SHIFT) != 0;
	paramFile = NULL;
	Playback();
}


static BOOL_T ReadDemo(
		char * line )
{
		static wMenu_p m;
		char * cp;
		char *oldLocale = NULL;
		char *path;

		if ( m == NULL )
			m = demoM;

		if ( strncmp( line, "DEMOGROUP ", 10 ) == 0 ) {
			if (userLocale)
				oldLocale = SaveLocale(userLocale);
			m = wMenuMenuCreate( demoM, NULL, _(line+10) );
			if (oldLocale)
				RestoreLocale(oldLocale);
		} else if ( strncmp( line, "DEMO ", 5 ) == 0 ) {
			if (line[5] != '"')
				goto error;
			cp = line+6;
			while (*cp && *cp != '"') cp++;
			if ( !*cp )
				goto error;
			*cp++ = '\0';
			while (*cp && *cp == ' ') cp++;
			if ( strlen(cp)==0 )
				goto error;
			DYNARR_APPEND( demoList_t, demoList_da, 10 );
			if (userLocale)
				oldLocale = SaveLocale(userLocale);
			demoList( demoList_da.cnt-1 ).title = MyStrdup( _(line+6) );
			MakeFullpath(&path, libDir, "demos", cp, NULL);
			demoList(demoList_da.cnt - 1).fileName = path;
			wMenuPushCreate( m, NULL, _(line+6), 0, DoDemo, (void*)(intptr_t)(demoList_da.cnt-1) );
			if (oldLocale)
				RestoreLocale(oldLocale);
		}
		return TRUE;
error:
		InputError( "Expected 'DEMO \"<Demo Name>\" <File Name>'", TRUE );
		return FALSE;
}



EXPORT BOOL_T MacroInit( void )
{
	AddParam( "DEMOGROUP ", ReadDemo );
	AddParam( "DEMO ", ReadDemo );

	recordMouseMoves = ( getenv( "XTRKCADNORECORDMOUSEMOVES" ) == NULL );

	rightDragColor = drawColorRed;
	leftDragColor = drawColorBlue;

	arrow0_bm = wDrawBitMapCreate( mainD.d, arrow0_width, arrow0_height, 12, 12, arrow0_bits );
	arrow0_shift_bm = wDrawBitMapCreate( mainD.d, arrow0_shift_width, arrow0_shift_height, 12, 12, arrow0_shift_bits );
	arrow0_ctl_bm = wDrawBitMapCreate( mainD.d, arrow0_ctl_width, arrow0_ctl_height, 12, 12, arrow0_ctl_bits );
	arrow3_bm = wDrawBitMapCreate( mainD.d, arrow3_width, arrow3_height, 12, 12, arrow3_bits );
	arrow3_shift_bm = wDrawBitMapCreate( mainD.d, arrow3_shift_width, arrow3_shift_height, 12, 12, arrow3_shift_bits );
	arrow3_ctl_bm = wDrawBitMapCreate( mainD.d, arrow3_ctl_width, arrow3_ctl_height, 12, 12, arrow3_ctl_bits );
	arrowr3_bm = wDrawBitMapCreate( mainD.d, arrowr3_width, arrowr3_height, 12, 12, arrowr3_bits );
    arrowr3_shift_bm = wDrawBitMapCreate( mainD.d, arrowr3_shift_width, arrowr3_shift_height, 12, 12, arrowr3_shift_bits );
	arrowr3_ctl_bm = wDrawBitMapCreate( mainD.d, arrowr3_ctl_width, arrowr3_ctl_height, 12, 12, arrowr3_ctl_bits );
	arrows_bm = wDrawBitMapCreate( mainD.d, arrows_width, arrows_height, 12, 12, arrows_bits );
	flash_bm = wDrawBitMapCreate( mainD.d, flash_width, flash_height, 12, 12, flash_bits );

	ParamRegister( &recordPG );
	ParamRegister( &demoPG );

	log_playbackCursor = LogFindIndex( "playbackcursor" );
	log_regression = LogFindIndex( "regression" );

	return TRUE;
}
