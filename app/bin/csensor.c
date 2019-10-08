/** \file csensor.c
 * Sensors
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
 *  Last Modified : <170314.1407>
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
#include "utility.h"
#include "messages.h"

EXPORT TRKTYP_T T_SENSOR = -1;

static int log_sensor = 0;


#if 0
static drawCmd_t sensorD = {
	NULL,
	&screenDrawFuncs,
	0,
	1.0,
	0.0,
	{0.0,0.0}, {0.0,0.0},
	Pix2CoOrd, CoOrd2Pix };

static char sensorName[STR_SHORT_SIZE];
static char sensorScript[STR_LONG_SIZE];
#endif

typedef struct sensorData_t {
    coOrd orig;
    BOOL_T IsHilite;
    char * name;
    char * script;
} sensorData_t, *sensorData_p;

static sensorData_p GetsensorData ( track_p trk )
{
    return (sensorData_p) GetTrkExtraData(trk);
}

#define RADIUS 6

#define sensor_SF (3.0)

static void DDrawSensor(drawCmd_p d, coOrd orig, DIST_T scaleRatio, 
                         wDrawColor color )
{
    coOrd p1, p2;
    
    p1 = orig;
    DrawFillCircle(d,p1, RADIUS * sensor_SF / scaleRatio,color);
    Translate (&p2, orig, 45, RADIUS * sensor_SF / scaleRatio);
    DrawLine(d, p1, p2, 2, wDrawColorWhite);
    Translate (&p2, orig, 45+90, RADIUS * sensor_SF / scaleRatio);
    DrawLine(d, p1, p2, 2, wDrawColorWhite);
    Translate (&p2, orig, 45+180, RADIUS * sensor_SF / scaleRatio);
    DrawLine(d, p1, p2, 2, wDrawColorWhite);
    Translate (&p2, orig, 45+270, RADIUS * sensor_SF / scaleRatio);
    DrawLine(d, p1, p2, 2, wDrawColorWhite);
}

static void DrawSensor (track_p t, drawCmd_p d, wDrawColor color )
{
    sensorData_p xx = GetsensorData(t);
    DDrawSensor(d,xx->orig,GetScaleRatio(GetTrkScale(t)),color);
}

static void SensorBoundingBox (coOrd orig, DIST_T scaleRatio, coOrd *hi, 
                                coOrd *lo)
{
    coOrd p1, p2;
    
    p1 = orig;
    Translate (&p1, orig, 0, -RADIUS * sensor_SF / scaleRatio);
    Translate (&p2, orig, 0, RADIUS * sensor_SF / scaleRatio);
    *hi = p1; *lo = p1;
    if (p2.x > hi->x) hi->x = p2.x;
    if (p2.x < lo->x) lo->x = p2.x;
    if (p2.y > hi->y) hi->y = p2.y;
    if (p2.y < lo->y) lo->y = p2.y;
}    


static void ComputeSensorBoundingBox (track_p t )
{
    coOrd lo, hi;
    sensorData_p xx = GetsensorData(t);
    SensorBoundingBox(xx->orig, GetScaleRatio(GetTrkScale(t)), &hi, &lo);
    SetBoundingBox(t, hi, lo);
}

static DIST_T DistanceSensor (track_p t, coOrd * p )
{
    sensorData_p xx = GetsensorData(t);
    return FindDistance(xx->orig, *p);
}

static struct {
    char name[STR_SHORT_SIZE];
    coOrd pos;
    char script[STR_LONG_SIZE];
} sensorProperties;

typedef enum { NM, PS, SC } sensorDesc_e;
static descData_t sensorDesc[] = {
    /* NM */ { DESC_STRING, N_("Name"),     &sensorProperties.name, sizeof(sensorProperties.name) },
    /* PS */ { DESC_POS,    N_("Position"), &sensorProperties.pos },
    /* SC */ { DESC_STRING, N_("Script"),   &sensorProperties.script, sizeof(sensorProperties.script) },
    { DESC_NULL } };

static void UpdateSensorProperties (  track_p trk, int inx, descData_p
                                     descUpd, BOOL_T needUndoStart )
{
    sensorData_p xx = GetsensorData(trk);
    const char *thename, *thescript;
    char *newName, *newScript;
    unsigned int max_str;
    BOOL_T changed, nChanged, pChanged, sChanged;
    
    switch (inx) {
    case NM:
        break;
    case PS:
        break;
    case SC:
        break;
    case -1:
        changed = nChanged = pChanged = sChanged = FALSE;
        thename = wStringGetValue( (wString_p) sensorDesc[NM].control0 );
        if (strcmp(thename,xx->name) != 0) {
            nChanged = changed = TRUE;
            max_str = sensorDesc[NM].max_string;
			if (max_str && strlen(thename)>max_str-1) {
				newName = MyMalloc(max_str);
				newName[max_str-1] = '\0';
				strncat(newName,thename,max_str-1);
				NoticeMessage2(0, MSG_ENTERED_STRING_TRUNCATED, _("Ok"), NULL, max_str-1);
			} else newName = MyStrdup(thename);
        }

        thescript = wStringGetValue( (wString_p) sensorDesc[SC].control0 );
        if (strcmp(thescript,xx->script) != 0) {
            sChanged = changed = TRUE;
            max_str = sensorDesc[SC].max_string;
			if (max_str && strlen(thename)>max_str-1) {
				newScript = MyMalloc(max_str);
				newScript[max_str-1] = '\0';
				strncat(newScript,thescript,max_str-1);
				NoticeMessage2(0, MSG_ENTERED_STRING_TRUNCATED, _("Ok"), NULL, max_str-1);
			} else newScript = MyStrdup(thescript);
        }

        if (sensorProperties.pos.x != xx->orig.x ||
            sensorProperties.pos.y != xx->orig.y) {
            pChanged = changed = TRUE;
        }
        if (!changed) break;
        if (needUndoStart)
            UndoStart( _("Change Sensor"), "Change Sensor" );
        UndoModify( trk );
        if (nChanged) {
            MyFree(xx->name);
            xx->name = newName;
        }
        if (pChanged) {
            UndrawNewTrack( trk );
        }
        if (pChanged) {
            xx->orig = sensorProperties.pos;
        }
        if (sChanged) {
            MyFree(xx->script);
            xx->script = newScript;
        }
        if (pChanged) { 
            ComputeSensorBoundingBox( trk );
            DrawNewTrack( trk );
        }
        break;
    }
}

        

static void DescribeSensor (track_p trk, char * str, CSIZE_T len )
{
    sensorData_p xx = GetsensorData(trk);
    
    strcpy( str, _(GetTrkTypeName( trk )) );
    str++;
    while (*str) {
        *str = tolower((unsigned char)*str);
        str++;
    }
    sprintf( str, _("(%d [%s]): Layer=%d, at %0.3f,%0.3f"),
             GetTrkIndex(trk),
             xx->name,GetTrkLayer(trk)+1, xx->orig.x, xx->orig.y);
    strncpy(sensorProperties.name,xx->name,STR_SHORT_SIZE-1);
    sensorProperties.name[STR_SHORT_SIZE-1] = '\0';
    strncpy(sensorProperties.script,xx->script,STR_LONG_SIZE-1);
    sensorProperties.script[STR_LONG_SIZE-1] = '\0';
    sensorProperties.pos = xx->orig;
    sensorDesc[NM].mode = 
          sensorDesc[SC].mode = DESC_NOREDRAW;
    DoDescribe( _("Sensor"), trk, sensorDesc, UpdateSensorProperties );
}

static void DeleteSensor ( track_p trk ) 
{
    sensorData_p xx = GetsensorData(trk);
    MyFree(xx->name); xx->name = NULL;
    MyFree(xx->script); xx->script = NULL;
}

static BOOL_T WriteSensor ( track_p t, FILE * f )
{
    BOOL_T rc = TRUE;
    sensorData_p xx = GetsensorData(t);
    rc &= fprintf(f, "SENSOR %d %d %s %d %0.6f %0.6f \"%s\" \"%s\"\n",
                  GetTrkIndex(t), GetTrkLayer(t), GetTrkScaleName(t),
                  GetTrkVisible(t), xx->orig.x, xx->orig.y, xx->name, 
                  xx->script)>0;
    return rc;
}

static void ReadSensor ( char * line )
{
    wIndex_t index;
    /*TRKINX_T trkindex;*/
    track_p trk;
    /*char * cp = NULL;*/
    char *name;
    char *script;
    coOrd orig;
    BOOL_T visible;
    char scale[10];
    wIndex_t layer;
    sensorData_p xx;
    if (!GetArgs(line+7,"dLsdpqq",&index,&layer,scale, &visible, &orig,&name,&script)) {
        return;
    }
    trk = NewTrack(index, T_SENSOR, 0, sizeof(sensorData_t));
    SetTrkVisible(trk, visible); 
    SetTrkScale(trk, LookupScale( scale ));
    SetTrkLayer(trk, layer);
    xx = GetsensorData ( trk );
    xx->name = name;
    xx->orig = orig;
    xx->script = script;
    ComputeSensorBoundingBox(trk);
}

static void MoveSensor (track_p trk, coOrd orig )
{
    sensorData_p xx = GetsensorData ( trk );
    xx->orig.x += orig.x;
    xx->orig.y += orig.y;
    ComputeSensorBoundingBox(trk);
}

static void RotateSensor (track_p trk, coOrd orig, ANGLE_T angle )
{
}

static void RescaleSensor (track_p trk, FLOAT_T ratio )
{
}

static void FlipSensor (track_p trk, coOrd orig, ANGLE_T angle ) 
{
    sensorData_p xx = GetsensorData ( trk );
    FlipPoint(&(xx->orig), orig, angle);
    ComputeSensorBoundingBox(trk);
}


static trackCmd_t sensorCmds = {
    "SENSOR",
    DrawSensor,
    DistanceSensor,
    DescribeSensor,
    DeleteSensor,
    WriteSensor,
    ReadSensor,
    MoveSensor,
    RotateSensor,
    RescaleSensor,
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
    FlipSensor, /* flip */
    NULL, /* drawPositionIndicator */
    NULL, /* advancePositionIndicator */
    NULL, /* checkTraverse */
    NULL, /* makeParallel */
    NULL  /* drawDesc */
};

static coOrd sensorEditOrig;
static track_p sensorEditTrack;
static char sensorEditName[STR_SHORT_SIZE];
static char sensorEditScript[STR_LONG_SIZE];

static paramFloatRange_t r_1000_1000    = { -1000.0, 1000.0, 80 };
static paramData_t sensorEditPLs[] = {
#define I_SENSORNAME (0)
    /*0*/ { PD_STRING, sensorEditName, "name", PDO_NOPREF|PDO_STRINGLIMITLENGTH, (void*)200, N_("Name"), 0, 0, sizeof(sensorEditName)},
#define I_ORIGX (1)
    /*1*/ { PD_FLOAT, &sensorEditOrig.x, "origx", PDO_DIM, &r_1000_1000, N_("Orgin X") }, 
#define I_ORIGY (2)
    /*2*/ { PD_FLOAT, &sensorEditOrig.y, "origy", PDO_DIM, &r_1000_1000, N_("Origin Y") },
#define I_SENSORSCRIPT (3)
    /*3*/ { PD_STRING, sensorEditScript, "script", PDO_NOPREF|PDO_STRINGLIMITLENGTH, (void*)350, N_("Script"), 0, 0, sizeof(sensorEditScript)},
};

static paramGroup_t sensorEditPG = { "sensorEdit", 0, sensorEditPLs, sizeof sensorEditPLs/sizeof sensorEditPLs[0] };
static wWin_p sensorEditW;

static void SensorEditOk ( void * junk )
{
    track_p trk;
    sensorData_p xx;
    
    if (sensorEditTrack == NULL) {
        UndoStart( _("Create Sensor"), "Create Sensor");
        trk = NewTrack(0, T_SENSOR, 0, sizeof(sensorData_t));
    } else {
        UndoStart( _("Modify Sensor"), "Modify Sensor");
        trk = sensorEditTrack;
    }
    xx = GetsensorData(trk);
    xx->orig = sensorEditOrig;
    if ( xx->name == NULL || strncmp (xx->name, sensorEditName, STR_SHORT_SIZE) != 0) {
        MyFree(xx->name);
        xx->name = MyStrdup(sensorEditName);
    }
    if ( xx->script == NULL || strncmp (xx->script, sensorEditScript, STR_LONG_SIZE) != 0) {
        MyFree(xx->script);
        xx->script = MyStrdup(sensorEditScript);
    }
    UndoEnd();
    DoRedraw();
    ComputeSensorBoundingBox(trk);
    wHide( sensorEditW );
}

#if 0
static void SensorEditCancel ( wWin_p junk )
{
    wHide( sensorEditW );
}
#endif

static void EditSensorDialog()
{
    sensorData_p xx;
    
    if ( !sensorEditW ) {
        ParamRegister( &sensorEditPG );
        sensorEditW = ParamCreateDialog (&sensorEditPG,
                                          MakeWindowTitle(_("Edit sensor")),
                                          _("Ok"), SensorEditOk,
                                          wHide, TRUE, NULL,
                                          F_BLOCK,
                                          NULL );
    }
    if (sensorEditTrack == NULL) {
        sensorEditName[0] = '\0';
        sensorEditScript[0] = '\0';
    } else {
        xx = GetsensorData ( sensorEditTrack );
        strncpy(sensorEditName,xx->name,STR_SHORT_SIZE);
        strncpy(sensorEditScript,xx->script,STR_LONG_SIZE);
        sensorEditOrig = xx->orig;
    }
    ParamLoadControls( &sensorEditPG );
    wShow( sensorEditW );
}

static void EditSensor (track_p trk)
{
    sensorEditTrack = trk;
    EditSensorDialog();
}

static void CreateNewSensor (coOrd orig)
{
    sensorEditOrig = orig;
    sensorEditTrack = NULL;
    EditSensorDialog();
}

static STATUS_T CmdSensor ( wAction_t action, coOrd pos )
{
    switch (action) {
    case C_START:
        InfoMessage(_("Place sensor"));
        return C_CONTINUE;
    case C_DOWN:
	case C_MOVE:
		SnapPos(&pos);
        DDrawSensor( &tempD, pos, GetScaleRatio(GetLayoutCurScale()), wDrawColorBlack );
        return C_CONTINUE;
    case C_UP:
        SnapPos(&pos);
        DDrawSensor( &tempD, pos, GetScaleRatio(GetLayoutCurScale()), wDrawColorBlack );
        CreateNewSensor(pos);
        return C_TERMINATE;
    case C_REDRAW:
    case C_CANCEL:
        DDrawSensor( &tempD, pos, GetScaleRatio(GetLayoutCurScale()), wDrawColorBlack );
        return C_CONTINUE;
    default:
        return C_CONTINUE;
    }
}

static coOrd ctlhiliteOrig, ctlhiliteSize;
static POS_T ctlhiliteBorder;
static wDrawColor ctlhiliteColor = 0;
static void DrawSensorTrackHilite( void )
{
	wPos_t x, y, w, h;
	if (ctlhiliteColor==0)
		ctlhiliteColor = wDrawColorGray(87);
	w = (wPos_t)((ctlhiliteSize.x/mainD.scale)*mainD.dpi+0.5);
	h = (wPos_t)((ctlhiliteSize.y/mainD.scale)*mainD.dpi+0.5);
	mainD.CoOrd2Pix(&mainD,ctlhiliteOrig,&x,&y);
	wDrawFilledRectangle( tempD.d, x, y, w, h, ctlhiliteColor, wDrawOptTemp );
}

static int SensorMgmProc ( int cmd, void * data )
{
    track_p trk = (track_p) data;
    sensorData_p xx = GetsensorData(trk);
    /*char msg[STR_SIZE];*/
    
    switch ( cmd ) {
    case CONTMGM_CAN_EDIT:
        return TRUE;
        break;
    case CONTMGM_DO_EDIT:
        EditSensor(trk);
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
            DrawSensorTrackHilite();
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
            DrawSensorTrackHilite();
            xx->IsHilite = FALSE;
        }
        break;
    case CONTMGM_GET_TITLE:
        sprintf(message,"\t%s\t",xx->name);
        break;
    }
    return FALSE;
}

#include "bitmaps/sensor.xpm"

EXPORT void SensorMgmLoad ( void )
{
    track_p trk;
    static wIcon_p sensorI = NULL;
    
    if (sensorI == NULL) {
        sensorI = wIconCreatePixMap( sensor_xpm );
    }
    
    TRK_ITERATE(trk) {
        if (GetTrkType(trk) != T_SENSOR) continue;
        ContMgmLoad (sensorI, SensorMgmProc, (void *) trk );
    }
}

#define ACCL_SENSOR 0

EXPORT void InitCmdSensor ( wMenu_p menu )
{
    AddMenuButton( menu, CmdSensor, "cmdSensor", _("Sensor"), 
                   wIconCreatePixMap( sensor_xpm ), LEVEL0_50, IC_STICKY|IC_POPUP2, ACCL_SENSOR, NULL );
}

EXPORT void InitTrkSensor ( void )
{
    T_SENSOR = InitObject ( &sensorCmds );
    log_sensor = LogFindIndex ( "sensor" );
}
