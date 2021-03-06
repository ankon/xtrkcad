/** \file cparalle.c
 * PARALLEL
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

#include <math.h>

#include "ccurve.h"
#include "cstraigh.h"
#include "cundo.h"
#include "i18n.h"
#include "messages.h"
#include "param.h"
#include "track.h"
#include "utility.h"
#include "layout.h"

static struct {
		track_p Trk;
		coOrd orig;
		track_p anchor_Trk;
		} Dpa;

static DIST_T parSeparation = 1.0;
static double parSepFactor = 0.0;
static long parType = 0;

enum PAR_TYPE_E { PAR_TRACK, PAR_LINE };

static paramFloatRange_t r_0o1_100 = { 0.0, 100.0, 100 };
static paramFloatRange_t r_0_10 = { 0.0, 10.0 };
static paramData_t parSepPLs[] = {
#define parSepPD (parSepPLs[0])
#define parSepI 0
	{	PD_FLOAT, &parSeparation, "separation", PDO_DIM, &r_0o1_100, N_("Separation") },
#define parFactorPD (parSepPLs[1])
#define parFactorI 1
	{   PD_FLOAT, &parSepFactor, "factor", 0, &r_0_10, N_("Radius Factor") }
};
static paramGroup_t parSepPG = { "parallel", 0, parSepPLs, sizeof parSepPLs/sizeof parSepPLs[0] };


static STATUS_T CmdParallel(wAction_t action, coOrd pos)
{

    DIST_T d;
    track_p t=NULL;
    coOrd p;
    static coOrd p0, p1;
    ANGLE_T a;
    track_p t0, t1;
    EPINX_T ep0=-1, ep1=-1;
    wControl_p controls[4];
    char * labels[3];
    static DIST_T parRFactor;

    parType = (long)commandContext;

    switch (action&0xFF) {

    case C_START:
        if (parSepPLs[0].control==NULL) {
            ParamCreateControls(&parSepPG, NULL);
        }
        if (parType == PAR_TRACK) {
        	sprintf(message, "parallel-separation-%s", curScaleName);
        	parSeparation = ceil(13.0*12.0/curScaleRatio);
        } else {
        	sprintf(message, "parallel-line-separation-%s", curScaleName);
            parSeparation = 5.0*12.0/curScaleRatio;
        }
        wPrefGetFloat("misc", message, &parSeparation, parSeparation);
        ParamLoadControls(&parSepPG);
        ParamGroupRecord(&parSepPG);
        parSepPD.option |= PDO_NORECORD;
		parFactorPD.option |= PDO_NORECORD;
        controls[0] = parSepPD.control;
        if (parType == PAR_TRACK)
        	controls[1] = parFactorPD.control;
        else
        	controls[1] = NULL;
        controls[2] = NULL;
        labels[0] = N_("Separation");
        labels[1] = N_("Radius Factor");
        InfoSubstituteControls(controls, labels);
        parSepPD.option &= ~PDO_NORECORD;
        parFactorPD.option &= ~PDO_NORECORD;
        Dpa.anchor_Trk = NULL;
        tempSegs_da.cnt = 0;
        return C_CONTINUE;

    case wActionMove:
        tempSegs_da.cnt = 0;
        Dpa.anchor_Trk = NULL;
        if (parType == PAR_TRACK)
        	Dpa.anchor_Trk = OnTrack(&pos, FALSE, TRUE);
        else
        	Dpa.anchor_Trk = OnTrack(&pos, FALSE, FALSE);

        if (!Dpa.anchor_Trk) {
            return C_CONTINUE;
        }
        if (Dpa.anchor_Trk && !CheckTrackLayerSilent(Dpa.anchor_Trk)) {
        	Dpa.anchor_Trk = NULL;
            return C_CONTINUE;
        }
        if (!QueryTrack(Dpa.anchor_Trk, Q_CAN_PARALLEL)) {
        	Dpa.anchor_Trk = NULL;
            return C_CONTINUE;
        }
        break;
    case C_DOWN:
        Dpa.anchor_Trk = NULL;
        tempSegs_da.cnt = 0;
        if (parSeparation < 0.0) {
            ErrorMessage(MSG_PARALLEL_SEP_GTR_0);
            return C_ERROR;
        }

        controls[0] = parSepPD.control;
        controls[1] = parFactorPD.control;
        controls[2] = NULL;
        labels[0] = N_("Separation");
        labels[1] = N_("Radius factor");
        InfoSubstituteControls(controls, labels);
        ParamLoadData(&parSepPG);
        Dpa.orig = pos;
        if (parType == PAR_TRACK)
			Dpa.Trk = OnTrack(&pos, FALSE, TRUE);
		else
			Dpa.Trk = OnTrack(&pos, FALSE, FALSE);  //Also lines for line
        if (!Dpa.Trk) {
            return C_CONTINUE;
        }
        if (!QueryTrack(Dpa.Trk, Q_CAN_PARALLEL)) {
            Dpa.Trk = NULL;
            InfoMessage(_(" Track/Line doesn't support parallel"));
            wBeep();
            return C_CONTINUE;
        }

        parRFactor = (2864.0*(double)parSepFactor)/curScaleRatio;

        if ((parType == PAR_TRACK) && (parSeparation == 0.0)) {
            DIST_T orig_gauge = GetTrkGauge(Dpa.Trk);
            DIST_T new_gauge = GetScaleTrackGauge(GetLayoutCurScale());
            if (orig_gauge == new_gauge) {
                ErrorMessage(MSG_PARALLEL_SEP_GTR_0);
                return C_ERROR;
            }
            parSeparation = fabs(orig_gauge/2-new_gauge/2);
            parRFactor = 0.0;
        } else if (parType != PAR_TRACK)
        	parRFactor = 0.0;
        /* in case query has changed things (eg joint) */
        /*
         * this seems to cause problems so I commented it out
         * until further investigation shows the necessity
         */
        //Dpa.Trk = OnTrack( &Dpa.orig, TRUE, TRUE );
        tempSegs_da.cnt = 0;
        /* no break */

    case C_MOVE:
        if (Dpa.Trk == NULL) {
            return C_CONTINUE;
        }
        tempSegs_da.cnt = 0;
        if (!MakeParallelTrack(Dpa.Trk, pos, parSeparation, parRFactor, NULL, &p0, &p1,
                               parType == PAR_TRACK)) {
            Dpa.Trk = NULL;
            return C_CONTINUE;
        }
        return C_CONTINUE;

    case C_UP:
        Dpa.anchor_Trk = NULL;
        if (Dpa.Trk == NULL) {
            return C_CONTINUE;
        }
        t0=t1=NULL;
        if (parType == PAR_TRACK) {
			p = p0;
			tempSegs_da.cnt = 0;
			if ((t0=OnTrack(&p, FALSE, TRUE)) != NULL) {
				ep0 = PickEndPoint(p, t0);
				if (GetTrkEndTrk(t0,ep0) != NULL) {
					t0 = NULL;
				} else {
					p = GetTrkEndPos(t0, ep0);
					d = FindDistance(p, p0);
					if (d > connectDistance) {
						t0 = NULL;
					}
				}
			}
			p = p1;
			if ((t1=OnTrack(&p, FALSE, TRUE)) != NULL) {
				ep1 = PickEndPoint(p, t1);
				if (GetTrkEndTrk(t1,ep1) != NULL) {
					t1 = NULL;
				} else {
					p = GetTrkEndPos(t1, ep1);
					d = FindDistance(p, p1);
					if (d > connectDistance) {
						t1 = NULL;
					}
				}
			}
        }
        UndoStart(_("Create Parallel Track"), "newParallel");
        if (!MakeParallelTrack(Dpa.Trk, pos, parSeparation, parRFactor, &t, NULL, NULL,
                               parType == PAR_TRACK)) {
            tempSegs_da.cnt = 0;
            return C_TERMINATE;
        }
        if (parType == PAR_TRACK) {
        	if (GetTrkGauge(Dpa.Trk)> parSeparation) {
        		SetTrkNoTies(t, TRUE);
        	}
        //CopyAttributes( Dpa.Trk, t );    Don't force scale or track width or Layer
        	SetTrkBits(t,(GetTrkBits(t)&TB_HIDEDESC) | (GetTrkBits(Dpa.Trk)&~TB_HIDEDESC));

			if (t0) {
				a = NormalizeAngle(GetTrkEndAngle(t0, ep0) - GetTrkEndAngle(t,
								   0) + (180.0+connectAngle/2.0));
				if (a < connectAngle) {
					DrawEndPt(&mainD, t0, ep0, wDrawColorWhite);
					ConnectTracks(t0, ep0, t, 0);
					DrawEndPt(&mainD, t0, ep0, wDrawColorBlack);
				}
			}
			if (t1) {
				a = NormalizeAngle(GetTrkEndAngle(t1, ep1) - GetTrkEndAngle(t,
								   1) + (180.0+connectAngle/2.0));
				if (a < connectAngle) {
					DrawEndPt(&mainD, t1, ep1, wDrawColorWhite);
					ConnectTracks(t1, ep1, t, 1);
					DrawEndPt(&mainD, t1, ep1, wDrawColorBlack);
				}
			}
        }
        DrawNewTrack(t);
        UndoEnd();
        InfoSubstituteControls(NULL, NULL);
        if (parType == PAR_TRACK)
        	sprintf(message, "parallel-separation-%s", curScaleName);
        else
        	sprintf(message, "parallel-line-separation-%s", curScaleName);
        wPrefSetFloat("misc", message, parSeparation);
        tempSegs_da.cnt = 0;
        return C_TERMINATE;

    case C_REDRAW:
        if (Dpa.anchor_Trk) {
			DrawTrack(Dpa.anchor_Trk,&tempD,
				  wDrawColorPreviewSelected);    //Special color means THICK3 as well
        }
        if (tempSegs_da.cnt>0) {
			DrawSegs( &tempD, zero, 0.0, &tempSegs(0), tempSegs_da.cnt, trackGauge,
				  wDrawColorBlack );
        }
        return C_CONTINUE;

    case C_CANCEL:
        Dpa.anchor_Trk = NULL;
        tempSegs_da.cnt = 0;
        InfoSubstituteControls(NULL, NULL);
        return C_TERMINATE;

    }
    return C_CONTINUE;
}


#include "bitmaps/parallel.xpm"
#include "bitmaps/parallel-line.xpm"

EXPORT void InitCmdParallel( wMenu_p menu )
{
	ButtonGroupBegin( _("Parallel"), "cmdParallelSetCmd", _("Parallel") );
	AddMenuButton( menu, CmdParallel, "cmdParallelTrack", _("Parallel Track"), wIconCreatePixMap(parallel_xpm), LEVEL0_50, IC_STICKY|IC_POPUP|IC_WANT_MOVE, ACCL_PARALLEL, (void*)0 );
	AddMenuButton( menu, CmdParallel, "cmdParallelLine", _("Parallel Line"), wIconCreatePixMap(parallel_line_xpm), LEVEL0_50, IC_STICKY|IC_POPUP|IC_WANT_MOVE, ACCL_PARALLEL, (void*)1 );
	ButtonGroupEnd();
	ParamRegister( &parSepPG );
}
