/*
 * csignalparm.c
 *
 * Handle the Parameter files for Signals
 * SIGNALPART - A signal you can install
 * SIGNALPROTO - A signal you can build Parts out of
 * SIGNALHEADPROTO - Part of a Proto signal
 * SIGNALPOSTPROTO - Part of a Proto signal
 * SIGNALSYSTEM - The container that holds SIGNALPROTO, SIGNALHEADPROTO, SIGNALPOSTPROTO
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

/* Global Anchors for Definitional Objects*/
static struct {
	dynArr_t signalHeadTypes;
	dynArr_t signalPosts;
	dynArr_t signalParms;
	dynArr_t signalAspectTypes;
} Da;

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

EXPORT void FormatSignalParmTitle(
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
 * SignalParm
 */

signalPart_p FindSignalPart(SCALEINX_T scaleInx, char * name) {
	signalPart_p si = NULL;
	for (int i=0;i<Da.signalParms.cnt;i++) {
		si = &DYNARR_N(signalPart_t, Da.signalParms,i);
		if ( IsParamValid(si->paramFileIndex) &&
			((scaleInx == -1) || (si->scaleInx == scaleInx) || (si->scaleInx == SCALE_ANY)) &&
			strcmp( si->title, name ) == 0 ) {
				return si;
		}
	}
	return NULL;
}

signalPart_p CreateSignalPart(SCALEINX_T scale, char * name) {

	signalPart_p si = NULL;

	si = FindSignalPart(scale, name);

	if (!si) {
		for (int i=0;i<3;i++) {
			CleanSegs(&si->staticSegs[i]);
		}
		si->aspects.cnt = 0;
		si->groups.cnt = 0;
		si->heads.cnt = 0;
		//wipe out
	} else {
		DYNARR_APPEND( signalPart_t, Da.signalParms, 10 );
		si = &DYNARR_LAST(signalPart_t, Da.signalParms);
		si->title = MyStrdup( name );
		si->scaleInx = scale;
		si->barScale = curBarScale>0?curBarScale:-1;
	}

	return si;

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

signalAspect_p FindAspectParm(signalPart_p si, char * name) {

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
 * Signal Post
 */

signalPost_p FindSignalPost(char* scale, char * name) {
	signalPost_t * sp = NULL;
	SCALEINX_T scaleInx;
	if ( scale )
			scaleInx = LookupScale( scale );
		else
			scaleInx = -1;

	for (int i=0;i<Da.signalPosts.cnt;i++) {
		sp = &DYNARR_N(signalPost_t, Da.signalPosts,i);
		if ( IsParamValid(sp->paramFileIndex) &&
			(scaleInx == -1 || sp->scale == scaleInx || sp->scale == SCALE_ANY) &&
			strcmp( sp->title, name ) == 0 ) {
				return sp;
		}
	}
	return NULL;
}


signalPost_p CreateSignalPost(char* scale, char * name) {

	signalPost_p sp = NULL;

	sp = FindSignalPost(scale, name);

	if (!sp) {
		for (int i=0;i<3;i++) {
			CleanSegs(&sp->drawings[i]);
		}
		//wipe out
	} else {
		sp = (signalPost_p)MyMalloc(sizeof(signalPost_t));
		DYNARR_APPEND(signalPost_t, Da.signalPosts, 1 );
		sp = &DYNARR_LAST(signalPost_t, Da.signalPosts);
		sp->title = MyStrdup(name);
		sp->scale = LookupScale( scale );
	}
	return sp;

}

BOOL_T ReadSignalPost (char * line) {

	signalPost_t *sp;
	char *cp = 0;
	char* name;
	char* scale;
	if (!GetArgs(line+6,"sq",&scale,&name)) return FALSE;

	sp = CreateSignalPost(scale,name);

	sp->postName = name;
	while ( (cp = GetNextLine()) != NULL ) {
		if ( strncmp( cp, "END", 3 ) == 0 ) {
			break;
		}
		if ( *cp == '\n' || *cp == '#' ) {
			continue;
		}
		if ( strncmp( cp, "VIEW", 4) == 0) {
			char * dispname;
			if (!GetArgs(cp+5,"q",&dispname)) return FALSE;
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
		}
	}
	return TRUE;

}

/*
 * HeadType
 */

int FindAppearanceNum(signalHeadType_p headtype,char * name) {
	for (int i=0;i<headtype->headAppearances.cnt;i++) {
		HeadAppearance_p ha = &DYNARR_N(HeadAppearance_t,headtype->headAppearances,i);
		if (strcmp(ha->appearanceName,name) == 0) return i;
	}
	return -1;

}

signalHeadType_p FindHeadType(char* name) {
	for (int i=0;i<Da.signalHeadTypes.cnt;i++) {
		signalHeadType_p ht = &DYNARR_N(signalHeadType_t,Da.signalHeadTypes,i);
		if (strcmp(ht->headTypeName,name) == 0) return ht;
	}
	return NULL;
}

/*
 * SignalParm
 */

signalAspect_p SignalParmFindAspect(signalPart_p sig, char*name) {
	for (int i=0;i<sig->aspects.cnt;i++) {
		signalAspect_p sa = &DYNARR_N(signalAspect_t,sig->aspects,i);
		if (strcmp(sa->aspectName, name) == 0) {
			return sa;
		}
	}
	return NULL;

}

static void RebuildSignalPartSegs(signalPart_p sp) {
	CleanSegs(&sp->currSegs);
	AppendSegsToArray(&sp->currSegs,&sp->staticSegs[SIGNAL_DISPLAY_ELEV]);
	signalAspect_t aspect;
	for (int i=0;i<sp->heads.cnt;i++) {
		/* All Heads */
		signalHead_p head = &DYNARR_N(signalHead_t,sp->heads,i);
		AppendTransformedSegs(&sp->currSegs,&head->headSegs[SIGNAL_DISPLAY_ELEV],head->headPos,zero,0.0);
		int indIndex;
		indIndex = 0;
		signalHeadType_p type = head->headType;
		if (type == NULL) continue;
		if ((indIndex >=0) && (indIndex <= type->headAppearances.cnt)) {
			/* Now need to be be placed relative to the rest, rotated and scaled*/
			HeadAppearance_p a = &DYNARR_N(HeadAppearance_t,type->headAppearances,indIndex);
			int start_inx = sp->currSegs.cnt;
			AppendTransformedSegs(&sp->currSegs,&a->appearanceSegs, a->orig, a->orig, a->angle);
			if (type->headScale != sp->scaleInx) {
				DIST_T ratioh = GetScaleRatio(type->headScale)/GetScaleRatio(sp->scaleInx);
				RescaleSegs(a->appearanceSegs.cnt,&DYNARR_N(trkSeg_t,sp->currSegs,start_inx),ratioh,ratioh,ratioh);
			}
			MoveSegs(a->appearanceSegs.cnt,&DYNARR_N(trkSeg_t,sp->currSegs,start_inx),head->headPos);
		}
	}
}



static void ComputeSignalPartBoundingBox (signalPart_p sp )
{
    coOrd lo, hi, lo2, hi2;
    if (sp->currSegs.cnt == 0) {
       	RebuildSignalPartSegs(sp);
    }
    coOrd offset = zero;
    ANGLE_T a = 0.0;
	GetSegBounds(offset,a,sp->currSegs.cnt,(trkSeg_p)sp->currSegs.ptr,&sp->orig,&sp->size);
}


/*
 * SIGNALITEM  - Something that can be copied to create a SIGNAL def in the layout
 */

BOOL_T ReadSignalPart( char * line ) {
	char scale[10];
	char * name;
	if (!GetArgs(line+10,"sq",scale,&name)) return FALSE;    //SIGNALPART
	SCALEINX_T input_scale = LookupScale(scale);
	SCALEINX_T curr_scale = GetLayoutCurScale();
	DIST_T ratio = GetScaleRatio(curr_scale)/GetScaleRatio(input_scale);

	signalPart_t * sig;
	char *cp;

	sig = CreateSignalPart(curr_scale,name);

	while ( (cp = GetNextLine()) != NULL ) {
		while (isspace((unsigned char)*cp) || *cp == '\t') cp++;
		if ( strncmp( cp, "END", 3 ) == 0 ) {
			break;
		}
		if ( *cp == '\n' || *cp == '#'  ) {
			continue;
		}
		if ( strncmp(cp, "VIEW", 4)==0) {
			char * viewname;
			if (!GetArgs(cp+4, "s", &viewname)) {
				Purge("ENDSIGNAL");
				return FALSE;
			}
			ReadSegs();
			int type;
			if (strncmp(viewname,"PLAN", 4) ==0 ) type = SIGNAL_DISPLAY_PLAN;
			if (strncmp(viewname,"ELEV", 4) ==0 ) type = SIGNAL_DISPLAY_ELEV;
			else type = SIGNAL_DISPLAY_DIAG;
			RescaleSegs(tempSegs_da.cnt,tempSegs_da.ptr,ratio,ratio,ratio);
			AppendSegsToArray(&sig->staticSegs[type],&tempSegs_da);
		}
		if ( strncmp (cp, "HEAD", 4) == 0) {
			char * headname;
			coOrd headPos;
			char * headType;
			if (!GetArgs(cp+4, "pqq",&headPos, &headname, &headType)) return FALSE;
			DYNARR_APPEND( signalHead_t, sig->heads, 1 );
			signalHead_p sh = &DYNARR_LAST( signalHead_t,sig->heads);
			sh->currentHeadAppearance = 0;
			sh->headName = headname;
			sh->headPos = headPos;
			sh->headTypeName = headType;
			if ((sh->headType = FindHeadType(headType)) == NULL) {
				ErrorMessage(MSG_SIGNAL_MISSING_HEADTYPE,name,headname,headType);
				--sig->heads.cnt;
			} else {
				DIST_T ratioh = GetScaleRatio(sh->headType->headScale)/GetScaleRatio(curr_scale);
				AppendSegsToArray(&sh->headSegs[SIGNAL_DISPLAY_ELEV],&sh->headType->headSegs);
				RescaleSegs(sh->headSegs[SIGNAL_DISPLAY_ELEV].cnt,sh->headSegs[SIGNAL_DISPLAY_ELEV].ptr,ratioh,ratioh,ratioh);
			}
		}
		if ( strncmp( cp, "ASPECT", 6 ) == 0 ) {
			char *aspname = NULL, *aspectType = NULL, *aspscript = NULL;
			if (!GetArgs(cp+6,"qqq",&aspname,&aspectType,&aspscript)) {
				Purge("ENDASPECT");
				return FALSE;
			}
			if (FindAspectParm(sig,aspname) != NULL)
				ErrorMessage(MSG_SIGNAL_DUPLICATE_ASPECT,name,aspname);
			else {
				DYNARR_APPEND( signalAspect_t, sig->aspects, 1 );
				signalAspect_p sa =  &DYNARR_LAST(signalAspect_t,sig->aspects);
				sa->aspectName = aspname;
				if ((sa->aspectType = FindAspectType(aspectType)) == NULL)
					ErrorMessage(MSG_SIGNAL_NO_BASE_ASPECT,name,aspname,aspectType);
				sa->aspectScript = aspscript;
				while ( (cp = GetNextLine()) != NULL ) {
					while (isspace((unsigned char)*cp)) cp++;
					if ( *cp == '\n' || *cp == '#' || *cp == '\t') continue;
					if ( strncmp( cp, "ENDASPECT", 9) == 0 ) break;  //END of Signal or of ASPECT
					if ( strncmp( cp, "ASPECTMAP", 9 ) == 0 ) {
						char * appName;
						int headNum;
						int number = 1;
						if (!GetArgs(cp+13,"ds",&headNum,&appName)) {
							Purge("ENDASPECT");
							return FALSE;
						}
						DYNARR_APPEND( headAspectMap_t, sa->headAspectMap, 1 );
						headAspectMap_p am = &DYNARR_LAST(headAspectMap_t,sa->headAspectMap);
						if (headNum>sig->heads.cnt || headNum <1 ) {
							ErrorMessage(MSG_SIGNAL_MISSING_HEAD,name,headNum,aspname);

						} else {
							am->aspectMapHeadNumber = headNum;
							am->aspectMapHeadAppearanceNumber = FindAppearanceNum(DYNARR_N(signalHead_t,sig->heads,am->aspectMapHeadNumber).headType,appName);
							if (am->aspectMapHeadAppearanceNumber == -1)
								ErrorMessage(MSG_SIGNAL_MISSING_APPEARANCE,name,appName,headNum,aspname);
							else am->aspectMapHeadAppearance = appName;
						}
					}
				}
			}
		}

		if ( strncmp (cp, "GROUP", 5) == 0) {
			char * groupName;
			if (!GetArgs(cp+12,"q",&groupName)) {
				Purge("ENDGROUP");
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
					char * groupAspect;
					if (!GetArgs(cp+12,"s",&groupAspect)) {
						Purge("ENDGROUP");
						return FALSE;
					}
					signalAspect_p sa = SignalParmFindAspect(sig,groupAspect);
					if (!sa) ErrorMessage(MSG_SIGNAL_GRP_ASPECT_INVALID, name, sig->groups.cnt, groupAspect);
					else {
						DYNARR_APPEND(groupAspectList_t,sg->aspects,1);
						DYNARR_LAST(groupAspectList_t,sg->aspects).aspectName = groupAspect;
						DYNARR_LAST(groupAspectList_t,sg->aspects).aspect = sa;
					}
				}
				if ( strncmp( cp, "INDICATE", 8 ) == 0 ) {
					char* indOnName, *indOffName, *conditions;
					int headNum;
					if (!GetArgs(cp+8,"dssq",&headNum,&indOnName,&indOffName,&conditions)) {
						Purge("ENDGROUP");
						break;
					}
					if (headNum > sig->heads.cnt) {
						ErrorMessage(MSG_SIGNAL_GRP_GROUPHEAD_INVALID, name, sig->groups.cnt, headNum );
						Purge("ENDGROUP");
						break;
					} else {
						signalHead_p sh = &DYNARR_N(signalHead_t,sig->heads,headNum);
						if(FindAppearanceNum(sh->headType, indOffName) == -1)
							ErrorMessage(MSG_SIGNAL_GRP_IND_INVALID, name, sig->groups.cnt, indOffName);
						else {
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
		}
	}
	ComputeSignalPartBoundingBox(sig);
	return TRUE;
}






