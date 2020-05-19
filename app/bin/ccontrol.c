/** \file ccontrol.c
 * Controls
 */

/* -*- C -*- ****************************************************************
 *
 *  System        : 
 *  Module        : 
 *  Object Name   : $RCSfile$
 *  Revision      : $Revision$
 *  Date          : $Date$
 *  Author        : $Author$
 *  Created By    : Robert Heller
 *  Created       : Sun Mar 5 16:01:37 2017
 *  Last Modified : <170314.1418>
 *
 *  Description	
 *
 *  Notes
 *
 *  History
 *	
 ****************************************************************************
 *
 *    Copyright (C) 2017  Robert Heller D/B/A Deepwoods Software
 *			51 Locke Hill Road
 *			Wendell, MA 01379-9728
 *
 *    This program is free software; you can redistribute it and/or modify
 *    it under the terms of the GNU General Public License as published by
 *    the Free Software Foundation; either version 2 of the License, or
 *    (at your option) any later version.
 *
 *    This program is distributed in the hope that it will be useful,
 *    but WITHOUT ANY WARRANTY; without even the implied warranty of
 *    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *    GNU General Public License for more details.
 *
 *    You should have received a copy of the GNU General Public License
 *    along with this program; if not, write to the Free Software
 *    Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 *
 * 
 *
 ****************************************************************************/

static const char rcsid[] = "@(#) : $Id$";

#include <ctype.h>
#include <string.h>

#include "compound.h"
#include "cundo.h"
#include "custom.h"
#include "fileio.h"
#include "i18n.h"
#include "layout.h"
#include "param.h"
#include "track.h"
#include "trackx.h"
#ifdef WINDOWS
#include "include/utf8convert.h"
#endif // WINDOWS
#include "utility.h"
#include "messages.h"

EXPORT TRKTYP_T T_CONTROL = -1;

static int log_control = 0;


#if 0
static drawCmd_t controlD = {
	NULL,
	&screenDrawFuncs,
	0,
	1.0,
	0.0,
	{0.0,0.0}, {0.0,0.0},
	Pix2CoOrd, CoOrd2Pix };

static char controlName[STR_SHORT_SIZE];
static char controlOnScript[STR_LONG_SIZE];
static char controlOffScript[STR_LONG_SIZE];
#endif

typedef struct controlData_t {
    coOrd orig;
    BOOL_T IsHilite;
    char * name;
    char * onscript;
    char * offscript;
} controlData_t, *controlData_p;

static controlData_p GetcontrolData ( track_p trk )
{
    return (controlData_p) GetTrkExtraData(trk);
}

#define RADIUS 6
#define LINE 8

#define control_SF (3.0)

static void DDrawControl(drawCmd_p d, coOrd orig, DIST_T scaleRatio, 
                         wDrawColor color )
{
    coOrd p1, p2;
    
    p1 = orig;
    DrawFillCircle(d,p1, RADIUS * control_SF / scaleRatio,color);
    Translate (&p1, orig, 45, RADIUS * control_SF / scaleRatio);
    Translate (&p2, p1, 45, LINE * control_SF / scaleRatio);
    DrawLine(d, p1, p2, 2, color);
    Translate (&p1, orig, 45+90, RADIUS * control_SF / scaleRatio);
    Translate (&p2, p1, 45+90, LINE * control_SF / scaleRatio);
    DrawLine(d, p1, p2, 2, color);
    Translate (&p1, orig, 45+180, RADIUS * control_SF / scaleRatio);
    Translate (&p2, p1, 45+180, LINE * control_SF / scaleRatio);
    DrawLine(d, p1, p2, 2, color);
    Translate (&p1, orig, 45+270, RADIUS * control_SF / scaleRatio);
    Translate (&p2, p1, 45+270, LINE * control_SF / scaleRatio);
    DrawLine(d, p1, p2, 2, color);
}

static void DrawControl (track_p t, drawCmd_p d, wDrawColor color )
{
    controlData_p xx = GetcontrolData(t);
    DDrawControl(d,xx->orig,GetScaleRatio(GetTrkScale(t)),color);
}

static void ControlBoundingBox (coOrd orig, DIST_T scaleRatio, coOrd *hi, 
                                coOrd *lo)
{
    coOrd p1, p2;
    
    p1 = orig;
    Translate (&p1, orig, 0, -(RADIUS+LINE) * control_SF / scaleRatio);
    Translate (&p2, orig, 0, (RADIUS+LINE) * control_SF / scaleRatio);
    *hi = p1; *lo = p1;
    if (p2.x > hi->x) hi->x = p2.x;
    if (p2.x < lo->x) lo->x = p2.x;
    if (p2.y > hi->y) hi->y = p2.y;
    if (p2.y < lo->y) lo->y = p2.y;
}    


static void ComputeControlBoundingBox (track_p t )
{
    coOrd lo, hi;
    controlData_p xx = GetcontrolData(t);
    ControlBoundingBox(xx->orig, GetScaleRatio(GetTrkScale(t)), &hi, &lo);
    SetBoundingBox(t, hi, lo);
}

static DIST_T DistanceControl (track_p t, coOrd * p )
{
    controlData_p xx = GetcontrolData(t);
    return FindDistance(xx->orig, *p);
}

static struct {
    char name[STR_SHORT_SIZE];
    coOrd pos;
    char onscript[STR_LONG_SIZE];
    char offscript[STR_LONG_SIZE];
} controlProperties;

typedef enum { NM, PS, ON, OF } controlDesc_e;
static descData_t controlDesc[] = {
    /* NM */ { DESC_STRING, N_("Name"),      &controlProperties.name, sizeof(controlProperties.name) },
    /* PS */ { DESC_POS,    N_("Position"),  &controlProperties.pos },
    /* ON */ { DESC_STRING, N_("On Script"), &controlProperties.onscript, sizeof(controlProperties.onscript) },
    /* OF */ { DESC_STRING, N_("Off Script"),&controlProperties.offscript, sizeof(controlProperties.offscript) },
    { DESC_NULL } };

static void UpdateControlProperties (  track_p trk, int inx, descData_p
                                     descUpd, BOOL_T needUndoStart )
{
    controlData_p xx = GetcontrolData(trk);
    const char *thename, *theonscript, *theoffscript;
    unsigned int max_str;
    char *newName, *newOnScript, *newOffScript;
    BOOL_T changed, nChanged, pChanged, onChanged, offChanged;
    
    switch (inx) {
    case NM:
        break;
    case PS:
        break;
    case ON:
        break;
    case OF:
        break;
    case -1:
        changed = nChanged = pChanged = onChanged = offChanged = FALSE;
        thename = wStringGetValue( (wString_p) controlDesc[NM].control0 );
        if (strcmp(thename,xx->name) != 0) {
            nChanged = changed = TRUE;
            max_str = controlDesc[NM].max_string;
			if (max_str && strlen(thename)>max_str-1) {
				newName = MyMalloc(max_str);
				newName[0] = '\0';
				strncat(newName,thename,max_str-1);
				NoticeMessage2(0, MSG_ENTERED_STRING_TRUNCATED, _("Ok"), NULL, max_str-1);
			} else newName = MyStrdup(thename);
        }


        theonscript = wStringGetValue( (wString_p) controlDesc[ON].control0 );
        if (strcmp(theonscript,xx->onscript) != 0) {
            onChanged = changed = TRUE;
        	max_str = controlDesc[ON].max_string;
			if (max_str && strlen(theonscript)>max_str-1) {
				newOnScript = MyMalloc(max_str);
				newOnScript[0] = '\0';
				strncat(newOnScript,theonscript,max_str-1);
				NoticeMessage2(0, MSG_ENTERED_STRING_TRUNCATED, _("Ok"), NULL, max_str-1);
			} else newOnScript = MyStrdup(theonscript);
        }

        theoffscript = wStringGetValue( (wString_p) controlDesc[OF].control0 );
        if (strcmp(theoffscript,xx->offscript) != 0) {
            offChanged = changed = TRUE;
            max_str = controlDesc[OF].max_string;
			if (max_str && strlen(theoffscript)>max_str-1) {
				newOffScript = MyMalloc(max_str);
				newOffScript[max_str-1] = '\0';
				strncat(newOffScript,theoffscript,max_str-1);
				NoticeMessage2(0, MSG_ENTERED_STRING_TRUNCATED, _("Ok"), NULL, max_str);
			} else newOffScript = MyStrdup(theoffscript);
        }

        if (controlProperties.pos.x != xx->orig.x ||
            controlProperties.pos.y != xx->orig.y) {
            pChanged = changed = TRUE;
        }
        if (!changed) break;
        if (needUndoStart)
            UndoStart( _("Change Control"), "Change Control" );
        UndoModify( trk );
        if (nChanged) {
            MyFree(xx->name);
            xx->name = newName;
        }
        if (pChanged) {
            UndrawNewTrack( trk );
        }
        if (pChanged) {
            xx->orig = controlProperties.pos;
        }
        if (onChanged) {
            MyFree(xx->onscript);
            xx->onscript = newOnScript;
        }
        if (offChanged) {
            MyFree(xx->offscript);
            xx->offscript = newOffScript;
        }
        if (pChanged) { 
            ComputeControlBoundingBox( trk );
            DrawNewTrack( trk );
        }
        break;
    }
}

        



static void DescribeControl (track_p trk, char * str, CSIZE_T len )
{
    controlData_p xx = GetcontrolData(trk);
    
    strcpy( str, _(GetTrkTypeName( trk )) );
    str++;
    while (*str) {
        *str = tolower((unsigned char)*str);
        str++;
    }
    sprintf( str, _("(%d [%s]): Layer=%u, at %0.3f,%0.3f"),
             GetTrkIndex(trk),
             xx->name,GetTrkLayer(trk)+1, xx->orig.x, xx->orig.y);
    strncpy(controlProperties.name,xx->name,STR_SHORT_SIZE-1);
    controlProperties.name[STR_SHORT_SIZE-1] = '\0';
    strncpy(controlProperties.onscript,xx->onscript,STR_LONG_SIZE-1);
    controlProperties.onscript[STR_LONG_SIZE-1] = '\0';
    strncpy(controlProperties.offscript,xx->offscript,STR_LONG_SIZE-1);
    controlProperties.offscript[STR_LONG_SIZE-1] = '\0';
    controlProperties.pos = xx->orig;
    controlDesc[NM].mode = 
          controlDesc[ON].mode = 
          controlDesc[OF].mode = DESC_NOREDRAW;
    DoDescribe( _("Control"), trk, controlDesc, UpdateControlProperties );
    
}

static void DeleteControl ( track_p trk ) 
{
    controlData_p xx = GetcontrolData(trk);
    MyFree(xx->name); xx->name = NULL;
    MyFree(xx->onscript); xx->onscript = NULL;
    MyFree(xx->offscript); xx->offscript = NULL;
}

static BOOL_T WriteControl ( track_p t, FILE * f )
{
    BOOL_T rc = TRUE;
    controlData_p xx = GetcontrolData(t);
	char *controlName = MyStrdup(xx->name);

#ifdef WINDOWS
	controlName = Convert2UTF8(controlName);
#endif // WINDOWS

    rc &= fprintf(f, "CONTROL %d %u %s %d %0.6f %0.6f \"%s\" \"%s\" \"%s\"\n",
                  GetTrkIndex(t), GetTrkLayer(t), GetTrkScaleName(t),
                  GetTrkVisible(t), xx->orig.x, xx->orig.y, controlName, 
                  xx->onscript, xx->offscript)>0;

	MyFree(controlName);
    return rc;
}

static BOOL_T ReadControl ( char * line )
{
    wIndex_t index;
    /*TRKINX_T trkindex;*/
    track_p trk;
    /*char * cp = NULL;*/
    char *name;
    char *onscript, *offscript;
    coOrd orig;
    BOOL_T visible;
    char scale[10];
    wIndex_t layer;
    controlData_p xx;
    if (!GetArgs(line+7,"dLsdpqqq",&index,&layer,scale, &visible, &orig,&name,&onscript,&offscript)) {
        return FALSE;
    }

#ifdef WINDOWS
	ConvertUTF8ToSystem(name);
#endif // WINDOWS

    trk = NewTrack(index, T_CONTROL, 0, sizeof(controlData_t));
    SetTrkVisible(trk, visible); 
    SetTrkScale(trk, LookupScale( scale ));
    SetTrkLayer(trk, layer);
    xx = GetcontrolData ( trk );
    xx->name = name;
    xx->orig = orig;
    xx->onscript = onscript;
    xx->offscript = offscript;
    ComputeControlBoundingBox(trk);
    return TRUE;
}

static void MoveControl (track_p trk, coOrd orig )
{
    controlData_p xx = GetcontrolData ( trk );
    xx->orig.x += orig.x;
    xx->orig.y += orig.y;
    ComputeControlBoundingBox(trk);
}

static void RotateControl (track_p trk, coOrd orig, ANGLE_T angle )
{
}

static void RescaleControl (track_p trk, FLOAT_T ratio )
{
}

static void FlipControl (track_p trk, coOrd orig, ANGLE_T angle ) 
{
    controlData_p xx = GetcontrolData ( trk );
    FlipPoint(&(xx->orig), orig, angle);
    ComputeControlBoundingBox(trk);
}

static trackCmd_t controlCmds = {
    "CONTROL",
    DrawControl,
    DistanceControl,
    DescribeControl,
    DeleteControl,
    WriteControl,
    ReadControl,
    MoveControl,
    RotateControl,
    RescaleControl,
    NULL, /* audit */
    NULL, /* getAngle */
    NULL, /* split */
    NULL, /* traverse */
    NULL, /* enumerate */
    NULL, /* redraw */
    NULL, /* trim */
    NULL, /* merge */
    NULL, /* modify */
    NULL, /* getLength */
    NULL, /* getTrkParams */
    NULL, /* moveEndPt */
    NULL, /* query */
    NULL, /* ungroup */
    FlipControl, /* flip */
    NULL, /* drawPositionIndicator */
    NULL, /* advancePositionIndicator */
    NULL, /* checkTraverse */
    NULL, /* makeParallel */
    NULL  /* drawDesc */
};

static coOrd controlEditOrig;
static track_p controlEditTrack;
static char controlEditName[STR_SHORT_SIZE];
static char controlEditOnScript[STR_LONG_SIZE];
static char controlEditOffScript[STR_LONG_SIZE];

static paramFloatRange_t r_1000_1000    = { -1000.0, 1000.0, 80 };
static paramData_t controlEditPLs[] = {
#define I_CONTROLNAME (0)
    /*0*/ { PD_STRING, controlEditName, "name", PDO_NOPREF | PDO_STRINGLIMITLENGTH, (void*)200, N_("Name"), 0, 0, sizeof(controlEditName) },
#define I_ORIGX (1)
    /*1*/ { PD_FLOAT, &controlEditOrig.x, "origx", PDO_DIM, &r_1000_1000, N_("Origin X") },
#define I_ORIGY (2)
    /*2*/ { PD_FLOAT, &controlEditOrig.y, "origy", PDO_DIM, &r_1000_1000, N_("Origin Y") },
#define I_CONTROLONSCRIPT (3)
    /*3*/ { PD_STRING, controlEditOnScript, "script", PDO_NOPREF | PDO_STRINGLIMITLENGTH, (void*)350, N_("On Script"), 0, 0, sizeof(controlEditOnScript)},
#define I_CONTROLOFFSCRIPT (4)
    /*4*/ { PD_STRING, controlEditOffScript, "script", PDO_NOPREF | PDO_STRINGLIMITLENGTH, (void*)350, N_("Off Script"), 0, 0, sizeof(controlEditOffScript)},
};

static paramGroup_t controlEditPG = { "controlEdit", 0, controlEditPLs, sizeof controlEditPLs/sizeof controlEditPLs[0] };
static wWin_p controlEditW;

static void ControlEditOk ( void * junk )
{
    track_p trk;
    controlData_p xx;
    
    if (controlEditTrack == NULL) {
        UndoStart( _("Create Control"), "Create Control");
        trk = NewTrack(0, T_CONTROL, 0, sizeof(controlData_t));
    } else {
        UndoStart( _("Modify Control"), "Modify Control");
        trk = controlEditTrack;
    }
    xx = GetcontrolData(trk);
    xx->orig = controlEditOrig;
    if ( xx->name == NULL || strncmp (xx->name, controlEditName, STR_SHORT_SIZE) != 0) {
        MyFree(xx->name);
        xx->name = MyStrdup(controlEditName);
    }
    if ( xx->onscript == NULL || strncmp (xx->onscript, controlEditOnScript, STR_LONG_SIZE) != 0) {
        MyFree(xx->onscript);
        xx->onscript = MyStrdup(controlEditOnScript);
    }
    if ( xx->offscript == NULL || strncmp (xx->offscript, controlEditOffScript, STR_LONG_SIZE) != 0) {
        MyFree(xx->offscript);
        xx->offscript = MyStrdup(controlEditOffScript);
    }
    UndoEnd();
    ComputeControlBoundingBox(trk);
    DoRedraw();
    wHide( controlEditW );
}

#if 0
static void ControlEditCancel ( wWin_p junk )
{
    wHide( controlEditW );
}
#endif

static void EditControlDialog()
{
    controlData_p xx;
    
    if ( !controlEditW ) {
        ParamRegister( &controlEditPG );
        controlEditW = ParamCreateDialog (&controlEditPG,
                                          MakeWindowTitle(_("Edit control")),
                                          _("Ok"), ControlEditOk,
                                          wHide, TRUE, NULL,
                                          F_BLOCK,
                                          NULL );
    }
    if (controlEditTrack == NULL) {
        controlEditName[0] = '\0';
        controlEditOnScript[0] = '\0';
        controlEditOffScript[0] = '\0';
    } else {
        xx = GetcontrolData ( controlEditTrack );
        strncpy(controlEditName,xx->name,STR_SHORT_SIZE);
        strncpy(controlEditOnScript,xx->onscript,STR_LONG_SIZE);
        strncpy(controlEditOffScript,xx->offscript,STR_LONG_SIZE);
        controlEditOrig = xx->orig;
    }
    ParamLoadControls( &controlEditPG );
    wShow( controlEditW );
}

static void EditControl (track_p trk)
{
    controlEditTrack = trk;
    EditControlDialog();
}

static void CreateNewControl (coOrd orig)
{
    controlEditOrig = orig;
    controlEditTrack = NULL;
    EditControlDialog();
}

static STATUS_T CmdControl ( wAction_t action, coOrd pos )
{
    
    
    switch (action) {
    case C_START:
        InfoMessage(_("Place control"));
        return C_CONTINUE;
    case C_DOWN:
	case C_MOVE:
		SnapPos(&pos);
        DDrawControl( &tempD, pos, GetScaleRatio(GetLayoutCurScale()), wDrawColorBlack );
        return C_CONTINUE;
    case C_UP:
        SnapPos(&pos);
        DDrawControl( &tempD, pos, GetScaleRatio(GetLayoutCurScale()), wDrawColorBlack );
        CreateNewControl(pos);
        return C_TERMINATE;
    case C_REDRAW:
    case C_CANCEL:
        DDrawControl( &tempD, pos, GetScaleRatio(GetLayoutCurScale()), wDrawColorBlack );
        return C_CONTINUE;
    default:
        return C_CONTINUE;
    }
}

static coOrd ctlhiliteOrig, ctlhiliteSize;
static POS_T ctlhiliteBorder;
static wDrawColor ctlhiliteColor = 0;
static void DrawControlTrackHilite( void )
{
	wPos_t x, y, w, h;
	if (ctlhiliteColor==0)
		ctlhiliteColor = wDrawColorGray(87);
	w = (wPos_t)((ctlhiliteSize.x/mainD.scale)*mainD.dpi+0.5);
	h = (wPos_t)((ctlhiliteSize.y/mainD.scale)*mainD.dpi+0.5);
	mainD.CoOrd2Pix(&mainD,ctlhiliteOrig,&x,&y);
	wDrawFilledRectangle( mainD.d, x, y, w, h, ctlhiliteColor, wDrawOptTemp|wDrawOptTransparent );
}

static int ControlMgmProc ( int cmd, void * data )
{
    track_p trk = (track_p) data;
    controlData_p xx = GetcontrolData(trk);
    /*char msg[STR_SIZE];*/
    
    switch ( cmd ) {
    case CONTMGM_CAN_EDIT:
        return TRUE;
        break;
    case CONTMGM_DO_EDIT:
        EditControl(trk);
        return TRUE;
        break;
    case CONTMGM_CAN_DELETE:
        return TRUE;
        break;
    case CONTMGM_DO_DELETE:
        DeleteTrack(trk, FALSE);
        return TRUE;
        break;
    case CONTMGM_DO_HILIGHT:
        if (!xx->IsHilite) {
            ctlhiliteBorder = mainD.scale*0.1;
            if ( ctlhiliteBorder < trackGauge ) ctlhiliteBorder = trackGauge;
            GetBoundingBox( trk, &ctlhiliteSize, &ctlhiliteOrig );
            ctlhiliteOrig.x -= ctlhiliteBorder;
            ctlhiliteOrig.y -= ctlhiliteBorder;
            ctlhiliteSize.x -= ctlhiliteOrig.x-ctlhiliteBorder;
            ctlhiliteSize.y -= ctlhiliteOrig.y-ctlhiliteBorder;
            DrawControlTrackHilite();
            xx->IsHilite = TRUE;
        }
        break;
    case CONTMGM_UN_HILIGHT:
        if (xx->IsHilite) {
            ctlhiliteBorder = mainD.scale*0.1;
            if ( ctlhiliteBorder < trackGauge ) ctlhiliteBorder = trackGauge;
            GetBoundingBox( trk, &ctlhiliteSize, &ctlhiliteOrig );
            ctlhiliteOrig.x -= ctlhiliteBorder;
            ctlhiliteOrig.y -= ctlhiliteBorder;
            ctlhiliteSize.x -= ctlhiliteOrig.x-ctlhiliteBorder;
            ctlhiliteSize.y -= ctlhiliteOrig.y-ctlhiliteBorder;
            DrawControlTrackHilite();
            xx->IsHilite = FALSE;
        }
        break;
    case CONTMGM_GET_TITLE:
        sprintf(message,"\t%s\t",xx->name);
        break;
    }
    return FALSE;
}

#include "bitmaps/control.xpm"

EXPORT void ControlMgmLoad ( void )
{
    track_p trk;
    static wIcon_p controlI = NULL;
    
    if (controlI == NULL) {
        controlI = wIconCreatePixMap( control_xpm );
    }
    
    TRK_ITERATE(trk) {
        if (GetTrkType(trk) != T_CONTROL) continue;
        ContMgmLoad (controlI, ControlMgmProc, (void *) trk );
    }
}

#define ACCL_CONTROL 0

EXPORT void InitCmdControl ( wMenu_p menu )
{
    AddMenuButton( menu, CmdControl, "cmdControl", _("Control"), 
                   wIconCreatePixMap( control_xpm ), LEVEL0_50, IC_STICKY|IC_POPUP2, ACCL_CONTROL, NULL );
}

EXPORT void InitTrkControl ( void )
{
    T_CONTROL = InitObject ( &controlCmds );
    log_control = LogFindIndex ( "control" );
}
