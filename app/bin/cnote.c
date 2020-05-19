/** \file cnote.c
 * Main layout note
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
#include <string.h>

#include "custom.h"
#include "dynstring.h"
#include "fileio.h"
#include "i18n.h"
#include "misc.h"
#include "param.h"
#include "include/utf8convert.h"

static char * mainText = NULL;
static wWin_p noteW;

static paramTextData_t noteTextData = { 300, 150 };
static paramData_t notePLs[] = {
#define I_NOTETEXT		(0)
#define noteT			((wText_p)notePLs[I_NOTETEXT].control)
    {	PD_TEXT, NULL, "text", PDO_DLGRESIZE, &noteTextData }
};
static paramGroup_t notePG = { "note", 0, notePLs, sizeof notePLs/sizeof notePLs[0] };


void ClearNote(void)
{
    if (mainText) {
        MyFree(mainText);
        mainText = NULL;
    }
}

static void NoteOk(void * junk)
{
    if (wTextGetModified(noteT)) {
        int len;
        ClearNote();
        len = wTextGetSize(noteT);
        mainText = (char*)MyMalloc(len+2);
        wTextGetText(noteT, mainText, len);
    }

    wHide(noteW);
}


void DoNote(void)
{
    if (noteW == NULL) {
        noteW = ParamCreateDialog(&notePG, MakeWindowTitle(_("Note")), _("Ok"), NoteOk,
                                  wHide, FALSE, NULL, F_RESIZE, NULL);
    }

    wTextClear(noteT);
    wTextAppend(noteT, mainText?mainText:
                _("Replace this text with your layout notes"));
    wTextSetReadonly(noteT, FALSE);
    wShow(noteW);
}

/**
 * Read the text for a note. Lines are read from the input file
 * until either the maximum length is reached or the END statement is
 * found.
 *
 * \todo Handle premature end as an error
 *
 * \param textLength
 * \return pointer to string, has to be myfree'd by caller
 */

char *
ReadMultilineText(size_t textLength)
{
	char *string;
	DynString noteText;
	DynStringMalloc(&noteText, 0);
	size_t charsRead = 0;
	char *line;

	line = GetNextLine();

	while ( !IsEND("END") ) {
		DynStringCatCStr(&noteText, line);
		DynStringCatCStr(&noteText, "\n");
		line = GetNextLine();
	}
	charsRead = DynStringSize(&noteText);
	if (charsRead > textLength+1) {				// Cope with trailing '/n' and only care if larger
		if (!(InputError("Expected note length: %d read: %d",
			TRUE, textLength, charsRead)))		//If TRUE, carry on
			exit(1);
	}
	string = MyStrdup(DynStringToCStr(&noteText));
	string[strlen(string) - 1] = '\0';

#ifdef WINDOWS
	if (wIsUTF8(string)) {
		ConvertUTF8ToSystem(string);
	}
#endif // WINDOWS

	DynStringFree(&noteText);
	return(string);
}


BOOL_T WriteMainNote(FILE* f)
{
    BOOL_T rc = TRUE;
	char *noteText = mainText;

	if (noteText && *noteText) {
#ifdef WINDOWS
		char *out = NULL;
		if (RequiresConvToUTF8(mainText)) {
			unsigned cnt = strlen(mainText) * 2 + 1;
			out = MyMalloc(cnt);
			wSystemToUTF8(mainText, out, cnt);
			noteText = out;
		}
#endif // WINDOWS


	char * sText = ConvertToEscapedText( noteText );
        rc &= fprintf(f, "NOTE MAIN 0 0 0 0 0 \"%s\"\n", sText )>0;
	MyFree( sText );

#ifdef WINDOWS
		if (out) {
			MyFree(out);
		}
#endif // WINDOWS
    }
    return rc;
}

/**
 * Read the layout main note
 *
 * \param line complete NOTE statement
 */

BOOL_T ReadMainNote(char *line)
{
    long size;
    char * sNote = NULL;

    if (!GetArgs(line + 9,
	paramVersion < 3 ? "l" :
	paramVersion < 12 ? "0000l":
	"0000lq", &size, &sNote)) {
        return FALSE;
    }

    if (mainText) {
        MyFree(mainText);
    }

    if ( paramVersion < 12 )
	mainText = ReadMultilineText(size);
    else
	mainText = sNote;
    return TRUE;
}

void InitCmdNote()
{
    ParamRegister(&notePG);
}
