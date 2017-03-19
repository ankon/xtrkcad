/** \file cswitchmotor.c
 * Switch Motors
 */

/* Created by Robert Heller on Sat Mar 14 10:39:56 2009
 * ------------------------------------------------------------------
 * Modification History: $Log: not supported by cvs2svn $
 * Modification History: Revision 1.5  2009/11/23 19:46:16  rheller
 * Modification History: Block and Switchmotor updates
 * Modification History:
 * Modification History: Revision 1.4  2009/09/16 18:32:24  m_fischer
 * Modification History: Remove unused locals
 * Modification History:
 * Modification History: Revision 1.3  2009/09/05 16:40:53  m_fischer
 * Modification History: Make layout control commands a build-time choice
 * Modification History:
 * Modification History: Revision 1.2  2009/07/08 19:13:58  m_fischer
 * Modification History: Make compile under MSVC
 * Modification History:
 * Modification History: Revision 1.1  2009/07/08 18:40:27  m_fischer
 * Modification History: Add switchmotor and block for layout control
 * Modification History:
 * Modification History: Revision 1.1  2002/07/28 14:03:50  heller
 * Modification History: Add it copyright notice headers
 * Modification History:
 * ------------------------------------------------------------------
 * Contents:
 * ------------------------------------------------------------------
 *
 *     Generic Project
 *     Copyright (C) 2005  Robert Heller D/B/A Deepwoods Software
 * 			51 Locke Hill Road
 * 			Wendell, MA 01379-9728
 *
 *     This program is free software; you can redistribute it and/or modify
 *     it under the terms of the GNU General Public License as published by
 *     the Free Software Foundation; either version 2 of the License, or
 *     (at your option) any later version.
 *
 *     This program is distributed in the hope that it will be useful,
 *     but WITHOUT ANY WARRANTY; without even the implied warranty of
 *     MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *     GNU General Public License for more details.
 *
 *     You should have received a copy of the GNU General Public License
 *     along with this program; if not, write to the Free Software
 *     Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 *
 *
 */

#include <ctype.h>
#include "track.h"
#include "trackx.h"
#include "compound.h"
#include "i18n.h"

EXPORT TRKTYP_T T_SWITCHMOTOR = -1;

static int log_switchmotor = 0;

static track_p switchmotorTurnout;

static char switchmotorEditName[STR_SHORT_SIZE];
static char switchmotorEditNormal[STR_LONG_SIZE];
static char switchmotorEditReverse[STR_LONG_SIZE];
static char switchmotorEditPointSense[STR_LONG_SIZE];
static long int switchmotorEditTonum;
static track_p switchmotorEditTrack;

static paramIntegerRange_t r0_999999 = { 0, 999999 };

static paramData_t switchmotorEditPLs[] = {
/*0*/ { PD_STRING, switchmotorEditName, "name", PDO_NOPREF, (void*)200, N_("Name") },
/*1*/ { PD_STRING, switchmotorEditNormal, "normal", PDO_NOPREF, (void*)350, N_("Normal") },
/*2*/ { PD_STRING, switchmotorEditReverse, "reverse", PDO_NOPREF, (void*)350, N_("Reverse") },
/*3*/ { PD_STRING, switchmotorEditPointSense, "pointSense", PDO_NOPREF, (void*)350, N_("Point Sense") },
/*4*/ { PD_LONG,   &switchmotorEditTonum, "turnoutNumber", PDO_NOPREF, &r0_999999, N_("Turnout Number"), BO_READONLY }, 
};

static paramGroup_t switchmotorEditPG = { "switchmotorEdit", 0, switchmotorEditPLs, sizeof switchmotorEditPLs/sizeof switchmotorEditPLs[0] };
static wWin_p switchmotorEditW;

/*
static dynArr_t switchmotorTrk_da;
#define switchmotorTrk(N) DYNARR_N( track_p , switchmotorTrk_da, N )
*/

typedef struct switchmotorData_t {
    char * name;
    char * normal;
    char * reverse;
    char * pointsense;
    BOOL_T IsHilite;
    TRKINX_T turnindx;
    track_p turnout;
} switchmotorData_t, *switchmotorData_p;

static switchmotorData_p GetswitchmotorData ( track_p trk )
{
	return (switchmotorData_p) GetTrkExtraData(trk);
}


static coOrd switchmotorPoly_Pix[] = {
    {6,0}, {6,13}, {4,13}, {4,19}, {6,19}, {6,23}, {9,23}, {9,19}, {13,19},
    {13,23}, {27,23}, {27,10}, {13,10}, {13,13}, {9,13}, {9,0}, {6,0} };
#define switchmotorPoly_CNT (sizeof(switchmotorPoly_Pix)/sizeof(switchmotorPoly_Pix[0]))
#define switchmotorPoly_SF (3.0)

static void ComputeSwitchMotorBoundingBox (track_p t)
{
    coOrd hi, lo, p;
    switchmotorData_p data_p = GetswitchmotorData(t);
    struct extraData *xx = GetTrkExtraData(data_p->turnout);
    coOrd orig = xx->orig;
    ANGLE_T angle = xx->angle;
    SCALEINX_T s = GetTrkScale(data_p->turnout);
    DIST_T scaleRatio = GetScaleRatio(s);
    int iPoint;
    ANGLE_T x_angle, y_angle;
    
    x_angle = 90-(360-angle);
    if (x_angle < 0) x_angle += 360;
    y_angle = -(360-angle);
    if (y_angle < 0) y_angle += 360;
    
    
    for (iPoint = 0; iPoint < switchmotorPoly_CNT; iPoint++) {
        Translate (&p, orig, x_angle, switchmotorPoly_Pix[iPoint].x * switchmotorPoly_SF / scaleRatio );
        Translate (&p, p, y_angle, (10+switchmotorPoly_Pix[iPoint].y) * switchmotorPoly_SF / scaleRatio );
        if (iPoint == 0) {
            lo = p;
            hi = p;
        } else {
            if (p.x < lo.x) lo.x = p.x;
            if (p.y < lo.y) lo.y = p.y;
            if (p.x > hi.x) hi.x = p.x;
            if (p.y > hi.y) hi.y = p.y;
        }
    }
    SetBoundingBox(t, hi, lo);
}
    
    
static void DrawSwitchMotor (track_p t, drawCmd_p d, wDrawColor color )
{
    coOrd p[switchmotorPoly_CNT];
    switchmotorData_p data_p = GetswitchmotorData(t);
    struct extraData *xx = GetTrkExtraData(data_p->turnout);
    coOrd orig = xx->orig;
    ANGLE_T angle = xx->angle;
    SCALEINX_T s = GetTrkScale(data_p->turnout);
    DIST_T scaleRatio = GetScaleRatio(s);
    int iPoint;
    ANGLE_T x_angle, y_angle;
    
    x_angle = 90-(360-angle);
    if (x_angle < 0) x_angle += 360;
    y_angle = -(360-angle);
    if (y_angle < 0) y_angle += 360;
    
    
    for (iPoint = 0; iPoint < switchmotorPoly_CNT; iPoint++) {
        Translate (&p[iPoint], orig, x_angle, switchmotorPoly_Pix[iPoint].x * switchmotorPoly_SF / scaleRatio );
        Translate (&p[iPoint], p[iPoint], y_angle, (10+switchmotorPoly_Pix[iPoint].y) * switchmotorPoly_SF / scaleRatio );
    }
    DrawFillPoly(d, switchmotorPoly_CNT, p,  wDrawColorBlack);
}

static DIST_T DistanceSwitchMotor (track_p t, coOrd * p )
{
	switchmotorData_p xx = GetswitchmotorData(t);
        if (xx->turnout == NULL) return 0;
	return GetTrkDistance(xx->turnout,*p);
}

static void DescribeSwitchMotor (track_p trk, char * str, CSIZE_T len )
{
    switchmotorData_p xx = GetswitchmotorData(trk);
    long listLabelsOption = listLabels;
    
    LOG( log_switchmotor, 1, ("*** DescribeSwitchMotor(): trk is T%d\n",GetTrkIndex(trk)))
    FormatCompoundTitle( listLabelsOption, xx->name );
    if (message[0] == '\0')
        FormatCompoundTitle( listLabelsOption|LABEL_DESCR, xx->name );
    strcpy( str, _(GetTrkTypeName( trk )) );
    str++;
    while (*str) {
        *str = tolower((unsigned char)*str);
        str++;
    }
    sprintf( str, _("(%d): Layer=%d %s"),
             GetTrkIndex(trk), GetTrkLayer(trk)+1, message );
}

static void switchmotorDebug (track_p trk)
{
	switchmotorData_p xx = GetswitchmotorData(trk);
	LOG( log_switchmotor, 1, ("*** switchmotorDebug(): trk = %08x\n",trk))
	LOG( log_switchmotor, 1, ("*** switchmotorDebug(): Index = %d\n",GetTrkIndex(trk)))
	LOG( log_switchmotor, 1, ("*** switchmotorDebug(): name = \"%s\"\n",xx->name))
	LOG( log_switchmotor, 1, ("*** switchmotorDebug(): normal = \"%s\"\n",xx->normal))
	LOG( log_switchmotor, 1, ("*** switchmotorDebug(): reverse = \"%s\"\n",xx->reverse))
	LOG( log_switchmotor, 1, ("*** switchmotorDebug(): pointsense = \"%s\"\n",xx->pointsense))
        LOG( log_switchmotor, 1, ("*** switchmotorDebug(): turnindx = %d\n",xx->turnindx))
        if (xx->turnout != NULL) {       
             LOG( log_switchmotor, 1, ("*** switchmotorDebug(): turnout = T%d, %s\n",
                                       GetTrkIndex(xx->turnout), GetTrkTypeName(xx->turnout)))
        }
}

static void DeleteSwitchMotor ( track_p trk )
{
        LOG( log_switchmotor, 1,("*** DeleteSwitchMotor(%p)\n",trk))
        LOG( log_switchmotor, 1,("*** DeleteSwitchMotor(): index is %d\n",GetTrkIndex(trk)))
        switchmotorData_p xx = GetswitchmotorData(trk);
        LOG( log_switchmotor, 1,("*** DeleteSwitchMotor(): xx = %p, xx->name = %p, xx->normal = %p, xx->reverse = %p, xx->pointsense = %p\n",
                xx,xx->name,xx->normal,xx->reverse,xx->pointsense))
	MyFree(xx->name); xx->name = NULL;
	MyFree(xx->normal); xx->normal = NULL;
	MyFree(xx->reverse); xx->reverse = NULL;
	MyFree(xx->pointsense); xx->pointsense = NULL;
}

static BOOL_T WriteSwitchMotor ( track_p t, FILE * f )
{
	BOOL_T rc = TRUE;
	switchmotorData_p xx = GetswitchmotorData(t);
    
        if (xx->turnout == NULL) return FALSE;
	rc &= fprintf(f, "SWITCHMOTOR %d %d \"%s\" \"%s\" \"%s\" \"%s\"\n",
		GetTrkIndex(t), GetTrkIndex(xx->turnout), xx->name,
		xx->normal, xx->reverse, xx->pointsense)>0;
	return rc;
}

static void ReadSwitchMotor ( char * line )
{
	TRKINX_T trkindex;
	wIndex_t index;
	track_p trk;
	switchmotorData_p xx;
	char *name, *normal, *reverse, *pointsense;

	LOG( log_switchmotor, 1, ("*** ReadSwitchMotor: line is '%s'\n",line))
	if (!GetArgs(line+12,"ddqqqq",&index,&trkindex,&name,&normal,&reverse,&pointsense)) {
		return;
	}
	trk = NewTrack(index, T_SWITCHMOTOR, 0, sizeof(switchmotorData_t)+1);
	xx = GetswitchmotorData( trk );
	xx->name = name;
	xx->normal = normal;
	xx->reverse = reverse;
	xx->pointsense = pointsense;
        xx->turnindx = trkindex;
        LOG( log_switchmotor, 1,("*** ReadSwitchMotor(): trk = %p (%d), xx = %p\n",trk,GetTrkIndex(trk),xx))
        LOG( log_switchmotor, 1,("*** ReadSwitchMotor(): name = %p, normal = %p, reverse = %p, pointsense = %p\n",
                name,normal,reverse,pointsense))
        switchmotorDebug(trk);
}

EXPORT void ResolveSwitchmotorTurnout ( track_p trk )
{
    LOG( log_switchmotor, 1,("*** ResolveSwitchmotorTurnout(%p)\n",trk))
    switchmotorData_p xx;
    track_p t_trk;
    if (GetTrkType(trk) != T_SWITCHMOTOR) return;
    xx = GetswitchmotorData(trk);
    LOG( log_switchmotor, 1, ("*** ResolveSwitchmotorTurnout(%d)\n",GetTrkIndex(trk)))
    t_trk = FindTrack(xx->turnindx);
    if (t_trk == NULL) {
        NoticeMessage( _("ResolveSwitchmotor: Turnout T%d: T%d doesn't exist"), _("Continue"), NULL, GetTrkIndex(trk), xx->turnindx );
    }
    xx->turnout = t_trk;
    ComputeSwitchMotorBoundingBox(trk);
    LOG( log_switchmotor, 1,("*** ResolveSwitchmotorTurnout(): t_trk = (%d) %p\n",xx->turnindx,t_trk))
}

static void MoveSwitchMotor (track_p trk, coOrd orig ) {}
static void RotateSwitchMotor (track_p trk, coOrd orig, ANGLE_T angle ) {}
static void RescaleSwitchMotor (track_p trk, FLOAT_T ratio ) {}


static trackCmd_t switchmotorCmds = {
	"SWITCHMOTOR",
	DrawSwitchMotor,
	DistanceSwitchMotor,
	DescribeSwitchMotor,
	DeleteSwitchMotor,
	WriteSwitchMotor,
	ReadSwitchMotor,
	MoveSwitchMotor,
	RotateSwitchMotor,
	RescaleSwitchMotor,
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
	NULL, /* flip */
	NULL, /* drawPositionIndicator */
	NULL, /* advancePositionIndicator */
	NULL, /* checkTraverse */
	NULL, /* makeParallel */
	NULL  /* drawDesc */
};

static track_p FindSwitchMotor (track_p trk)
{
	track_p a_trk;
	switchmotorData_p xx;

	for (a_trk = NULL; TrackIterate( &a_trk ) ;) {
		if (GetTrkType(a_trk) == T_SWITCHMOTOR) {
			xx =  GetswitchmotorData(a_trk);
			if (xx->turnout == trk) return a_trk;
		}
	}
	return NULL;
}

static void EditSwitchMotor (track_p trk);

static STATUS_T CmdSwitchMotorCreate( wAction_t action, coOrd pos )
{
    track_p trk;
    
    LOG( log_switchmotor, 1, ("*** CmdSwitchMotorCreate(%08x,{%f,%f})\n",action,pos.x,pos.y))
    switch (action & 0xFF) {
    case C_START:
        InfoMessage( _("Select a turnout") );
        return C_CONTINUE;
    case C_DOWN:
        if ((trk = OnTrack(&pos, TRUE, TRUE )) == NULL) {
            return C_CONTINUE;
        }
        if (GetTrkType( trk ) != T_TURNOUT) {
            ErrorMessage( _("Not a turnout!") );
            return C_CONTINUE;
        }
        switchmotorTurnout = trk;
        EditSwitchMotor( NULL );
        return C_CONTINUE;
    case C_REDRAW:
        return C_CONTINUE;
    case C_CANCEL:
        return C_TERMINATE;
    default:
        return C_CONTINUE;
    }
}


static void SwitchMotorEditOk ( void * junk )
{
    switchmotorData_p xx;
    track_p trk;

    LOG( log_switchmotor, 1, ("*** SwitchMotorEditOk()\n"))
    ParamUpdate (&switchmotorEditPG );
    if ( switchmotorEditName[0]==0 ) {
        NoticeMessage( _("Switch motor must have a name!") , _("Ok"), NULL);
        return;
    }
    wDrawDelayUpdate( mainD.d, TRUE );
    if (switchmotorEditTrack == NULL) {
        UndoStart( _("Create Switch Motor"), "Create Switch Motor" );
        /* Create a switchmotor object */
        trk = NewTrack(0, T_SWITCHMOTOR, 0, sizeof(switchmotorData_t)+1); 
        xx = GetswitchmotorData( trk );
        xx->turnout = switchmotorTurnout;
    } else {
        UndoStart( _("Modify Switch Motor"), "Modify Switch Motor" );
        trk = switchmotorEditTrack;
        xx = GetswitchmotorData( trk );
    }
    if (xx->name == NULL || strcmp(xx->name,switchmotorEditName) != 0) {
        MyFree(xx->name);
        xx->name = MyStrdup(switchmotorEditName);
    }
    if (xx->normal == NULL || strcmp(xx->normal,switchmotorEditNormal) != 0) {
        MyFree(xx->normal);
        xx->normal = MyStrdup(switchmotorEditNormal);
    }
    if (xx->reverse == NULL || strcmp(xx->reverse,switchmotorEditReverse) != 0) {
        MyFree(xx->reverse);
        xx->reverse = MyStrdup(switchmotorEditReverse);
    }
    if (xx->pointsense == NULL || strcmp(xx->pointsense,switchmotorEditPointSense) != 0) {
        MyFree(xx->pointsense);
        xx->pointsense = MyStrdup(switchmotorEditPointSense);
    }
    switchmotorDebug(trk);
    UndoEnd();
    wHide( switchmotorEditW );
    ComputeSwitchMotorBoundingBox(trk);
    DrawNewTrack(trk);
}


static void EditSwitchMotor (track_p trk)
{
    switchmotorData_p xx;
    
    if ( trk == NULL ) {
        if (switchmotorTurnout == NULL) switchmotorEditTonum = 0; 
        else switchmotorEditTonum = GetTrkIndex(switchmotorTurnout);
    } else {
        xx = GetswitchmotorData(trk);
        strncpy(switchmotorEditName,xx->name,STR_SHORT_SIZE);
        strncpy(switchmotorEditNormal,xx->normal,STR_LONG_SIZE);
        strncpy(switchmotorEditReverse,xx->reverse,STR_LONG_SIZE);
        strncpy(switchmotorEditPointSense,xx->pointsense,STR_LONG_SIZE);
        if (xx->turnout == NULL) switchmotorEditTonum = 0;
        else switchmotorEditTonum = GetTrkIndex(xx->turnout);
    }
    switchmotorEditTrack = trk;
    if ( !switchmotorEditW ) {
        ParamRegister( &switchmotorEditPG );
        switchmotorEditW = ParamCreateDialog (&switchmotorEditPG, 
                                              MakeWindowTitle(_("Edit switch motor")), 
                                              _("Ok"), SwitchMotorEditOk, 
                                              wHide, TRUE, NULL, F_BLOCK, 
                                              NULL );
    }
    ParamLoadControls( &switchmotorEditPG );
    if (trk == NULL) {
        sprintf( message, _("Create new motor"));
    } else {
        sprintf( message, _("Edit switch motor %d"), GetTrkIndex(trk) );
    }
    wWinSetTitle( switchmotorEditW, message );
    wShow (switchmotorEditW);
}

static coOrd swmhiliteOrig, swmhiliteSize;
static POS_T swmhiliteBorder;
static wDrawColor swmhiliteColor = 0;
static void DrawSWMotorTrackHilite( void )
{
	wPos_t x, y, w, h;
	if (swmhiliteColor==0)
		swmhiliteColor = wDrawColorGray(87);
	w = (wPos_t)((swmhiliteSize.x/mainD.scale)*mainD.dpi+0.5);
	h = (wPos_t)((swmhiliteSize.y/mainD.scale)*mainD.dpi+0.5);
	mainD.CoOrd2Pix(&mainD,swmhiliteOrig,&x,&y);
	wDrawFilledRectangle( mainD.d, x, y, w, h, swmhiliteColor, wDrawOptTemp );
}

static int SwitchmotorMgmProc ( int cmd, void * data )
{
    track_p trk = (track_p) data;
    switchmotorData_p xx = GetswitchmotorData(trk);
    /*char msg[STR_SIZE];*/
    
    switch ( cmd ) {
    case CONTMGM_CAN_EDIT:
        return TRUE;
        break;
    case CONTMGM_DO_EDIT:
        EditSwitchMotor (trk);
        return TRUE;
        break;
    case CONTMGM_CAN_DELETE:
        return TRUE;
        break;
    case CONTMGM_DO_DELETE:
        DeleteTrack (trk, FALSE);
        return TRUE;
        break;
    case CONTMGM_DO_HILIGHT:
        if (xx->turnout != NULL && !xx->IsHilite) {
            swmhiliteBorder = mainD.scale*0.1;
            if ( swmhiliteBorder < trackGauge ) swmhiliteBorder = trackGauge;
            GetBoundingBox( xx->turnout, &swmhiliteSize, &swmhiliteOrig );
            swmhiliteOrig.x -= swmhiliteBorder;
            swmhiliteOrig.y -= swmhiliteBorder;
            swmhiliteSize.x -= swmhiliteOrig.x-swmhiliteBorder;
            swmhiliteSize.y -= swmhiliteOrig.y-swmhiliteBorder;
            DrawSWMotorTrackHilite();
            xx->IsHilite = TRUE;
        }
        break;
    case CONTMGM_UN_HILIGHT:
        if (xx->turnout != NULL && xx->IsHilite) {
            swmhiliteBorder = mainD.scale*0.1;
            if ( swmhiliteBorder < trackGauge ) swmhiliteBorder = trackGauge;
            GetBoundingBox( xx->turnout, &swmhiliteSize, &swmhiliteOrig );
            swmhiliteOrig.x -= swmhiliteBorder;
            swmhiliteOrig.y -= swmhiliteBorder;
            swmhiliteSize.x -= swmhiliteOrig.x-swmhiliteBorder;
            swmhiliteSize.y -= swmhiliteOrig.y-swmhiliteBorder;
            DrawSWMotorTrackHilite();
            xx->IsHilite = FALSE;
        }
        break;
    case CONTMGM_GET_TITLE:
        if (xx->turnout == NULL) {
            sprintf( message, "\t%s\t%d", xx->name, 0);
        } else {
            sprintf( message, "\t%s\t%d", xx->name, GetTrkIndex(xx->turnout));
        }
        break;
    }
    return FALSE;
}

//#include "bitmaps/switchmotor.xpm"

//#include "bitmaps/switchmnew.xpm"
//#include "bitmaps/switchmedit.xpm"
//#include "bitmaps/switchmdel.xpm"
#include "bitmaps/switchm.xpm"

EXPORT void SwitchmotorMgmLoad( void )
{
    track_p trk;
    static wIcon_p switchmI = NULL;
    
    if ( switchmI == NULL)
        switchmI = wIconCreatePixMap( switchm_xpm );
    
    TRK_ITERATE(trk) {
        if (GetTrkType(trk) != T_SWITCHMOTOR) continue;
        ContMgmLoad( switchmI, SwitchmotorMgmProc, (void *)trk );
    }
}

EXPORT void InitCmdSwitchMotor( wMenu_p menu )
{
    /*switchmotorName[0] = '\0';*/
    /*switchmotorNormal[0] = '\0';*/
    /*switchmotorReverse[0] = '\0';*/
    /*switchmotorPointSense[0] = '\0';*/
    AddMenuButton( menu, CmdSwitchMotorCreate, "cmdSwitchMotorCreate", 
                   _("Switch Motor"), wIconCreatePixMap( switchm_xpm ),
                   LEVEL0_50, IC_STICKY|IC_POPUP2, ACCL_SWITCHMOTOR1, 
                   NULL );
    /*ParamRegister( &switchmotorPG );*/
}
EXPORT void CheckDeleteSwitchmotor(track_p t)
{
    track_p sm;
    switchmotorData_p xx;
    
    sm = FindSwitchMotor( t );
    if (sm == NULL) return;
    xx = GetswitchmotorData (sm);
    NoticeMessage(_("Deleting Switch Motor %s"),_("Ok"),NULL,xx->name);
    DeleteTrack (sm, FALSE);
}



EXPORT void InitTrkSwitchMotor( void )
{
	T_SWITCHMOTOR = InitObject ( &switchmotorCmds );
	log_switchmotor = LogFindIndex ( "switchmotor" );
}


