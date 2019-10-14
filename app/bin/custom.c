#define RENAME_H
/** \file custom.c
 * 
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

#include <stdlib.h>
#include <stdio.h>
#ifndef WINDOWS
#include <unistd.h>
#include <dirent.h>
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
#include <errno.h>

#include "cjoin.h"
#include "common.h"
#include "custom.h"
#include "fileio.h"
#include "i18n.h"
#include "misc.h"
#include "track.h"
#include "version.h"

#define Product "XTrackCAD"
#define product "xtrkcad"
#define PRODUCT "XTRKCAD"
#define Version VERSION
#define PARAMKEY		(0)


char * sProdName = Product;
char * sProdNameLower = product;
char * sProdNameUpper = PRODUCT;

char * sEnvExtra = PRODUCT "EXTRA";

char * sTurnoutDesignerW = NULL;

char * sAboutProd = NULL;

char * sCustomF = product ".cus";
char * sCheckPointF = product ".ckp";
char * sCheckPoint1F = product ".ck1";
char * sClipboardF = product ".clp";
char * sParamQF = product ".xtq";
char * sUndoF = product ".und";
char * sAuditF = product ".aud";
char * sTipF = product ".tip";

char * sSourceFilePattern = NULL;
char * sSaveFilePattern = NULL;
char * sImageFilePattern = NULL;
char * sImportFilePattern = NULL;
char * sDXFFilePattern = NULL;
char * sRecordFilePattern = NULL;
char * sNoteFilePattern = NULL;
char * sLogFilePattern = NULL;
char * sPartsListFilePattern = NULL;

char * sVersion = Version;
int iParamVersion = PARAMVERSION;
int iMinParamVersion = MINPARAMVERSION;
long lParamKey = PARAMKEY;

extern char *userLocale;

EXPORT char * MakeWindowTitle( char * name )
{
	static char title[STR_SHORT_SIZE];
	sprintf( title, "%s", name );
	return title;
}

static addButtonCallBack_t easementP;

void InitCmdEasement( void )
{
	easementP = EasementInit();
}
void DoEasementRedir( void )
{
	if (easementP)
		easementP(NULL);
}

#ifdef STRUCTDESIGNER
static addButtonCallBack_t structDesignerP;
void DoStructDesignerRedir( void )
{
	if (structDesignerP)
		structDesignerP(NULL);
}
#endif

/**
 * Initialize track commands
 *
 * \return    always TRUE
 */

BOOL_T Initialize( void )
{
	InitTrkCurve();
	InitTrkBezier();
	InitTrkStraight();
	InitTrkEase();
	InitTrkCornu();
	InitTrkTurnout();
	InitTrkTurntable();
	InitTrkStruct();
	InitTrkText();
	InitTrkDraw();
	
	InitTrkBlock();
        InitTrkSwitchMotor();
        InitTrkSignal();
        InitTrkControl();
        InitTrkSensor();
	InitCarDlg();
	InitCmdNote();

	memset( message, 0, sizeof message );
	
	return TRUE;
}

/**
 * Initialize siome localized strings for filename patterns etc. 
 */

void InitCustom( void )
{
	char *buf = malloc(1024);

	/* Initialize some localized strings */
	if (sTurnoutDesignerW == NULL)
	{
		sprintf(buf, _("%s Turnout Designer"), Product);
		sTurnoutDesignerW = strdup(buf);
	}
	if (sAboutProd == NULL)
	{
		sprintf(buf, _("%s Version %s"), Product, Version);
		sAboutProd = strdup(buf);
	}
	if (sSourceFilePattern == NULL)
	{
		sprintf(buf, _("All %s Files (*.xtc,*.xtce)|*.xtc;*.xtce|"
					   "%s Trackplan (*.xtc)|*.xtc|"
					   "%s Extended Trackplan (*.xtce)|*.xtce|"
					   "All Files (*)|*"), 
						Product,
						Product, 
						Product );
		sSourceFilePattern = strdup(buf);
	}
	if (sSaveFilePattern == NULL)
	{
		sprintf(buf, _("%s Trackplan (*.xtc)|*.xtc|"
					   "%s Extended Trackplan (*.xtce)|*.xtce|"
					   "All Files (*)|*"),
						Product,
						Product );
		sSaveFilePattern = strdup(buf);
	}
	if (sImageFilePattern == NULL)
	{
		sprintf(buf,_("All Files (*)|*"));
		sImageFilePattern = strdup(buf);
	}
	if (sImportFilePattern == NULL)
	{
		sprintf(buf, _("%s Import Files (*.xti)|*.xti"), Product );
		sImportFilePattern = strdup(buf);
	}
	if (sDXFFilePattern == NULL)
	{
		sDXFFilePattern = strdup(_("Data Exchange Format Files (*.dxf)|*.dxf"));
	}
	if (sRecordFilePattern == NULL)
	{
		sprintf(buf, _("%s Record Files (*.xtr)|*.xtr"), Product);
		sRecordFilePattern = strdup(buf);
	}
	if (sNoteFilePattern == NULL)
	{
		sprintf(buf, _("%s Note Files (*.not)|*.not"), Product);
		sNoteFilePattern = strdup(buf);
	}
	if (sLogFilePattern == NULL)
	{
		sprintf(buf, _("%s Log Files (*.log)|*.log"), Product);
		sLogFilePattern = strdup(buf);
	}
	if (sPartsListFilePattern == NULL)
	{
		sprintf(buf, _("%s PartsList Files (*.txt)|*.txt"), Product);
		sPartsListFilePattern = strdup(buf);
	}

	free(buf);
}


void CleanupCustom( void )
{
	/* Free dynamically allocated strings */
	if (sTurnoutDesignerW)
	{
		free(sTurnoutDesignerW);
		sTurnoutDesignerW = NULL;
	}
	if (sAboutProd)
	{
		free(sAboutProd);
		sAboutProd = NULL;
	}
	if (sSourceFilePattern)
	{
		free(sSourceFilePattern);
		sSourceFilePattern = NULL;
	}
	if (sImportFilePattern)
	{
		free(sImportFilePattern);
		sImportFilePattern = NULL;
	}
	if (sDXFFilePattern)
	{
		free(sDXFFilePattern);
		sDXFFilePattern = NULL;
	}
	if (sRecordFilePattern)
	{
		free(sRecordFilePattern);
		sRecordFilePattern = NULL;
	}
	if (sNoteFilePattern)
	{
		free(sNoteFilePattern);
		sNoteFilePattern = NULL;
	}
	if (sLogFilePattern)
	{
		free(sLogFilePattern);
		sLogFilePattern = NULL;
	}
	if (sPartsListFilePattern)
	{
		free(sPartsListFilePattern);
		sPartsListFilePattern = NULL;
	}
	if (userLocale)
	{
		free(userLocale);
		userLocale = NULL;
	}
}
