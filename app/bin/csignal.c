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

EXPORT TRKTYP_T T_SIGNAL = -1;

static int log_signal = 0;


#if 0
static drawCmd_t signalD = {
	NULL,
	&screenDrawFuncs,
	0,
	1.0,
	0.0,
	{0.0,0.0}, {0.0,0.0},
	Pix2CoOrd, CoOrd2Pix };

static char signalName[STR_SHORT_SIZE];
static int  signalHeadCount;
#endif



/*
 * A Signaling System is a set of elements that can be used to send instructions to drivers
 * It has a set of HeadTypes which show indications and a set of MastTypes that integrate those Heads into a SignalMast.
 * There can be several Systems active on one layout - reflecting different heritages or vintages.
 * Each HeadType and mastType is qualified by its system.
 */
typedef struct signalSystem_t {
	char * systemName;					//Name of System
	char * notes;						//Explanation
	dynArr_t headTypes;					//Types of Heads - Heads to add to Signals
	dynArr_t mastTypes;					//Types of Masts - Cloneable signals
	dynArr_t aspectTypes;				//Types of Aspects - Names only
} signalSystem_t, *signalSystem_p;

static dynArr_t signalSystems;
#define signalSystem(N) DYNARR_N( signalSystem_t, signalSystems, N )



/*
 * An AspectType maps an AspectName to a Speed
 */
typedef struct aspectType_t {
	char * aspectName;
	int speed;
} aspectType_t, *aspectType_p;


/* These are the normal Aspect Names that JMRI will recognize they are used in XTrackCAD if no Aspects are defined */
typedef enum baseAspects {Danger, Proceed, PreliminaryCaution, Caution, FlashPreliminaryCaution, FlashCaution, Off, On, CallOn, Shunt, Warning};
const char *baseAspectsNames[] = { "Danger", "Proceed", "Preliminary Caution", "Flash Preliminary Caution", "Caution", "Flash Caution", "Off", "On", "Call-On", "Shunt", "Warning" };

static aspectType_t defaultAspectsSpeedMap[] = {
		{Proceed,100},
		{Caution, 40},
		{FlashCaution, 40},
		{PreliminaryCaution,75},
		{FlashPreliminaryCaution, 50},
		{Danger, 0},
		{CallOn, 15},
		{Shunt, 15},
		{Warning, 15},
		{Off, 15},
		{On, 0},
};

/*
 * A Map from the Aspects from this Signal to all the Heads and the Indications that they show
 */
typedef struct headAspectMap_t {
	char * aspectMapHeadName;					//Head Name
	int * aspectMapHeadNumber;					//Which Head is that on this Signal
	char * aspectMapHeadIndication;				//Indication name
	int * aspectMapHeadIndicationNumber;		//Which indication is that on the head
} headAspectMap_t, *headAspectMap_p;


/*
 * An Aspect is an indication by a Signal Mast to a driver of what he is allowed to do. It may be shown by using one or more Heads.
 * The Aspect in JMRI communicates a max speed to the "driver" - this is achieved.
 */
typedef struct signalAspect_t {
    char * aspectName;					//Aspect
    char * aspectScript;
    dynArr_t headMap;					//Array of all Head settings for this Aspect
    aspectType_p baseAspect;
} signalAspect_t, *signalAspect_p;

#define headaspect(N) DYNARR_N( headAspectMap_t, headMap, N )

/*
 * An Indication is a unique display by one Head
 */
typedef struct Indication_t {
	char * indicationName;				//Indication name
	dynArr_t indicationSegs; 			//How to Draw it
} Indication_t, *Indication_p;

typedef enum baseIndicators {Diagram, UnLit, Red, Green, Yellow, Lunar, FlashRed, FlashGreen, FlashYellow, FlashLunar, On, Off, Lit}; //Eight predefined Indications

/*
 * A HeadType is a parameter definition from which shows all the Indications of a Head
 */
typedef struct headType_t {
	char * headName;
	dynArr_t headSegs;                  //Draw Segments that don't change for head (background, lamp body, etc)
	dynArr_t headIndications;	 		//All things the head can show
} headType_t, *headType_p;

static dynArr_t headTypes_da;			//Array of headTypes of all signals in use in the layout
#define signalHeadType(N) DYNARR_N( headType_t, headTypes_da, N )

/*
 * A Head is the smallest controllable unit. It could be as small as one light or arm, or as complex as a matrix display
 * The head is found by its ordinal position on the post.
 */
typedef struct signalHead_t {
	char * headName;					//Often includes Lever #
	coOrd headPos;						//Relative to Post
	char * headTypeName;				//Type Name
	headType_p headType;				//Pointer to common HeadType definition
	int currentHeadIndication;			//Index of indication within HeadType.Indications
} signalHead_t, *signalHead_p;

static dynArr_t signalHead_da;          //Array of active heads
#define signalHead(N) DYNARR_N( signalHead_t, signalHead_da, N )

static dynArr_t signalAspect_da;		//Array of signals Aspects
#define signalAspect(N) DYNARR_N( signalAspect_t, signalAspect_da, N )

/*
 * A Map from the Aspects from this Signal to all the Heads and the Indications that they show
 */
typedef struct headAspectMap_t {
	char * aspectMapHeadName;					//Head Name
	int * aspectMapHeadNumber;					//Which Head is on this Signal
	char * aspectMapHeadIndication;				//Indication name
	int * aspectMapHeadIndicationNumber;		//Which indication is needed for that Head
} headAspectMap_t, *headAspectMap_p;

/*
 * A Real Signal (Mast). This will contain an arbitrary number of Heads.
 *
 * For display, the signal segs are first,
 * all the Heads ->segs are drawn,
 * the Active Aspect -> HeadAspectMap entries -> Indications -> segs for each Head by Indication
 */
typedef struct signalData_t {
    coOrd orig;							//Relative to a track/structure or to the origin
    ANGLE_T angle;						//Relative to a track/structure or to the origin
    track_p track;						//Track/Structure for Signal to align to or NULL
    char * signalName;					//Unique Name of Signal
    char * plate;						//Identification of Signal - often includes lever #
    dynArr_t signalHeads;				//Array of all the heads
    BOOL_T IsHilite;
    dynArr_t signalAspects;					//Array of all the Aspects for the Signal
    dynArr_t staticSignalSegs;			//Static draw elements for the signal (like dolls/posts)
    dynArr_t currSegs;					//The current set of Segs to draw
    int currentAspect;					//Current Aspect index within Aspects
} signalData_t, *signalData_p;

#define head(N) DYNARR_N( signalHead_t, signalHead, N )
#define aspect(N) DYNARR_N( signalAspect_t, signalAspects, N )

typedef struct signalMastType_t {
	char * mastTypeName;				//Type of Mast
	dynArr_t staticSegs;				//Static segs (like dolls or posts)
	dynArr_t signalHeads;				//Array of Heads on this type of Mast
	dynArr_t Aspects;					//Array of Aspects these can show
} signalMastType_t, *signalMastType_p;

static signalData_p GetsignalData ( track_p trk )
{
    return (signalData_p) GetTrkExtraData(trk);
}

#define BASEX 6
#define BASEY 0

#define MASTX 0
#define MASTY 12

#define HEADR 4


#define signal_SF (3.0)

static void DDrawSignal(drawCmd_p d, coOrd orig, ANGLE_T angle, 
                        wIndex_t numHeads, DIST_T scaleRatio, 
                        wDrawColor color )
{


	coOrd p1, p2;
    ANGLE_T x_angle, y_angle;
    DIST_T hoffset;
    wIndex_t ihead;
    
    x_angle = 90-(360-angle);
    if (x_angle < 0) x_angle += 360;
    y_angle = -(360-angle);
    if (y_angle < 0) y_angle += 360;
    
    Translate (&p1, orig, x_angle, (-BASEX) * signal_SF / scaleRatio);
    Translate (&p1, p1,   y_angle, BASEY * signal_SF / scaleRatio);
    Translate (&p2, orig, x_angle, BASEX * signal_SF / scaleRatio);
    Translate (&p2, p2,   y_angle, BASEY * signal_SF / scaleRatio);
    DrawLine(d, p1, p2, 2, color);
    p1 = orig;
    Translate (&p2, orig, x_angle, MASTX * signal_SF / scaleRatio);
    Translate (&p2, p2,   y_angle, MASTY * signal_SF / scaleRatio);
    DrawLine(d, p1, p2, 2, color);
    hoffset = MASTY;
    for (ihead = 0; ihead < numHeads; ihead++) {
        Translate (&p1, orig, x_angle, MASTX * signal_SF / scaleRatio);
        Translate (&p1, p1,   y_angle, (hoffset+HEADR) * signal_SF / scaleRatio);
        DrawFillCircle(d,p1,HEADR * signal_SF / scaleRatio,color);
        hoffset += HEADR*2;
    }
}

static void DrawSignal (track_p t, drawCmd_p d, wDrawColor color )
{
    signalData_p xx = GetsignalData(t);
    /* Draw Base */
    DrawSegsO(d,t,xx->orig,xx->angle,(trkSeg_p)xx->signalSegs.ptr,xx->signalSegs.cnt,GetTrkGauge(t),color,0);
	for (int i=0;i<xx->signalHeads.cnt;i++) {
		signalHead_t h = signalHead(i);
		coOrd orig;
		Translate(&orig,xx->orig,h.headPos,xx->angle);
		headType_t ht = h.headType;
		if (!ht) continue;
		/*Draw fixed HeadSegs */
		DrawSegsO(d,t,orig,xx->angle,ht.headSegs,ht.headSegs.cnt,GetTrkGauge(t),color,0);
		if (programMode == MODE_TRAIN) {
			if (h.currentHeadIndication < 0 || (h.currentHeadIndication > ht.headIndications.cnt-1)) continue;
			Indication_t si = ht.headIndications.ptr[h.currentHeadIndication];
			DrawSegsO(d,t,orig,xx->angle,si.indicationSegs,si.indicationSegs.cnt,GetTrkGauge(t),color,0);
		} else {
			Indication_t si = ht.headIndications.ptr[0];  /* Base state is always appearance 0 */
			DrawSegsO(d,t,orig,xx->angle,si.indicationSegs,si.indicationSegs.cnt,GetTrkGauge(t),color,0);
		}
	}

}

static void ComputeSignalBoundingBox (track_p t )
{
    coOrd lo, hi, lo2, hi2;
    signalData_p xx = GetsignalData(t);
    GetSegBounds(xx->orig,xx->angle,(trkSeg_p)xx->signalSegs.ptr,xx->signalSegs.cnt,&lo,&hi);
    hi.x += lo.x;
    hi.y += lo.y;
	for (int i=0;i<xx->signalHeads.cnt;i++) {
		signalHead_t h = signalHead(i);
		coOrd orig;
		Translate(&orig,xx->orig,h.headPos,xx->angle);
		headType_t ht = h.headType;
		if (!ht) continue;
		/*Draw fixed HeadSegs */
		GetSegBounds(orig,xx->angle,ht.headSegs,ht.headSegs.cnt,&lo2,hi2);
		hi2.x +=lo2.x;
		hi2.y +=hi2.x;
		if (lo.x>lo2.x) lo.x = lo2.x;
		if (lo2.y>lo2.y) lo.x = lo2.y;
		if (hi.x<hi2.x)  hi.x = hi2.x;
		if (hi.y<hi2.y)  hi.y = hi2.y;
		for (int j=0;j<ht.headIndications.cnt;j++) {
			Indication_t si = ht.headIndications.ptr[j];  /* Base state is always appearance 0 */
			GetSegBounds(orig,xx->angle,si.indicationSegs,si.indicationSegs.cnt,&lo2,&hi2);
			hi2.x +=lo2.x;
			hi2.y +=hi2.x;
			if (lo.x>lo2.x) lo.x = lo2.x;
			if (lo2.y>lo2.y) lo.x = lo2.y;
			if (hi.x<hi2.x)  hi.x = hi2.x;
			if (hi.y<hi2.y)  hi.y = hi2.y;
		}
	}
    SetBoundingBox(t, hi, lo);
}

static DIST_T DistanceSignal (track_p t, coOrd * p )
{
    signalData_p xx = GetsignalData(t);
    return FindDistance(xx->orig, *p);
}

static struct {
    char name[STR_SHORT_SIZE];
    coOrd pos;
    ANGLE_T orient;
    long heads;
    char aspectName[STR_SHORT_SIZE];
} signalProperties;

typedef enum { NM, PS, OR, HD, AS } signalDesc_e;
static descData_t signalDesc[] = {
    /* NM */ { DESC_STRING, N_("Name"),     &signalProperties.name, sizeof(signalProperties.name) },
    /* PS */ { DESC_POS,    N_("Position"), &signalProperties.pos },
    /* OR */ { DESC_ANGLE,  N_("Angle"),    &signalProperties.orient },
    /* HD */ { DESC_LONG,   N_("Number Of Heads"), &signalProperties.heads },
	/* AS */ { DESC_STRING, N_("Aspect"),   &signalProperties.aspectName, sizeof(signalProperties.aspectName) },
    { DESC_NULL } };

static void UpdateSignalProperties ( track_p trk, int inx, descData_p
                                     descUpd, BOOL_T needUndoStart )
{
    signalData_p xx = GetsignalData( trk );
    const char *thename;
    char *newName;
    BOOL_T changed, nChanged, pChanged, oChanged;
    
    switch (inx) {
    case NM: break;
    case PS: break;
    case OR: break;
    case HD: break;
    case -1:
        changed = nChanged = pChanged = oChanged = FALSE;
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

        if (signalProperties.pos.x != xx->orig.x ||
            signalProperties.pos.y != xx->orig.y) {
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
        }
        if (oChanged) {
            xx->angle = signalProperties.orient;
        }
        if (pChanged || oChanged) { 
            ComputeSignalBoundingBox( trk );
            DrawNewTrack( trk );
        }
        break;
    }
}


static void DescribeSignal (track_p trk, char * str, CSIZE_T len ) 
{
    signalData_p xx = GetsignalData(trk);
    
    strcpy( str, _(GetTrkTypeName( trk )) );
    str++;
    while (*str) {
        *str = tolower((unsigned char)*str);
        str++;
    }
    sprintf( str, _("(%d [%s]): Layer=%d, %d heads at %0.3f,%0.3f A%0.3f, Aspect=%s"),
             GetTrkIndex(trk), 
             xx->signalName,GetTrkLayer(trk)+1, xx->signalHeads.cnt,
             xx->orig.x, xx->orig.y,xx->angle, ((signalAspect_t)xx->Aspects.ptr[xx->currentAspect]).aspectName);
    strncpy(signalProperties.name,xx->signalName,STR_SHORT_SIZE-1);
    signalProperties.name[STR_SHORT_SIZE-1] = '\0';
    signalProperties.pos = xx->orig;
    signalProperties.orient = xx->angle;
    signalProperties.heads = xx->signalHeads.cnt;
    signalDesc[HD].mode = DESC_RO;
    signalDesc[AS].mode = DESC_RO;
    signalDesc[NM].mode = DESC_NOREDRAW;
    DoDescribe( _("Signal"), trk, signalDesc, UpdateSignalProperties );
}

static void DeleteSignal ( track_p trk )
{
    wIndex_t ia;
    signalData_p xx = GetsignalData(trk);
    MyFree(xx->signalName); xx->signalName = NULL;
    for (ia = 0; ia < xx->Aspects.cnt; ia++) {
    	MyFree(DYNARR_N(signalAspect_t,xx->Aspects,ia).aspectName);
        MyFree(DYNARR_N(signalAspect_t,xx->Aspects,ia).aspectScript);
        signalAspect_t sa = DYNARR_N(signalAspect_t,xx->Aspects,ia);
        for (int hm = 0; hm < sa.headMap.cnt; hm++) {
        	headAspectMap_t headm = DYNARR_N(headAspectMap_t,sa.headMap,hm);
        	MyFree(headm.aspectMapHeadIndication);
        	MyFree(headm.aspectMapHeadName);
        }
        MyFree(sa.headMap.ptr);
    }
    MyFree(xx->Aspects.ptr); xx->Aspects.ptr = NULL;
    for (int h = 0; h <xx->signalHeads.cnt; h++) {
    	MyFree(DYNARR_N(signalHead_t,xx->signalHeads,h).headName);
    	MyFree(DYNARR_N(signalHead_t,xx->signalHeads,h).headTypeName);
    }
    MyFree(xx->signalHeads.ptr); xx->signalHeads.ptr = NULL;
}

static BOOL_T WriteSignal ( track_p t, FILE * f )
{
    BOOL_T rc = TRUE;
    wIndex_t ia,ih,im;
    signalData_p xx = GetsignalData(t);
    rc &= fprintf(f, "SIGNAL %d %d %s %d %0.6f %0.6f %0.6f %d \"%s\"\n",
                  GetTrkIndex(t), GetTrkLayer(t), GetTrkScaleName(t), 
                  GetTrkVisible(t), xx->orig.x, xx->orig.y, xx->angle, 
                  xx->signalHeads.cnt, xx->signalName)>0;
    rc &= WriteSegs(f,xx->signalSegs.cnt,xx->signalSegs.ptr);
    for (ih = 0; ih < xx->signalHeads.cnt; ih++) {
    	signalHead_t sh = DYNARR_N(signalHead_t,xx->signalHeads,ih);
    	rc &= fprintf(f, "\tHEAD %d %0.6f %0.6f \"%s\" \"%s\"\n",
    			ih,
				(sh.headPos.x),
				(sh.headPos.y),
    			(sh.headName),
				(sh.headTypeName))>0;
    }
    for (ia = 0; ia < xx->Aspects.cnt; ia++) {
    	signalAspect_t sa = DYNARR_N(signalAspect_t,xx->Aspects,ia);
        rc &= fprintf(f, "\tASPECT \"%s\" \"%s\"\n",
			  (sa.aspectName),
			  (sa.aspectScript))>0;
        for (im = 0; im < sa.headMap.cnt; im++) {
			rc &= fprintf(f, "\tASPECTMAP \"%s\" \"%s\"\n",
			  (DYNARR_N(headAspectMap_t,sa.headMap,im).aspectMapHeadName),
			  (DYNARR_N(headAspectMap_t,sa.headMap,im).aspectMapHeadIndication))>0;
        }
    }
    rc &= fprintf( f, "\tEND\n" )>0;
    return rc;
}

static void ReadHeadType ( char * line) {
	char * typename;
	char * cp = NULL;
	if (!GetArgs(line+6,"q",&typename)) {
	        return;
	}
	DYNARR_APPEND(headType_p *, headTypes_da,10);
	headType_p ht = signalHeadType(headTypes_da.cnt-1);
	ht->headName = typename;
	while ( (cp = GetNextLine()) != NULL ) {
		while (isspace((unsigned char)*cp)) cp++;
		if ( strncmp( cp, "END", 3 ) == 0 ) {
			break;
		}
		if ( *cp == '\n' || *cp == '#' ) {
			continue;
		}
		if ( strncmp( cp, "SEGS", 4) == 0 ) {
			CleanSegs(&tempSegs_da);
			ReadSegs();
			AppendSegs(signalHeadType(headTypes_da.cnt-1).headSegs,tempSegs_da);
			continue;
		}
		char * indname;
		if ( strncmp( cp, "INDICATION", 10 ) == 0 ) {
			if (!GetArgs(cp+10,"q",&indname)) continue;
			DYNARR_APPEND( Indication_p *, ht->headIndications, 1 );
			Indication_p ind = DYNARR_N(Indication_t,ht->headIndications,ht->headIndications.cnt-1);
			ind->indicationName = indname;
			while ( (cp = GetNextLine()) != NULL ) {
				while (isspace((unsigned char)*cp)) cp++;
				if ( strncmp( cp, "END", 3 ) == 0 ) {
					break;
				}
				if ( *cp == '\n' || *cp == '#' ) {
					continue;
				}
				if ( strncmp( cp, "SEGS", 4) == 0 ) {
					CleanSegs(&tempSegs_da);
					ReadSegs();
					AppendSegs(ind->indicationSegs,tempSegs_da);
					continue;
				}
			}
		}
	}

}

static void ReadSignal ( char * line )
{
    /*TRKINX_T trkindex;*/
    wIndex_t index;
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
    if (!GetArgs(line+6,"dLsdpfdq",&index,&layer,scale, &visible, &orig, 
                 &angle, &numHeads,&name)) {
        return;
    }
    DYNARR_RESET( signalAspect_p, signalAspect_da );
    DYNARR_RESET( signalHead_t, xx->signalHeads);
    while ( (cp = GetNextLine()) != NULL ) {
        while (isspace((unsigned char)*cp)) cp++;
        if ( strncmp( cp, "END", 3 ) == 0 ) {
            break;
        }
        if ( *cp == '\n' || *cp == '#' ) {
            continue;
        }
        if ( strncmp( cp, "ASPECT", 6 ) == 0 ) {
            if (!GetArgs(cp+6,"qq",&aspname,&aspscript)) return;
            DYNARR_APPEND( signalAspect_p *, signalAspect_da, 1 );
            signalAspect_p sa =  signalAspect(signalAspect_da.cnt-1);
            sa->aspectName = aspname;
            sa->aspectScript = aspscript;
            if ( strncmp( cp, "ASPECTHEADMAP", 13 ) == 0 ) {
            	char * appname;
            	int headid;
				if (!GetArgs(cp+13,"dq",&headid,&appname)) return;
				DYNARR_APPEND( headAspectMap_p *, sa->headMap, 1 );
				headAspectMap_p am = DYNARR_N(headAspectMap_p *,sa->headMap,sa->headMap.cnt-1);
				am->aspectMapHeadNumber = headid;
				am->aspectMapHeadIndication = appname;
			}
        }
        if ( strncmp (cp, "HEAD", 4) == 0) {
        	char * headname;
        	coOrd headPos;
        	char * headType;
        	if (!GetArgs(cp+4, "qpq", &headname,&headPos,&headType)) return;
        	DYNARR_APPEND( signalHead_p *, xx->signalHeads, 1 );
        	signalHead_p sh = signalHead(xx->signalHeads.cnt-1);
        	sh->currentHeadIndication = 0;
        	sh->headName = headname;
        	sh->headPos = headPos;
        	sh->headTypeName = headType;
        	sh->headType = FindHeadType(headType);
        }
    }
    trk = NewTrack(index, T_SIGNAL, 0, sizeof(signalData_t)+1);
    SetTrkVisible(trk, visible);
    SetTrkScale(trk, LookupScale( scale ));
    SetTrkLayer(trk, layer);
    xx = GetsignalData ( trk );
    xx->name = name;
    DYNARR_APPEND(headData_t,xx->signalHeads,numHeads);
    for (int h=0; h<xx->signalHeads.cnt; h++) {
    	(DYNARR_N(headData_t,xx->signalHeads,h)).headName = signalHead(h).headName;
    	(DYNARR_N(headData_t,xx->signalHeads,h)).headNo = h+1;
    	(DYNARR_N(headData_t,xx->signalHeads,h)).headPos = signalHead(h).headPos;
    	(DYNARR_N(headData_t,xx->signalHeads,h)).headType = signalHead(h).headType;
    }
    xx->orig = orig;
    xx->angle = angle;
    DYNARR_APPEND(signalAspect_t,xx->Aspects,signalAspect_da.cnt);
    for (ia = 0; ia < xx->Aspects.cnt; ia++) {
        (DYNARR_N(signalAspect_t,xx->Aspects,ia)).aspectName = signalAspect(ia).aspectName;
        (DYNARR_N(signalAspect_t,xx->Aspects,ia)).aspectScript = signalAspect(ia).aspectScript;
    }
    ComputeSignalBoundingBox(trk);
}

static void MoveSignal (track_p trk, coOrd orig )
{
    signalData_p xx = GetsignalData ( trk );
    xx->orig.x += orig.x;
    xx->orig.y += orig.y;
    ComputeSignalBoundingBox(trk);
}

static void RotateSignal (track_p trk, coOrd orig, ANGLE_T angle ) 
{
    signalData_p xx = GetsignalData ( trk );
    Rotate(&(xx->orig), orig, angle);
    xx->angle = NormalizeAngle(xx->angle + angle);
    ComputeSignalBoundingBox(trk);
}

static void RescaleSignal (track_p trk, FLOAT_T ratio ) 
{
}

static void FlipSignal (track_p trk, coOrd orig, ANGLE_T angle )
{
    signalData_p xx = GetsignalData ( trk );
    FlipPoint(&(xx->orig), orig, angle);
    xx->angle = NormalizeAngle(2*angle - xx->angle);
    ComputeSignalBoundingBox(trk);
}

static trackCmd_t signalCmds = {
    "SIGNAL",
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
    NULL, /* query */
    NULL, /* ungroup */
    FlipSignal, /* flip */
    NULL, /* drawPositionIndicator */
    NULL, /* advancePositionIndicator */
    NULL, /* checkTraverse */
    NULL, /* makeParallel */
    NULL  /* drawDesc */
};

static BOOL_T signalCreate_P;
static coOrd signalEditOrig;
static ANGLE_T signalEditAngle;
static track_p signalEditTrack;

static char signalEditName[STR_SHORT_SIZE];
static long signalEditHeadCount;
static char signalAspectEditName[STR_SHORT_SIZE];
static char signalAspectEditScript[STR_LONG_SIZE];
static long signalAspectEditIndex;

static char headMapHeadName[STR_SHORT_SIZE];
static char headMapIndicatorName[STR_SHORT_SIZE];
static long headMapEditIndex;

static paramIntegerRange_t r1_3 = {1, 3};
static wPos_t aspectListWidths[] = { STR_SHORT_SIZE, 150 };
static const char * aspectListTitles[] = { N_("Name"), N_("Script") };
static paramListData_t aspectListData = {10, 400, 2, aspectListWidths, aspectListTitles};
static wPos_t headMapListWidths[] = { STR_SHORT_SIZE, STR_SHORT_SIZE };
static const char * headMapListTitles[] = { N_("Head Name"), N_("Indication Name") };
static paramListData_t headMapListData = {10, 400, 2, headMapListWidths, headMapListTitles};

static void AspectEdit( void * action );
static void AspectAdd( void * action );
static void AspectDelete( void * action );

static paramFloatRange_t r_1000_1000    = { -1000.0, 1000.0, 80 };
static paramFloatRange_t r0_360         = { 0.0, 360.0, 80 };
static paramData_t signalEditPLs[] = {
#define I_SIGNALNAME (0)
    /*0*/ { PD_STRING, signalEditName, "name", PDO_NOPREF|PDO_STRINGLIMITLENGTH, (void*)200, N_("Name"), 0, 0, sizeof(signalEditName)},
#define I_ORIGX (1)
    /*1*/ { PD_FLOAT, &signalEditOrig.x, "origx", PDO_DIM, &r_1000_1000, N_("Orgin X") }, 
#define I_ORIGY (2)
    /*2*/ { PD_FLOAT, &signalEditOrig.y, "origy", PDO_DIM, &r_1000_1000, N_("Origin Y") },
#define I_ANGLE (3)
    /*3*/ { PD_FLOAT, &signalEditAngle, "origa", PDO_ANGLE, &r0_360, N_("Angle") },
#define I_SIGNALHEADCOUNT (4)
    /*4*/ { PD_LONG,   &signalEditHeadCount, "headCount", PDO_NOPREF, &r1_3, N_("Number of Heads") },
#define I_SIGNALASPECTLIST (5)
#define aspectSelL ((wList_p)signalEditPLs[I_SIGNALASPECTLIST].control)
    /*5*/ { PD_LIST, NULL, "inx", PDO_DLGRESETMARGIN|PDO_DLGRESIZE, &aspectListData, NULL, BL_MANY },
#define I_SIGNALASPECTEDIT (6)
    /*6*/ { PD_BUTTON, (void*)AspectEdit, "edit", PDO_DLGCMDBUTTON, NULL, N_("Edit Aspect") },
#define I_SIGNALASPECTADD (7)
    /*7*/ { PD_BUTTON, (void*)AspectAdd, "add", PDO_DLGCMDBUTTON, NULL, N_("Add Aspect") },
#define I_SIGNALASPECTDELETE (8)
    /*8*/ { PD_BUTTON, (void*)AspectDelete, "delete", 0, NULL, N_("Delete Aspect") },
};
static paramGroup_t signalEditPG = { "signalEdit", 0, signalEditPLs, sizeof signalEditPLs/sizeof signalEditPLs[0] };
static wWin_p signalEditW;

static paramIntegerRange_t rm1_999999 = { -1, 999999 };

static paramData_t aspectEditPLs[] = {
#define I_ASPECTNAME (0)
    /*0*/ { PD_STRING, signalAspectEditName, "name", PDO_NOPREF|PDO_STRINGLIMITLENGTH, (void*)200,  N_("Name"), 0, 0, sizeof(signalAspectEditName)},
#define I_ASPECTSCRIPT (1)
    /*1*/ { PD_STRING, signalAspectEditScript, "script", PDO_NOPREF|PDO_STRINGLIMITLENGTH, (void*)350, N_("Script"), 0, 0, sizeof(signalAspectEditScript)},

#define I_ASPECTINDEX (2)
    /*2*/ { PD_LONG,   &signalAspectEditIndex, "index", PDO_NOPREF, &rm1_999999, N_("Aspect Index"), BO_READONLY },
};

static paramData_t headMapEditPLs[] = {
#define I_HEADNAMES (0)
    /*0*/ { PD_STRING, headMapHeadName, "headname", PDO_NOPREF|PDO_STRINGLIMITLENGTH, (void*)350, N_("Name"), 0, 0, sizeof(headMapEditHeadName)},
#define I_APPEARANCELIST (1)
#define headMapIndicatorSelL ((Wlist_p)headMapEditPls[I_APPEARANCELIST].control)
	/*1*/ {PD_LIST, NULL, "inx", PDO_DLGRESETMARGIN|PDO_DLGRESIZE, &headMapListData, NULL, BL_MANY },

#define I_HEADINDEX (2)
    /*2*/ { PD_LONG,   &headMapEditIndex, "index", PDO_NOPREF, &rm1_999999, N_("Aspect Index"), BO_READONLY },
};

static paramGroup_t aspectEditPG = { "aspectEdit", 0, aspectEditPLs, sizeof aspectEditPLs/sizeof aspectEditPLs[0] };
static wWin_p aspectEditW;


static void SignalEditOk ( void * junk )
{
    track_p trk;
    signalData_p xx;
    wIndex_t ia;
    CSIZE_T newsize;
    
    if (signalCreate_P) {
        UndoStart( _("Create Signal"), "Create Signal");
        trk = NewTrack(0, T_SIGNAL, 0, sizeof(signalData_t)+(sizeof(signalAspect_t)*(signalAspect_da.cnt-1))+1);
        xx = GetsignalData(trk);
    } else {
        UndoStart( _("Modify Signal"), "Modify Signal");
        trk = signalEditTrack;
        xx = GetsignalData(trk);
        if (xx->Aspects.cnt != signalAspect_da.cnt) {
            /* We need to reallocate the extra data. */
            for (ia = 0; ia < xx->Aspects.cnt; ia++) {
                MyFree(DYNARR_N(signalAspect_t,xx->Aspects,ia).aspectName);
                MyFree(DYNARR_N(signalAspect_t,xx->Aspects,ia).aspectScript);
                (DYNARR_N(signalAspect_t,xx->Aspects,ia)).aspectName = NULL;
                (DYNARR_N(signalAspect_t,xx->Aspects,ia)).aspectScript = NULL;
            }
            DYNARR_RESET(signalAspect_t,xx->Aspects);
            DYNARR_APPEND(signalAspect_t,xx->Aspects,signalAspect_da.cnt);
            xx = GetsignalData(trk);
        }
    }
    xx->orig = signalEditOrig;
    xx->angle = signalEditAngle;
    xx->numHeads = signalEditHeadCount;
    if ( xx->name == NULL || strncmp (xx->name, signalEditName, STR_SHORT_SIZE) != 0) {
        MyFree(xx->name);
        xx->name = MyStrdup(signalEditName);
    }
    xx->Aspects.cnt = signalAspect_da.cnt;
    for (ia = 0; ia < xx->Aspects.cnt; ia++) {
        if ((DYNARR_N(signalAspect_t,xx->Aspects,ia)).aspectName == NULL) {
            (DYNARR_N(signalAspect_t,xx->Aspects,ia)).aspectName = signalAspect(ia).aspectName;
        } else if (strcmp((DYNARR_N(signalAspect_t,xx->Aspects,ia)).aspectName,signalAspect(ia).aspectName) != 0) {
            MyFree((DYNARR_N(signalAspect_t,xx->Aspects,ia)).aspectName);
            (DYNARR_N(signalAspect_t,xx->Aspects,ia)).aspectName = signalAspect(ia).aspectName;
        } else {
            MyFree(signalAspect(ia).aspectName);
        }
        if ((DYNARR_N(signalAspect_t,xx->Aspects,ia)).aspectScript == NULL) {
            (DYNARR_N(signalAspect_t,xx->Aspects,ia)).aspectScript = signalAspect(ia).aspectScript;
        } else if (strcmp((DYNARR_N(signalAspect_t,xx->Aspects,ia)).aspectScript,signalAspect(ia).aspectScript) != 0) {
            MyFree((DYNARR_N(signalAspect_t,xx->Aspects,ia)).aspectScript);
            (DYNARR_N(signalAspect_t,xx->Aspects,ia)).aspectScript = signalAspect(ia).aspectScript;
        } else {
            MyFree(signalAspect(ia).aspectScript);
        }
    }
    UndoEnd();
    DoRedraw();
    ComputeSignalBoundingBox(trk);
    wHide( signalEditW );
}

static void SignalEditCancel ( wWin_p junk )
{
    wIndex_t ia;

    for (ia = 0; ia < signalAspect_da.cnt; ia++) {
        MyFree(signalAspect(ia).aspectName);
        MyFree(signalAspect(ia).aspectScript);
    }
    DYNARR_RESET( signalAspect_p, signalAspect_da );
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
        DYNARR_APPEND( signalAspect_p *, signalAspect_da, 10 );
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
 * List of Heads and Selection box of Indications for Each
 */
static void EditMapDialog (wIndex_t inx )
{
	if (inx <0){
		signalMapEditHeadName[0] = '\0';
		signalMapEditIndicationName[0] = '\0';
	} else {
		strncpy(signalMapEditHeadName,((headAspectMap_t)aspectHeadMap(inx)).aspectMapHeadName,STR_SHORT_SIZE);
		strncpy(signalMapEditIndicationName,((headAspectMap_t)aspectHeadMap(inx)).aspectMapHeadIndication,STR_SHORT_SIZE);
	}
}

static void EditAspectDialog ( wIndex_t inx )
{
    if (inx < 0) {
        signalAspectEditName[0] = '\0';
        signalAspectEditScript[0] = '\0';
    } else {
        strncpy(signalAspectEditName,signalAspect(inx).aspectName,STR_SHORT_SIZE);
        strncpy(signalAspectEditScript,signalAspect(inx).aspectScript,STR_LONG_SIZE);
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
	wIndex_t selcnt = wListGetSelectedCount( aspectMapL );
	wIndex_t inx, cnt;

	if (selcnt !=1 ) return;
	cnt = wListGetCount (aspectMapL);
	for ( inx =0;
		  inx<cnt && wListGetItemSelected (aspectMapL, inx) != TRUE;
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
    for(int i=0;i<signalAspect(inx).headMap.cnt;i++) {
    	headAspectMap_t hm = DYNARR_N(headAspectMap_t,signalAspect(inx).headMap,i);
    	MyFree(hm.aspectMapHeadIndication);
    	MyFree(hm.aspectMapHeadName);
    }
    MyFree(signalAspect(inx).headMap.ptr);
    for (ia = inx+1; ia < cnt; ia++) {
        signalAspect(ia-1).aspectName = signalAspect(ia).aspectName;
        signalAspect(ia-1).aspectScript = signalAspect(ia).aspectScript;
        signalAspect(ia-1).headMap.cnt = signalAspect(ia).headMap.cnt;
        signalAspect(ia-1).headMap.max = signalAspect(ia).headMap.max;
        signalAspect(ia-1).headMap.ptr = signalAspect(ia).headMap.ptr;
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
        signalEditHeadCount = 1;
        wListClear( aspectSelL );
        DYNARR_RESET( signalAspect_p, signalAspect_da );
    } else {
        xx = GetsignalData ( signalEditTrack );
        strncpy(signalEditName,xx->signalName,STR_SHORT_SIZE);
        signalEditHeadCount = xx->signalHeads.cnt;
        signalEditOrig = xx->orig;
        signalEditAngle = xx->angle;
        wListClear( aspectSelL );
        DYNARR_RESET( signalAspect_p, signalAspect_da );
        for (ia = 0; ia < xx->Aspects.cnt; ia++) {
            snprintf(message,sizeof(message),"%s\t%s",(DYNARR_N(signalAspect_t,xx->Aspects,ia)).aspectName,
                    (DYNARR_N(signalAspect_t,xx->Aspects,ia)).aspectScript);
            wListAddValue( aspectSelL, message, NULL, NULL );
            DYNARR_APPEND( signalAspect_p *, signalAspect_da, 10 );
            signalAspect(signalAspect_da.cnt-1).aspectName = MyStrdup((DYNARR_N(signalAspect_t,xx->Aspects,ia)).aspectName);
            signalAspect(signalAspect_da.cnt-1).aspectScript = MyStrdup((DYNARR_N(signalAspect_t,xx->Aspects,ia)).aspectScript);
        }
    }
    ParamLoadControls( &signalEditPG );
    ParamControlActive( &signalEditPG, I_SIGNALASPECTEDIT, FALSE );
    ParamControlActive( &signalEditPG, I_SIGNALASPECTADD, TRUE );
    ParamControlActive( &signalEditPG, I_SIGNALASPECTDELETE, FALSE );
    wShow( signalEditW );
}




static void EditSignal (track_p trk)
{
    signalCreate_P = FALSE;
    signalEditTrack = trk;
    EditSignalDialog();
}

static void CreateNewSignal (coOrd orig, ANGLE_T angle)
{
    signalCreate_P = TRUE;
    signalEditOrig = orig;
    signalEditAngle = angle;
    EditSignalDialog();
}

static coOrd pos0;
static ANGLE_T orient;

static STATUS_T CmdSignal ( wAction_t action, coOrd pos )
{
    
    
    switch (action) {
    case C_START:
        InfoMessage(_("Place base of signal"));
        return C_CONTINUE;
    case C_DOWN:
        SnapPos(&pos);
        pos0 = pos;
        InfoMessage(_("Drag to orient signal"));
        return C_CONTINUE;
    case C_MOVE:
        SnapPos(&pos);
        orient = FindAngle(pos0,pos);
        DDrawSignal( &tempD, pos0, orient, 1, GetScaleRatio(GetLayoutCurScale()), wDrawColorBlack );
        return C_CONTINUE;
    case C_UP:
        SnapPos(&pos);
        orient = FindAngle(pos0,pos);
        CreateNewSignal(pos0,orient);
        return C_TERMINATE;
    case C_REDRAW:
    case C_CANCEL:
        DDrawSignal( &tempD, pos0, orient, 1, GetScaleRatio(GetLayoutCurScale()), wDrawColorBlack );
        return C_CONTINUE;
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
	wDrawFilledRectangle( mainD.d, x, y, w, h, sighiliteColor, wDrawOptTemp );
}

static int SignalMgmProc ( int cmd, void * data )
{
    track_p trk = (track_p) data;
    signalData_p xx = GetsignalData(trk);
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

#define ACCL_SIGNAL 0

EXPORT void InitCmdSignal ( wMenu_p menu )
{
    AddMenuButton( menu, CmdSignal, "cmdSignal", _("Signal"), 
                   wIconCreatePixMap( signal_xpm ), LEVEL0_50, IC_STICKY|IC_POPUP2, ACCL_SIGNAL, NULL );
}

EXPORT void InitTrkSignal ( void )
{
    log_signal = LogFindIndex ( "signal" );
    AddParam( "SIGNALHEAD", ReadHeadType );
}
