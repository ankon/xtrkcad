/** \file csignal.c
 * Signals
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
 *  Created       : Sun Feb 19 13:11:45 2017
 *  Last Modified : <170417.1113>
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
#include <math.h>

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
#include "common.h"
#include "condition.h"
#include "paramfile.h"
#include "csignal.h"




EXPORT TRKTYP_T T_SIGNAL = -1;

#define SIGNAL_DISPLAY_PLAN (1)
#define SIGNAL_DISPLAY_DIAG (0)
#define SIGNAL_DISPLAY_ELEV (2)

EXPORT long SignalDisplay = SIGNAL_DISPLAY_DIAG;

EXPORT long SignalSide = SIGNAL_SIDE_LEFT;

EXPORT double SignalDiagramFactor = 1.0;

EXPORT double SignalOffset = 0.0;

static int log_signal = 0;


static drawCmd_t signalD = {
	NULL,
	&screenDrawFuncs,
	0,
	1.0,
	0.0,
	{0.0,0.0}, {0.0,0.0},
	Pix2CoOrd, CoOrd2Pix };

static wWin_p signalW;

static char signalName[STR_SHORT_SIZE];
static int  signalHeadCount;

static void ComputeSignalBoundingBox (track_p, long);

static wIndex_t signalHotBarCmdInx;
static wIndex_t signalInx;
static long hideSignalWindow;
static void RedrawSignal(void);

static void SelSignalAspect(wIndex_t, coOrd);

static wPos_t signalListWidths[] = { 80, 80, 220 };
static const char * signalListTitles[] = { N_("Manufacturer"), N_("Part No"), N_("Description") };
static paramListData_t listData = { 13, 400, 3, signalListWidths, signalListTitles };
static const char * hideLabels[] = { N_("Hide"), NULL };
static paramDrawData_t signalDrawData = { 490, 200, (wDrawRedrawCallBack_p)RedrawSignal, SelSignalAspect, &signalD };
static paramData_t signalPLs[] = {
#define I_LIST		(0)
#define signalListL    ((wList_p)signalPLs[I_LIST].control)
	{   PD_LIST, &signalInx, "list", PDO_NOPREF|PDO_DLGRESIZEW, &listData, NULL, BL_DUP },
#define I_DRAW		(1)
#define signalDrawD    ((wDraw_p)signalPLs[I_DRAW].control)
	{   PD_DRAW, NULL, "canvas", PDO_NOPSHUPD|PDO_DLGUNDERCMDBUTT|PDO_DLGRESIZE, &signalDrawData, NULL, 0 },
#define I_NEW		(2)
#define signalNewM     ((wMenu_p)signalPLs[I_NEW].control)
	{   PD_MENU, NULL, "new", PDO_DLGCMDBUTTON, NULL, N_("New") },
#define I_HIDE		(3)
#define signalHideT    ((wChoice_p)signalPLs[I_HIDE].control)
	{   PD_TOGGLE, &hideSignalWindow, "hide", PDO_DLGCMDBUTTON, /*CAST_AWAY_CONST*/(void*)hideLabels, NULL, BC_NOBORDER } };
static paramGroup_t signalPG = { "Signal", 0, signalPLs, sizeof signalPLs/sizeof signalPLs[0] };


EXPORT dynArr_t signalData_da;       //HotBar Signals - These can be picked from but are not in Layout


static signalAspectType_t defaultAspectsMap[] = {
		{N_("None"), ASPECT_NONE},
		{N_("Danger"), ASPECT_DANGER},
		{N_("Proceed"), ASPECT_PROCEED},
		{N_("Caution"), ASPECT_CAUTION},
		{N_("Flash-Caution"), ASPECT_FLASHCAUTION},
		{N_("Preliminary-Caution"), ASPECT_FLASHCAUTION},
		{N_("Flash-Preliminary-Caution"), ASPECT_PRELIMINARYCAUTION},
		{N_("Off"), ASPECT_OFF},
		{N_("On"), ASPECT_ON},
		{N_("Call-On"), ASPECT_CALLON},
		{N_("Shunt"), ASPECT_SHUNT},
		{N_("Warning"), ASPECT_WARNING},
		{N_(""),-2}
};




#define headaspect(N) DYNARR_N( headAspectMap_t, headMap, N )

static signalIndicatorType_t defaultBaseIndicatorsMap[] =
{
		{N_("Diagram"),IND_DIAGRAM,ASPECT_NONE},
		{N_("Unlit"),IND_UNLIT,ASPECT_NONE},
		{N_("Red"),IND_RED,ASPECT_DANGER},
		{N_("Green"),IND_GREEN,ASPECT_PROCEED},
		{N_("Yellow"),IND_YELLOW,ASPECT_CAUTION},
		{N_("Lunar"),IND_LUNAR,ASPECT_PRELIMINARYCAUTION},
		{N_("Flash Red"),IND_FLASHRED,ASPECT_DANGER},
		{N_("Flash Green"),IND_FLASHGREEN,ASPECT_SHUNT},
		{N_("Flash Yellow"),IND_FLASHYELLOW,ASPECT_FLASHCAUTION},
		{N_("Flash Lunar"),IND_FLASHLUNAR,ASPECT_FLASHPRELIMINARYCAUTION},
		{N_("On"),IND_ON,ASPECT_ON},
		{N_("Off"),IND_OFF,ASPECT_OFF},
		{N_("Lit"),IND_LIT,ASPECT_CALLON},
};


static dynArr_t headTypes_da;			//Array of headTypes of all signals in use in the layout
#define signalHeadType(N) DYNARR_N( signalHeadType_t, headTypes_da, N )

static dynArr_t signalHead_da;          //Array of active heads
#define signalHead(N) DYNARR_N( signalHead_t, signalHead_da, N )

static dynArr_t baseSignalAspect_da;		//Array of signals Aspects
#define basesSignalAspect(N) DYNARR_N( signalAspect_t, signalAspect_da, N )

static dynArr_t signalAspect_da;

#define signalAspect(N) DYNARR_N(signalAspect_t, signalAspect_da, N)

#define headMap(N,M) DYNARR_N( headAspectMap_t, signalAspect(N),M)


/*
 *
 * A Real Signal (Mast) on the Layout. This will contain an arbitrary number of Heads.
 *
 * For display, the signal segs are first,
 * all the Heads ->segs are drawn,
 * the Active Aspect -> HeadAspectMap entries -> Appearances -> segs for each Head by Appearance
 */
typedef struct signalData_t {
    coOrd orig;							//Relative to a track/structure or to the origin
    ANGLE_T angle;						//Relative to a track/structure or to the origin
    TRKINX_T index;						//Index Number of Track
    track_p track;						//Track/Structure for Signal to align to or NULL
    EPINX_T ep;							//EndPoint of Track
    char * signalName;					//Unique Name of Signal
    char * plate;						//Signal #
    char * title;						//The definition Name
    dynArr_t signalHeads;				//Array of all the heads
    BOOL_T IsHilite;
    dynArr_t signalAspects;			    //Array of all the Aspects for the Signal
    dynArr_t staticSignalSegs[3];		//Static draw elements for the signal (like dolls/posts)
    dynArr_t currSegs;					//The current set of Segs to draw
    dynArr_t signalGroups;				//Grouped Head/Appearances
    int numberHeads;					//Legacy Number of Heads
    int currentAspect;					//Current Aspect index within Aspects
    DIST_T barscale;
    coOrd size;							//Drawing Bounds
    SCALEINX_T scaleInx;				//Scale of Signal Def (or SCALE_ANY)
    int paramFileIndex;
    struct signalData_t * next_signal;
    track_p signal_track;
    BOOL_T track_side;					//Left or Right of Track
	} signalData_t, *signalData_p;


EXPORT signalData_p sig_first = NULL;
EXPORT TRKINX_T max_sig_index = 0;
EXPORT signalData_p *sig_last = &sig_first;


static struct {
		signalData_p first;
		signalData_p *last;
		} savedSignalState;

EXPORT void ClearSignals() {
	signalData_p sig;
	for (sig = sig_first; sig; sig=sig->next_signal) {
	}
	sig_first = NULL;
	sig_last = &sig_first;
}

EXPORT void SaveSignals() {
	savedSignalState.first = sig_first;
	savedSignalState.last = sig_last;
}

EXPORT void RestoreSignals() {
	sig_first = savedSignalState.first;
	sig_last = savedSignalState.last;
}

static signalData_p GetSignalData ( track_p trk )
{
	return (signalData_p) GetTrkExtraData(trk);
}

EXPORT BOOL_T SignalIterate( track_p * trk )
{
	signalData_p sig1;
	if (!*trk)
		sig1 = sig_first;
	else
		sig1 = GetSignalData(*trk)->next_signal;
	while (sig1 && IsTrackDeleted(sig1->signal_track))
		sig1 = sig1->next_signal;
	if (!sig1) {
		*trk = NULL;
		return FALSE;
	}
	*trk = sig1->signal_track;
	return TRUE;
}

EXPORT signalData_t * curSignal = NULL;
EXPORT EPINX_T curSignalEp = 0;

#define head(N) DYNARR_N( signalHead_t, xx->signalHeads, N )
#define aspect(N) DYNARR_N( signalAspect_t, xx->signalAspects, N )

static signalData_p getSignalData ( track_p trk )
{
    return (signalData_p) GetTrkExtraData(trk);
}

#define BASEX 6
#define BASEY 0

#define MASTX 0
#define MASTY 12

#define HEADR 4


#define signal_SF (3.0)

/**
 * Signal shows -
 * Base (Post, etc)
 * Each Head's Base (Lamp, perhaps)
 * Each Head's current Appearance from HeadType respecting origin and angle. This is then moved to the right relative position on the Post
 *
 *\param diagram - Force mode to diagram
 *
 */
static void RebuildSignalSegs(signalData_p sp, int display) {
	CleanSegs(&sp->currSegs);
	AppendSegsToArray(&sp->currSegs,&sp->staticSignalSegs[display]);
	signalAspect_t aspect;
	DIST_T ratioh = 1.0;
	for (int i=0;i<sp->signalHeads.cnt;i++) {
		/* All Heads */
		signalHead_p head = &DYNARR_N(signalHead_t,sp->signalHeads,i);
		int start_inx = sp->currSegs.cnt;
		AppendTransformedSegs(&sp->currSegs,&head->headSegs[display],zero,zero,0.0);       //Already scaled
		if (SignalDiagramFactor != 1.0) {
			RescaleSegs(head->headSegs[display].cnt,&DYNARR_N(trkSeg_t,sp->currSegs,start_inx),SignalDiagramFactor,SignalDiagramFactor,SignalDiagramFactor);
		}
		MoveSegs(head->headSegs[display].cnt,&DYNARR_N(trkSeg_t,sp->currSegs,start_inx),head->headPos);
		int indIndex;
		switch(display) {
			case SIGNAL_DISPLAY_ELEV:
				indIndex = head->currentHeadAppearance;
				break;
			case SIGNAL_DISPLAY_DIAG:
				indIndex = 0;			//Indication 0 is Diagram
				break;
			default:
				indIndex = -1;			//Nothing for Plan
		}
		signalHeadType_p type = head->headType;
		if (type == NULL) continue;
		start_inx = sp->currSegs.cnt;

		DIST_T ratioh = (GetScaleRatio(type->headScale)*SignalDiagramFactor)/GetScaleRatio(sp->scaleInx);
		//if (display == SIGNAL_DISPLAY_ELEV) {
		//	AppendTransformedSegs(&sp->currSegs,&type->headSegs, zero, zero, 0.0);
		//	if (ratioh != 1.0) {
		//		RescaleSegs(type->headSegs.cnt,&DYNARR_N(trkSeg_t,sp->currSegs,start_inx),ratioh,ratioh,ratioh);
		//	}
		//	MoveSegs(type->headSegs.cnt,&DYNARR_N(trkSeg_t,sp->currSegs,start_inx),head->headPos);
		//}
		if ((indIndex >=0) && (type->headAppearances.cnt>0) && (indIndex <= type->headAppearances.cnt)) {
			/* Now need to be be placed relative to the rest, rotated and scaled*/
			HeadAppearance_p a = &DYNARR_N(HeadAppearance_t,type->headAppearances,indIndex);
			start_inx = sp->currSegs.cnt;
			AppendTransformedSegs(&sp->currSegs,&a->appearanceSegs, zero, a->orig, a->angle);
			if (ratioh != 1.0) {
				RescaleSegs(a->appearanceSegs.cnt,&DYNARR_N(trkSeg_t,sp->currSegs,start_inx),ratioh,ratioh,ratioh);
			}
			MoveSegs(a->appearanceSegs.cnt,&DYNARR_N(trkSeg_t,sp->currSegs,start_inx),head->headPos);
		}
	}
}

static dynArr_t anchor_array;

static void DrawSignal (track_p t, drawCmd_p d, wDrawColor color )
{
    signalData_p sp = getSignalData(t);
    d->options = DC_TICKS;
    if (sp->currSegs.cnt == 0) {
    	int display = (programMode==MODE_TRAIN)?SIGNAL_DISPLAY_ELEV:SignalDisplay;
    	RebuildSignalSegs(sp, display);
    	ComputeSignalBoundingBox (t, display);
    }
    coOrd o;
    ANGLE_T a;
    if (sp->track) {
    	a = NormalizeAngle(GetTrkEndAngle(sp->track,sp->ep)+180.0);
    	o = GetTrkEndPos(sp->track,sp->ep);
    	if (SignalDisplay == SIGNAL_DISPLAY_DIAG) {
			DYNARR_SET(trkSeg_t,anchor_array,3);
			trkSeg_p s = (trkSeg_p)anchor_array.ptr;
			for ( int inx=0; inx<3; inx++ ) {
				s[inx].type = SEG_STRLIN;
				s[inx].width = 0;
				s[inx].color = color;
			}
			coOrd p0,p1;
			DIST_T ds, ws;
			int inx;
			ds = GetTrkGauge(sp->track);
			Translate(&p0,o,a-90+(sp->track_side?180:0),ds/2);
			Translate(&p1,p0,a-90+(sp->track_side?180:0),ds*1.5);
			coOrd offset;
			offset = sp->orig;
			Rotate(&offset,zero,a);
			p1.x += offset.x;
			p1.y += offset.y;
			ANGLE_T a1 = FindAngle(p1,p0);
			s[0].u.l.pos[0] = p0;
			s[0].u.l.pos[1] = p1;
			s[1].u.l.pos[0] = p0;
			Translate( &s[1].u.l.pos[1], p0, a1+135, ds/2.0 );
			s[2].u.l.pos[0] = p0;
			Translate( &s[2].u.l.pos[1], p0, a1-135, ds/2.0 );
			DrawSegsO(d,NULL,zero,0.0,(trkSeg_p)anchor_array.ptr,anchor_array.cnt,GetTrkGauge(t),color,0);
    	}
    	coOrd o1;
    	o1.x = -(GetTrkGauge(sp->track)*2.0)*(SignalSide?-1:1)+sp->orig.x;
    	o1.y = sp->orig.y;
    	Rotate(&o1,zero,a);
    	o.x += o1.x;
    	o.y += o1.y;
    } else {
    	o = sp->orig;
    	a = sp->angle;
    }
    /*Draw entire signal at pos and angle */
    DrawSegsO(d,t,o,a,(trkSeg_p)sp->currSegs.ptr,sp->currSegs.cnt,GetTrkGauge(t),color,0);
}


EXPORT long curSignalAspect = 0;

static void SelSignalAspect(
		wIndex_t action,
		coOrd pos )
{
	if (action != C_DOWN) return;
	curSignalAspect++;
	if (curSignalAspect>=curSignal->signalAspects.cnt)
		curSignalAspect = 0;

LOG( log_signal, 3, (" selected (action=%d) %ld\n", action, curSignalAspect ) )
}



void setAspect(track_p t, int aspect) {
	signalData_p xx = getSignalData(t);
	if (aspect>xx->signalAspects.cnt) aspect = 0;
	signalAspect_p a = &DYNARR_N(signalAspect_t,xx->signalAspects,aspect);
	if (a && (a->headAspectMap.cnt != 0)) {
		for (int i=0;i<a->headAspectMap.cnt;i++) {
			headAspectMap_p ham = &DYNARR_N(headAspectMap_t,a->headAspectMap,i);
			int hi = ham->aspectMapHeadNumber;
			if (hi<xx->signalHeads.cnt) {
				signalHead_p head = &DYNARR_N(signalHead_t,xx->signalHeads,hi);
				head->currentHeadAppearance = ham->aspectMapHeadAppearanceNumber;
			}
		}
	}
	int display = (programMode==MODE_TRAIN)?SIGNAL_DISPLAY_ELEV:SignalDisplay;
	RebuildSignalSegs(xx, display);
	ComputeSignalBoundingBox (t, display);
}



static void ComputeSignalBoundingBox (track_p t, long display )
{
    coOrd lo, hi, lo2, hi2;
    signalData_p xx = getSignalData(t);
    if (xx->currSegs.cnt == 0) {
       	RebuildSignalSegs(xx, (programMode==MODE_TRAIN)?SIGNAL_DISPLAY_ELEV:SignalDisplay);
    }
    coOrd offset = zero;
    ANGLE_T a = 0.0;
	if (xx->track) {						//Reset origin if track attached
	   coOrd p = GetTrkEndPos(xx->track,xx->ep);
	   a = NormalizeAngle(GetTrkEndAngle(xx->track, xx->ep)+180);
	   offset.x = xx->orig.x-GetTrkGauge(xx->track)*2.0;
	   offset.y = xx->orig.y;
	   Rotate(&offset,zero,a);
	   offset.x +=p.x;
	   offset.y +=p.y;
	} else {
		offset = xx->orig;
		a = xx->angle;
	}
	GetSegBounds(offset,a,xx->currSegs.cnt,(trkSeg_p)xx->currSegs.ptr,&lo,&hi);
	hi.x += lo.x;
	hi.y += lo.y;
    SetBoundingBox(t, hi, lo);
}

static DIST_T DistanceSignal (track_p t, coOrd * p )
{
    signalData_p xx = getSignalData(t);
    if (xx->track) {
    	coOrd p1 = GetTrkEndPos(xx->track,xx->ep);
    	ANGLE_T a = NormalizeAngle(GetTrkEndAngle(xx->track,xx->ep)+180);
    	coOrd offset;
    	offset.x = xx->orig.x-GetTrkGauge(xx->track)*2.0;
    	offset.y = xx->orig.y;
    	Rotate(&offset,zero,a);
    	p1.x +=offset.x;
    	p1.y +=offset.y;
    	return DistanceSegs(p1,a,xx->currSegs.cnt,(trkSeg_p)xx->currSegs.ptr,p,NULL);
    }
    return FindDistance(xx->orig, *p);
}

static struct {
    char signalName[STR_SHORT_SIZE];
    long leverNum;
    coOrd pos;
    ANGLE_T orient;
    long heads;
    long trackNum;
    char aspectName[STR_SHORT_SIZE];
    char headType[STR_SHORT_SIZE];
    char headName[STR_SHORT_SIZE];
    char manuf[STR_SHORT_SIZE];
    char name[STR_SHORT_SIZE];
    char partno[STR_SHORT_SIZE];
    BOOL_T side;
} signalProperties;

typedef enum { SN, LN, PS, SD, OR, HD, HN, AS, MN, NM, PN } signalDesc_e;
static descData_t signalDesc[] = {
    /* SN */ { DESC_STRING, N_("Name"),     &signalProperties.signalName, sizeof(signalProperties.signalName) },
	/* LN */ { DESC_LONG,   N_("Lever"),    &signalProperties.leverNum },
    /* PS */ { DESC_POS,    N_("Position"), &signalProperties.pos },
	/* SD */ { DESC_LIST, 	N_("Track Side"), &signalProperties.side},
    /* OR */ { DESC_ANGLE,  N_("Angle"),    &signalProperties.orient },
    /* HN */ { DESC_STRING, N_("Head"),     &signalProperties.headName, sizeof(signalProperties.headName) },
	/* AS */ { DESC_STRING, N_("Aspect"),   &signalProperties.aspectName, sizeof(signalProperties.aspectName) },
	/* MN */ { DESC_STRING, N_("Manufacturer"), &signalProperties.manuf, sizeof(signalProperties.manuf)},
	/* NM */ { DESC_STRING, N_("Name"), &signalProperties.name, sizeof(signalProperties.name) },
	/* PN */ { DESC_STRING, N_("Part No"), &signalProperties.partno, sizeof(signalProperties.partno)},
	/* */
    { DESC_NULL } };

static void UpdateSignalProperties ( track_p trk, int inx, descData_p
                                     descUpd, BOOL_T needUndoStart )
{
    signalData_p xx = getSignalData( trk );
    const char *thename;
    char *newName,*newTitle;
    const char * manufS, * nameS, * partnoS;
    char * mP, *nP, *pP;
    int mL,nL,pL;
    BOOL_T changed, nChanged, pChanged, oChanged, titleChanged;
    coOrd pos;
    
    switch (inx) {
    case -1:
        changed = nChanged = pChanged = oChanged = FALSE;
        titleChanged = FALSE;
		ParseCompoundTitle( xtitle(xx), &mP, &mL, &nP, &nL, &pP, &pL );
		if (mP == NULL) mP = "";
		if (nP == NULL) nP = "";
		if (pP == NULL) pP = "";
		manufS = wStringGetValue( (wString_p)signalDesc[MN].control0 );
		size_t max_manustr = 256, max_partstr = 256, max_namestr = 256;
		if (signalDesc[MN].max_string)
			max_manustr = signalDesc[MN].max_string-1;
		if (strlen(manufS)>max_manustr) {
			NoticeMessage2(0, MSG_ENTERED_STRING_TRUNCATED, _("Ok"), NULL, max_manustr-1);
		}
		message[0] = '\0';
		strncat( message, manufS, max_manustr-1 );

		if ( strncmp( manufS, mP, mL ) != 0 || mL != strlen(manufS) ) {
			titleChanged = TRUE;
		}
		nameS = wStringGetValue( (wString_p)signalDesc[NM].control0 );
		max_namestr = 256;
		if (signalDesc[NM].max_string)
			max_namestr = signalDesc[NM].max_string;
		if (strlen(nameS)>max_namestr) {
			NoticeMessage2(0, MSG_ENTERED_STRING_TRUNCATED, _("Ok"), NULL, max_namestr-1);
		}
		strcat( message, "\t" );
		strncat( message, nameS, max_namestr-1 );

		partnoS = wStringGetValue( (wString_p)signalDesc[PN].control0 );
		max_partstr = 256;
		if (signalDesc[PN].max_string)
			max_partstr = signalDesc[PN].max_string;
		if (strlen(partnoS)>max_partstr) {
			NoticeMessage2(0, MSG_ENTERED_STRING_TRUNCATED, _("Ok"), NULL, max_partstr-1);
		}
		strcat( message, "\t");
		strncat( message, partnoS, max_partstr-1 );
		newTitle = MyStrdup( message );

		if ( strncmp( partnoS, pP, pL ) != 0 || pL != strlen(partnoS) ) {
					titleChanged = TRUE;
		}
		if ( ! titleChanged ) {
			MyFree(newTitle);
			return;
		}
		if (xx->title) MyFree(xx->title);
			xx->title = newTitle;


        if ((signalProperties.pos.x != xx->orig.x ||
            signalProperties.pos.y != xx->orig.y) ||
            (xx->track_side != signalProperties.side)){

            pChanged = changed = TRUE;
        }
        if (signalProperties.orient != xx->angle) {
            oChanged = changed = TRUE;
        }
        if (!changed) break;
        if (needUndoStart)
            UndoStart( _("Change Signal"), "Change Signal" );
        UndoModify( trk );
        if (nChanged) {
            MyFree(xx->signalName);
            xx->signalName = newName;
        }
        if (pChanged || oChanged) {
            UndrawNewTrack( trk );
        }
        if (pChanged) {
            xx->orig = signalProperties.pos;
            xx->track_side = signalProperties.side;
        }
        if (oChanged) {
            xx->angle = signalProperties.orient;
        }
        if (pChanged || oChanged) { 
            ComputeSignalBoundingBox( trk, SignalDisplay );
            DrawNewTrack( trk );
        }
        break;
    	case SN: break;
    		thename = wStringGetValue( (wString_p) signalDesc[NM].control0 );
			if (strcmp(thename,xx->signalName) != 0) {
				nChanged = changed = TRUE;
				unsigned int max_str = signalDesc[NM].max_string;
				if (max_str && strlen(thename)>max_str) {
					newName = MyMalloc(max_str);
					newName[max_str-1] = '\0';
					strncat(newName,thename, max_str-1);
					NoticeMessage2(0, MSG_ENTERED_STRING_TRUNCATED, _("Ok"), NULL, max_str-1);
				} else newName = MyStrdup(thename);
			}
			break;
    	case LN:
    		break;
    	case PS: break;
        case OR:
        	xx->orig.x = signalProperties.pos.x - GetTrkEndPos(xx->track,0).x;
			xx->orig.y = signalProperties.pos.y - GetTrkEndPos(xx->track,0).y;
			ComputeSignalBoundingBox( trk, SignalDisplay );
        	break;
        case SD: break;
        case HN: break;
        case AS: break;
        case MN: break;
        case NM: break;
        case PN: break;
    }
}


static void DescribeSignal (track_p trk, char * str, CSIZE_T len ) 
{
    signalData_p xx = getSignalData(trk);
    
    signalDesc[SD].control0 = NULL;

    strcpy( str, _(GetTrkTypeName( trk )) );
    str++;
    while (*str) {
        *str = tolower((unsigned char)*str);
        str++;
    }
    sprintf( str, _("(%d [%s]): Layer=%d, %d heads at %0.3f,%0.3f A%0.3f, Aspect=%s"),
             GetTrkIndex(trk), 
             xx->signalName,GetTrkLayer(trk)+1, xx->signalHeads.cnt,
             xx->orig.x, xx->orig.y,xx->angle, (DYNARR_N(signalAspect_t,xx->signalAspects,xx->currentAspect)).aspectName
			);
    strncpy(signalProperties.name,xx->signalName,STR_SHORT_SIZE-1);
    signalProperties.signalName[STR_SHORT_SIZE-1] = '\0';
    signalProperties.manuf[STR_SHORT_SIZE-1] = '\0';
    signalProperties.name[STR_SHORT_SIZE-1] = '\0';
    signalProperties.partno[STR_SHORT_SIZE-1] = '\0';
    signalProperties.pos = xx->orig;
    signalProperties.orient = xx->angle;
    signalProperties.heads = xx->signalHeads.cnt;
    signalDesc[HD].mode = DESC_RO;
    signalDesc[HN].mode = DESC_RO;
    signalDesc[AS].mode = DESC_RO;
    signalDesc[MN].mode = DESC_RO;
    signalDesc[SD].mode = 0;
    signalDesc[NM].mode = DESC_NOREDRAW;

    DoDescribe( _("Signal"), trk, signalDesc, UpdateSignalProperties );


    if (signalDesc[SD].control0 != NULL) {
		wListClear( (wList_p)signalDesc[SD].control0 );
		wListAddValue( (wList_p)signalDesc[SD].control0, _("Left"), NULL, (void*)0 );
		wListAddValue( (wList_p)signalDesc[SD].control0, _("Right"), NULL, (void*)1 );
		wListSetIndex( (wList_p)signalDesc[SD].control0, signalProperties.side);
    }

}

static void DeleteSignal ( track_p trk )
{
    wIndex_t ia;
    signalData_p xx = getSignalData(trk);
    MyFree(xx->signalName); xx->signalName = NULL;
    for (ia = 0; ia < xx->signalAspects.cnt; ia++) {
    	MyFree(DYNARR_N(signalAspect_t,xx->signalAspects,ia).aspectName);
        MyFree(DYNARR_N(signalAspect_t,xx->signalAspects,ia).aspectScript);
        signalAspect_p sa = &DYNARR_N(signalAspect_t,xx->signalAspects,ia);
        for (int hm = 0; hm < sa->headAspectMap.cnt; hm++) {
        	headAspectMap_p headm = &DYNARR_N(headAspectMap_t,sa->headAspectMap,hm);
        	MyFree(headm->aspectMapHeadAppearance);
        }
        MyFree(sa->headAspectMap.ptr); sa->headAspectMap.ptr = NULL;
        sa->headAspectMap.max = 0;
        sa->headAspectMap.cnt = 0;
    }
    MyFree(xx->signalAspects.ptr); xx->signalAspects.ptr = NULL;
    for (int h = 0; h <xx->signalHeads.cnt; h++) {
    	MyFree(DYNARR_N(signalHead_t,xx->signalHeads,h).headName);
    	MyFree(DYNARR_N(signalHead_t,xx->signalHeads,h).headTypeName);
    }
    MyFree(xx->signalHeads.ptr); xx->signalHeads.ptr = NULL;
    xx->signalHeads.max = 0;
    xx->signalHeads.cnt = 0;
    for (int g = 0; g <xx->signalGroups.cnt; g++) {
    	MyFree(DYNARR_N(signalGroup_t,xx->signalGroups,g).name);
    	signalGroup_p sg = &DYNARR_N(signalGroup_t,xx->signalGroups,g);
    	for (int a = 0; a < sg->aspects.cnt;a++) {
    		MyFree(DYNARR_N(groupAspectList_t,sg->aspects,a).aspectName);
    	}
    	MyFree(sg->aspects.ptr); sg->aspects.ptr = NULL;
    	sg->aspects.max = 0;
    	sg->aspects.cnt = 0;
    	for (int i = 0; sg->groupInstances.cnt; i++) {
    		MyFree(DYNARR_N(signalGroupInstance_t,sg->groupInstances,i).appearanceOffName);
    		MyFree(DYNARR_N(signalGroupInstance_t,sg->groupInstances,i).appearanceOnName);
    		MyFree(DYNARR_N(signalGroupInstance_t,sg->groupInstances,i).conditions);
    	}
    	MyFree(sg->groupInstances.ptr); sg->groupInstances.ptr = NULL;
    	sg->groupInstances.max = 0;
    	sg->groupInstances.cnt = 0;
    }
    MyFree(xx->signalGroups.ptr); xx->signalGroups.ptr = NULL;
    xx->signalGroups.max = 0;
    xx->signalGroups.cnt = 0;

    signalData_p xx1;
    if (sig_first == xx) sig_first = xx->next_signal;
	signalData_p sig1;
	sig1 = sig_first;
	while(sig1) {
		xx1 = sig1;
		if (xx1->next_signal == xx) {
			xx1->next_signal = xx->next_signal;
			break;
		}
		sig1 = xx1->next_signal;
	}
	if (xx == *sig_last)
		sig_last = &sig_first;


}

int WriteStaticSegs(FILE * f, dynArr_t * staticSegs) {
    int rc = TRUE;
	for (int i=0;i<3;i++) {
    	char * disp;
    	switch(i) {
    		case SIGNAL_DISPLAY_PLAN:
    			disp = "PLAN";
    			break;
    		case SIGNAL_DISPLAY_ELEV:
    			disp = "ELEV";
    			break;
    		default:
    			disp = "DIAG";
    	}
    	rc &= fprintf(f, "\tVIEW %s\n",disp)>0;
    	rc &= WriteSegs(f,staticSegs[i].cnt,staticSegs[i].ptr)>0;
    }
    return rc;
}

static BOOL_T WriteSignal ( track_p t, FILE * f )
{
    BOOL_T rc = TRUE;
    wIndex_t ia,ih,im;
    signalData_p xx = getSignalData(t);
    rc = TRUE;

    rc &= fprintf(f, "SIGNAL %d %d %s %d %0.6f %0.6f %0.6f %d \"%s\"\n",
                  GetTrkIndex(t), GetTrkLayer(t), GetTrkScaleName(t), 
                  GetTrkVisible(t), xx->orig.x, xx->orig.y, xx->angle, 
                  xx->numberHeads, xx->signalName)>0;
    if (xx->track) {
    	rc &= fprintf(f, "\tTRACK %d %d\n",xx->index,xx->ep)>0;
    }
    rc &= WriteStaticSegs(f, &xx->staticSignalSegs[0])>0;

    for (ih = 0; ih < xx->signalHeads.cnt; ih++) {
    	signalHead_p sh = &DYNARR_N(signalHead_t,xx->signalHeads,ih);
    	rc &= fprintf(f, "\tHEAD %0.6f %0.6f \"%s\" \"%s\"\n",
				(sh->headPos.x),
				(sh->headPos.y),
    			(sh->headName),
				(sh->headTypeName))>0;
    }
    for (ia = 0; ia < xx->signalAspects.cnt; ia++) {
    	char * asName = "NONE";
    	signalAspect_p sa = &DYNARR_N(signalAspect_t,xx->signalAspects,ia);
    	if (sa->aspectType) asName = sa->aspectType->aspectName;
        rc &= fprintf(f, "\tASPECT \"%s\" %s \"%s\"\n",
			  (sa->aspectName),
			  (asName),
			  (sa->aspectScript))>0;
        for (im = 0; im < sa->headAspectMap.cnt; im++) {
			rc &= fprintf(f, "\t\tASPECTMAP %d \"%s\" \n",
			  (DYNARR_N(headAspectMap_t,sa->headAspectMap,im).aspectMapHeadNumber+1),    //Readable number
			  (DYNARR_N(headAspectMap_t,sa->headAspectMap,im).aspectMapHeadAppearance))>0;
        }
        rc &= fprintf(f, "\tENDASPECT\n");
    }
    for (int gn=0; gn< xx->signalGroups.cnt; gn++) {
    	signalGroup_p sg = &DYNARR_N(signalGroup_t,xx->signalGroups,gn);
    	rc &= fprintf(f, "\tGROUP \"%s\" \n",sg->name ) >0;
    	for (int sa=0; sa<sg->aspects.cnt;sa++) {
    		groupAspectList_p al = &DYNARR_N(groupAspectList_t,sg->aspects,sa);
    		rc &= fprintf(f, "\t\tTRACKASPECT \"%s\" \n", al->aspectName) >0;
    	}
    	for (int gi=0; gi<sg->groupInstances.cnt;gi++) {
    		signalGroupInstance_p sgi = &DYNARR_N(signalGroupInstance_t,sg->groupInstances,gi);
    		rc &= fprintf(f, "\t\tINDICATE %d %s %s \"%s\"\n", sgi->headId,sgi->appearanceOnName, sgi->appearanceOffName, sgi->conditions) >0;
    	}
    	rc &= fprintf( f, "\tENDGROUP\n" )>0;
    }
    rc &= fprintf( f, "ENDSIGNAL\n" )>0;
    return rc;
}

static dynArr_t tempWriteSegs;

static BOOL_T WriteHeadType ( signalHeadType_p ht, FILE * f, SCALEINX_T out_scale ) {
	BOOL_T rc = TRUE;
	DIST_T ratio = GetScaleRatio(ht->headScale)/GetScaleRatio(out_scale);
	rc &= fprintf(f, "SIGNALHEAD %s \"%s\"\n",GetScaleName(out_scale),ht->headTypeName)>0;
	if (ht->headSegs.cnt>0) {
		CleanSegs(&tempWriteSegs);
		AppendSegsToArray(&tempWriteSegs,&ht->headSegs);
		if (ratio != 1.0)
			RescaleSegs(tempWriteSegs.cnt, (trkSeg_p)tempWriteSegs.ptr, ratio, ratio, ratio);
		rc &= WriteSegs(f,tempWriteSegs.cnt,(trkSeg_p)tempWriteSegs.ptr)>0;
	}
	for (int i=0;i<ht->headAppearances.cnt;i++) {
		HeadAppearance_p a = &DYNARR_N(HeadAppearance_t,ht->headAppearances,i);
		coOrd scaled_orig = a->orig;
		scaled_orig.x *=ratio;   //Scale rotate origin
		scaled_orig.y *=ratio;
		rc &= fprintf(f, "APPEARANCE %s %0.6f %0.6f %0.6f\n",a->appearanceName,scaled_orig.x,scaled_orig.y,a->angle)>0;
		/* Put them back if there is rotation or an offset */
		CleanSegs(&tempWriteSegs);
		AppendSegsToArray(&tempWriteSegs,&a->appearanceSegs);
		if (ratio != 1.0)
			RescaleSegs(tempWriteSegs.cnt, (trkSeg_p)tempWriteSegs.ptr, ratio, ratio, ratio);
		rc &= WriteSegs(f,tempWriteSegs.cnt,(trkSeg_p)tempWriteSegs.ptr)>0;
	}
	rc &= fprintf( f, "ENDSIGNALHEAD\n" )>0;
	return rc;
}

EXPORT BOOL_T WriteHeadTypes(FILE * f, SCALEINX_T out_scale) {
	BOOL_T rc = TRUE;
	for (int i=0;i<headTypes_da.cnt;i++) {
		signalHeadType_p ht = &DYNARR_N(signalHeadType_t,headTypes_da,i);
		//TODO Only write if used by a Signal
		rc &= WriteHeadType(ht,f,out_scale)>0;
	}
	return rc;
}

static dynArr_t signalDefs_da;
static dynArr_t signalPostDefs_da;

EXPORT BOOL_T ResolveSignalTrack ( track_p trk )
{
    int rc =0;
    track_p t_trk;
    if (GetTrkType(trk) != T_SIGNAL) return TRUE;
    signalData_p xx = getSignalData(trk);
    if (xx->index == -1 || xx->ep == -1) return 0;
    t_trk = FindTrack(xx->index);
    if ((t_trk == NULL) || (GetTrkEndPtCnt(t_trk)<xx->ep)) {
        if(NoticeMessage( _("ResolveSignal: Signal T%d %s: T%d E%d doesn't exist"), _("Continue"), NULL, GetTrkIndex(trk), xx->signalName, xx->index, xx->ep )) {
        	t_trk = NULL;
        } else {
        	rc = 4;
        }
    }
    xx->track = t_trk;
    ComputeSignalBoundingBox(trk,SignalDisplay);
    return (rc == 0);
}

/*
 * Find Appearance in Array by Name
 */
static int FindAppearanceNum( signalHeadType_p t, char * name) {
	if (t==NULL) return -1;
	for (int i=0;i<t->headAppearances.cnt;i++) {
		HeadAppearance_p a  = &DYNARR_N(HeadAppearance_t,t->headAppearances,i);
		if ((strlen(a->appearanceName) == strlen(name)) &&
				(strncmp(a->appearanceName,name,50) == 0)) {
			return i;
		}
	}
	return -1;
}

/*
 * Look up HeadType in Array By Name and Scale - match scale first and then match any
 */
static signalHeadType_p FindHeadType( char * name, SCALEINX_T scale) {
	signalHeadType_p found = NULL;
	for (int i=0;i<headTypes_da.cnt;i++) {
		signalHeadType_p ht = &DYNARR_N(signalHeadType_t,headTypes_da,i);
		if ((strlen(ht->headTypeName) == strlen(name))&& (strncmp(name,ht->headTypeName,strlen(name))==0)) {
			if (ht->headScale != SCALE_DEMO && ht->headScale == scale)     // Ignore Demo scale
				return ht;    									//Closest first
			if (ht->headScale == SCALE_ANY) {
				found = ht;
			}
		}
	}
	if (found) return found;									//Match on any
	return NULL;
}


BOOL_T ReadHeadType ( char * line) {
	char * typename;
	char * cp = NULL;
	char scale[10];
	if (!GetArgs(line+10,"sq",scale,&typename) || strlen(typename) == 0 ) {
			PurgeLinesUntil("ENDSIGNALHEAD");
	        return FALSE;
	}
	SCALEINX_T input_scale = LookupScale(scale);
	signalHeadType_p ht = NULL;
	// Find if dup name at same scale, overwrite it //
	signalHeadType_p ht1 = FindHeadType(typename,input_scale);
	if (ht1 && (ht1->headScale == input_scale)) {
		ht = ht1;
		CleanSegs(&ht->headSegs);
		for (int i=0;i<ht->headAppearances.cnt;i++) {
			HeadAppearance_p ap = &DYNARR_N( HeadAppearance_t, ht->headAppearances, i );
			CleanSegs(&ap->appearanceSegs);
		}
		memset(ht,0,sizeof(signalHeadType_t));
	} else {
		//Allocate new if not found
		DYNARR_APPEND(signalHeadType_t, headTypes_da,10);
		ht = &DYNARR_LAST(signalHeadType_t,headTypes_da);
	}
	//Fill out HeadType
	ht->headTypeName = typename;
	ht->headScale = input_scale;                                       //Now this scale
	CleanSegs(&tempSegs_da);
	BOOL_T first = TRUE;
	while((cp = GetNextLine()) != NULL) {
		if ( strncmp( cp, "ENDSIGNALHEAD", 13 ) == 0 ) break;
		if ( *cp == '\n' || *cp == '#' ) continue;
		while (isspace((unsigned char)*cp) || *cp == '\t') cp++;
		if (first && (strncmp( cp, "APPEARANCE ", 11 ) != 0)) {							//Static Segs for HeadType
			ReadSegs();
			AppendSegsToArray(&ht->headSegs,&tempSegs_da);
			CleanSegs(&tempSegs_da);
			continue;
		}
		first = FALSE;
		if ( strncmp( cp, "APPEARANCE ", 11 ) == 0 ) {
			char appname[50];
			coOrd pos;
			DIST_T angle;
			if (!GetArgs(cp+11,"spf",appname,&pos,&angle)) continue;
			if (FindAppearanceNum(ht,appname) != -1 ) {
				ErrorMessage(MSG_SIGNAL_DUPLICATE_APPEARANCE,appname,ht->headTypeName);
				return FALSE;
			} else {
				DYNARR_APPEND(HeadAppearance_t, ht->headAppearances, 2 );
				HeadAppearance_p a = &DYNARR_LAST(HeadAppearance_t,ht->headAppearances);
				a->appearanceName = strdup(appname);
				a->orig = pos;
				a->angle = angle;
				CleanSegs(&tempSegs_da);
				ReadSegs();
				AppendSegsToArray(&a->appearanceSegs,&tempSegs_da);
			}
		} else {
			ErrorMessage(MSG_SIGNAL_HEADTYPE_UNEXPECTED_DATA,ht->headTypeName);
			return FALSE;
		}
		if (cp == NULL) break;
	}
	return TRUE;
}


/*
 * Look up Head in Array By Name
 */
static int FindHeadNum( signalData_p s, char * name) {
	for (int i=0;i<s->signalHeads.cnt;i++) {
		signalHead_p h = &DYNARR_N(signalHead_t,s->signalHeads,i);
		if ((strlen(h->headName) == strlen(name)) &&
				(strncmp(h->headName,name,strlen(name)) == 0)) {
			return i;
		}
	}
	return -1;
}


/*
 * Look up Base Aspect in Array By Name
 */
static signalAspectType_p FindBaseAspect(char * name) {
	if (strlen(name) == 0) name = "None";
	for (int i=0;i<sizeof(defaultAspectsMap);i++) {
		signalAspectType_p a = &defaultAspectsMap[i];
		if (a->baseAspect == -2) return NULL;
		if ((strlen(a->aspectName) == strlen(name)) && (strncmp(name,a->aspectName,strlen(name))==0)) {
			return a;
		}
	}
	return NULL;
}

static int FindSignalHeadAppearance(track_p sig, int headId, char * appearanceName) {
	signalData_p xx = getSignalData(sig);
	//if (headId>xx->signalHeads.cnt) return -1;
	signalHead_p h = &DYNARR_N(signalHead_t,xx->signalHeads,headId-1);
	signalHeadType_p ht = h->headType;
	if (ht) {
		for (int i=0;i<ht->headAppearances.cnt;i++) {
			HeadAppearance_p a = &DYNARR_N(HeadAppearance_t,ht->headAppearances,i);
			if ((strlen(a->appearanceName) == strlen(appearanceName)) && (strcmp(appearanceName,a->appearanceName)==0)) {
				return i;
			}
		}
	}
	return -1;
}

static signalAspect_p FindSignalAspect(track_p sig,char * aspect) {
	signalData_p xx = getSignalData(sig);
	for (int i=0; i<xx->signalAspects.cnt;i++) {
		signalAspect_p sa = &DYNARR_N(signalAspect_t,xx->signalAspects,i);
		if ((strlen(sa->aspectName) == strlen(aspect)) && (strcmp(sa->aspectName, aspect) == 0))
			return sa;
	}
	return NULL;
}



void ReadSignal( char * line ) {
	    TRKINX_T index, trkIndex = -1;
	    EPINX_T ep=-1;
	    track_p trk;
	    char * cp = NULL;
	    wIndex_t ia;
	    char *name;
	    char *aspname, *aspscript;
	    wIndex_t numHeads;
	    coOrd orig;
	    ANGLE_T angle;
	    BOOL_T visible;
	    char scale[10];
	    wIndex_t layer;
	    signalData_p xx;

		ep = -1;
		if (!GetArgs(line+6,"dLsdpfdq",&index,&layer,scale, &visible, &orig,
					&angle, &numHeads, &name)) {
				NoticeMessage(MSG_SIGNAL_INVALID_ENTRY, _("Yes"), NULL, line);
				PurgeLinesUntil("ENDSIGNAL");
				return;
		}

	    SCALEINX_T input_scale = LookupScale(scale);
	    SCALEINX_T curr_scale = GetLayoutCurScale();
	    DIST_T ratio = GetScaleRatio(curr_scale)/GetScaleRatio(input_scale);
	    trk = NewTrack(index, T_SIGNAL, 0, sizeof(signalData_t));

	    xx = GetSignalData( trk );

	    *sig_last = xx;
	    sig_last = &xx->next_signal;
	    xx->next_signal = NULL;
	    xx->signal_track = trk;

	    SetTrkVisible(trk, visible);
		SetTrkScale(trk, LookupScale( scale ));
		SetTrkLayer(trk, layer);
		xx->signalName = name;
		xx->numberHeads = numHeads;         //Legacy
		xx->orig = orig;
		xx->angle = angle;
		xx->scaleInx = curr_scale;
		xx->currentAspect = 0;
		xx->currSegs.cnt = 0;
		xx->currSegs.max = 0;
		xx->currSegs.ptr = NULL;

	    DYNARR_RESET( signalAspect_t, xx->signalAspects );
	    while ( (cp = GetNextLine()) != NULL ) {
	        while (isspace((unsigned char)*cp) || *cp == '\t') cp++;
	        if ( strncmp( cp, "END", 3 ) == 0 ) {
	            break;
	        }
	        if ( *cp == '\n' || *cp == '#' ) {
	            continue;
	        }
	        if (strncmp( cp, "TRACK", 5) ==0) {
	        	if (!GetArgs(cp+6,"dd",&trkIndex,&ep)) {
	        		NoticeMessage(MSG_SIGNAL_INVALID_ENTRY, _("Yes"), NULL, line);
	        		PurgeLinesUntil("ENDSIGNAL");
	        		return;
	        	}
	        	xx->index = trkIndex;
	        	xx->ep = ep;
	        } else if (strncmp( cp, "VIEW", 4) == 0) {
	        	char viewname[10];
	        	if (!GetArgs(cp+4,"s",viewname)) {
	        		NoticeMessage(MSG_SIGNAL_INVALID_ENTRY, _("Yes"), NULL, line);
	        		PurgeLinesUntil("ENDSIGNAL");
	        		return;
	        	}
	        	int type;
	        	if (strncmp(viewname,"ELEV",4)==0) type = SIGNAL_DISPLAY_ELEV;
	        	else if (strncmp(viewname,"PLAN",4)==0) type = SIGNAL_DISPLAY_PLAN;
	        	else type = SIGNAL_DISPLAY_DIAG;
	        	ReadSegs();
	        	RescaleSegs(tempSegs_da.cnt,tempSegs_da.ptr,ratio,ratio,ratio);
	        	AppendSegsToArray(&xx->staticSignalSegs[type],&tempSegs_da);
	        } else if (strncmp( cp, "HEAD", 4) == 0) {
	        	char * headName;
	        	char * headType;
	        	coOrd pos;
	        	if (!GetArgs(cp+4,"pqq",&pos, &headName, &headType)) {
	        		NoticeMessage(MSG_SIGNAL_INVALID_ENTRY, _("Yes"), NULL, line);
	        		PurgeLinesUntil("ENDSIGNAL");
	        		return;
	        	}
	        	DYNARR_APPEND(signalHead_t, xx->signalHeads, 1);
	        	signalHead_p sh = &DYNARR_LAST(signalHead_t, xx->signalHeads);
	        	sh->headName = headName;
	        	sh->headTypeName = headType;
	        	sh->headPos = pos;
	        	if ((sh->headType = FindHeadType(headType,curr_scale))==NULL) {
	        		NoticeMessage(MSG_SIGNAL_MISSING_HEADTYPE, _("Yes"), NULL, xx->signalName,headName,headType);
	        		--xx->signalHeads.cnt;
	        	} else {
	        		signalHeadType_p ht = sh->headType;
	        		sh->currentHeadAppearance = 0;
	        		DIST_T ratioh = GetScaleRatio(sh->headType->headScale)/GetScaleRatio(curr_scale);
	        		AppendSegsToArray(&sh->headSegs[SIGNAL_DISPLAY_ELEV],&ht->headSegs);
	        		RescaleSegs(sh->headSegs[SIGNAL_DISPLAY_ELEV].cnt,sh->headSegs[SIGNAL_DISPLAY_ELEV].ptr,ratioh,ratioh,ratioh);
	        	}
	        } else if ( strncmp( cp, "ASPECT", 6 ) == 0 ) {
				char baseaspect[20];
				if (!GetArgs(cp+4,"qsq",&aspname,baseaspect,&aspscript)) {
					NoticeMessage(MSG_SIGNAL_INVALID_ENTRY, _("Yes"), NULL, line);
					PurgeLinesUntil("ENDASPECT");
					break;
				}
				DYNARR_APPEND( signalAspect_t, xx->signalAspects, 2 );
				signalAspect_p sa = &DYNARR_LAST(signalAspect_t, xx->signalAspects);
				sa->aspectName = aspname;
				sa->aspectType = FindBaseAspect(baseaspect);
				if (!sa->aspectType)
					NoticeMessage(MSG_SIGNAL_NO_BASE_ASPECT, _("Yes"), NULL, xx->signalName, aspname, baseaspect);
				sa->aspectScript = aspscript;
				while ( (cp = GetNextLine()) != NULL ) {
					while (isspace((unsigned char)*cp) || *cp == '\t') cp++;
					if ( *cp == '\n' || *cp == '#' ) continue;
					if ( strncmp( cp, "ENDASPECT", 9 ) == 0 ) {     //END of Aspect
						break;
					} else if (strncmp(cp, "ASPECTMAP",9) == 0) {
						DYNARR_APPEND(headAspectMap_t,sa->headAspectMap,1);
						headAspectMap_p ha = &DYNARR_LAST(headAspectMap_t,sa->headAspectMap);
						int headNum;
						char * app;
						if (!GetArgs(cp+10,"dq",&headNum,&app)) {
							NoticeMessage(MSG_SIGNAL_INVALID_ENTRY, _("Yes"), NULL, line);
							PurgeLinesUntil("ENDASPECT");
							break;
						}
						ha->aspectMapHeadAppearance = app;
						ha->aspectMapHeadNumber = headNum-1;  //Readable number
						if (headNum>xx->signalHeads.cnt || headNum < 1)
							NoticeMessage(MSG_SIGNAL_MISSING_HEAD, _("Yes"), NULL, xx->signalName, headNum, sa->aspectName);
						else
							ha->aspectMapHeadAppearanceNumber = FindSignalHeadAppearance(trk,headNum,app);
					} else
						NoticeMessage(MSG_SIGNAL_INVALID_ENTRY, _("Yes"), NULL,  line);
				}
	        } else if ( strncmp (cp, "GROUP", 5) == 0) {
				char * groupName;
				if (!GetArgs(cp+12,"q",&groupName)) {
					NoticeMessage(MSG_SIGNAL_INVALID_ENTRY, _("Yes"), NULL,  line);
					PurgeLinesUntil("ENDGROUP");
					return;
				}
				DYNARR_APPEND(signalGroup_t, xx->signalGroups, 1 );
				signalGroup_p sg =  &DYNARR_LAST(signalGroup_t, xx->signalGroups);
				sg->name = groupName;
				while ( (cp = GetNextLine()) != NULL ) {
					while (isspace((unsigned char)*cp) || *cp == '\t') cp++;
					if ( *cp == '\n' || *cp == '#' ) continue;
					if ( strncmp( cp, "ENDGROUP", 8) == 0 ) break;  //END of Group
					char *aspect;
					if ( strncmp( cp, "TRACKASPECT", 11 ) == 0 ) {
						char groupAspect[20];
						if (!GetArgs(cp+12,"s",groupAspect)) {
							NoticeMessage(MSG_SIGNAL_INVALID_ENTRY, _("Yes"), NULL,  line);
							PurgeLinesUntil("ENDGROUP");
							break;
						}
						signalAspect_p sa = FindSignalAspect(trk,groupAspect);
						if (!sa) NoticeMessage(MSG_SIGNAL_GRP_ASPECT_INVALID, _("Yes"), NULL, name,  xx->signalGroups.cnt, groupAspect);
						else {
							DYNARR_APPEND(groupAspectList_t,sg->aspects,1);
							DYNARR_LAST(groupAspectList_t,sg->aspects).aspectName = groupAspect;
							DYNARR_LAST(groupAspectList_t,sg->aspects).aspect = sa;
						}
					} else if ( strncmp( cp, "INDICATE", 8 ) == 0 ) {
						char indOnName[20], indOffName[20], *conditions;
						int headNum;
						if (!GetArgs(cp+8,"dssq",&headNum,indOnName,indOffName,&conditions)) {
							NoticeMessage(MSG_SIGNAL_INVALID_ENTRY, _("Yes"), NULL,  line);
							PurgeLinesUntil("ENDGROUP");
							break;
						}
						if (headNum >xx->signalHeads.cnt) {
								NoticeMessage(MSG_SIGNAL_GRP_GROUPHEAD_INVALID, _("Yes"), NULL, name, xx->signalGroups.cnt, headNum );
								PurgeLinesUntil("ENDGROUP");
								break;
						} else {
							signalHead_p sh = &DYNARR_N(signalHead_t,xx->signalHeads,headNum);
							if(FindAppearanceNum(sh->headType, indOffName) == -1) {
								NoticeMessage(MSG_SIGNAL_GRP_IND_INVALID, _("Yes"), NULL, name, xx->signalGroups.cnt, indOffName);
							} else if (FindAppearanceNum(sh->headType, indOnName) == -1) {
								NoticeMessage(MSG_SIGNAL_GRP_IND_INVALID, _("Yes"), NULL, name, xx->signalGroups.cnt, indOnName);
							} else {
								DYNARR_APPEND(signalGroupInstance_t, sg->groupInstances, 1 );
								signalGroupInstance_p gi = &DYNARR_LAST(signalGroupInstance_t, sg->groupInstances);
								gi->headId = headNum;
								gi->appearanceOnName = indOnName;
								gi->appearanceOffName = indOffName;
								gi->conditions = conditions;            //Dont check yet
							}
						}
					} else
						NoticeMessage(MSG_SIGNAL_INVALID_ENTRY, _("Yes"), NULL, line);
				}
	        } else if ( strncmp( cp, "ENDSIGNAL", 9) == 0 ) break;

	        else {
	        	NoticeMessage(MSG_SIGNAL_INVALID_ENTRY, _("Yes"), NULL, line);
	        }
	    }
	    setAspect(trk,xx->currentAspect);
	    ComputeSignalBoundingBox(trk,SignalDisplay);
	    return;
}


static void MoveSignal (track_p trk, coOrd orig )
{
    signalData_p xx = getSignalData ( trk );
    if (xx->track) {
    	ComputeSignalBoundingBox(trk, SignalDisplay);
    	return;
    }
    xx->orig.x += orig.x;
    xx->orig.y += orig.y;
    ComputeSignalBoundingBox(trk, SignalDisplay);
}

static void RotateSignal (track_p trk, coOrd orig, ANGLE_T angle ) 
{
	signalData_p xx = getSignalData ( trk );
	if (xx->track) {
		ComputeSignalBoundingBox(trk, SignalDisplay);
		return;
	}
    Rotate(&(xx->orig), orig, angle);
    xx->angle = NormalizeAngle(xx->angle + angle);
    ComputeSignalBoundingBox(trk, SignalDisplay);
}

static void RescaleSignal (track_p trk, FLOAT_T ratio ) 
{
	signalData_p xx = getSignalData ( trk );
	if (xx->track) {
		ComputeSignalBoundingBox(trk, SignalDisplay);
		return;
	}
	ComputeSignalBoundingBox(trk, SignalDisplay);
}

static void FlipSignal (track_p trk, coOrd orig, ANGLE_T angle )
{
    signalData_p xx = getSignalData ( trk );
    if (xx->track) {
    	ComputeSignalBoundingBox(trk, SignalDisplay);
    	return;
    }
    FlipPoint(&(xx->orig), orig, angle);
    xx->angle = NormalizeAngle(2*angle - xx->angle);
    ComputeSignalBoundingBox(trk, SignalDisplay);
}


/*
 * Do Pub/Sub Responses
 *
 * The Events are Signal Aspect changes
 * The Actions can be Signal Aspects (for the Signal) or Head Appearances (for the Heads)
 *
 * The names are the signalName and the signalName.HeadName(s)
 *
 * The type is always Signal
 *
 */
static int pubSubSignal(track_p trk, pubSubParmList_p parm) {
	signalData_p sd = getSignalData(trk);
	char * cp;

	switch(parm->command) {
	case GET_STATE:
		DYNARR_RESET(signalAspect_t,parm->actions);
		parm->type = TYPE_SIGNAL;
		if (strcmp(sd->signalName,parm->name) == 0) {
			signalAspect_p sa = &DYNARR_N(signalAspect_t,sd->signalAspects,sd->currentAspect);

			parm->state = sa->aspectName;
		}
		cp = parm->name+strlen(sd->signalName);
		if (cp[0] == '.' && cp[1]) {
			cp++;
			for (int i=0;i<sd->signalHeads.cnt;i++) {
				signalHead_p sh = &DYNARR_N(signalHead_t,sd->signalHeads,i);
				if (strcmp(cp,sh->headName) == 0) {
					if (sh->headType == NULL) continue;
					HeadAppearance_p a = &DYNARR_N(HeadAppearance_t,sh->headType->headAppearances,sh->currentHeadAppearance);
					parm->state = a->appearanceName;
					return 0;
				}
			}
		}
		break;
	case FIRE_ACTION:
		if (parm->type != TYPE_SIGNAL) return 4;
		if (strncmp(sd->signalName,parm->name,strlen(sd->signalName)) == 0) {
			//Try Signal Aspect changes first
			if (strlen(sd->signalName) == strlen(parm->name)) {
				for (int i=0;i<sd->signalAspects.cnt;i++) {
					signalAspect_p sa = &DYNARR_N(signalAspect_t,sd->signalAspects,i);
					if ((strncmp(parm->action,sa->aspectName,strlen(parm->action)) == 0)) {
						sd->currentAspect = i;
						publishEvent(sd->signalName,TYPE_SIGNAL,sa->aspectName);
						return 0;
					}
				}
			} else {
				//See if a head matched for head Appearance changes  Signal+'.'+Head
				cp = parm->name+strlen(sd->signalName);
				if (cp[0] && (cp[0] == '.') && cp[1]) {
					cp++;
					for (int i=0;i<sd->signalHeads.cnt;i++) {
						signalHead_p sh = &DYNARR_N(signalHead_t,sd->signalHeads,i);
						if (strncmp(cp,sh->headName,strlen(sh->headName)) == 0) {
							if (sh->headType == NULL) continue;
							for (int j=0;j<sh->headType->headAppearances.cnt;j++) {
								HeadAppearance_p a = &DYNARR_N(HeadAppearance_t,sh->headType->headAppearances,j);
								if ((strncmp(a->appearanceName,parm->action,strlen(parm->action)) == 0)) {
									sh->currentHeadAppearance = j;
									// No Head Events to publish - publishEvent(sd->signalName,TYPE_HEAD,a->appearanceName);
									return 0;
								}
							}
						}
					}
				}
			}
		}
		break;
	case DESCRIBE_NAMES:
		DYNARR_RESET(ParmName_t,parm->names);
		DYNARR_APPEND(ParmName_t,parm->names,2);
		char ** name = &DYNARR_LAST(char *,parm->names);
		parm->type = TYPE_SIGNAL;
		*name = sd->signalName;
		for (int i=0;i<sd->signalHeads.cnt;i++) {
			signalHead_p sh = &DYNARR_N(signalHead_t,sd->signalHeads,i);
			DYNARR_APPEND(ParmName_t,parm->names,1);
			ParmName_p p = &DYNARR_LAST(ParmName_t,parm->names);
			p->name = MyMalloc(50);
			snprintf(p->name,50,"%s.%s",sd->signalName,sh->headName);
		}
		break;
	case DESCRIBE_STATES:
		if (parm->type != TYPE_SIGNAL) return 4;
		DYNARR_RESET(char *,parm->states);
		if (strcmp(sd->signalName,parm->name) == 0) {
			for (int i=0;i<sd->signalAspects.cnt;i++) {
				signalAspect_p sa = &DYNARR_N(signalAspect_t,sd->signalAspects,i);
				DYNARR_APPEND(ParmName_t,parm->states,1);
				ParmName_p p = &DYNARR_LAST(ParmName_t,parm->states);
				p->name = sa->aspectName;
			}
		} else return 4;
		break;
	case DESCRIBE_ACTIONS:
		if (parm->type != TYPE_SIGNAL) return 4;
		DYNARR_RESET(ParmName_t,parm->actions);
		if (strncmp(sd->signalName,parm->name,strlen(sd->signalName)) == 0) {
			for (int i=0;i<sd->signalAspects.cnt;i++) {
				signalAspect_p sa = &DYNARR_N(signalAspect_t,sd->signalAspects,i);
				DYNARR_APPEND(ParmName_t,parm->actions,1);
				ParmName_p p = &DYNARR_LAST(ParmName_t,parm->actions);
				p->name = sa->aspectName;
			}
		} else {
			char * cp = parm->name+strlen(sd->signalName);
			if ((cp[0] = '.') && cp[1]) {
				cp++;
				for (int i=0;i<sd->signalHeads.cnt;i++) {
					signalHead_p sh = &DYNARR_N(signalHead_t,sd->signalHeads,i);
					if (strcmp(cp,sh->headName) == 0) {
						if (sh->headType == NULL) continue;
						for (int j=0;j<sh->headType->headAppearances.cnt;j++) {
							HeadAppearance_p ha = &DYNARR_LAST(HeadAppearance_t,sh->headType->headAppearances);
							DYNARR_APPEND(ParmName_t,parm->actions,1);
							ParmName_p p = &DYNARR_LAST(ParmName_t,parm->actions);
							p->name = ha->appearanceName;
						}
					}
				}
			} else return 4;
		}
		break;
	}
	return 0;
}

static void ActivateSignal( track_p trk) {
	signalData_p xx = getSignalData(trk);
	if (xx->currentAspect>=xx->signalAspects.cnt-1)
		xx->currentAspect = 0;
	else
		xx->currentAspect++;
	setAspect(trk,xx->currentAspect);
	MainRedraw();
}

static BOOL_T QuerySignal( track_p trk, int query )
{
	switch ( query ) {
	case Q_IS_ACTIVATEABLE:;
		return TRUE;
		break;
	default:
		return FALSE;
	}
	return FALSE;
}

static trackCmd_t signalCmds = {
    "SIGNAL ",
    DrawSignal,
    DistanceSignal,
    DescribeSignal,
    DeleteSignal,
    WriteSignal,
    ReadSignal,
    MoveSignal,
    RotateSignal,
    RescaleSignal,
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
    QuerySignal, /* query */
    NULL, /* ungroup */
    FlipSignal, /* flip */
    NULL, /* drawPositionIndicator */
    NULL, /* advancePositionIndicator */
    NULL, /* checkTraverse */
    NULL, /* makeParallel */
    NULL, /* drawDesc */
	NULL, /* rebuild  */
	NULL, /* replay   */
	NULL, /* store    */
	ActivateSignal, /* activate */
	pubSubSignal /* Publish/Subscribe  */
};

/*
 * Windows Defs - These enable editing of the Signal Layout characteristics.
 * You can't add heads, or change head types, or edit the way the heads and static elements are laid out
 * in these windows
 *
 * In the main Signal Window, name, offset
 *
 *
 */

static drawCmd_t signalEditD = {
		NULL,
		&screenDrawFuncs,
		0,
		1.0,
		0.0,
		{0.0,0.0}, {0.0,0.0},
		Pix2CoOrd, CoOrd2Pix };

static track_p signalEditTrack;

static BOOL_T signalCreate_P;
static coOrd signalEditOrig;
static ANGLE_T signalEditAngle;

static long signalEditTrackInx;
static long signalEditTrackEP;
static long signalDlgDispMode;

static track_p signalEditTrackConnected;
static EPINX_T signalEditEP;

static char signalEditName[STR_SHORT_SIZE];
static char signalEditPlate[STR_SHORT_SIZE];
static char signalEditLever[STR_SHORT_SIZE];
static long signalEditHeadCount;
static wIndex_t signalEditAspectChoice;
static char signalEditTrackInxEP[10];

static void RedrawEditSignal( wDraw_p, void *, wPos_t, wPos_t );

static char *dispmodeLabels[] = { N_("Aspects"), N_("Heads"), N_("Groups"), N_("Static"), NULL };

void NextAspect(track_p trk) {
	signalData_p xx;
	xx = getSignalData(trk);
	if (xx->currentAspect >= xx->signalAspects.cnt-1) {
		xx->currentAspect = 0;
	}
	else {
		xx->currentAspect++;
	}
	CleanSegs(&xx->currSegs);
}



void SetSignalHead(track_p sig, int headId, char * appearanceName) {
	signalData_p xx = getSignalData(sig);
	signalHead_p h = &DYNARR_N(signalHead_t,xx->signalHeads,headId);
	signalHeadType_p ht = h->headType;
	for (int i=0;i<ht->headAppearances.cnt;i++) {
		if (strcmp(appearanceName,ht->headTypeName)==0) {
			h->currentHeadAppearance = i;
		}
	}
}

void AdvanceDisplayAspect( wAction_t action, coOrd pos) {
	track_p trk = signalEditTrack;
	signalData_p xx;
	xx = getSignalData(trk);
	if (action != C_DOWN) return;
	if (xx->currentAspect >= xx->signalAspects.cnt-1) {
		xx->currentAspect = 0;
	} else {
		xx->currentAspect++;
	}
	setAspect(trk,xx->currentAspect);
	for (int i=0;i<xx->signalGroups.cnt;i++) {
		signalGroup_p g = &DYNARR_N(signalGroup_t,xx->signalGroups,i);
		for (int j=0;j<g->aspects.cnt;j++) {
			groupAspectList_p ap = &DYNARR_N(groupAspectList_t,g->aspects,j);
			if (&DYNARR_N(signalAspect_t,xx->signalAspects,xx->currentAspect)==ap->aspect) {
				for (int k=0;k<g->groupInstances.cnt;k++) {
					signalGroupInstance_p gi = &DYNARR_N(signalGroupInstance_t,g->groupInstances,k);
					signalHead_p h = &DYNARR_N(signalHead_t, xx->signalHeads, gi->headId);
					SetSignalHead(trk,gi->headId,gi->appearanceOffName);
				}
			} else {
				for (int k=0;k<g->groupInstances.cnt;k++) {
					signalGroupInstance_p gi = &DYNARR_N(signalGroupInstance_t,g->groupInstances,k);
					signalHead_p h = &DYNARR_N(signalHead_t, xx->signalHeads, gi->headId);
					SetSignalHead(trk,gi->headId,gi->appearanceOnName);
				}
			}
		}
	}

	CleanSegs(&xx->currSegs);				// Set so they will be rebuilt
}


static wPos_t aspectListWidths[] = { 50, 150 };
static const char * aspectListTitles[] = { N_("Name"), N_("Script") };
static paramListData_t aspectListData = {10, 400, 2, aspectListWidths, aspectListTitles};
static wPos_t headListWidths[] = { 50, 50, 6};
static const char * headListTitles[] = { N_("Head Name"), N_("Head Type"), N_("Head #") };
static paramListData_t headListData = {5, 400, 3, headListWidths, headListTitles};
static paramDrawData_t signalEditDrawData = { 490, 200, (wDrawRedrawCallBack_p)RedrawEditSignal, AdvanceDisplayAspect, &signalEditD };

static void AspectEdit( void * action );
static void AspectAdd( void * action );
static void AspectDelete( void * action );

static void HeadEdit (void * action);

static paramFloatRange_t r_1000_1000    = { -1000.0, 1000.0, 80 };
static paramFloatRange_t r0_360         = { 0.0, 360.0, 80 };
/*
 * Main Signal Screen - with List of Heads and Aspects
 */
static paramData_t signalEditPLs[] = {
#define A                       (0)
#define I_SIGNALNAME (A+0)
    /*0*/ { PD_STRING, signalEditName, "signalname", PDO_NOPREF|PDO_STRINGLIMITLENGTH, (void*)50, N_("Name"), 0, 0, sizeof(signalEditName)},
#define I_SIGNALPLATE (A+1)
    /*1*/ { PD_STRING, signalEditPlate, "signalplate", PDO_NOPREF|PDO_STRINGLIMITLENGTH|PDO_DLGHORZ, (void*)10, N_("Plate"), 0, 0, sizeof(signalEditPlate)},
#define I_ORIGX (A+2)
    /*2*/ { PD_FLOAT, &signalEditOrig.x, "origx", PDO_DIM, &r_1000_1000, N_("Origin: X,Y") },
#define I_ORIGY (A+3)
    /*3*/ { PD_FLOAT, &signalEditOrig.y, "origy", PDO_DIM | PDO_DLGHORZ, &r_1000_1000, NULL },
#define I_TRACK (A+4)
    /*4*/ { PD_LONG, &signalEditTrackInx, "trackInx", 0, 0, N_("Track: Index & EP")},
#define I_EP (A+5)
	/*5*/ { PD_LONG, &signalEditTrackEP, "trackEP", PDO_DLGHORZ, 0, NULL},

#define I_CD_DISPMODE           (A+6)
	{ PD_RADIO, &signalDlgDispMode, "dispmode", PDO_NOPREF|PDO_DLGWIDE, dispmodeLabels, N_("Mode"), BC_HORZ|BC_NOBORDER },

#define B                       (A+7)
#define I_SIGNALASPECTLIST (B+0)
#define aspectSelL ((wList_p)signalEditPLs[I_SIGNALASPECTLIST].control)
    /*6*/ { PD_LIST, NULL, "inxA", PDO_DLGRESETMARGIN|PDO_DLGRESIZE, &aspectListData, NULL, BL_MANY },
#define I_SIGNALASPECTEDIT (B+1)
    /*7*/ { PD_BUTTON, (void*)AspectEdit, "editA", PDO_DLGCMDBUTTON, NULL, N_("Edit Aspect") },
#define I_SIGNALASPECTADD (B+2)
    /*8*/ { PD_BUTTON, (void*)AspectAdd, "addA", PDO_DLGCMDBUTTON, NULL, N_("Add Aspect") },
#define I_SIGNALASPECTDELETE (B+3)
    /*9*/ { PD_BUTTON, (void*)AspectDelete, "deleteA", 0, NULL, N_("Delete Aspect") },
#define I_SIGNALHEADLIST (10)
#define headSelL ((wList_p)signalEditPLs[I_SIGNALHEADLIST].control)
	/*10*/ { PD_LIST, NULL, "inxH", PDO_DLGRESETMARGIN|PDO_DLGRESIZE, &headListData, NULL, BL_MANY },
#define I_SIGNALHEADEDIT (11)
    /*11*/ { PD_BUTTON, (void*)HeadEdit, "editHA", PDO_DLGCMDBUTTON, NULL, N_("Edit Head") },
#define I_SIGNALDRAW (12)
    /*12*/ { PD_DRAW, NULL, "signalEditDraw", PDO_NOPSHUPD|PDO_DLGUNDERCMDBUTT|PDO_DLGRESIZE, &signalEditDrawData, NULL, 0}
};
static paramGroup_t signalEditPG = { "signalEdit", 0, signalEditPLs, sizeof signalEditPLs/sizeof signalEditPLs[0] };
static wWin_p signalEditW;

static paramIntegerRange_t rm1_999999 = { -1, 999999 };

static char signalAspectEditName[STR_SHORT_SIZE];
static char signalAspectEditScript[STR_LONG_SIZE];
static long signalAspectEditIndex;
static wIndex_t signalAspectEditDefaultChoice;

static void HeadAppearanceEdit( void * action);


static paramData_t aspectEditPLs[] = {
#define I_ASPECTNAME (0)
    /*0*/ { PD_STRING, signalAspectEditName, "aspectname", PDO_NOPREF|PDO_STRINGLIMITLENGTH, (void*)50,  N_("Name"), 0, 0, sizeof(signalAspectEditName)},
#define I_ASPECTSCRIPT (1)
    /*1*/ { PD_STRING, signalAspectEditScript, "aspectscript", PDO_NOPREF|PDO_STRINGLIMITLENGTH, (void*)350, N_("Script"), 0, 0, sizeof(signalAspectEditScript)},
#define I_ASPECTDEFAULT (2)
    /*2*/ { PD_DROPLIST, &signalAspectEditDefaultChoice, "aspectdefault", PDO_NOPREF|PDO_LISTINDEX, (void*)50, N_("Equals Aspect") },
#define I_ASPECTTOHEADLIST (3)
#define headSelL2 ((wList_p)aspectEditPLs[I_ASPECTTOHEADLIST].control)
    /*3*/ { PD_LIST, NULL, "inx", PDO_DLGRESETMARGIN|PDO_DLGRESIZE, &headListData, NULL, BL_MANY },
#define I_ASPECTHEADEDIT (4)
    /*4*/ { PD_BUTTON, (void*)HeadAppearanceEdit, "edit", PDO_DLGCMDBUTTON, NULL, N_("Edit Head Appearance") }
};

static paramGroup_t aspectEditPG = { "aspectEdit", 0, aspectEditPLs, sizeof aspectEditPLs/sizeof aspectEditPLs[0] };
static wWin_p aspectEditW;

static wIndex_t AppearanceEditChoice;
static char AppearanceEditName[50];
static long AppearanceInx;

static paramData_t AppearanceEditPLs[] = {
#define I_HEADNAME (0)
    /*0*/ { PD_STRING, AppearanceEditName, "headname", PDO_NOPREF|PDO_STRINGLIMITLENGTH, (void*)350, N_("Name"), 0, 0, sizeof(AppearanceEditName) },
#define I_APPEARANCES (1)
	/*1*/ { PD_LONG, &AppearanceInx, "headInx", 0, 0, N_("Head Number") },
#define I_APPEARANCELIST (2)
#define AppearanceIndicatorSelL ((Wlist_p)AppearanceEditPls[I_APPEARANCELIST].control)
	/*2*/ {PD_DROPLIST, &AppearanceEditChoice, "appearance", PDO_NOPREF|PDO_LISTINDEX, (void*)50, N_("Appearance")  }
};

static paramGroup_t AppearanceEditPG = { "headAppearanceEdit", 0, AppearanceEditPLs, sizeof AppearanceEditPLs/sizeof( AppearanceEditPLs[0]) };
static wWin_p AppearanceEditW;

static long headEditInx;
static char headEditName[STR_SHORT_SIZE];
static char headEditType[STR_SHORT_SIZE];

static paramData_t headEditPLs[] = {
#define I_HEADNAMES (0)
    /*0*/ { PD_STRING, headEditName, "headname", PDO_NOPREF|PDO_STRINGLIMITLENGTH, (void*)350, N_("Name"), 0, 0, sizeof(headEditName)},
#define I_HEADTYPES (1)
    /*1*/ { PD_STRING, headEditType, "headtype", PDO_NOPREF|PDO_STRINGLIMITLENGTH, (void*)350, N_("Head Type"), 0, 0, sizeof(headEditType)},
#define I_HEADNUM (2)
	/*1*/ { PD_LONG, &headEditInx, "headInx", 0, 0, N_("Head Number") }
};

static paramGroup_t headEditPG = { "aspectEdit", 0, headEditPLs, sizeof headEditPLs/sizeof headEditPLs[0] };
static wWin_p headEditW;

static struct {
	track_p trk;
	ANGLE_T orient;
	coOrd pos0,pos1;
	dynArr_t currSegs;
} Da;


static void SignalEditOk ( void * junk )
{
    track_p trk;
    signalData_p xx;
    wIndex_t ia;
    CSIZE_T newsize;
    
    if (signalCreate_P) {
        UndoStart( _("Create Signal"), "Create Signal");
        trk = NewTrack(0, T_SIGNAL, 0, sizeof(signalData_t));
        xx = getSignalData(trk);
    } else {
        UndoStart( _("Modify Signal"), "Modify Signal");
        trk = signalEditTrack;
        xx = getSignalData(trk);
    }
    xx->orig = signalEditOrig;
    xx->angle = signalEditAngle;
    xx->numberHeads = signalEditHeadCount;
    if ( xx->signalName == NULL || strncmp (xx->signalName, signalEditName, STR_SHORT_SIZE) != 0) {
        MyFree(xx->signalName);
        xx->signalName = MyStrdup(signalEditName);
    }
    for (ia = 0; ia < xx->signalAspects.cnt; ia++) {
        if ((DYNARR_N(signalAspect_t,xx->signalAspects,ia)).aspectName == NULL) {
            (DYNARR_N(signalAspect_t,xx->signalAspects,ia)).aspectName = signalAspect(ia).aspectName;
        } else if (strcmp((DYNARR_N(signalAspect_t,xx->signalAspects,ia)).aspectName,signalAspect(ia).aspectName) != 0) {
            MyFree((DYNARR_N(signalAspect_t,xx->signalAspects,ia)).aspectName);
            (DYNARR_N(signalAspect_t,xx->signalAspects,ia)).aspectName = signalAspect(ia).aspectName;
        } else {
            MyFree(signalAspect(ia).aspectName);
        }
        if ((DYNARR_N(signalAspect_t,xx->signalAspects,ia)).aspectScript == NULL) {
            (DYNARR_N(signalAspect_t,xx->signalAspects,ia)).aspectScript = signalAspect(ia).aspectScript;
        } else if (strcmp((DYNARR_N(signalAspect_t,xx->signalAspects,ia)).aspectScript,signalAspect(ia).aspectScript) != 0) {
            MyFree((DYNARR_N(signalAspect_t,xx->signalAspects,ia)).aspectScript);
            (DYNARR_N(signalAspect_t,xx->signalAspects,ia)).aspectScript = signalAspect(ia).aspectScript;
        } else {
            MyFree(signalAspect(ia).aspectScript);
        }
    }
    UndoEnd();
    DoRedraw();
    ComputeSignalBoundingBox(trk, SignalDisplay);
    wHide( signalEditW );
}

static void SignalEditCancel ( wWin_p junk )
{
    wIndex_t ia;
    wHide( signalEditW );
}

static void SignalEditDlgUpdate (paramGroup_p pg, int inx, void *valueP )
{
    wIndex_t selcnt = wListGetSelectedCount( aspectSelL );
    
    if ( inx != I_SIGNALASPECTLIST ) return;
    ParamControlActive( &signalEditPG, I_SIGNALASPECTEDIT, selcnt>0 );
    ParamControlActive( &signalEditPG, I_SIGNALASPECTADD, TRUE );
    ParamControlActive( &signalEditPG, I_SIGNALASPECTDELETE, selcnt>0 );
}

static void aspectEditOK ( void * junk )
{
    if (signalAspectEditIndex < 0) {
        DYNARR_APPEND( signalAspect_t, signalAspect_da, 10 );
        signalAspect(signalAspect_da.cnt-1).aspectName = MyStrdup(signalAspectEditName);
        signalAspect(signalAspect_da.cnt-1).aspectScript = MyStrdup(signalAspectEditScript);
        snprintf(message,sizeof(message),"%s\t%s",signalAspectEditName,signalAspectEditScript);
        wListAddValue( aspectSelL, message, NULL, NULL );
    } else {
        if ( strncmp( signalAspectEditName, signalAspect(signalAspectEditIndex).aspectName,STR_SHORT_SIZE ) != 0 ) {
            MyFree(signalAspect(signalAspectEditIndex).aspectName);
            signalAspect(signalAspectEditIndex).aspectName = MyStrdup(signalAspectEditName);
        }
        if ( strncmp( signalAspectEditScript, signalAspect(signalAspectEditIndex).aspectScript, STR_LONG_SIZE ) != 0 ) {
            MyFree(signalAspect(signalAspectEditIndex).aspectScript);
            signalAspect(signalAspectEditIndex).aspectScript = MyStrdup(signalAspectEditScript);
        }
        snprintf(message,sizeof(message),"%s\t%s",signalAspect(signalAspectEditIndex).aspectName,signalAspect(signalAspectEditIndex).aspectScript);
        wListSetValues( aspectSelL, signalAspectEditIndex, message, NULL, NULL );
    }
    wHide( aspectEditW );
}

/*
 * List of Heads and Selection box of Types for Each
 */
static void EditMapDialog (wIndex_t inx )
{

/*	if (inx <0 ){
		headEditName[0] = '\0';
		headEditType[0] = '\0';
	} else {
		signalData_p sd = getSignalData(signalEditTrack);
		signalAspect_p a = &DYNARR_N(signalAspect_t,sd->signalAspects,inx);
		strncpy(headEditName,DYNARR_N(headAspectMap_t,a->headMap,inx).aspectMapHeadName,STR_SHORT_SIZE);
		strncpy(headEditType,((headAspectMap_t)aspectHeadMap(inx)).aspectMapHeadAppearance,STR_SHORT_SIZE);
	}
	signalHeadEditIndex = inx;
	if ( !headEditW ) {
		ParamRegister( &headEditPG );
		headEditW = ParamCreateDialog (&headEditPG,
										 MakeWindowTitle(_("Edit aspect")),
										 _("Ok"), headEditOK,
										 wHide, TRUE, NULL,F_BLOCK,NULL);
	}
	ParamLoadControls( &headEditPG );
	wShow( headEditW );
*/

}

static void EditAspectDialog ( wIndex_t inx )
{
    if (inx < 0) {
        signalAspectEditName[0] = '\0';
        signalAspectEditScript[0] = '\0';
    } else {
        strncpy(signalAspectEditName,signalAspect(inx).aspectName,STR_SHORT_SIZE);
        strncpy(signalAspectEditScript,signalAspect(inx).aspectScript,STR_SHORT_SIZE);
    }
    signalAspectEditIndex = inx;
    if ( !aspectEditW ) {
        ParamRegister( &aspectEditPG );
        aspectEditW = ParamCreateDialog (&aspectEditPG,
                                         MakeWindowTitle(_("Edit aspect")),
                                         _("Ok"), aspectEditOK,
                                         wHide, TRUE, NULL,F_BLOCK,NULL);
    }
    ParamLoadControls( &aspectEditPG );
    wShow( aspectEditW );
}

static void MapEdit (void * action) {
	wIndex_t selcnt = wListGetSelectedCount( headSelL );
	wIndex_t inx, cnt;

	if (selcnt !=1 ) return;
	cnt = wListGetCount (headSelL);
	for ( inx =0;
		  inx<cnt && wListGetItemSelected (headSelL, inx) != TRUE;
		  inx++);
	if (inx >= cnt ) return;
	EditMapDialog(inx);
}

static void AspectEdit( void * action )
{
    wIndex_t selcnt = wListGetSelectedCount( aspectSelL );
    wIndex_t inx, cnt;
    
    if ( selcnt != 1) return;
    cnt = wListGetCount( aspectSelL );
    for ( inx=0;
          inx<cnt && wListGetItemSelected( aspectSelL, inx ) != TRUE;
          inx++ );
    if ( inx >= cnt ) return;
    EditAspectDialog(inx);
}

static void AspectAdd( void * action )
{
    EditAspectDialog(-1);
}

static void MoveAspectUp (wIndex_t inx)
{
    wIndex_t cnt = signalAspect_da.cnt;
    wIndex_t ia;
    
    MyFree(signalAspect(inx).aspectName);
    MyFree(signalAspect(inx).aspectScript);
    for(int i=0;i<signalAspect(inx).headAspectMap.cnt;i++) {
    	headAspectMap_t hm = DYNARR_N(headAspectMap_t,signalAspect(inx).headAspectMap,i);
    	MyFree(hm.aspectMapHeadAppearance);
    }
    MyFree(signalAspect(inx).headAspectMap.ptr);
    for (ia = inx+1; ia < cnt; ia++) {
        signalAspect(ia-1).aspectName = signalAspect(ia).aspectName;
        signalAspect(ia-1).aspectScript = signalAspect(ia).aspectScript;
        signalAspect(ia-1).headAspectMap.cnt = signalAspect(ia).headAspectMap.cnt;
        signalAspect(ia-1).headAspectMap.max = signalAspect(ia).headAspectMap.max;
        signalAspect(ia-1).headAspectMap.ptr = signalAspect(ia).headAspectMap.ptr;
    }
    DYNARR_SET(signalAspect_t,signalAspect_da,cnt-1);
}

static void AspectDelete( void * action )
{
    wIndex_t selcnt = wListGetSelectedCount( aspectSelL );
    wIndex_t inx, cnt;
    
    if ( selcnt <= 0) return;
    if ( (!NoticeMessage2( 1, _("Are you sure you want to delete the %d aspect(s)"), _("Yes"), _("No"), selcnt ) ) )
        return;
    cnt = wListGetCount( aspectSelL );
    for ( inx=0; inx<cnt; inx++ ) {
        if ( !wListGetItemSelected( aspectSelL, inx ) ) continue;
        wListDelete( aspectSelL, inx );
        MoveAspectUp(inx);
        inx--;
        cnt--;
    }
    DoChangeNotification( CHANGE_PARAMS );
}

static void HeadEdit (void * action) {

}

static void HeadAppearanceEdit (void * action) {

}

EXPORT signalData_t * SignalAdd( long mode, SCALEINX_T scale, wList_p list, coOrd * maxDim, EPINX_T epCnt )
{
	wIndex_t inx;
	signalData_t * sd, * sd1 = NULL;
	for ( inx = 0; inx < signalData_da.cnt; inx++ ) {
		sd = &DYNARR_N(signalData_t,signalData_da,inx);
		if ( IsParamValid(sd->paramFileIndex) &&
			 sd->signalHeads.cnt > 0 &&
			 CompatibleScale( TRUE, sd->scaleInx, scale )) {
			if (sd1==NULL)
				sd1 = sd;
			FormatCompoundTitle( mode, sd->title );
			if (message[0] != '\0') {
				wListAddValue( list, message, NULL, sd );
				if (maxDim) {
					if (sd->size.x > maxDim->x)
						maxDim->x = sd->size.x;
					if (sd->size.y > maxDim->y)
						maxDim->y = sd->size.y;
				}
			}
		}
	}
	return sd1;
}

static coOrd maxSignalDim;

static void RescaleSignalItem( void )
{
	DIST_T xscale, yscale;
	wPos_t ww, hh;
	DIST_T w, h;
	wDrawGetSize( signalD.d, &ww, &hh );
	w = ww/signalD.dpi;
	h = hh/signalD.dpi;
	xscale = maxSignalDim.x/w;
	yscale = maxSignalDim.y/h;
	signalD.scale = max(xscale,yscale);
	if (signalD.scale == 0.0)
		signalD.scale = 1.0;
	signalD.size.x = w*signalD.scale;
	signalD.size.y = h*signalD.scale;
	return;
}

static void RedrawSignal()
{
	coOrd p, s;
	RescaleSignalItem();
LOG( log_signal, 2, ( "SelSignal(%s)\n", (curSignal?curSignal->title:"<NULL>") ) )

	wDrawClear( signalD.d );
	if (curSignal == NULL) {
		return;
	}
	signalD.orig.x = curSignal->orig.x+trackGauge;
	signalD.orig.y = (curSignal->size.y + curSignal->orig.y) - signalD.size.y+trackGauge;
	DrawSegs( &signalD, zero, 0.0, curSignal->staticSignalSegs[SIGNAL_DISPLAY_ELEV].ptr, curSignal->staticSignalSegs[SIGNAL_DISPLAY_ELEV].cnt,
					 trackGauge, wDrawColorBlack );
	for (int i=0;i<curSignal->signalHeads.cnt;i++) {
		signalHead_p sh = &DYNARR_N(signalHead_t,curSignal->signalHeads,i);
		DrawSegs (&signalD, sh->headPos, 0.0, sh->headType->headSegs.ptr, sh->headType->headSegs.cnt, trackGauge, wDrawColorBlack);
		HeadAppearance_p a = &DYNARR_N(HeadAppearance_t,sh->headType->headAppearances,sh->currentHeadAppearance);
		DrawSegs (&signalD, sh->headPos, 0.0, a->appearanceSegs.ptr, a->appearanceSegs.cnt, trackGauge, wDrawColorBlack);
	}
}


static void SignalChange( long changes )
{
	static char * lastScaleName = NULL;
	if (signalW == NULL)
		return;
	wListSetIndex( signalListL, 0 );
	if ( (!wWinIsVisible(signalW)) ||
	   ( ((changes&CHANGE_SCALE) == 0 || lastScaleName == curScaleName) &&
		  (changes&CHANGE_PARAMS) == 0 ) )
		return;
	lastScaleName = curScaleName;
	curSignal = NULL;
	curSignalEp = 0;
	wControlShow( (wControl_p)signalListL, FALSE );
	wListClear( signalListL );
	maxSignalDim.x = maxSignalDim.y = 0.0;
	if (turnoutInfo_da.cnt <= 0)
		return;
	curSignal = SignalAdd( LABEL_TABBED|LABEL_MANUF|LABEL_PARTNO|LABEL_DESCR, GetLayoutCurScale(), signalListL, &maxSignalDim, -1 );
	wListSetIndex( signalListL, 0 );
	wControlShow( (wControl_p)signalListL, TRUE );
	if (curTurnout == NULL) {
		wDrawClear( signalD.d );
		return;
	}
	signalD.orig.x = -trackGauge;
	signalD.orig.y = -trackGauge;
	maxSignalDim.x += 2*trackGauge;
	maxSignalDim.y += 2*trackGauge;
	/*RescaleTurnout();*/
	RedrawSignal();
	return;
}



static void RedrawEditSignal()
{
	coOrd p, s;
LOG( log_signal, 2, ( "SelSignal(%s)\n", (curSignal?curSignal->title:"<NULL>") ) )

	wDrawClear( signalEditD.d );
	if (curSignal == NULL) {
		return;
	}

	DrawSegs( &signalEditD, zero, 0.0, curSignal->staticSignalSegs[SIGNAL_DISPLAY_ELEV].ptr, curSignal->staticSignalSegs[SIGNAL_DISPLAY_ELEV].cnt,
					 trackGauge, wDrawColorBlack );
	for (int i=0;i<curSignal->signalHeads.cnt;i++) {
		signalHead_p sh = &DYNARR_N(signalHead_t,curSignal->signalHeads,i);
		DrawSegs (&signalEditD, sh->headPos, 0.0, sh->headType->headSegs.ptr, sh->headType->headSegs.cnt, trackGauge, wDrawColorBlack);
		HeadAppearance_p a = &DYNARR_N(HeadAppearance_t,sh->headType->headAppearances,sh->currentHeadAppearance);
		DrawSegs (&signalEditD, sh->headPos, 0.0, a->appearanceSegs.ptr, a->appearanceSegs.cnt, trackGauge, wDrawColorBlack);
	}
}


static void EditSignalDialog()
{
    signalData_p xx;
    wIndex_t ia;
    
    if ( !signalEditW ) {
        ParamRegister( &signalEditPG );
        signalEditW = ParamCreateDialog (&signalEditPG,
                                         MakeWindowTitle(_("Edit signal")),
                                         _("Ok"), SignalEditOk, 
                                         SignalEditCancel, TRUE, NULL, 
                                         F_RESIZE|F_RECALLSIZE|F_BLOCK,
                                         SignalEditDlgUpdate );
    }
    if (signalCreate_P) {
        signalEditName[0] = '\0';
        signalEditLever[0] = '\0';
        signalEditHeadCount = xx->signalHeads.cnt;
        wListClear( aspectSelL );
        wListClear( headSelL );
        DYNARR_RESET( signalAspect_t, signalAspect_da );
        DYNARR_RESET( signalHead_t, signalHead_da );
    }
	xx = getSignalData ( signalEditTrack );
	strncpy(signalEditName,xx->signalName,STR_SHORT_SIZE);
	signalEditHeadCount = xx->signalHeads.cnt;
	signalEditOrig = xx->orig;
	signalEditAngle = xx->angle;
	signalEditTrackConnected = xx->track;
	signalEditEP = xx->ep;
	wListClear( aspectSelL );
	DYNARR_RESET( signalAspect_p, signalAspect_da );
	for (ia = 0; ia < xx->signalAspects.cnt; ia++) {
		snprintf(message,sizeof(message),"%s\t%s",(DYNARR_N(signalAspect_t,xx->signalAspects,ia)).aspectName,
				(DYNARR_N(signalAspect_t,xx->signalAspects,ia)).aspectScript);
		wListAddValue( aspectSelL, message, NULL, NULL );
		DYNARR_APPEND( signalAspect_t, signalAspect_da, 10 );
		signalAspect(signalAspect_da.cnt-1).aspectName = MyStrdup((DYNARR_N(signalAspect_t,xx->signalAspects,ia)).aspectName);
		signalAspect(signalAspect_da.cnt-1).aspectScript = MyStrdup((DYNARR_N(signalAspect_t,xx->signalAspects,ia)).aspectScript);
	}
    ParamLoadControls( &signalEditPG );
    ParamControlActive( &signalEditPG, I_SIGNALASPECTEDIT, FALSE );
    ParamControlActive( &signalEditPG, I_SIGNALASPECTADD, TRUE );
    ParamControlActive( &signalEditPG, I_SIGNALASPECTDELETE, FALSE );
    wShow( signalEditW );
}

EXPORT track_p NewSignal(
		TRKINX_T index,
		coOrd pos,
		ANGLE_T angle,
		char * title,
		char * name,
		char * plate,
		track_p track,
		EPINX_T ep,
		dynArr_t staticSegs[],
		dynArr_t heads,
		dynArr_t aspects,
		dynArr_t groups
		)
{
	track_p trk;
	struct signalData_t * xx;
	// Deep copy Signal details from another Signal
	trk = NewTrack( index, T_SIGNAL, 0, sizeof (*xx) + 1 );
	xx = getSignalData(trk);

	*sig_last = xx;
	sig_last = &xx->next_signal;
	xx->next_signal = NULL;

	xx->signal_track = trk;
	xx->orig = pos;
	xx->angle = angle;
	xx->signalName = MyStrdup(name);
	xx->title = MyStrdup( title );

	xx->track_side = SignalSide;

	for (int i=i;i<3;i++) {
		AppendSegsToArray(&xx->staticSignalSegs[i],&staticSegs[i]);
	}
	for (int i=0;i<aspects.cnt;i++) {
		DYNARR_APPEND(signalAspect_t,xx->signalAspects,aspects.cnt);
		signalAspect_p sa = &DYNARR_N(signalAspect_t,xx->signalAspects,i);
		signalAspect_p sa1 = &DYNARR_N(signalAspect_t,aspects,i);
		sa->aspectName = strdup(sa1->aspectName);
		sa->aspectScript = strdup(sa1->aspectScript);
		sa->aspectType = sa1->aspectType;
		for (int j=0;j<sa1->headAspectMap.cnt;j++) {
			DYNARR_APPEND(headAspectMap_t,sa->headAspectMap,sa1->headAspectMap.cnt);
			headAspectMap_p hm = &DYNARR_N(headAspectMap_t,sa->headAspectMap,j);
			headAspectMap_p hm1 = &DYNARR_N(headAspectMap_t,sa1->headAspectMap,j);
			hm->aspectMapHeadAppearance = strdup(hm1->aspectMapHeadAppearance);
			hm->aspectMapHeadAppearanceNumber = hm1->aspectMapHeadAppearanceNumber;
			hm->aspectMapHeadNumber = hm1->aspectMapHeadNumber;
		}
	}
	for (int i=0;i<heads.cnt;i++) {
		DYNARR_APPEND(signalHead_t,xx->signalHeads,heads.cnt);
		signalHead_p h = &DYNARR_N(signalHead_t,xx->signalHeads,i);
		signalHead_p h1 = &DYNARR_N(signalHead_t,heads,i);
		h->headName = strdup(h1->headName);
		h->headPos= h1->headPos;
		h->headTypeName = strdup(h1->headTypeName);
		h->headType = FindHeadType(h1->headTypeName,GetLayoutCurScale());
		for (int i=0;i<3;i++) {
			AppendSegsToArray(&h->headSegs[i],&h1->headSegs[i]);
		}
	}
	for (int i=0;i<groups.cnt;i++) {
		DYNARR_APPEND(signalGroup_t,xx->signalGroups,groups.cnt);
		signalGroup_p g = &DYNARR_N(signalGroup_t,xx->signalGroups,i);
		signalGroup_p g1 = &DYNARR_N(signalGroup_t,groups,i);
		for (int j=0;j<g1->groupInstances.cnt;j++) {
			DYNARR_APPEND(signalGroupInstance_t,g->groupInstances,5);
			signalGroupInstance_p gi = &DYNARR_N(signalGroupInstance_t,g->groupInstances,j);
			signalGroupInstance_p gi1 = &DYNARR_N(signalGroupInstance_t,g1->groupInstances,j);
			gi->conditions = strdup(gi1->conditions);
		}
	}
	xx->paramFileIndex = curParamFileIndex;
	xx->track = track;
	xx->ep = ep;
	xx->plate = plate;
	xx->barscale = curBarScale>0?curBarScale:-1;
	ComputeSignalBoundingBox( trk, SignalDisplay );
	xx->size.x = trk->hi.x-trk->lo.x;
	xx->size.y = trk->hi.y-trk->lo.y;
	SetDescriptionOrig( trk );

	return trk;
}

EXPORT void UpdateSignals() {
	track_p sig = NULL;
	while(SignalIterate(&sig)) {
		CleanSegs(&GetSignalData(sig)->currSegs);
		setAspect(sig,GetSignalData(sig)->currentAspect);
	}

}


static void AddSignal( void )
{
	track_p trk;
	struct extraData *xx;
	wIndex_t titleLen;

	UndoStart( _("Place Signal"), "newSignal" );
	titleLen = strlen( curSignal->title );
	trk = NewSignal(0, signalEditOrig, signalEditAngle, curSignal->title, signalEditName, signalEditLever, signalEditTrackConnected, signalEditEP,
			&curSignal->staticSignalSegs[0], curSignal->signalHeads, curSignal->signalAspects, curSignal->signalGroups);
	xx = GetTrkExtraData(trk);

	SetTrkVisible( trk, TRUE );
	ComputeSignalBoundingBox( trk, SignalDisplay );

	SetDescriptionOrig( trk );
	xx->descriptionOff = zero;
	xx->descriptionSize = zero;

	DrawNewTrack( trk );

	UndoEnd();

	signalEditOrig = zero;
	signalEditAngle = 0.0;
	signalEditEP = -1;
	signalEditName[0] = '\0';
	signalEditLever[0] = '\0';
	signalEditTrackConnected = NULL;
}

static void SignalOk( void )
{
	AddSignal();
	Reset();
}

static void SignalDlgUpdate(
		paramGroup_p pg,
		int inx,
		void * valueP )
{
	signalData_t * sd;
	if ( inx != I_LIST ) return;
	sd = (signalData_t*)wListGetItemContext( (wList_p)pg->paramPtr[inx].control, (wIndex_t)*(long*)valueP );
	AddSignal();
	curSignal = sd;
	RedrawSignal();

}


static void EditSignal (track_p trk)
{
    signalCreate_P = FALSE;
    signalEditTrack = trk;
    EditSignalDialog();
}

static void CreateNewSignal (coOrd orig, ANGLE_T angle, track_p track, EPINX_T ep)
{
    signalCreate_P = TRUE;
    signalEditOrig = orig;
    signalEditAngle = angle;
    signalEditTrackConnected = track;
    signalEditEP = ep;
    EditSignalDialog();

}

/*
 * Always draw the Signal in the HotBar and for placement as an Diagram...
 */
static void DDrawSignal(drawCmd_p d, coOrd orig, ANGLE_T angle,
                        signalData_p signal, DIST_T scaleRatio,
                        wDrawColor color )
{
	if (signal == NULL) return;
		CleanSegs(&Da.currSegs);
		AppendSegsToArray(&signal->currSegs,&signal->staticSignalSegs[SIGNAL_DISPLAY_DIAG]);
		signalAspect_t aspect;
		for (int i=0;i<signal->signalHeads.cnt-1;i++) {
			/* All Heads */
			signalHead_p head = &DYNARR_N(signalHead_t,signal->signalHeads,i);
			AppendTransformedSegs(&signal->currSegs,&head->headSegs[SIGNAL_DISPLAY_DIAG],head->headPos,zero,0.0);
			//Drawing is always Appearance 0
			signalHeadType_p type = head->headType;
			int pre_cnt = signal->currSegs.cnt;
			/* Note Segs already moved to appearance origin and rotated */
			/* Now only need to be be placed relative to the rest */
			HeadAppearance_p a = &DYNARR_N(HeadAppearance_t,type->headAppearances,SIGNAL_DISPLAY_DIAG);
			AppendTransformedSegs(&signal->currSegs,&a->appearanceSegs, head->headPos, zero, 0.0 );
		}

	/*Draw entire signal at pos and angle */
	DrawSegsO(d,NULL,orig,angle,(trkSeg_p)signal->currSegs.ptr,signal->currSegs.cnt,trackGauge,color,0);

}



static STATUS_T CmdSignalAction ( wAction_t action, coOrd pos )
{
    static track_p trk;
    switch (action) {
    case C_START:
        InfoMessage(_("Place base of signal on Track"));
        trk = NULL;
        return C_CONTINUE;
    case C_DOWN:
        trk = OnTrack(&pos,FALSE,TRUE);
        if (!trk) {
        	NoticeMessage2( 0, MSG_SIGNAL_NO_TRACK_HERE, _("Ok"), NULL );
        	return C_TERMINATE;
        }
        EPINX_T ep = PickEndPoint(pos,trk);
        Da.pos1 = GetTrkEndPos(trk,ep);
        Da.orient = NormalizeAngle(GetTrkEndAngle(trk,ep)+180.0);
        Translate(&pos,Da.pos1,NormalizeAngle(Da.orient+((SignalSide==SIGNAL_SIDE_LEFT)?-90:90)),ceil(8.0*12.0/curScaleRatio));  //Start one Track Width away
        Da.pos0 = pos;
        InfoMessage(_("Drag to re-position signal"));
        return C_CONTINUE;
    case C_MOVE:
    	if (!trk) return C_CONTINUE;
    	coOrd diff;
    	ANGLE_T a = DifferenceBetweenAngles(Da.orient,FindAngle(Da.pos0,pos));
    	DIST_T d = FindDistance(pos,Da.pos0)*cos(R2D(a));
    	Translate(&Da.pos1,Da.pos0,d,NormalizeAngle(GetTrkEndAngle(trk,ep)+90.0));
        DDrawSignal( &tempD, Da.pos1, Da.orient, curSignal, GetScaleRatio(GetLayoutCurScale()), wDrawColorBlack );
        return C_CONTINUE;
    case C_UP:
    	if (!trk) return C_CONTINUE;
        CreateNewSignal(Da.pos1,Da.orient,trk,ep);
        return C_TERMINATE;
    case C_REDRAW:
    case C_CANCEL:
        DDrawSignal( &tempD, Da.pos1, Da.orient, curSignal, GetScaleRatio(GetLayoutCurScale()), wDrawColorBlack );
        return C_CONTINUE;
    default:
        return C_CONTINUE;
    }
}


static STATUS_T CmdSignal (wAction_t action, coOrd pos) {
	wIndex_t signalIndex;
	signalData_t * signalPtr;

	switch (action & 0xFF) {


	case C_START:
		if (signalW == NULL) {
			ParamRegister( &signalPG );
			signalW = ParamCreateDialog( &signalPG, MakeWindowTitle(_("Signal")), _("Close"),
					(paramActionOkProc)SignalOk, NULL, TRUE, NULL, F_RESIZE|F_RECALLSIZE|PD_F_ALT_CANCELLABEL, SignalDlgUpdate );
			InitNewTurn( signalNewM );
		}
		signalIndex = wListGetIndex( signalListL );
		signalPtr = curSignal;
		wShow( signalW );
		SignalChange( CHANGE_PARAMS|CHANGE_SCALE );
		if (curSignal == NULL) {
			NoticeMessage2( 0, MSG_SIGNAL_NO_SIGNAL, _("Ok"), NULL );
			return C_TERMINATE;
		}
		if (signalIndex > 0 && signalPtr) {
			curSignal = signalPtr;
			wListSetIndex( signalListL, signalIndex );
			RedrawSignal();
		}
		InfoMessage( _("Pick signal and place on the layout"));
		ParamLoadControls( &signalPG );
		ParamGroupRecord( &signalPG );
		return CmdSignalAction( action, pos );
	case C_DOWN:
	case C_RDOWN:
		ParamDialogOkActive( &signalPG, TRUE );
		if (hideSignalWindow)
			wHide( signalW );
			/* no break */
	case C_MOVE:
	case C_RMOVE:
		return CmdSignalAction( action, pos );
	case C_UP:
	case C_RUP:
		if (hideSignalWindow)
			wShow( signalW );
		InfoMessage( _("Left drag to move, right drag to rotate, press Space or Return to fix Signal in place or Esc to cancel") );
		return CmdSignalAction( action, pos );
	case C_LCLICK:
		//HilightEndPt();
		CmdSignalAction( action, pos );
		//HilightEndPt();
		return C_CONTINUE;
	case C_CANCEL:
		wHide( signalW );
		return CmdSignalAction( action, pos );
	case C_TEXT:
		CmdSignalAction( action, pos );
		return C_CONTINUE;
	case C_OK:
	case C_FINISH:
	case C_CMDMENU:
	case C_REDRAW:
		return CmdSignalAction( action, pos );

	default:
		return C_CONTINUE;
	}
}


static coOrd sighiliteOrig, sighiliteSize;
static POS_T sighiliteBorder;
static wDrawColor sighiliteColor = 0;

static void DrawSignalTrackHilite( void )
{
	wPos_t x, y, w, h;
	if (sighiliteColor==0)
		sighiliteColor = wDrawColorGray(87);
	w = (wPos_t)((sighiliteSize.x/mainD.scale)*mainD.dpi+0.5);
	h = (wPos_t)((sighiliteSize.y/mainD.scale)*mainD.dpi+0.5);
	mainD.CoOrd2Pix(&mainD,sighiliteOrig,&x,&y);
	wDrawFilledRectangle( tempD.d, x, y, w, h, sighiliteColor, wDrawOptTemp );
}

static int SignalMgmProc ( int cmd, void * data )
{
    track_p trk = (track_p) data;
    signalData_p xx = getSignalData(trk);
    /*char msg[STR_SIZE];*/
    
    switch ( cmd ) {
    case CONTMGM_CAN_EDIT:
        return TRUE;
        break;
    case CONTMGM_DO_EDIT:
        EditSignal(trk);
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
            sighiliteBorder = mainD.scale*0.1;
            if ( sighiliteBorder < trackGauge ) sighiliteBorder = trackGauge;
            GetBoundingBox( trk, &sighiliteSize, &sighiliteOrig );
            sighiliteOrig.x -= sighiliteBorder;
            sighiliteOrig.y -= sighiliteBorder;
            sighiliteSize.x -= sighiliteOrig.x-sighiliteBorder;
            sighiliteSize.y -= sighiliteOrig.y-sighiliteBorder;
            DrawSignalTrackHilite();
            xx->IsHilite = TRUE;
        }
        break;
    case CONTMGM_UN_HILIGHT:
        if (xx->IsHilite) {
            sighiliteBorder = mainD.scale*0.1;
            if ( sighiliteBorder < trackGauge ) sighiliteBorder = trackGauge;
            GetBoundingBox( trk, &sighiliteSize, &sighiliteOrig );
            sighiliteOrig.x -= sighiliteBorder;
            sighiliteOrig.y -= sighiliteBorder;
            sighiliteSize.x -= sighiliteOrig.x-sighiliteBorder;
            sighiliteSize.y -= sighiliteOrig.y-sighiliteBorder;
            DrawSignalTrackHilite();
            xx->IsHilite = FALSE;
        }
        break;
    case CONTMGM_GET_TITLE:
        sprintf(message,"\t%s\t",xx->signalName);
        break;
    }
    return FALSE;
}

#include "bitmaps/signal.xpm"

EXPORT void SignalMgmLoad ( void )
{
    track_p trk;
    static wIcon_p signalI = NULL;
    
    if (signalI == NULL) {
        signalI = wIconCreatePixMap( signal_xpm );
    }
    
    TRK_ITERATE(trk) {
        if (GetTrkType(trk) != T_SIGNAL) continue;
        ContMgmLoad (signalI, SignalMgmProc, (void *) trk );
    }
}


/**
 * Event procedure for the hotbar.
 *
 * \param op   IN requested function
 * \param data IN	pointer to info on selected element
 * \param d    IN
 * \param origP IN
 * \return
 */

static char * CmdSignalHotBarProc(
		hotBarProc_e op,
		void * data,
		drawCmd_p d,
		coOrd * origP )
{
	signalData_t * sd = (signalData_t*)data;
	switch ( op ) {
	case HB_SELECT:		/* new element is selected */
		CmdSignalAction( C_FINISH, zero ); 		/* finish current operation */
		curSignal = sd;
		DoCommandB( (void*)(intptr_t)signalHotBarCmdInx ); /* continue with new signal */
		return NULL;
	case HB_LISTTITLE:
		FormatSignalPartTitle( listLabels, sd->title );
		if (message[0] == '\0')
			FormatSignalPartTitle( listLabels|LABEL_DESCR, sd->title );
		return message;
	case HB_BARTITLE:
		FormatSignalPartTitle( hotBarLabels<<1, sd->title );
		return message;
	case HB_FULLTITLE:
		return sd->title;
	case HB_DRAW:
		if (sd->currSegs.cnt == 0) {
			RebuildSignalSegs(sd, SIGNAL_DISPLAY_DIAG);
		}
		DrawSegs( d, *origP, 0.0, sd->currSegs.ptr, sd->currSegs.cnt, trackGauge, wDrawColorBlack );
		return NULL;
	}
	return NULL;

}

EXPORT BOOL_T WriteSignalSystem(FILE * f) {
	return TRUE;
}

#define ACCL_SIGNAL 0

EXPORT void AddHotBarSignals( void )
{
	wIndex_t inx;
	signalData_p sd;
	for ( inx=0; inx < signalData_da.cnt; inx ++ ) {
		sd = &DYNARR_N(signalData_t,signalData_da,inx);
		if (!( IsParamValid(sd->paramFileIndex)  &&
			CompatibleScale( TRUE, sd->scaleInx, GetLayoutCurScale())))
			continue;
		AddHotBarElement( sd->signalName, sd->size, sd->orig, FALSE, FALSE, sd->barscale, sd, CmdSignalHotBarProc );
	}
}

EXPORT void InitCmdSignal ( wMenu_p menu )
{
    AddMenuButton( menu, CmdSignal, "cmdSignal", _("Signal"), 
                   wIconCreatePixMap( signal_xpm ), LEVEL0_50, IC_STICKY|IC_POPUP2, ACCL_SIGNAL, NULL );
}

EXPORT void InitTrkSignal ( void )
{
    log_signal = LogFindIndex ( "signal" );
    //InitSignalSystem();
    AddParam( "SIGNALPART", ReadSignalPart);
    AddParam( "SIGNALHEAD", ReadHeadType );
    AddParam( "SIGNALSYSTEM ", ReadSignalSystem);
    AddParam( "SIGNALPOST", ReadSignalPost );
    //AddParam( "SIGNALPROTO", ReadSignalProto);
    T_SIGNAL = InitObject(&signalCmds);
}
