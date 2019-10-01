/*
 * csignalparm.c
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

/* Global Anchors for Definitional Objects*/
static struct {
	dynArr_t signalHeadTypes;
	dynArr_t signalPosts;
	dynArr_t signalParms;
	dynArr_t signalAspectTypes;
} Da;

signalAspectType_p FindAspectType(char * name) {

	for (int i=0;i<Da.signalAspectTypes.cnt;i++) {
		signalAspect_p sa = DYNARR_N(signalAspect_t,Da.signalAspectTypes,i);
		if ( strcmp( sa->aspectName, name ) == 0 ) {
			return sa;
		}
	}
	return NULL;
}

int GetAspectSpeed(signalAspectType_p sat) { return sat->speed; }
const char * GetBaseAspectName(signalAspectType_p sat) { return sat->baseAspectName; }
char * GetAspectName(signalAspectType_p sat) {return sat->aspectName; }

signalAspectType_p AddAspectType(char* name, signalBaseAspects_e base, int speed) {

	signalAspectType_p sat = NULL;
	for (int i=0;i<Da.signalAspectTypes.cnt;i++) {
		signalAspectType_p sat1 = &DYNARR_N(signalAspectType_t,Da.signalAspectTypes,i);
		if (strcmp(sat1->aspectName,name) == 0) {
			sat = sat1;
			break;
		}
	}
	if (!sat) {
		DYNARR_APPEND(signalAspect_t,Da.signalAspectTypes,1);
		sat = &DYNARR_LAST(signalAspect_t,Da.signalAspectTypes);
	}
	sat->aspectName = name;
	sat->baseAspect = base;
	sat->baseAspectName = signalBaseAspectNames[sat->baseAspect];
	sat->speed = speed;
	return sat;
}

EXPORT void FormatSignalTitle(
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

signalParm_p FindSignalParm(char* scale, char * name) {
	signalParm_p si = NULL;
	SCALEINX_T scaleInx;
	if ( scale )
			scaleInx = LookupScale( scale );
		else
			scaleInx = -1;

	for (int i=0;i<Da.signalParms.cnt;i++) {
		si = &DYNARR_N(signalParm_t, Da.signalParms,i);
		if ( IsParamValid(si->paramFileIndex) &&
			(scaleInx == -1 || si->scaleInx == scaleInx || si->scaleInx = SCALE_ANY) &&
			strcmp( si->title, name ) == 0 ) {
				return si;
		}
	}
	return NULL;
}

signalParm_p CreateSignalParm(char* scale, char * name) {

	signalParm_p si = NULL;

	si = FindSignalParm(scale, name);

	if (!si) {
		for (int i=0;i<3;i++) {
			CleanSegs(&si->staticSegs[i]);
		}
		si->aspects.cnt = 0;
		si->groups.cnt = 0;
		si->heads.cnt = 0;
		//wipe out
	} else {
		si = (signalParm_p)MyMalloc(sizeof(signalParm_t));
		DYNARR_APPEND( signalParm_t, Da.signalParms, 10 );
		DYNARR_LAST(signalParm_t, Da.signalParms) = si;
		si->title = MyStrdup( name );
		si->scaleInx = LookupScale( scale );
		si->barScale = curBarScale>0?curBarScale:-1;
	}

	return si;

}

void setSignalParmOrig(signalParm_p si, coOrd orig) {si->orig = orig;}
coOrd getSignalParmSize(signalParm_p si) { return si->size; }

int FindHeadNum(signalParm_p si, char * headname) {

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

signalAspect_p FindAspectParm(signalParm_p si, char * name) {

	for (int i=0;i<si->aspects.cnt;i++) {
		signalAspect_p sa = DYNARR_N(signalAspect_t,si->aspects,i);
		if ( strcmp( sa->aspectName, name ) == 0 ) {
			return sa;
		}
	}
	return NULL;
}

signalAspect_p GetAspectParm(signalParm_p si,int num) {
	if (num<si->aspects.cnt)
		return &DYNARR_N(signalAspect_t,si->aspects,num);
	else return -1;
}

BOOL_T ReadSignalParam ( char * line ) {
	char scale[10];
	char * name;
	if (!GetArgs(line+6,"sq",scale,&name)) return FALSE;

	signalParm_t * sig;
	char *cp;

	sig = CreateSignalParm(scale,name);

	while ( (cp = GetNextLine()) != NULL ) {
		 if ( strncmp( cp, "END", 3 ) == 0 ) {
			break;
		}
		if ( *cp == '\n' || *cp == '#' ) {
			continue;
		}
		if ( strncmp(cp, "POST", 4) == 0) {
			char * postname;
			if (!GetArgs(cp+4, "q", &postname)) return FALSE;
			signalPost_t sp = FindSignalPost(scale,postname);
			if (!sp)
				ErrorMessage(MSG_SIGNAL_MISSING_POST,name,postname);
			for (int i=0;i<3;i++) {
				AppendSegs(&sig->staticSegs[i],sp->drawings[i]);
			}
		}
		if ( strncmp(cp, "VIEW", 4)==0) {
			char * viewname;
			if (!GetArgs(cp+4, "s", &viewname)) return FALSE;
			ReadSegs();
			if (strncmp(viewname,"PLAN", 4) ==0 )
				AppendSegs(&sig->staticSegs[0],tempSegs_da);
			if (strncmp(viewname,"ELEV", 4) ==0 )
				AppendSegs(&sig->staticSegs[1],tempSegs_da);
			if (strncmp(viewname,"DIAG", 4) ==0 )
				AppendSegs(&sig->staticSegs[2],tempSegs_da);
		}
		if ( strncmp (cp, "HEAD", 4) == 0) {
			char * headname, * diagramText;
			coOrd headPos;
			char * headType;
			if (!GetArgs(cp+4, "qpdqq", &headname,&headPos,&headType,&diagramText)) return FALSE;
			DYNARR_APPEND( signalHead_t, sig->heads, 1 );
			signalHead_p sh = &DYNARR_LAST( signalHead_t,sig->heads);
			sh->currentHeadAppearance = 0;
			sh->headName = headname;
			sh->headPos = headPos;
			sh->headTypeName = headType;
			if ((sh->headType = FindHeadType(headType)) == NULL)
				ErrorMessage(MSG_SIGNAL_MISSING_HEADTYPE,name,headname,headType);
			if (diagramText[0]) {
				sh->diagramText = diagramText;    //Override for Matrix/Stencil
			}
		}
		if ( strncmp( cp, "ASPECT", 6 ) == 0 ) {
			char *aspname = NULL, *aspectType = NULL, *aspscript = NULL;
			if (!GetArgs(cp+6,"qqq",&aspname,&aspectType,&aspscript)) return FALSE;
			if (FindAspectParm(sig,aspname) != NULL)
				ErrorMessage(MSG_SIGNAL_DUPLICATE_ASPECT,name,aspname);
			else {
				DYNARR_APPEND( signalAspect_p *, sig->aspects, 1 );
				signalAspect_p sa =  &DYNARR_LAST(signalAspect_t,sig->aspects);
				sa->aspectName = aspname;
				if ((sa->aspectType = FindAspectType(&aspectType)) == NULL)
					ErrorMessage(MSG_SIGNAL_MISSING_ASPECT_TYPE,name,aspname,aspectType);
				sa->aspectScript = aspscript;
				while ( (cp = GetNextLine()) != NULL ) {
					while (isspace((unsigned char)*cp)) cp++;
					if ( *cp == '\n' || *cp == '#' ) continue;
					if ( strncmp( cp, "END", 3) == 0 ) break;  //END of Signal or of ASPECT
					if ( strncmp( cp, "HEADMAP", 7 ) == 0 ) {
						char * appName;
						int headNum;
						int number = 1;
						if (!GetArgs(cp+13,"ds",&headNum,&appName)) return FALSE;
						DYNARR_APPEND( headAspectMap_t, sa->headMap, 1 );
						headAspectMap_p am = &DYNARR_N(headAspectMap_t,sa->headMap,sa->headMap.cnt-1);
						if (headNum>sig->heads.cnt || headNum <1 )
							ErrorMessage(MSG_SIGNAL_MISSING_HEAD,name,headNum,aspname);
						else {
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
			if (!GetArgs(cp+12,"q",&groupName)) return FALSE;
			DYNARR_APPEND(signalGroup_t, sig->groups, 1 );
			signalGroup_p sg =  &DYNARR_LAST(signalGroup_t,sig->groups);
			sg->name = groupName;
			while ( (cp = GetNextLine()) != NULL ) {
				while (isspace((unsigned char)*cp)) cp++;
				if ( *cp == '\n' || *cp == '#' ) continue;
				if ( strncmp( cp, "END", 3) == 0 ) break;  //END of Signal or of ASPECTS
				char *aspect;
				if ( strncmp( cp, "TRACKASPECT", 11 ) == 0 ) {
					char * groupAspect;
					if (!GetArgs(cp+12,"s",&groupAspect)) return FALSE;
					signalAspect_p sa = FindAspect(name,groupAspect);
					if (!sa) ErrorMessage(MSG_SIGNAL_GRP_ASPECT_INVALID, name, sig->groups.cnt, groupAspect);
					else {
						DYNARR_APPEND(AspectList_t,sg->aspects,1);
						DYNARR_LAST(AspectList_t,sg->aspects).aspectName = groupAspect;
						DYNARR_LAST(AspectList_t,sg->aspects).aspect = sa;
					}
				}
				if ( strncmp( cp, "INDICATE", 8 ) == 0 ) {
					char* indOnName, *indOffName, *conditions;
					int headNum;
					if (!GetArgs(cp+8,"dssq",&headNum,&indOnName,&indOffName,&conditions)) return FALSE;
					if (headNum > sig->heads.cnt)
							ErrorMessage(MSG_SIGNAL_GRP_GROUPHEAD_INVALID, name, sig->groups.cnt, headNum );
					else {
						signalHead_t sh = DYNARR_N(signalHead_t,sig->heads,headNum);
						if(FindAppearanceNum(sh, indOnName) == -1)
							ErrorMessage(MSG_SIGNAL_GRP_IND_INVALID, name, sig->groups.cnt, indOnName);
						else {
							if(FindAppearanceNum(sh, indOffName) == -1)
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
	}
	SetTrkVisible(sig, visible);
	SetTrkScale(sig, LookupScale( scale ));
	SetTrkLayer(sig, layer);
	ComputeSignalBoundingBox(sig,SignalDisplay)
	return TRUE;
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
		sp = DYNARR_N(signalPost_t, Da.signalPosts,i);
		if ( IsParamValid(sp->paramFileIndex) &&
			(scaleInx == -1 || sp->scale == scaleInx || sp->scale = SCALE_ANY) &&
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
			CleanSegs(sp->drawings[i]);
		}
		//wipe out
	} else {
		sp = (signalPost_p)MyMalloc(sizeof(signalPost_t));
		DYNARR_APPEND(signalPost_t, Da.signalPosts, 1 );
		sp = DYNARR_LAST(signalPost_t, Da.signalPosts);
		sp->title = MyStrdup( title );
		sp->scale = LookupScale( scale );
	}
	return sp;

}

BOOL_T ReadSignalPost (char * line) {

	signalPost_t *sp;
	char *cp = 0;
	if (!GetArgs(line+6,"sq",scale,&name)) return FALSE;

	sp = CreateSignalPost(scale,name);

	sp->name = strdup(name);
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
					AppendSegs(&sp->drawings[SIGNAL_DISPLAY_PLAN],&tempSegs_da);
				} else if (strncmp("DIAG",dispname,4) == 0) {
					AppendSegs(&sp->drawings[SIGNAL_DISPLAY_DIAG],&tempSegs_da);
				} else if (strncmp("ELEV",dispname,4) == 0) {
					AppendSegs(&sp->drawings[SIGNAL_DISPLAY_ELEV],&tempSegs_da);
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
		Appearance_t ha = DYNARR_N(Appearance_t,headtype->headAppearances,i);
		if (strcmp(ha->appearanceName,name) == 0) return i;
	}
	return -1;

}





