/** \file cstruct.c
 * T_STRUCTURE
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

#include <ctype.h>
#include <math.h>
#include <stdint.h>
#include <string.h>

#include "compound.h"
#include "cundo.h"
#include "custom.h"
#include "fileio.h"
#include "i18n.h"
#include "layout.h"
#include "messages.h"
#include "param.h"
#include "include/paramfile.h"
#include "track.h"
#include "utility.h"
#include "ccurve.h"

EXPORT TRKTYP_T T_STRUCTURE = -1;

EXPORT dynArr_t structureInfo_da;

typedef struct compoundData extraData;


static wIndex_t pierListInx;
EXPORT turnoutInfo_t * curStructure = NULL;
static int log_structure = 0;

static wMenu_p structPopupM;

static drawCmd_t structureD = {
		NULL,
		&screenDrawFuncs,
		0,
		1.0,
		0.0,
		{0.0,0.0}, {0.0,0.0},
		Pix2CoOrd, CoOrd2Pix };

static wIndex_t structureHotBarCmdInx;
static wIndex_t structureInx;
static long hideStructureWindow;
static void RedrawStructure(void);

static wPos_t structureListWidths[] = { 80, 80, 220 };
static const char * structureListTitles[] = { N_("Manufacturer"), N_("Part No"), N_("Description") };
static paramListData_t listData = { 13, 400, 3, structureListWidths, structureListTitles };
static const char * hideLabels[] = { N_("Hide"), NULL };
static paramDrawData_t structureDrawData = { 490, 200, (wDrawRedrawCallBack_p)RedrawStructure, NULL, &structureD };
static paramData_t structurePLs[] = {
#define I_LIST	(0)
#define structureListL	((wList_p)structurePLs[I_LIST].control)
	{	PD_LIST, &structureInx, "list", PDO_NOPREF|PDO_DLGRESIZEW, &listData, NULL, BL_DUP },
#define I_DRAW	(1)
	{	PD_DRAW, NULL, "canvas", PDO_NOPSHUPD|PDO_DLGUNDERCMDBUTT|PDO_DLGRESIZE, &structureDrawData, NULL, 0 },
#define I_HIDE	(2)
	{	PD_TOGGLE, &hideStructureWindow, "hide", PDO_DLGCMDBUTTON, /*CAST_AWAY_CONST*/(void*)hideLabels, NULL, BC_NOBORDER },
#define I_MSGSCALE		(3)
	{	PD_MESSAGE, NULL, NULL, 0, (void*)80 },
#define I_MSGWIDTH		(4)
	{	PD_MESSAGE, NULL, NULL, 0, (void*)80 },
#define I_MSGHEIGHT		(5)
	{	PD_MESSAGE, NULL, NULL, 0, (void*)80 } };
static paramGroup_t structurePG = { "structure", 0, structurePLs, sizeof structurePLs/sizeof structurePLs[0] };



/****************************************
 *
 * STRUCTURE LIST MANAGEMENT
 *
 */




EXPORT turnoutInfo_t * CreateNewStructure(
		char * scale,
		char * title,
		wIndex_t segCnt,
		trkSeg_p segData,
		BOOL_T updateList )
{
	turnoutInfo_t * to;
#ifdef REORIGSTRUCT
	coOrd orig;
#endif

	if (segCnt == 0)
		return NULL; 
	to = FindCompound( FIND_STRUCT, scale, title );
	if (to == NULL) {
		DYNARR_APPEND( turnoutInfo_t *, structureInfo_da, 10 );
		to = (turnoutInfo_t*)MyMalloc( sizeof *to );
		structureInfo(structureInfo_da.cnt-1) = to;
		to->title = MyStrdup( title );
		to->scaleInx = LookupScale( scale );
	}
	to->segCnt = segCnt;
	to->segs = (trkSeg_p)memdup( segData, (sizeof *segData) * segCnt );
	CopyPoly(to->segs,segCnt);
	GetSegBounds( zero, 0.0, to->segCnt, to->segs, &to->orig, &to->size );
#ifdef REORIGSTRUCT
	GetSegBounds( zero, 0.0, to->segCnt, to->segs, &orig, &to->size );
	orig.x = - orig.x;
	orig.y = - orig.y;
	MoveSegs( to->segCnt, to->segs, orig );
	to->orig = zero;
#endif
	to->paramFileIndex = curParamFileIndex;
	if (curParamFileIndex == PARAM_CUSTOM)
		to->contentsLabel = MyStrdup("Custom Structures");
	else
		to->contentsLabel = curSubContents;
	to->endCnt = 0;
	to->pathLen = 0;
	to->paths = (PATHPTR_T)"";
	if (updateList && structureListL != NULL) {
		FormatCompoundTitle( LABEL_TABBED|LABEL_MANUF|LABEL_PARTNO|LABEL_DESCR, to->title );
		if (message[0] != '\0')
			wListAddValue( structureListL, message, NULL, to );
	}

	to->barScale = curBarScale>0?curBarScale:-1;
	return to;
}

/**
 * Delete a structure definition from memory. 
 * \TODO Find a better way to handle Custom Structures (see CreateNewStructure)
 *
 * \param [IN] structure the structure to be deleted
 */

BOOL_T
StructureDelete(void *structure)
{
	turnoutInfo_t * to = (turnoutInfo_t *)structure;
	MyFree(to->title);
	MyFree(to->segs);

	if (to->special) {
		switch(to->special) {
		case TOpier:
			MyFree(to->u.pier.name);
			to->u.pier.name = NULL;
			break;
		case TOpierInfo:
			for(int pierInx=0;pierInx<to->u.pierInfo.cnt;pierInx++) {
				if (to->u.pierInfo.info[pierInx].name)
					MyFree(to->u.pierInfo.info[pierInx].name);
				to->u.pierInfo.info[pierInx].name = NULL;
			}
			MyFree(to->u.pierInfo.info);
			to->u.pierInfo.cnt = 0;
			break;
		default:;
		}
	}

	MyFree(to);
	return(TRUE);
}

/**
 * Delete all structure definitions that came from a specific parameter
 * file. Due to the way the definitions are loaded from file it is safe to
 * assume that they form a contiguous block in the array.
 *
 * \param  fileIndex parameter file.
 */

void
DeleteStructures(int fileIndex)
{
    int inx = 0;
    int startInx = -1;
    int cnt = 0;

    // go to the start of the block
    while (inx < structureInfo_da.cnt &&
            structureInfo(inx)->paramFileIndex != fileIndex) {
        startInx = inx++;
    }

    // delete them
    for (; inx < structureInfo_da.cnt &&
            structureInfo(inx)->paramFileIndex == fileIndex; inx++) {
        turnoutInfo_t * to = structureInfo(inx);
        if (to->paramFileIndex == fileIndex) {
            StructureDelete(to);
            cnt++;
        }
    }

    // copy down the rest of the list to fill the gap
    startInx++;
    while (inx < structureInfo_da.cnt) {
        structureInfo(startInx++) = structureInfo(inx++);
    }

    // and reduce the actual number
    structureInfo_da.cnt -= cnt;
}

enum paramFileState
GetStructureCompatibility(int paramFileIndex, SCALEINX_T scaleIndex)
{
	int i;
	enum paramFileState ret = PARAMFILE_NOTUSABLE;
	DIST_T ratio = GetScaleRatio(scaleIndex);

	if (!IsParamValid(paramFileIndex)) {
		return(PARAMFILE_UNLOADED);
	}

	for (i = 0; i < structureInfo_da.cnt; i++) {
		turnoutInfo_t *to = structureInfo(i);
		if (to->paramFileIndex == paramFileIndex) {
			if (GetScaleRatio(to->scaleInx) == ratio || to->scaleInx == SCALE_ANY) {
				ret = PARAMFILE_FIT;
				break;
			} 
		}
	}
	return(ret);
}

static BOOL_T ReadStructureParam(
		char * firstLine )
{
	char scale[10];
	char *title;
	turnoutInfo_t * to;
	char * cp;
static dynArr_t pierInfo_da;
#define pierInfo(N) DYNARR_N( pierInfo_t, pierInfo_da, N )

	if ( !GetArgs( firstLine+10, "sq", scale, &title ) )
		return FALSE;
	if ( !ReadSegs() )
		return FALSE; 
	to = CreateNewStructure( scale, title, tempSegs_da.cnt, &tempSegs(0), FALSE );
	if (to == NULL)
		return FALSE;
	if (tempSpecial[0] != '\0') {
		if (strncmp( tempSpecial, PIER, strlen(PIER) ) == 0) {
			DYNARR_RESET( pierInfo_t, pierInfo_da );
			to->special = TOpierInfo;
			cp = tempSpecial+strlen(PIER);
			while (cp) {
				DYNARR_APPEND( pierInfo_t, pierInfo_da, 10 );
				if ( !GetArgs( cp, "fqc", &pierInfo(pierInfo_da.cnt-1).height, &pierInfo(pierInfo_da.cnt-1).name, &cp ) )
					return FALSE;
			}
			to->u.pierInfo.cnt = pierInfo_da.cnt;
			to->u.pierInfo.info = (pierInfo_t*)MyMalloc( pierInfo_da.cnt * sizeof *(pierInfo_t*)NULL );
			memcpy( to->u.pierInfo.info, &pierInfo(0), pierInfo_da.cnt * sizeof *(pierInfo_t*)NULL );
		} else {
				InputError("Unknown special case", TRUE);
		}
	}
	if (tempCustom[0] != '\0') {
		to->customInfo = MyStrdup( tempCustom );
	}
	MyFree( title );
	return TRUE;
}


EXPORT turnoutInfo_t * StructAdd( long mode, SCALEINX_T scale, wList_p list, coOrd * maxDim )
{
	wIndex_t inx;
	turnoutInfo_t * to, *to1=NULL;
	structureInx = 0;
	for ( inx = 0; inx < structureInfo_da.cnt; inx++ ) {
		to = structureInfo(inx);
		if ( IsParamValid(to->paramFileIndex) &&
			 to->segCnt > 0 &&
			 CompatibleScale( FALSE, to->scaleInx, scale ) &&
			 to->segCnt != 0 ) {
			if (to1 == NULL)
				to1 = to;
			if ( to == curStructure ) {
				to1 = to;
				structureInx = wListGetCount( list );
			}
			FormatCompoundTitle( mode, to->title );
			if (message[0] != '\0') {
				wListAddValue( list, message, NULL, to );
				if (maxDim) {
					if (to->size.x > maxDim->x) 
						maxDim->x = to->size.x;
					if (to->size.y > maxDim->y) 
					  maxDim->y = to->size.y;
				}
			}
		}
	}
	return to1;
}


/****************************************
 *
 * GENERIC FUNCTIONS
 *
 */

static void DrawStructure(
		track_p t,
		drawCmd_p d,
		wDrawColor color )
{
	struct extraData *xx = GetTrkExtraData(t);

	d->options &= ~DC_NOTSOLIDLINE;
	switch(xx->lineType) {
	case DRAWLINESOLID:
		break;
	case DRAWLINEDASH:
		d->options |= DC_DASH;
		break;
	case DRAWLINEDOT:
		d->options |= DC_DOT;
		break;
	case DRAWLINEDASHDOT:
		d->options |= DC_DASHDOT;
		break;
	case DRAWLINEDASHDOTDOT:
		d->options |= DC_DASHDOTDOT;
		break;
	case DRAWLINECENTER:
		d->options |= DC_CENTER;
		break;
	case DRAWLINEPHANTOM:
		d->options |= DC_CENTER;
		break;
	}
	DrawSegs( d, xx->orig, xx->angle, xx->segs, xx->segCnt, 0.0, color );
	d->options &= ~DC_NOTSOLIDLINE;
	if ( ((d->options & DC_SIMPLE)==0) &&
		 (labelWhen == 2 || (labelWhen == 1 && (d->options&DC_PRINT))) &&
		 labelScale >= d->scale &&
		 ( GetTrkBits( t ) & TB_HIDEDESC ) == 0 ) {
		DrawCompoundDescription( t, d, color );
	}
}


static BOOL_T ReadStructure(
		char * line )
{
	return ReadCompound( line+10, T_STRUCTURE );
}


static ANGLE_T GetAngleStruct(
		track_p trk,
		coOrd pos,
		EPINX_T * ep0,
		EPINX_T * ep1 )
{
	struct extraData * xx = GetTrkExtraData(trk);
	ANGLE_T angle;

	pos.x -= xx->orig.x;
	pos.y -= xx->orig.y;
	Rotate( &pos, zero, -xx->angle );
	angle = GetAngleSegs( xx->segCnt, xx->segs, &pos, NULL, NULL, NULL, NULL, NULL);
	if ( ep0 ) *ep0 = -1;
	if ( ep1 ) *ep1 = -1;
	return NormalizeAngle( angle+xx->angle );
}


static BOOL_T QueryStructure( track_p trk, int query )
{
	switch ( query ) {
	case Q_HAS_DESC:
		return TRUE;
	case Q_IS_STRUCTURE:
		return TRUE;
	default:
		return FALSE;
	}
}


static wBool_t CompareStruct( track_cp trk1, track_cp trk2 )
{
	struct extraData *xx1 = GetTrkExtraData( trk1 );
	struct extraData *xx2 = GetTrkExtraData( trk2 );
	char * cp = message + strlen(message);
	REGRESS_CHECK_POS( "Orig", xx1, xx2, orig )
	REGRESS_CHECK_ANGLE( "Angle", xx1, xx2, angle )
	REGRESS_CHECK_INT( "Flipped", xx1, xx2, flipped )
	REGRESS_CHECK_INT( "Ungrouped", xx1, xx2, ungrouped )
	REGRESS_CHECK_INT( "Split", xx1, xx2, split )
	/* desc orig is not stable
	REGRESS_CHECK_POS( "DescOrig", xx1, xx2, descriptionOrig ) */
	REGRESS_CHECK_POS( "DescOff", xx1, xx2, descriptionOff )
	REGRESS_CHECK_POS( "DescSize", xx1, xx2, descriptionSize )
	return CompareSegs( xx1->segs, xx1->segCnt, xx1->segs, xx1->segCnt );
}

static trackCmd_t structureCmds = {
		"STRUCTURE",
		DrawStructure,
		DistanceCompound,
		DescribeCompound,
		DeleteCompound,
		WriteCompound,
		ReadStructure,
		MoveCompound,
		RotateCompound,
		RescaleCompound,
		NULL,
		GetAngleStruct,
		NULL,	/* split */
		NULL,	/* traverse */
		EnumerateCompound,
		NULL,	/* redraw */
		NULL,	/* trim */
		NULL,	/* merge */
		NULL,	/* modify */
		NULL,	/* getLength */
		NULL,	/* getTrkParams */
		NULL,	/* moveEndPt */
		QueryStructure,
		UngroupCompound,
		FlipCompound,
		NULL,
		NULL,
		NULL,
		NULL,
		NULL,
		NULL,
		NULL,
		NULL,
		NULL,
		CompareStruct };

static paramData_t pierPLs[] = {
	{	PD_DROPLIST, &pierListInx, "inx", 0, (void*)50, N_("Pier Number") } };
static paramGroup_t pierPG = { "structure-pier", 0, pierPLs, sizeof pierPLs/sizeof pierPLs[0] };
#define pierL ((wList_p)pierPLs[0].control)

static void ShowPierL( void )
{
	int inx;
	wIndex_t currInx;
	wControl_p controls[2];
	char * labels[1];

	if ( curStructure->special==TOpierInfo && curStructure->u.pierInfo.cnt > 1) {
		if (pierL == NULL) {
			ParamCreateControls( &pierPG, NULL );
		}
		currInx = wListGetIndex( pierL );
		wListClear( pierL );
		for (inx=0;inx<curStructure->u.pierInfo.cnt; inx++) {
			wListAddValue( pierL, curStructure->u.pierInfo.info[inx].name, NULL, NULL );
		}
		if ( currInx < 0 )
		   currInx = 0;
		if ( currInx >= curStructure->u.pierInfo.cnt )
		   currInx = curStructure->u.pierInfo.cnt-1;
		wListSetIndex( pierL, currInx );
		controls[0] = (wControl_p)pierL;
		controls[1] = NULL;
		labels[0] = N_("Pier Number");
		InfoSubstituteControls( controls, labels );
	} else {
		InfoSubstituteControls( NULL, NULL );
	}
}


/*****************************************
 *
 *	 Structure Dialog
 *
 */

static void NewStructure();
static coOrd maxStructureDim;
static wWin_p structureW;


static void RescaleStructure( void )
{
	DIST_T xscale, yscale;
	wPos_t ww, hh;
	DIST_T w, h;
	wDrawGetSize( structureD.d, &ww, &hh );
	w = ww/structureD.dpi - 0.2;
	h = hh/structureD.dpi - 0.2;
	if (curStructure) {
		xscale = curStructure->size.x/w;
		yscale = curStructure->size.y/h;
	} else {
		xscale = yscale = 0;
	}
	structureD.scale = ceil(max(xscale,yscale));
	structureD.size.x = (w+0.2)*structureD.scale;
	structureD.size.y = (h+0.2)*structureD.scale;
	return;
}


static void structureChange( long changes )
{
	static char * lastScaleName = NULL;
	if (structureW == NULL)
		return;
	wListSetIndex( structureListL, 0 );
	if ( (!wWinIsVisible(structureW)) ||
	   ( ((changes&CHANGE_SCALE) == 0  || lastScaleName == curScaleName) &&
		  (changes&CHANGE_PARAMS) == 0 ) )
		return;
	lastScaleName = curScaleName;
	//curStructure = NULL;
	wControlShow( (wControl_p)structureListL, FALSE );
	wListClear( structureListL );
	maxStructureDim.x = maxStructureDim.y = 0.0;
	if (structureInfo_da.cnt <= 0)
		return;
	curStructure = StructAdd( LABEL_TABBED|LABEL_MANUF|LABEL_PARTNO|LABEL_DESCR, GetLayoutCurScale(), structureListL, &maxStructureDim );
	wListSetIndex( structureListL, structureInx );
	wControlShow( (wControl_p)structureListL, TRUE );
	if (curStructure == NULL) {
		wDrawClear( structureD.d );
		return;
	}
	maxStructureDim.x += 2*trackGauge;
	maxStructureDim.y += 2*trackGauge;
	/*RescaleStructure();*/
	RedrawStructure();
	return;
}



static void RedrawStructure()
{
	RescaleStructure();
LOG( log_structure, 2, ( "SelStructure(%s)\n", (curStructure?curStructure->title:"<NULL>") ) )
	wDrawClear( structureD.d );
	if (curStructure == NULL) {
		return;
	}
	structureD.orig.x = -0.10*structureD.scale + curStructure->orig.x;
	structureD.orig.y = (curStructure->size.y + curStructure->orig.y) - structureD.size.y + trackGauge;
	DrawSegs( &structureD, zero, 0.0, curStructure->segs, curStructure->segCnt,
					 0.0, wDrawColorBlack );
	sprintf( message, _("Scale %d:1"), (int)structureD.scale );
	ParamLoadMessage( &structurePG, I_MSGSCALE, message );
	sprintf( message, _("Width %s"), FormatDistance(curStructure->size.x) );
	ParamLoadMessage( &structurePG, I_MSGWIDTH, message );
	sprintf( message, _("Height %s"), FormatDistance(curStructure->size.y) );
	ParamLoadMessage( &structurePG, I_MSGHEIGHT, message );
}


static void StructureDlgUpdate(
		paramGroup_p pg,
		int inx,
		void * valueP )
{
	turnoutInfo_t * to;
	if ( inx != I_LIST ) return;
	to = (turnoutInfo_t*)wListGetItemContext( (wList_p)pg->paramPtr[inx].control, (wIndex_t)*(long*)valueP );
	NewStructure();
	curStructure = to;
	ShowPierL();
	RedrawStructure();
	ParamDialogOkActive( &structurePG, FALSE );
}


static void DoStructOk( void )
{
	NewStructure();
	Reset();
}



/****************************************
 *
 * GRAPHICS COMMANDS
 *
 */

/*
 * STATE INFO
 */
static struct {
		int state;
		coOrd pos;
		ANGLE_T angle;
		} Dst;

static track_p pierTrk;
static EPINX_T pierEp;

static dynArr_t anchors_da;
#define anchors(N) DYNARR_N(trkSeg_t,anchors_da,N)

void static CreateArrowAnchor(coOrd pos,ANGLE_T a,DIST_T len) {
	DYNARR_APPEND(trkSeg_t,anchors_da,1);
	int i = anchors_da.cnt-1;
	anchors(i).type = SEG_STRLIN;
	anchors(i).width = 0;
	anchors(i).u.l.pos[0] = pos;
	Translate(&anchors(i).u.l.pos[1],pos,NormalizeAngle(a+135),len);
	anchors(i).color = wDrawColorBlue;
	DYNARR_APPEND(trkSeg_t,anchors_da,1);
	i = anchors_da.cnt-1;
	anchors(i).type = SEG_STRLIN;
	anchors(i).width = 0;
	anchors(i).u.l.pos[0] = pos;
	Translate(&anchors(i).u.l.pos[1],pos,NormalizeAngle(a-135),len);
	anchors(i).color = wDrawColorBlue;
}

void static CreateRotateAnchor(coOrd pos) {
	DIST_T d = tempD.scale*0.15;
	DYNARR_APPEND(trkSeg_t,anchors_da,1);
	int i = anchors_da.cnt-1;
	anchors(i).type = SEG_CRVLIN;
	anchors(i).width = 0.5;
	anchors(i).u.c.center = pos;
	anchors(i).u.c.a0 = 180.0;
	anchors(i).u.c.a1 = 360.0;
	anchors(i).u.c.radius = d*2;
	anchors(i).color = wDrawColorAqua;
	coOrd head;					//Arrows
	for (int j=0;j<3;j++) {
		Translate(&head,pos,j*120,d*2);
		CreateArrowAnchor(head,NormalizeAngle((j*120)+90),d);
	}
}

void static CreateMoveAnchor(coOrd pos) {
	DYNARR_SET(trkSeg_t,anchors_da,anchors_da.cnt+5);
	DrawArrowHeads(&DYNARR_N(trkSeg_t,anchors_da,anchors_da.cnt-5),pos,0,TRUE,wDrawColorBlue);
	DYNARR_SET(trkSeg_t,anchors_da,anchors_da.cnt+5);
	DrawArrowHeads(&DYNARR_N(trkSeg_t,anchors_da,anchors_da.cnt-5),pos,90,TRUE,wDrawColorBlue);
}

static ANGLE_T PlaceStructure(
		coOrd p0,
		coOrd p1,
		coOrd origPos,
		coOrd * resPos,
		ANGLE_T * resAngle )
{
	coOrd p2 = p1;
	if (curStructure->special == TOpierInfo) {
		pierTrk = OnTrack( &p1, FALSE, TRUE );
		if (pierTrk != NULL) {
			if (GetTrkType(pierTrk) == T_TURNOUT) {
				pierEp = PickEndPoint( p1, pierTrk );
				if (pierEp >= 0) {
					*resPos = GetTrkEndPos(pierTrk, pierEp);
					*resAngle = NormalizeAngle(GetTrkEndAngle(pierTrk, pierEp)-90.0);
					return TRUE;
				}
			}
			*resAngle = NormalizeAngle(GetAngleAtPoint( pierTrk, p1, NULL, NULL )+90.0);
			if ( NormalizeAngle( FindAngle( p1, p2 ) - *resAngle + 90.0 ) > 180.0 )
				*resAngle = NormalizeAngle( *resAngle + 180.0 );
			*resPos = p1;
			return TRUE;
		}
	}
	resPos->x = origPos.x + p1.x - p0.x;
	resPos->y = origPos.y + p1.y - p0.y;
	return FALSE;
}


static void NewStructure( void )
{
	track_p trk;
	struct extraData *xx;
	wIndex_t titleLen;
	wIndex_t pierInx;

	if (curStructure->segCnt < 1) {
		AbortProg( "newStructure: bad cnt" );
	}
	if (Dst.state == 0)
		return;
	if (curStructure->special == TOpierInfo &&
		curStructure->u.pierInfo.cnt>1 &&
		wListGetIndex(pierL) == -1) {
		return;
	}
	UndoStart( _("Place Structure"), "newStruct" );
	titleLen = strlen( curStructure->title );
	trk = NewCompound( T_STRUCTURE, 0, Dst.pos, Dst.angle, curStructure->title, 0, NULL, NULL, 0, "", curStructure->segCnt, curStructure->segs );
	xx = GetTrkExtraData(trk);
#ifdef LATER
	trk = NewTrack( 0, T_STRUCTURE, 0, sizeof (*xx) + 1 );
	xx->orig = Dst.pos;
	xx->angle = Dst.angle;
	xx->segs = MyMalloc( (curStructure->segCnt)*sizeof curStructure->segs[0] );

	/*
	 * copy data */
	xx->segCnt = curStructure->segCnt;
	memcpy( xx->segs, curStructure->segs, xx->segCnt * sizeof *(trkSeg_p)0 );
	xx->title = curStructure->title;
	xx->pathLen = 0;
	xx->paths = "";
#endif
	switch(curStructure->special) {
		case TOnormal:
			xx->special = TOnormal;
			break;
		case TOpierInfo:
			xx->special = TOpier;
			if (curStructure->u.pierInfo.cnt>1) {
				pierInx = wListGetIndex(pierL);
				if (pierInx < 0 || pierInx >= curStructure->u.pierInfo.cnt)
					pierInx = 0;
			} else {
				pierInx = 0;
			}
			xx->u.pier.height = curStructure->u.pierInfo.info[pierInx].height;
			xx->u.pier.name = curStructure->u.pierInfo.info[pierInx].name;
			if (pierTrk != NULL && xx->u.pier.height >= 0 ) {
				UpdateTrkEndElev( pierTrk, pierEp, ELEV_DEF, xx->u.pier.height, NULL );
			}
			break;
		default:
			AbortProg("bad special");
	}
		
	SetTrkVisible( trk, TRUE );
	SetTrkNoTies( trk, FALSE);
	SetTrkBridge( trk, FALSE);
#ifdef LATER
	ComputeCompoundBoundingBox( trk );

	SetDescriptionOrig( trk );
	xx->descriptionOff = zero;
	xx->descriptionSize = zero;
#endif

	DrawNewTrack( trk );
	/*DrawStructure( trk, &mainD, wDrawColorBlack, 0 );*/

	UndoEnd();
	Dst.state = 0;
	Dst.angle = 0.0;
}


static void StructRotate( void * pangle )
{
	if (Dst.state == 0)
		return;
	ANGLE_T angle = (ANGLE_T)(long)pangle;
	angle /= 1000.0;
	Dst.pos = cmdMenuPos;
	Rotate( &Dst.pos, cmdMenuPos, angle );
	Dst.angle += angle;
	TempRedraw(); // StructRotate
}


EXPORT STATUS_T CmdStructureAction(
		wAction_t action,
		coOrd pos )
{

	ANGLE_T angle;
	static BOOL_T validAngle;
	static ANGLE_T baseAngle;
	static coOrd origPos;
	static ANGLE_T origAngle;
	static coOrd rot0, rot1;

	switch (action & 0xFF) {

	case C_START:
		DYNARR_RESET(trkSeg_t,anchors_da);
		Dst.state = 0;
		Dst.angle = 00.0;
		ShowPierL();
		InfoMessage(_("Left-Drag to place, Ctrl+Left-Drag or Right-Drag to Rotate, Space or Enter to accept, Esc to Cancel"));
		return C_CONTINUE;

	case wActionMove:
		DYNARR_RESET(trkSeg_t,anchors_da);
		if (Dst.state && (MyGetKeyState()&WKEY_CTRL)) {
			CreateRotateAnchor(pos);
		} else {
			CreateMoveAnchor(pos);
		}
		return C_CONTINUE;
		break;

	case C_DOWN:
		DYNARR_RESET(trkSeg_t,anchors_da);
		if ( curStructure == NULL ) return C_CONTINUE;
		ShowPierL();
		Dst.pos = pos;
		rot0 = pos;
		origPos = Dst.pos;
		PlaceStructure( rot0, pos, origPos, &Dst.pos, &Dst.angle );
		Dst.state = 1;
		InfoMessage( _("Drag to place") );
		CreateMoveAnchor(pos);
		return C_CONTINUE;

	case C_MOVE:
		DYNARR_RESET(trkSeg_t,anchors_da);
		if ( curStructure == NULL ) return C_CONTINUE;
		PlaceStructure( rot0, pos, origPos, &Dst.pos, &Dst.angle );
		CreateMoveAnchor(pos);
		InfoMessage( "[ %0.3f %0.3f ]", pos.x - origPos.x, pos.y - origPos.y );
		return C_CONTINUE;

	case C_RDOWN:
		DYNARR_RESET(trkSeg_t,anchors_da);
		if ( curStructure == NULL ) return C_CONTINUE;
		if (Dst.state == 0) {
			Dst.pos = pos;		// If first, use pos, otherwise use current
		}
		rot0 = rot1 = pos;
		CreateRotateAnchor(pos);
		origPos = Dst.pos;
		origAngle = Dst.angle;
		InfoMessage( _("Drag to rotate") );
		Dst.state = 2;
		validAngle = FALSE;
		return C_CONTINUE;

	case C_RMOVE:
		DYNARR_RESET(trkSeg_t,anchors_da);
		if ( curStructure == NULL ) return C_CONTINUE;
		rot1 = pos;
		if ( FindDistance( rot0, rot1 ) > (6.0/75.0)*mainD.scale ) {
			angle = FindAngle( rot0, rot1 );
			if (!validAngle) {
				baseAngle = angle;
				validAngle = TRUE;
			}
			angle -= baseAngle;
			Dst.pos = origPos;
			Dst.angle = NormalizeAngle( origAngle + angle );
			Rotate( &Dst.pos, rot0, angle );
		}
		InfoMessage( _("Angle = %0.3f"), Dst.angle );
		Dst.state = 2;
		CreateRotateAnchor(rot0);
		return C_CONTINUE;
		
	case C_RUP:
	case C_UP:
		DYNARR_RESET(trkSeg_t,anchors_da);
		CreateMoveAnchor(pos);
		Dst.state = 1;
		InfoMessage(_("Left-Drag to place, Ctrl+Left-Drag or Right-Drag to Rotate, Space or Enter to accept, Esc to Cancel"));
		return C_CONTINUE;

	case C_CMDMENU:
		DYNARR_RESET(trkSeg_t,anchors_da);
		menuPos = pos;
		wMenuPopupShow( structPopupM );
		return C_CONTINUE;

	case C_REDRAW:
		if (Dst.state)
			DrawSegs( &tempD, Dst.pos, Dst.angle,
				curStructure->segs, curStructure->segCnt, 0.0, wDrawColorBlue );
		if (anchors_da.cnt>0) {
				DrawSegs( &tempD, zero, 0.0, &anchors(0), anchors_da.cnt, trackGauge, wDrawColorBlack );
			}
		if (Dst.state == 2)
			DrawLine( &tempD, rot0, rot1, 0, wDrawColorBlack );
		return C_CONTINUE;

	case C_CANCEL:
		DYNARR_RESET(trkSeg_t,anchors_da);
		Dst.state = 0;
		InfoSubstituteControls( NULL, NULL );
		HotBarCancel();
		/*wHide( newTurn.reg.win );*/
		return C_TERMINATE;

	case C_TEXT:
		if ((action>>8) != ' ')
			return C_CONTINUE;
		/*no break*/
	case C_OK:
		DYNARR_RESET(trkSeg_t,anchors_da);
		NewStructure();
		InfoSubstituteControls( NULL, NULL );
		return C_TERMINATE;

	case C_FINISH:
		DYNARR_RESET(trkSeg_t,anchors_da);
		if (Dst.state != 0)
			CmdStructureAction( C_OK, pos );
		else
			CmdStructureAction( C_CANCEL, pos );
		return C_TERMINATE;

	default:
		return C_CONTINUE;
	}
}


static STATUS_T CmdStructure(
		wAction_t action,
		coOrd pos )
{

	wIndex_t structureIndex;
	turnoutInfo_t * structurePtr;

	switch (action & 0xFF) {

	case C_START:
		if (structureW == NULL) {
			structureW = ParamCreateDialog( &structurePG, MakeWindowTitle(_("Structure")), _("Ok"), (paramActionOkProc)DoStructOk, (paramActionCancelProc)Reset, TRUE, NULL, F_RESIZE, StructureDlgUpdate );
			RegisterChangeNotification( structureChange );
		}
		ParamDialogOkActive( &structurePG, FALSE );
		structureIndex = wListGetIndex( structureListL );
		structurePtr = curStructure;
		wShow( structureW );
		structureChange( CHANGE_PARAMS );
		if (curStructure == NULL) {
			NoticeMessage( MSG_STRUCT_NO_STRUCTS, _("Ok"), NULL );
			return C_TERMINATE;
		}
		if (structureIndex > 0 && structurePtr) {
			curStructure = structurePtr;
			wListSetIndex( structureListL, structureIndex );
			RedrawStructure();
		}
		InfoMessage( _("Select Structure and then drag to place"));
		ParamLoadControls( &structurePG );
		ParamGroupRecord( &structurePG );
		return CmdStructureAction( action, pos );

	case wActionMove:
		DYNARR_RESET(trkSeg_t,anchors_da);
		if (Dst.state && (MyGetKeyState()&WKEY_CTRL)) {
			CreateRotateAnchor(pos);
		} else {
			CreateMoveAnchor(pos);
		}
		return C_CONTINUE;
		break;
	case C_DOWN:
		if (MyGetKeyState()&WKEY_CTRL) {
			return CmdStructureAction( C_RDOWN, pos );
		}
		/* no break*/
	case C_RDOWN:
		ParamDialogOkActive( &structurePG, TRUE );
		if (hideStructureWindow)
			wHide( structureW );
		return CmdStructureAction( action, pos );
	case C_MOVE:
		if (MyGetKeyState()&WKEY_CTRL) {
			return CmdStructureAction( C_RMOVE, pos );
		}
		/*no break*/
	case C_RMOVE:
		return CmdStructureAction( action, pos );
	case C_RUP:
	case C_UP:
		if (MyGetKeyState()&WKEY_CTRL) {
			return CmdStructureAction( C_RUP, pos );
		}
		if (hideStructureWindow)
			wShow( structureW );
		InfoMessage( _("Left drag to move, right drag to rotate, or press Return or click Ok to finalize") );
		return CmdStructureAction( action, pos );
		return C_CONTINUE;

	case C_CANCEL:
		wHide( structureW );
		/*no break*/
	case C_REDRAW:
	case C_TEXT:
	case C_OK:
	case C_FINISH:
	case C_CMDMENU:
		return CmdStructureAction( action, pos );

	default:
		return C_CONTINUE;
	}
}



static char * CmdStructureHotBarProc(
		hotBarProc_e op,
		void * data,
		drawCmd_p d,
		coOrd * origP )
{
	turnoutInfo_t * to = (turnoutInfo_t*)data;
	switch ( op ) {
	case HB_SELECT:
		CmdStructureAction( C_FINISH, zero );
		curStructure = to;
		DoCommandB( (void*)(intptr_t)structureHotBarCmdInx );
		return NULL;
	case HB_LISTTITLE:
		FormatCompoundTitle( listLabels, to->title );
		if (message[0] == '\0')
			FormatCompoundTitle( listLabels|LABEL_DESCR, to->title );
		return message;
	case HB_BARTITLE:
		FormatCompoundTitle( hotBarLabels<<1, to->title );
		return message;
	case HB_FULLTITLE:
		return to->title;
	case HB_DRAW:
		origP->x -= to->orig.x;
		origP->y -= to->orig.y;
		DrawSegs( d, *origP, 0.0, to->segs, to->segCnt, trackGauge, wDrawColorBlack );
		return NULL;
	}
	return NULL;
}


EXPORT void AddHotBarStructures( void )
{
	wIndex_t inx;
	turnoutInfo_t * to;
	for ( inx=0; inx < structureInfo_da.cnt; inx ++ ) {
		to = structureInfo(inx);
		if ( !( IsParamValid(to->paramFileIndex) &&
			    to->segCnt > 0 &&
			    CompatibleScale( FALSE, to->scaleInx, GetLayoutCurScale()) ) )
			 /*( (strcmp( to->scale, "*" ) == 0 && strcasecmp( curScaleName, "DEMO" ) != 0 ) ||
			   strncasecmp( to->scale, curScaleName, strlen(to->scale) ) == 0 ) ) )*/
				continue;
		AddHotBarElement( to->contentsLabel, to->size, to->orig, FALSE, FALSE, to->barScale, to, CmdStructureHotBarProc );
	}
}

static STATUS_T CmdStructureHotBar(
		wAction_t action,
		coOrd pos )
{
	switch (action & 0xFF) {

	case C_START:
		structureChange( CHANGE_PARAMS );
		if (curStructure == NULL) {
			NoticeMessage( MSG_STRUCT_NO_STRUCTS, _("Ok"), NULL );
			return C_TERMINATE;
		}
		FormatCompoundTitle( listLabels|LABEL_DESCR, curStructure->title );
		InfoMessage( _("Place %s and draw into position"), message );
        wIndex_t listIndex = FindListItemByContext( structureListL, curStructure );
        if ( listIndex > 0 )
            structureInx = listIndex;
		//ParamLoadControls( &structurePG );
		//ParamGroupRecord( &structurePG );
		return CmdStructureAction( action, pos );

	case wActionMove:
		return CmdStructureAction( action, pos );

	case C_RDOWN:
	case C_DOWN:
		if (MyGetKeyState()&WKEY_CTRL) {
			return CmdStructureAction( C_RDOWN, pos );
		}
		return CmdStructureAction( action, pos );

	case C_RMOVE:
	case C_MOVE:
		if (MyGetKeyState()&WKEY_CTRL) {
			return CmdStructureAction( C_RMOVE, pos );
		}
		return CmdStructureAction( action, pos );

	case C_RUP:
	case C_UP:
		if (MyGetKeyState()&WKEY_CTRL) {
			return CmdStructureAction( C_RUP, pos );
		}
		InfoMessage( _("Left-Drag to place, Ctrl+Left-Drag or Right-Drag to Rotate, Space or Enter to accept, Esc to Cancel") );
		return CmdStructureAction( action, pos );

	case C_TEXT:
		if ((action>>8) != ' ')
			return C_CONTINUE;
		/*no break*/
	case C_OK:
		CmdStructureAction( action, pos );
		return C_CONTINUE;

	case C_CANCEL:
		HotBarCancel();
		/* no break*/
	default:
		return CmdStructureAction( action, pos );
	}
}

#include "bitmaps/struct.xpm"

EXPORT void InitCmdStruct( wMenu_p menu )
{
	AddMenuButton( menu, CmdStructure, "cmdStructure", _("Structure"), wIconCreatePixMap(struct_xpm), LEVEL0_50, IC_WANT_MOVE|IC_STICKY|IC_CMDMENU|IC_POPUP2, ACCL_STRUCTURE, NULL );
	structureHotBarCmdInx = AddMenuButton( menu, CmdStructureHotBar, "cmdStructureHotBar", "", NULL, LEVEL0_50, IC_WANT_MOVE|IC_STICKY|IC_CMDMENU|IC_POPUP2, 0, NULL );
	ParamRegister( &structurePG );
	if ( structPopupM == NULL ) {
		structPopupM = MenuRegister( "Structure Rotate" );
		AddRotateMenu( structPopupM, StructRotate );
	}
}

EXPORT void InitTrkStruct( void )
{
	T_STRUCTURE = InitObject( &structureCmds );

	log_structure = LogFindIndex( "Structure" );
	AddParam( "STRUCTURE ", ReadStructureParam);
	ParamRegister( &pierPG );
}
