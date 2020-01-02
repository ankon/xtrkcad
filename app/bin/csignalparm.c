/*
 * csignalparm.c
 *
 * Handle the Parameter files for Signals
 * SIGNALPART - A signal you can install
 * SIGNALHEAD - Part of a signal
 * SIGNALPOSTPROT - A Part to be added to a signal
 * SIGNALSYSTEM - The container that holds links to parm files with SIGNALHEAD, SIGNALPOSTPROTO and possibly SIGNALPART
 *
 *
 *  Created on: Jul 29, 2019
 *      Author: richardsa
 */

#include <ctype.h>
#include <string.h>
#include <math.h>

#include "track.h"
#include "misc2.h"
#include "csignal.h"
#include "compound.h"
#include "paramfile.h"
#include "messages.h"
#include "layout.h"
#include "fileio.h"
#include "i18n.h"
#include "paramfilelist.h"

/* Global Anchors for Definitional Objects*/
static struct {
	dynArr_t signalHeadTypes;
	dynArr_t signalPostTypes;
	dynArr_t signalParts;
	dynArr_t signalAspectTypes;
} Da;

EXPORT signalPart_t * curSignalPart = NULL;

static wIndex_t signalHotBarCmdInx;

enum paramFileState
GetSignalPartCompatibility(int paramFileIndex, SCALEINX_T scaleIndex)
{
	int i;
	enum paramFileState ret = PARAMFILE_NOTUSABLE;
	DIST_T ratio = GetScaleRatio(scaleIndex);

	if (!IsParamValid(paramFileIndex)) {
		return(PARAMFILE_UNLOADED);
	}

	for (i = 0; i < Da.signalParts.cnt; i++) {
		signalPart_p sp = &DYNARR_N(signalPart_t,Da.signalParts,i);
		if (sp->paramFileIndex == paramFileIndex) {
			if ((sp->scaleInx == SCALE_ANY) || (sp->scaleInx == scaleIndex))
			ret = PARAMFILE_FIT;
			break;
		}
	}
	return(ret);
}

signalAspectType_p FindAspectType(char * name) {

	for (int i=0;i<Da.signalAspectTypes.cnt;i++) {
		signalAspectType_p sa = &DYNARR_N(signalAspectType_t,Da.signalAspectTypes,i);
		if ( strcmp( sa->aspectName, name ) == 0 ) {
			return sa;
		}
	}
	return NULL;
}

const char * GetBaseAspectName(signalAspectType_p sat) { return signalBaseAspectNames[sat->baseAspect]; }
char * GetAspectName(signalAspectType_p sat) {return sat->aspectName; }
signalBaseAspects_e FindBaseAspectType(char * name) {
	for (int i=0;i<BaseAspectCount;i++) {
		if (strcmp(signalBaseAspectNames[i],name) == 0) {
			return (signalBaseAspects_e)i;
		}
	}
	return -1;
}

signalAspectType_p AddAspectType(char* name, char* base) {

	signalAspectType_p sat = NULL;
	for (int i=0;i<Da.signalAspectTypes.cnt;i++) {
		signalAspectType_p sat1 = &DYNARR_N(signalAspectType_t,Da.signalAspectTypes,i);
		if (strcmp(sat1->aspectName,name) == 0) {
			if (strcmp(signalBaseAspectNames[sat1->baseAspect],base) !=0)
				ErrorMessage(MSG_SIGNAL_ASPECT_DUPLICATE,name,base,signalBaseAspectNames[sat1->baseAspect]);
			sat = sat1;
			break;
		}
	}
	if (!sat) {
		DYNARR_APPEND(signalAspectType_t,Da.signalAspectTypes,1);
		sat = &DYNARR_LAST(signalAspectType_t,Da.signalAspectTypes);
		sat->aspectName = name;
	}
	sat->baseAspect = FindBaseAspectType(base);
	return sat;
}

EXPORT void FormatSignalPartTitle(
		long format,
		char * title )
{
	char *cp1, *cp2=NULL, *cq;
	int len;
	FLOAT_T price;
	BOOL_T needSep;
	cq = message;
	if (format&LABEL_COST) {
		FormatCompoundTitle( LABEL_MANUF|LABEL_DESCR|LABEL_PARTNO, title );
		wPrefGetFloat( "price list", message, &price, 0.0 );
		if (price > 0.00) {
			sprintf( cq, "%7.2f\t", price );
		} else {
			strcpy( cq, "\t" );
		}
		cq += strlen(cq);
	}
	cp1 = strchr( title, '\t' );
	if ( cp1 != NULL )
		cp2 = strchr( cp1+1, '\t' );
	if (cp2 == NULL) {
		if ( (format&LABEL_TABBED) ) {
			*cq++ = '\t';
			*cq++ = '\t';
		}
		strcpy( cq, title );
	} else {
		len = 0;
		needSep = FALSE;
		if ((format&LABEL_MANUF) && cp1-title>1) {
			len = cp1-title;
			memcpy( cq, title, len );
			cq += len;
			needSep = TRUE;
		}
		if ( (format&LABEL_TABBED) ) {
			*cq++ = '\t';
			needSep = FALSE;
		}
		if ((format&LABEL_PARTNO) && *(cp2+1)) {
			if ( needSep ) {
				*cq++ = ' ';
				needSep = FALSE;
			}
			strcpy( cq, cp2+1 );
			cq += strlen( cq );
			needSep = TRUE;
		}
		if ( (format&LABEL_TABBED) ) {
			*cq++ = '\t';
			needSep = FALSE;
		}
		if ((format&LABEL_DESCR) || !(format&LABEL_PARTNO)) {
			if ( needSep ) {
				*cq++ = ' ';
				needSep = FALSE;
			}
			memcpy( cq, cp1+1, cp2-cp1-1 );
			cq += cp2-cp1-1;
			needSep = TRUE;
		}
		*cq = '\0';
	}
}

/*
 * SignalPart
 */

/*
 * Find SignalPart either in scale or scale_any
 */

signalPart_p FindSignalPart(SCALEINX_T scaleInx, char * name) {
	signalPart_p si,found = NULL;
	for (int i=0;i<Da.signalParts.cnt;i++) {
		si = &DYNARR_N(signalPart_t, Da.signalParts,i);
		if ( IsParamValid(si->paramFileIndex) &&
			((scaleInx == -1) || (si->scaleInx == scaleInx) || (si->scaleInx == SCALE_ANY)) &&
			strcmp( si->title, name ) == 0 ) {
			if (si->scaleInx == scaleInx)
				return si;
			found = si;
		}
	}
	if (found) return found;
	return NULL;
}

signalPart_p CreateSignalPart(SCALEINX_T scale, char * name, int paramFileIndex) {

	signalPart_p sp = NULL;

	sp = FindSignalPart(scale, name);

	if (sp && (sp->scaleInx == scale)) {
		for (int i=0;i<3;i++) {
			CleanSegs(&sp->staticSegs[i]);
		}
		CleanSegs(&sp->aspects);
		CleanSegs(&sp->groups);
		CleanSegs(&sp->heads);
		//wipe out
	} else {
		DYNARR_APPEND( signalPart_t, Da.signalParts, 10 );
		sp = &DYNARR_LAST(signalPart_t, Da.signalParts);
		sp->title = MyStrdup( name );
		sp->scaleInx = scale;
	}
	sp->paramFileIndex = paramFileIndex;
	sp->barScale = curBarScale>0?curBarScale:-1;

	return sp;

}

void setSignalParmOrig(signalPart_p si, coOrd orig) {si->orig = orig;}
coOrd getSignalParmSize(signalPart_p si) { return si->size; }

int FindHeadNum(signalPart_p si, char * headname) {

	for (int i=0;i<si->heads.cnt;i++) {
		signalHead_p sh = &DYNARR_N(signalHead_t,si->heads,i);
		if ( strcmp( sh->headName, headname ) == 0 ) {
			return i;
		}
	}
	return -1;
}

/*
 * SignalAspect
 */

signalAspect_p FindAspectPart(signalPart_p si, char * name) {

	for (int i=0;i<si->aspects.cnt;i++) {
		signalAspect_p sa = &DYNARR_N(signalAspect_t,si->aspects,i);
		if ( strcmp( sa->aspectName, name ) == 0 ) {
			return sa;
		}
	}
	return NULL;
}

signalAspect_p GetAspectParm(signalPart_p si,int num) {
	if (num<si->aspects.cnt)
		return &DYNARR_N(signalAspect_t,si->aspects,num);
	else return NULL;
}

/*********************************************************************************************/
/*
 * Signal Post Type  - Draw elements and feet defnitions.
 *
 */
/*
 *  Find one in the right scale before one in SCALE_ANY
 */

signalPostType_p FindSignalPostType(SCALEINX_T scale, char * name) {
	signalPostType_p sp = NULL;
	signalPostType_p found = NULL;

	for (int i=0;i<Da.signalPostTypes.cnt;i++) {
		sp = &DYNARR_N(signalPostType_t, Da.signalPostTypes,i);
		if ( IsParamValid(sp->paramFileIndex) &&
			(scale== -1 || sp->scale == scale || sp->scale == SCALE_ANY) &&
			strcmp( sp->postTypeName, name ) == 0 ) {
			 	 if (sp->scale == scale)
			 		 return sp;
			 	 found = sp;
		}
	}
	if (found) return found;
	return NULL;
}


signalPostType_p CreateSignalPostType(SCALEINX_T scale, char * name) {

	signalPostType_p sp = NULL;

	sp = FindSignalPostType(scale, name);

	if (sp && (sp->scale == scale)) {
		for (int i=0;i<3;i++) {
			CleanSegs(&sp->drawings[i]);
		}
		DYNARR_RESET(coOrd,sp->feet);
		//wipe out
	} else {
		sp = (signalPostType_p)MyMalloc(sizeof(signalPost_t));
		DYNARR_APPEND(signalPostType_t, Da.signalPostTypes, 1 );
		sp = &DYNARR_LAST(signalPostType_t, Da.signalPostTypes);
		sp->postTypeName = name;
		sp->scale = scale;
		DYNARR_RESET(coOrd,sp->feet);
	}
	return sp;

}

BOOL_T ReadSignalPostType (char * line) {

	signalPostType_t *sp;
	char *cp = 0;
	char* name;
	char scale[10];
	if (!GetArgs(line+15,"sq",scale,&name)) return FALSE;

	SCALEINX_T scaleInx = LookupScale(scale);

	sp = CreateSignalPostType(scaleInx,name);

	DYNARR_APPEND(coOrd,sp->feet,1);
	coOrd * p = &DYNARR_LAST(coOrd, sp->feet);
	*p = zero;
	while ( (cp = GetNextLine()) != NULL ) {
		if ( strncmp( cp, "END", 3 ) == 0 ) {
			break;
		}
		if ( *cp == '\n' || *cp == '#' ) {
			continue;
		}
		if ( strncmp( cp, "VIEW", 4) == 0) {
			char dispname[10];
			if (!GetArgs(cp+4,"s",dispname)) return FALSE;
			ReadSegs();
			if (tempSegs_da.cnt>0) {
				if (strncmp("PLAN",dispname,4) == 0) {
					AppendSegsToArray(&sp->drawings[SIGNAL_DISPLAY_PLAN],&tempSegs_da);
				} else if (strncmp("DIAG",dispname,4) == 0) {
					AppendSegsToArray(&sp->drawings[SIGNAL_DISPLAY_DIAG],&tempSegs_da);
				} else if (strncmp("ELEV",dispname,4) == 0) {
					AppendSegsToArray(&sp->drawings[SIGNAL_DISPLAY_ELEV],&tempSegs_da);
				}
			}
		} else if (strncmp (cp, "FOOT", 4) == 0) {
			coOrd pos;
			if (!GetArgs(cp+4,"p",&pos)) return FALSE;
			DYNARR_APPEND(coOrd,sp->feet,1);
			coOrd * p = &DYNARR_LAST(coOrd,sp->feet);
			*p = pos;
		} else
			InputError("Unknown SignalPostType param line", TRUE);
	}
	return TRUE;
}

dynArr_t tempWriteSegs;


static BOOL_T WriteSignalPostType ( signalPostType_p pt, FILE * f, SCALEINX_T out_scale ) {
	BOOL_T rc = TRUE;
	DIST_T ratio = GetScaleRatio(pt->scale)/GetScaleRatio(out_scale);
	rc &= fprintf(f, "SIGNALPOSTPROT %s \"%s\"\n",GetScaleName(out_scale),pt->postTypeName)>0;
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
		CleanSegs(&tempWriteSegs);
		AppendSegsToArray(&tempWriteSegs,&pt->drawings[i]);
		if (ratio != 1.0)
			RescaleSegs(tempWriteSegs.cnt, (trkSeg_p)tempWriteSegs.ptr, ratio, ratio, ratio);
		rc &= WriteSegs(f,tempWriteSegs.cnt,tempWriteSegs.ptr)>0;
	}
	for (int i=0;i<pt->feet.cnt;i++) {
		coOrd * pos = &DYNARR_N(coOrd,pt->feet,i);
		coOrd scaled_orig = *pos;
		scaled_orig.x *=ratio;   //Scale rotate origin
		scaled_orig.y *=ratio;
		rc &= fprintf(f, "FOOT %0.6f %0.6f\n",scaled_orig.x,scaled_orig.y)>0;
	}
	rc &= fprintf( f, "ENDSIGNALPOST\n" )>0;
	return rc;
}

EXPORT BOOL_T WriteSignalPostTypes(FILE * f, SCALEINX_T out_scale) {
	BOOL_T rc = TRUE;
	for (int i=0;i<Da.signalPostTypes.cnt;i++) {
		signalPostType_p ht = &DYNARR_N(signalPostType_t,Da.signalPostTypes,i);
		rc &= WriteSignalPostType(ht,f,out_scale)>0;
	}
	return rc;
}


/*
 * HeadType
 */

EXPORT int FindHeadAppearance(signalHeadType_p ht, char * appearanceName) {
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

EXPORT BOOL_T WriteSignalSystem(FILE * f) {
	return TRUE;
}

static wIndex_t signalPartInx;


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
	signalPart_t * sp = (signalPart_t*)data;
	switch ( op ) {
	case HB_SELECT:		/* new element is selected */
		CmdSignalAction( C_FINISH, zero ); 		/* finish current operation */
		curSignalPart = sp;
		DoCommandB( (void*)(intptr_t)signalHotBarCmdInx ); /* continue with new signal */
		return NULL;
	case HB_LISTTITLE:
		FormatSignalPartTitle( listLabels, sp->title );
		if (message[0] == '\0')
			FormatSignalPartTitle( listLabels|LABEL_DESCR, sp->title );
		return message;
	case HB_BARTITLE:
		FormatSignalPartTitle( hotBarLabels<<1, sp->title );
		return message;
	case HB_FULLTITLE:
		return sp->title;
	case HB_DRAW:
		DrawSegs( d, *origP, 0.0, sp->currSegs.ptr, sp->currSegs.cnt, trackGauge, wDrawColorBlack );
		return NULL;
	}
	return NULL;

}


EXPORT void AddHotBarSignals( void )
{
	wIndex_t inx;
	signalPart_p sp;
	for ( inx=0; inx < Da.signalParts.cnt; inx ++ ) {
		sp = &DYNARR_N(signalPart_t,Da.signalParts,inx);
		if (!( IsParamValid(sp->paramFileIndex)  &&
			CompatibleScale( TRUE, sp->scaleInx, GetLayoutCurScale())))
			continue;
		AddHotBarElement( sp->title, sp->size, sp->orig, FALSE, FALSE, sp->barscale, sp, CmdSignalHotBarProc );
	}
}

/*
 * SignalParm
 */

signalAspect_p SignalPartFindAspect(signalPart_p sig, char*name) {
	for (int i=0;i<sig->aspects.cnt;i++) {
		signalAspect_p sa = &DYNARR_N(signalAspect_t,sig->aspects,i);
		if (strcmp(sa->aspectName, name) == 0) {
			return sa;
		}
	}
	return NULL;

}

/*
 * A Signal Part only shows the Elev view and no linkage to Track
 */

static void RebuildSignalPartSegs(signalPart_p sp) {
	CleanSegs(&sp->currSegs);
	AppendSegsToArray(&sp->currSegs,&sp->staticSegs[SIGNAL_DISPLAY_DIAG]);
	RescaleSegs(sp->currSegs.cnt,&DYNARR_N(trkSeg_t,sp->currSegs,0),0.0,0.5,0.0);  //Make posts relatively shorter
	signalAspect_t aspect;
	for (int i=0;i<sp->heads.cnt;i++) {
		/* All Heads */
		signalHead_p head = &DYNARR_N(signalHead_t,sp->heads,i);
		AppendTransformedSegs(&sp->currSegs,&head->headSegs[SIGNAL_DISPLAY_DIAG],head->headPos,zero,0.0);
		coOrd pos_head = head->headPos;
		int indIndex;
		indIndex = 0;
		signalHeadType_p type = head->headType;
		if (type == NULL) continue;
		DIST_T ratio = GetScaleRatio(type->headScale)/GetScaleRatio(sp->scaleInx);
		if ((indIndex >=0) && (indIndex <= type->headAppearances.cnt)) {
			/* Now need to be be placed relative to the rest, rotated and scaled*/
			HeadAppearance_p a = &DYNARR_N(HeadAppearance_t,type->headAppearances,indIndex);
			int start_inx = sp->currSegs.cnt;
			AppendTransformedSegs(&sp->currSegs,&a->appearanceSegs, zero, a->orig, a->angle);
			RescaleSegs(sp->currSegs.cnt,&DYNARR_N(trkSeg_t,sp->currSegs,start_inx),ratio,ratio,ratio);
			MoveSegs(sp->currSegs.cnt,&DYNARR_N(trkSeg_t,sp->currSegs,start_inx),head->headPos);

		}
	}
}



static void ComputeSignalPartBoundingBox (signalPart_p sp )
{
    coOrd lo, hi, lo2, hi2;
    if (sp->currSegs.cnt == 0) {
       	RebuildSignalPartSegs(sp);
    }
	GetSegBounds(zero,0.0,sp->currSegs.cnt,(trkSeg_p)sp->currSegs.ptr,&sp->orig,&sp->size);
	coOrd offset;
	offset.y = -sp->orig.y;
	offset.x = -sp->orig.x;
	MoveSegs(sp->currSegs.cnt,(trkSeg_p)sp->currSegs.ptr,offset);
	sp->orig = zero;
}


/*
 * SIGNALPART  - Something that can be copied to create a SIGNAL def in the layout
 */

BOOL_T ReadSignalPart( char * line ) {
	char scale[10];
	char * name;
	if (!GetArgs(line+10,"sq",scale,&name)) return FALSE;    //SIGNALPART
	SCALEINX_T input_scale = LookupScale(scale);

	signalPart_t * sig;
	char *cp;

	sig = CreateSignalPart(input_scale,name,curParamFileIndex);

	while ( (cp = GetNextLine()) != NULL ) {
		while (isspace((unsigned char)*cp) || *cp == '\t') cp++;
		if ( strncmp( cp, "END", 3 ) == 0 ) {
			break;
		}
		if ( *cp == '\n' || *cp == '#'  ) {
			continue;
		}
		if ( strncmp(cp, "APPEARANCE", 10)==0) {
			char viewname[10];
			if (!GetArgs(cp+10, "s", viewname)) {
				NoticeMessage(MSG_SIGNAL_INVALID_ENTRY, _("Yes"), NULL, line);
				PurgeLinesUntil("ENDSIGNALPART");
				return FALSE;
			}
			ReadSegs();
			int type;
			if (strncmp(viewname,"PLAN", 4) ==0 ) type = SIGNAL_DISPLAY_PLAN;
			else if (strncmp(viewname,"ELEV", 4) ==0 ) type = SIGNAL_DISPLAY_ELEV;
			else type = SIGNAL_DISPLAY_DIAG;
			AppendSegsToArray(&sig->staticSegs[type],&tempSegs_da);
		} else if ( strncmp (cp, "HEAD", 4) == 0) {
			char * headname;
			coOrd headPos;
			char * headType;
			if (!GetArgs(cp+4, "pqq",&headPos, &headname, &headType)) {
				NoticeMessage(MSG_SIGNAL_INVALID_ENTRY, _("Yes"), NULL, line);
				PurgeLinesUntil("ENDSIGNALPART");
				return FALSE;
			}
			DYNARR_APPEND( signalHead_t, sig->heads, 1 );
			signalHead_p sh = &DYNARR_LAST( signalHead_t,sig->heads);
			sh->currentHeadAppearance = 0;
			sh->headName = headname;
			sh->headPos = headPos;
			sh->headTypeName = headType;
			if ((sh->headType = FindHeadType(headType,input_scale)) == NULL) {
				NoticeMessage(MSG_SIGNAL_MISSING_HEADTYPE,_("Yes"), NULL,name,headname,headType);
				--sig->heads.cnt;
			} else {
				DIST_T ratioh = GetScaleRatio(sh->headType->headScale)/GetScaleRatio(input_scale);
				AppendSegsToArray(&sh->headSegs[SIGNAL_DISPLAY_ELEV],&sh->headType->headSegs);
				RescaleSegs(sh->headSegs[SIGNAL_DISPLAY_ELEV].cnt,sh->headSegs[SIGNAL_DISPLAY_ELEV].ptr,ratioh,ratioh,ratioh);
			}
		} else if ( strncmp( cp, "ASPECT", 6 ) == 0 ) {
			char *aspname = NULL, baseAspect[10], *aspscript = NULL;
			if (!GetArgs(cp+6,"qsq",&aspname,baseAspect,&aspscript)) {
				NoticeMessage(MSG_SIGNAL_INVALID_ENTRY, _("Yes"), NULL, line);
				PurgeLinesUntil("ENDASPECT");
				return FALSE;
			}
			if (FindAspectPart(sig,aspname) != NULL) {
				NoticeMessage(MSG_SIGNAL_DUPLICATE_ASPECT,_("Yes"), NULL,name,aspname);
			} else {
				DYNARR_APPEND( signalAspect_t, sig->aspects, 1 );
				signalAspect_p sa =  &DYNARR_LAST(signalAspect_t,sig->aspects);
				sa->aspectName = aspname;
				if ((sa->aspectType = FindBaseAspect(baseAspect)) == NULL) {
					NoticeMessage(MSG_SIGNAL_NO_BASE_ASPECT,_("Yes"), NULL,name,aspname,baseAspect);
					PurgeLinesUntil("ENDASPECT");
				}
				sa->aspectScript = aspscript;
				while ( (cp = GetNextLine()) != NULL ) {
					while (isspace((unsigned char)*cp) || *cp == '\t') cp++;
					if ( *cp == '\n' || *cp == '#') continue;
					if ( strncmp( cp, "ENDASPECT", 9) == 0 ) break;  //END of Signal or of ASPECT
					if ( strncmp( cp, "ASPECTMAP", 9 ) == 0 ) {
						char * app;
						int headNum;
						int number = 1;
						if (!GetArgs(cp+10,"dq",&headNum,&app)) {
							NoticeMessage(MSG_SIGNAL_INVALID_ENTRY, _("Yes"), NULL, line);
							PurgeLinesUntil("ENDASPECT");
							break;
						}
						DYNARR_APPEND( headAspectMap_t, sa->headAspectMap, 1 );
						headAspectMap_p am = &DYNARR_LAST(headAspectMap_t,sa->headAspectMap);
						if (headNum>sig->heads.cnt || headNum <1 ) {
							NoticeMessage(MSG_SIGNAL_MISSING_HEAD,_("Yes"), NULL,name,headNum,aspname);
						} else {
							am->aspectMapHeadNumber = headNum;
							am->aspectMapHeadAppearanceNumber = FindHeadAppearance(DYNARR_N(signalHead_t,sig->heads,headNum-1).headType,app);
							if (am->aspectMapHeadAppearanceNumber == -1)
								NoticeMessage(MSG_SIGNAL_MISSING_APPEARANCE,_("Yes"), NULL,name,app,headNum,aspname);
							else am->aspectMapHeadAppearance = strdup(app);
						}
					}
				}
			}
		} else if ( strncmp (cp, "GROUP", 5) == 0) {
			char * groupName;
			if (!GetArgs(cp+6,"q",&groupName)) {
				PurgeLinesUntil("ENDGROUP");
				return FALSE;
			}
			DYNARR_APPEND(signalGroup_t, sig->groups, 1 );
			signalGroup_p sg =  &DYNARR_LAST(signalGroup_t,sig->groups);
			sg->name = groupName;
			while ( (cp = GetNextLine()) != NULL ) {
				while (isspace((unsigned char)*cp) || *cp == '\t') cp++;
				if ( *cp == '\n' || *cp == '#' ) continue;
				if ( strncmp( cp, "ENDGROUP", 8) == 0 ) break;  //END of Signal or of ASPECTS
				char *aspect;
				if ( strncmp( cp, "TRACKASPECT", 11 ) == 0 ) {
					char groupAspect[20];
					if (!GetArgs(cp+12,"s",groupAspect)) {
						PurgeLinesUntil("ENDGROUP");
						return FALSE;
					}
					signalAspect_p sa = SignalPartFindAspect(sig,groupAspect);
					if (!sa) NoticeMessage(MSG_SIGNAL_GRP_ASPECT_INVALID, _("Yes"), NULL,name, sig->groups.cnt, groupAspect);
					else {
						DYNARR_APPEND(groupAspectList_t,sg->aspects,1);
						DYNARR_LAST(groupAspectList_t,sg->aspects).aspectName = groupAspect;
						DYNARR_LAST(groupAspectList_t,sg->aspects).aspect = sa;
					}
				}
				if ( strncmp( cp, "INDICATE", 8 ) == 0 ) {
					char indOnName[20], indOffName[20], *conditions;
					int headNum;
					if (!GetArgs(cp+9,"dssq",&headNum,indOnName,indOffName,&conditions)) {
						PurgeLinesUntil("ENDGROUP");
						break;
					}
					if (headNum > sig->heads.cnt) {
						NoticeMessage(MSG_SIGNAL_GRP_GROUPHEAD_INVALID, _("Yes"), NULL,name, sig->groups.cnt, headNum );
						PurgeLinesUntil("ENDGROUP");
						break;
					} else {
						signalHead_p sh = &DYNARR_N(signalHead_t,sig->heads,headNum);
						if(FindHeadAppearance(sh->headType, indOffName) == -1) {
							NoticeMessage(MSG_SIGNAL_GRP_IND_INVALID, _("Yes"), NULL,name, sig->groups.cnt, indOffName);
						} else if (FindHeadAppearance(sh->headType, indOnName) == -1) {
							NoticeMessage(MSG_SIGNAL_GRP_IND_INVALID, _("Yes"), NULL,name, sig->groups.cnt, indOnName);
						} else {
							DYNARR_APPEND(signalGroupInstance_t, sg->groupInstances, 1 );
							signalGroupInstance_p gi = &DYNARR_LAST(signalGroupInstance_t, sg->groupInstances);
							gi->headId = headNum;
							gi->appearanceOnName = indOnName;
							gi->appearanceOffName = indOffName;
							gi->conditions = conditions;    //Dont check yet
						}
					}
				}
			}
		} else if ( strncmp( cp, "ENDSIGNALPART", 13) == 0 ) {
			break;
		} else {
			InputError("Unknown SignalPart param line", TRUE);
		}

	}
	ComputeSignalPartBoundingBox(sig);
	return TRUE;
}

BOOL_T LoadSubParamFile(char * dirName, char * fileName) {
	FILE * oldFile;
	wIndex_t oldLineNum;
	long oldCheckSum;
	char * oldFileName;
	BOOL_T rc;

	oldFile = paramFile;
	oldLineNum = paramLineNum;
	oldCheckSum = paramCheckSum;
	oldFileName = paramFileName;
	rc = ReadParams(0, dirName, fileName);
	paramFile = oldFile;
	paramLineNum = oldLineNum;
	paramCheckSum = oldCheckSum;
	paramFileName = oldFileName;
	return rc;
}

/*
 * The SignalSystem file is the pointer to a set of files that together have all the types needed
 */
BOOL_T ReadSignalSystem( char * line) {
	char * name, filename[80];
	char dirName[1024];
	char fullName[80];
	if (!GetArgs(line+13,"q",&name)) return FALSE;    //SIGNALSYSTEM
	char * cp;
	while ( (cp = GetNextLine()) != NULL ) {
		while (isspace((unsigned char)*cp) || *cp == '\t') cp++;
		if ( strncmp( cp, "END", 3 ) == 0 ) {
			break;
		}
		if ( *cp == '\n' || *cp == '#'  ) {
			continue;
		}
		if ( strncmp(cp, "BASEASPECT", 10)==0) {

		} else if ( strncmp(cp, "HEADTYPES", 9)==0) {
			if (!GetArgs(line+10,"s",filename)) return FALSE;    //SIGNALHEAD
			sprintf( dirName, "%s" FILE_SEP_CHAR "signals" FILE_SEP_CHAR "HeadTypes" FILE_SEP_CHAR, libDir );
			sprintf( fullName, "%s.xtps", filename );
			if (!LoadSubParamFile(dirName,fullName))
				NoticeMessage(MSG_SIGNAL_SYSTEM_INVALID_FILE, _("Yes"), NULL,fullName);
		} else if ( strncmp(cp, "SIGNALPOSTS", 11)==0) {
			if (!GetArgs(line+11,"s",filename)) return FALSE;    //SIGNALPOSTPROT
			sprintf( dirName, "%s" FILE_SEP_CHAR "signals" FILE_SEP_CHAR "Posts" FILE_SEP_CHAR, libDir );
			sprintf( fullName, "%s.xtps", filename );
			if (!LoadSubParamFile(dirName,fullName))
				NoticeMessage(MSG_SIGNAL_SYSTEM_INVALID_FILE, _("Yes"), NULL, fullName);
		} else if ( strncmp(cp, "SIGNALPARTS", 11)==0) {
			if (!GetArgs(line+11,"s",filename)) return FALSE;    //SIGNALPART
			sprintf( dirName, "%s" FILE_SEP_CHAR "signals" FILE_SEP_CHAR "Signals" FILE_SEP_CHAR, libDir );
			sprintf( fullName, "%s.xtps", filename );
			if (!LoadSubParamFile(dirName,fullName))
				NoticeMessage(MSG_SIGNAL_SYSTEM_INVALID_FILE, _("Yes"), NULL,fullName);
		} else {
			InputError("Unknown SignalSystem param line", TRUE);
		}
	}
	return TRUE;
}

EXPORT void InitCmdSignalHotBar ( wMenu_p menu ) {

	 signalHotBarCmdInx = AddMenuButton( menu, CmdSignalHotBar, "cmdSignalHotBar", "", NULL, LEVEL0_50, IC_WANT_MOVE|IC_STICKY|IC_CMDMENU|IC_POPUP2, 0, NULL );

}



