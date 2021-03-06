/** \file mswedit.c
 * Text entry widgets
 */
 
/*  XTrackCAD - Model Railroad CAD
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
 
#include <windows.h>
#include <string.h>
#include <malloc.h>
#include <stdlib.h>
#include <stdio.h>
#include <commdlg.h>
#include <math.h>
#include "mswint.h"


struct wString_t {
		WOBJ_COMMON
		char * valueP;
		wIndex_t valueL;
		wStringCallBack_p action;
		wBool_t enter_pressed;	/**< flag if enter was pressed */
		};

#ifdef LATER
struct wInteger_t {
		WOBJ_COMMON
		long low, high;
		long * valueP;
		long oldValue;
		wIntegerCallBack_p action;
		};

struct wFloat_t {
		WOBJ_COMMON
		double low, high;
		double * valueP;
		double oldValue;
		wFloatCallBack_p action;
		};
#endif // LATER


static XWNDPROC oldEditProc = NULL;
static XWNDPROC newEditProc;
static void triggerString( wString_p b );
#ifdef LATER
static void triggerInteger( wControl_p b );
static void triggerFloat( wControl_p b );
#endif


long FAR PASCAL _export pushEdit(
		HWND hWnd,
		UINT message,
		UINT wParam,
		LONG lParam )
{

	long inx = GetWindowLong( hWnd, GWL_ID );
	wString_p b = (wString_p)mswMapIndex(inx);

	switch (message)
	{
	case WM_CHAR:
	    if (b != NULL) {
	        switch (wParam) {
	        case VK_RETURN:
	            triggerString(b);
	            return (0L);
	            break;
	        case 0x1B:
	        case 0x09:
	            SetFocus(((wControl_p)(b->parent))->hWnd);
	            SendMessage(((wControl_p)(b->parent))->hWnd, WM_CHAR,
	                        wParam, lParam);
	            return 0L;
	        }
	    }
	    break;

	}
	return CallWindowProc(oldEditProc, hWnd, message, wParam, lParam);
}

/*
 *****************************************************************************
 *
 * String Boxes
 *
 *****************************************************************************
 */


void wStringSetValue(
		wString_p b,
		const char * arg )
{
	WORD len = (WORD)strlen( arg );
	SendMessage( b->hWnd, WM_SETTEXT, 0, (DWORD)arg );
	SendMessage( b->hWnd, EM_SETSEL, 0, -1 );
	SendMessage( b->hWnd, EM_SCROLLCARET, 0, 0L );
	SendMessage( b->hWnd, EM_SETMODIFY, FALSE, 0L );
}


void wStringSetWidth(
		wString_p b,
		wPos_t w )
{
	int rc;
	b->w = w;
	rc = SetWindowPos( b->hWnd, HWND_TOP, 0, 0,
				b->w, b->h, SWP_NOMOVE|SWP_NOZORDER );
}


const char * wStringGetValue(
		wString_p b )
{
	static char buff[1024];
	SendMessage( b->hWnd, WM_GETTEXT, sizeof buff, (DWORD)buff );
	return buff;
}

/**
 * Get the string from a entry field. The returned pointer has to be free() after processing is complete.
 * 
 * \param bs IN string entry field
 * 
 * \return    pointer to entered string or NULL if entry field is empty.
 */

static char *getString(wString_p bs)
{
    char *tmpBuffer = NULL;
    UINT chars = SendMessage(bs->hWnd, EM_LINELENGTH, (WPARAM)0, 0L);

    if (chars) {
        tmpBuffer = malloc(chars > sizeof(WORD)? chars + 1 : sizeof(WORD) + 1);
        *(WORD *)tmpBuffer = chars;
        SendMessage(bs->hWnd, (UINT)EM_GETLINE, 0, (LPARAM)tmpBuffer);
        tmpBuffer[chars] = '\0';
    }

    return (tmpBuffer);
}

/**
 * Retrieve and process string entry. If a string has been entered, the callback for
 * the specific entry field is called.
 *
 * \param b IN string entry field
 */

static void triggerString(
    wString_p b)
{
	const char *output = "\n";

    char *enteredString = getString(b);
    if (enteredString)
    {
        if (b->valueP) {
            strcpy(b->valueP, enteredString);
        }
		if (b->action) {
			b->enter_pressed = TRUE;
			b->action(output, b->data);
        }

		free(enteredString);
	}
}


LRESULT stringProc(
    wControl_p b,
    HWND hWnd,
    UINT message,
    WPARAM wParam,
    LPARAM lParam)
{
    wString_p bs = (wString_p)b;
    int modified;

    switch (message) {

    case WM_COMMAND:
        switch (WCMD_PARAM_NOTF) {
        case EN_KILLFOCUS:
            modified = (int)SendMessage(bs->hWnd, (UINT)EM_GETMODIFY, 0, 0L);
            if (!modified) {
                break;
            }

            char *enteredString = getString(bs);
            if (enteredString) {
                if (bs->valueP) {
                    strcpy(bs->valueP, enteredString);
                }
                if (bs->action) {
                    bs->action(enteredString, bs->data);
                    mswSetTrigger(NULL, NULL);
                }
                free(enteredString);
            }
            SendMessage(bs->hWnd, (UINT)EM_SETMODIFY, FALSE, 0L);
        }
        break;
    }

    return DefWindowProc(hWnd, message, wParam, lParam);
}


static callBacks_t stringCallBacks = {
		mswRepaintLabel,
		NULL,
		stringProc };


wString_p wStringCreate(
		wWin_p	parent,
		POS_T	x,
		POS_T	y,
		const char	* helpStr,
		const char	* labelStr,
		long	option,
		POS_T	width,
		char	*valueP,
		wIndex_t valueL,
		wStringCallBack_p action,
		void	*data )
{
	wString_p b;
	RECT rect;
	int index;
	DWORD style = 0;

	b = (wString_p)mswAlloc( parent, B_STRING, mswStrdup(labelStr), sizeof *b, data, &index );
	mswComputePos( (wControl_p)b, x, y );
	b->option = option;
	b->valueP =	 valueP;
	b->valueL = valueL;
	b->labelY += 2;
	b->action = action;
	if (option & BO_READONLY)
		style |= ES_READONLY;

	b->hWnd = CreateWindowEx( WS_EX_CLIENTEDGE, "EDIT", NULL,
						ES_LEFT | ES_AUTOHSCROLL | WS_CHILD | WS_VISIBLE | WS_BORDER | style,
						b->x, b->y,
						width, mswEditHeight,
						((wControl_p)parent)->hWnd, (HMENU)index, mswHInst, NULL );
	if (b->hWnd == NULL) {
		mswFail("CreateWindow(STRING)");
		return b;
	}

	newEditProc = MakeProcInstance( (XWNDPROC)pushEdit, mswHInst );
	oldEditProc = (XWNDPROC)GetWindowLong(b->hWnd, GWL_WNDPROC );
	SetWindowLong( b->hWnd, GWL_WNDPROC, (LONG)newEditProc );

	if (b->valueP) {
		SendMessage( b->hWnd, WM_SETTEXT, 0, (DWORD)b->valueP );
	}
	SendMessage( b->hWnd, EM_SETMODIFY, FALSE, 0L );
	if ( !mswThickFont )
		SendMessage( b->hWnd, WM_SETFONT, (WPARAM)mswLabelFont, 0L );
	GetWindowRect( b->hWnd, &rect );
	b->w = rect.right - rect.left;
	b->h = rect.bottom - rect.top;

	mswAddButton( (wControl_p)b, TRUE, helpStr );
	mswCallBacks[B_STRING] = &stringCallBacks;
	mswChainFocus( (wControl_p)b );
	return b;
}
#ifdef LATER

/*
 *****************************************************************************
 *
 * Integer Value Boxes
 *
 *****************************************************************************
 */


#define MININT	((long)0x80000000)
#define MAXINT	((long)0x7FFFFFFF)


void wIntegerSetValue(
		wInteger_p b,
		long arg )
{
	b->oldValue = arg;
	wsprintf( mswTmpBuff, "%ld", arg );
	SendMessage( b->hWnd, WM_SETTEXT, 0, (DWORD)(LPSTR)mswTmpBuff );
	SendMessage( b->hWnd, EM_SETMODIFY, FALSE, 0L );
}


long wIntegerGetValue(
		wInteger_p b )
{
	return b->oldValue;
}


static void triggerInteger(
		wControl_p b )
{
	wInteger_p bi = (wInteger_p)b;
	int cnt;
	long value;
	char * cp;

	if (bi->action) {
		*(WPARAM*)&mswTmpBuff[0] = 78;
		cnt = (int)SendMessage( bi->hWnd, (UINT)EM_GETLINE, 0, (DWORD)(LPSTR)mswTmpBuff );
		mswTmpBuff[cnt] = '\0';
		if (strcmp( mswTmpBuff, "-" )==0 )
			return;
		value = strtol( mswTmpBuff, &cp, 10 );
		if (*cp != '\0' || value < bi->low || value > bi->high )
			return;
		if (bi->oldValue == value)
			return;
		if (bi->valueP)
			*bi->valueP = value;
		bi->oldValue = value;
		bi->action( value, bi->data );
	}
}


LRESULT integerProc(
		wControl_p b,
		HWND hWnd,
		UINT message,
		WPARAM wParam,
		LPARAM lParam )
{			   
	wInteger_p bi = (wInteger_p)b;
	int inx;
	int cnt;
	long value;
	char * cp;				
	wBool_t ok;
	int modified;
		
	switch( message ) {
	
	case WM_COMMAND:
		switch (WCMD_PARAM_NOTF) {
		case EN_KILLFOCUS:
			ok = TRUE;
			modified = (int)SendMessage( bi->hWnd, (UINT)EM_GETMODIFY, 0, 0L );
			if (!modified)
				break;
			*(WPARAM*)&mswTmpBuff[0] = 78;
			cnt = (int)SendMessage( bi->hWnd, (UINT)EM_GETLINE, 0, (DWORD)(LPSTR)mswTmpBuff );
			mswTmpBuff[cnt] = '\0';
			if (strcmp( mswTmpBuff, "-" )==0 && 0 >= bi->low && 0 <= bi->high ) {
				value = 0;
			} else {
				value = strtol( mswTmpBuff, &cp, 10 );
				if (*cp != '\0' || value < bi->low || value > bi->high ) {
					inx = GetWindowWord( bi->hWnd, GWW_ID );
					if (wWinIsVisible(bi->parent)) {
						PostMessage( ((wControl_p)(bi->parent))->hWnd, 
							WM_NOTVALID, inx, 0L );
						return TRUE;
					} else {
						if (value < bi->low)
							value = bi->low;
						else
							value = bi->high;
						sprintf( mswTmpBuff, "%ld", value );
						SendMessage( bi->hWnd, (UINT)WM_SETTEXT, 0,
									(DWORD)(LPSTR)mswTmpBuff );
					}
				}
			}
			bi->oldValue = value;
			if (bi->valueP)
				*bi->valueP = value;
			if (bi->action) {
				bi->action( value, bi->data );
				mswSetTrigger( NULL, NULL );
			}
			SendMessage( bi->hWnd, (UINT)EM_SETMODIFY, FALSE, 0L );
		}
		break;

	case WM_NOTVALID:
		wsprintf( mswTmpBuff, "Please enter a value between %ld and %ld",
						bi->low, bi->high );
		if (bi->low > MININT && bi->high < MAXINT)
			sprintf( mswTmpBuff,
					 "Please enter an integer value between %ld and %ld",
					 bi->low, bi->high );
		else if (bi->low > MININT)
			sprintf( mswTmpBuff,
					 "Please enter an integer value greater or equal to %ld",
					 bi->low );
		else if (bi->high < MAXINT)
			sprintf( mswTmpBuff,
					 "Please enter an integer value less or equal to %ld",
					 bi->high );
		else
			strcpy( mswTmpBuff, "Please enter an integer value" );
		MessageBox( bi->hWnd, mswTmpBuff, "Invalid entry", MB_OK );
		SetFocus( bi->hWnd );
#ifdef WIN32
		SendMessage( bi->hWnd, EM_SETSEL, 0, 0x7fff );
		SendMessage( bi->hWnd, EM_SCROLLCARET, 0, 0L );
#else
		SendMessage( bi->hWnd, EM_SETSEL, 0, MAKELONG(0,0x7fff) );
#endif
		return TRUE;

	}													  
 
	return DefWindowProc( hWnd, message, wParam, lParam );
}					


static callBacks_t integerCallBacks = {
		mswRepaintLabel,
		NULL,
		integerProc };


wInteger_p wIntegerCreate(
		wWin_p	parent,
		POS_T	x,
		POS_T	y,
		const char	* helpStr,
		const char	* labelStr,
		long	option,
		POS_T	width,
		long	low,
		long	high,
		long	*valueP,
		wIntegerCallBack_p action,
		void	*data )
{
	wInteger_p b;
	RECT rect;
	int index;
	DWORD style = 0;

	b = mswAlloc( parent, B_INTEGER, mswStrdup(labelStr), sizeof *b, data, &index );
	mswComputePos( (wControl_p)b, x, y );
	b->option = option;
	b->low = low;
	b->high = high;
	b->valueP = valueP;
	b->labelY += 2;
	b->action = action;
	if (option & BO_READONLY)
		style |= ES_READONLY;

	b->hWnd = CreateWindow( "EDIT", NULL,
						ES_LEFT | ES_AUTOHSCROLL | WS_CHILD | WS_VISIBLE | WS_BORDER | style,
						b->x, b->y,
						width, mswEditHeight,
						((wControl_p)parent)->hWnd, (HMENU)index, mswHInst, NULL );
	if (b->hWnd == NULL) {
		mswFail("CreateWindow(INTEGER)");
		return b;
	}

	  
	newEditProc = MakeProcInstance( (XWNDPROC)pushEdit, mswHInst );
	oldEditProc = (XWNDPROC)GetWindowLong(b->hWnd, GWL_WNDPROC );
	SetWindowLong( b->hWnd, GWL_WNDPROC, (LONG)newEditProc );

	if ( !mswThickFont )
		SendMessage( b->hWnd, WM_SETFONT, (WPARAM)mswLabelFont, 0L );
	if (b->valueP) {
		wsprintf( mswTmpBuff, "%ld", *b->valueP );
		SendMessage( b->hWnd, WM_SETTEXT, 0, (DWORD)(LPSTR)mswTmpBuff );
		b->oldValue = *b->valueP;
	} else
		b->oldValue = 0;
	SendMessage( b->hWnd, EM_SETMODIFY, FALSE, 0L );

	GetWindowRect( b->hWnd, &rect );
	b->w = rect.right - rect.left;
	b->h = rect.bottom - rect.top;

	mswAddButton( (wControl_p)b, TRUE, helpStr );
	mswCallBacks[ B_INTEGER ] = &integerCallBacks;
	mswChainFocus( (wControl_p)b );
	return b;
}

/*
 *****************************************************************************
 *
 * Floating Point Value Boxes
 *
 *****************************************************************************
 */


#define MINFLT	(-1000000)
#define MAXFLT	(1000000)



void wFloatSetValue(
		wFloat_p b,
		double arg )
{
	b->oldValue = arg;
	sprintf( mswTmpBuff, "%0.3f", arg );
	SendMessage( b->hWnd, WM_SETTEXT, 0, (DWORD)(LPSTR)mswTmpBuff );
	SendMessage( b->hWnd, EM_SETMODIFY, FALSE, 0L );
}


double wFloatGetValue(
		wFloat_p b )
{
	return b->oldValue;
}


static void triggerFloat(
		wControl_p b )
{
	wFloat_p bf = (wFloat_p)b;
	int cnt;
	double value;
	char * cp;				

	if (bf->action) {
		*(WPARAM*)&mswTmpBuff[0] = 78;
		cnt = (int)SendMessage( bf->hWnd, (UINT)EM_GETLINE, 0,
									(DWORD)(LPSTR)mswTmpBuff );
		mswTmpBuff[cnt] = '\0';
		if (strcmp( mswTmpBuff, "-" )==0)
			return;
		value = strtod( mswTmpBuff, &cp );
		if (*cp != '\0' || value < bf->low || value > bf->high )
			return;
		if (bf->oldValue == value)
			return;
		bf->oldValue = value;
		if (bf->valueP)
		   *bf->valueP = value;
		bf->action( wFloatGetValue(bf), bf->data );
	}
}


LRESULT floatProc(
		wControl_p b,
		HWND hWnd,
		UINT message,
		WPARAM wParam,
		LPARAM lParam )
{			   
	wFloat_p bf = (wFloat_p)b;
	int inx;
	int cnt;
	double value;
	char * cp;				
	wBool_t ok;
	int modified;

	switch( message ) {
	
	case WM_COMMAND:
		switch (HIWORD(lParam)) {
		case EN_KILLFOCUS:
			ok = TRUE;
			modified = (int)SendMessage( bf->hWnd, (UINT)EM_GETMODIFY, 0, 0L );
			if (!modified)
				break;
			*(WPARAM*)&mswTmpBuff[0] = 78;
			cnt = (int)SendMessage( bf->hWnd, (UINT)EM_GETLINE, 0,
									(DWORD)(LPSTR)mswTmpBuff );
			mswTmpBuff[cnt] = '\0';
			if (strcmp( mswTmpBuff, "-" )==0 && 0 >= bf->low && 0 <= bf->high ) {
				value = 0;
			} else {
				value = strtod( mswTmpBuff, &cp );
				if (*cp != '\0' || value < bf->low || value > bf->high ) {
					inx = GetWindowWord( bf->hWnd, GWW_ID );
					if (wWinIsVisible(bf->parent)) {
						PostMessage( ((wControl_p)(bf->parent))->hWnd, 
							WM_NOTVALID, inx, 0L );
						return TRUE;
					} else {
						if (value < bf->low)
							value = bf->low;
						else
							value = bf->high;
						sprintf( mswTmpBuff, "%0.3f", value );
						SendMessage( bf->hWnd, (UINT)WM_SETTEXT, 0,
									(DWORD)(LPSTR)mswTmpBuff );
					}
				}
			}
			bf->oldValue = value;
			if (bf->valueP)
				*bf->valueP = value;
			if (bf->action) {
				bf->action( value, bf->data );
				mswSetTrigger( NULL, NULL );
			}
			SendMessage( bf->hWnd, (UINT)EM_SETMODIFY, FALSE, 0L );
		}
		break;

	case WM_NOTVALID:
		if (bf->low > MINFLT && bf->high < MAXFLT)
			sprintf( mswTmpBuff,
					 "Please enter an float value between %0.3f and %0.3f",
					 bf->low, bf->high );
		else if (bf->low > MINFLT)
			sprintf( mswTmpBuff,
					 "Please enter an float value greater or equal to %0.3f",
					 bf->low );
		else if (bf->high < MAXFLT)
			sprintf( mswTmpBuff,
					 "Please enter an float value less or equal to %0.3f",
					 bf->high );
		else
			strcpy( mswTmpBuff, "Please enter an float value" );
		MessageBox( bf->hWnd, mswTmpBuff, "Invalid entry", MB_OK );
		SetFocus( bf->hWnd );
#ifdef WIN32
		SendMessage( bi->hWnd, EM_SETSEL, 0, 0x7fff );
		SendMessage( bi->hWnd, EM_SCROLLCARET, 0, 0L );
#else
		SendMessage( bi->hWnd, EM_SETSEL, 0, MAKELONG(0,0x7fff) );
#endif
		return TRUE;

	}													  
	return DefWindowProc( hWnd, message, wParam, lParam );
}					


static callBacks_t floatCallBacks = {
		mswRepaintLabel,
		NULL,
		floatProc };


wFloat_p wFloatCreate(
		wWin_p	parent,
		POS_T	x,
		POS_T	y,
		const char	* helpStr,
		const char	* labelStr,
		long	option,
		POS_T	width,
		double	low,
		double	high,
		double	*valueP,
		wFloatCallBack_p action,
		void	*data )
{
	wFloat_p b;
	RECT rect;
	int index;
	DWORD style = 0;

	b = mswAlloc( parent, B_FLOAT, mswStrdup(labelStr), sizeof *b, data, &index );
	mswComputePos( (wControl_p)b, x, y );
	b->option = option;
	b->low = low;
	b->high = high;
	b->valueP = valueP;
	b->labelY += 2;
	b->action = action;
	if (option & BO_READONLY)
		style |= ES_READONLY;

	b->hWnd = CreateWindow( "EDIT", NULL,
						ES_LEFT | ES_AUTOHSCROLL | WS_CHILD | WS_VISIBLE | WS_BORDER | style,
						b->x, b->y,
						width, mswEditHeight,
						((wControl_p)parent)->hWnd, (HMENU)index, mswHInst, NULL );
	if (b->hWnd == NULL) {
		mswFail("CreateWindow(FLOAT)");
		return b;
	}

	
	newEditProc = MakeProcInstance( (XWNDPROC)pushEdit, mswHInst );
	oldEditProc = (XWNDPROC)GetWindowLong(b->hWnd, GWL_WNDPROC );
	SetWindowLong( b->hWnd, GWL_WNDPROC, (LONG)newEditProc );

	if (b->valueP) {
			b->oldValue = *b->valueP;
	} else
			b->oldValue = 0.0;
	if (b->valueP)
			sprintf( mswTmpBuff, "%0.3f", *b->valueP );
	else
			strcpy( mswTmpBuff, "0.000" );
	if ( !mswThickFont )
		SendMessage( b->hWnd, WM_SETFONT, (WPARAM)mswLabelFont, 0L );
	SendMessage( b->hWnd, WM_SETTEXT, 0, (DWORD)(LPSTR)mswTmpBuff );
	SendMessage( b->hWnd, EM_SETMODIFY, FALSE, 0L );

	GetWindowRect( b->hWnd, &rect );
	b->w = rect.right - rect.left;
	b->h = rect.bottom - rect.top;
	mswAddButton( (wControl_p)b, TRUE, helpStr );
	mswCallBacks[ B_FLOAT ] = &floatCallBacks;
	mswChainFocus( (wControl_p)b );
	return b;
}
#endif
