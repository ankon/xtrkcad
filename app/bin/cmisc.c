/** \file cmisc.c
 * Handling of the 'Describe' dialog
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

#include <stdint.h>

#include "common.h"
#include "utility.h"
#include "cundo.h"
#include "i18n.h"
#include "messages.h"
#include "param.h"
#include "track.h"

EXPORT wIndex_t describeCmdInx;
EXPORT BOOL_T inDescribeCmd;

extern wIndex_t selectCmdInx;
extern wIndex_t joinCmdInx;
extern wIndex_t modifyCmdInx;

static track_p descTrk;
static descData_p descData;
static descUpdate_t descUpdateFunc;
static coOrd descOrig, descSize;
static POS_T descBorder;
static wDrawColor descColor = 0;
static BOOL_T descUndoStarted;
static BOOL_T descNeedDrawHilite;
static wPos_t describeW_posy;
static wPos_t describeCmdButtonEnd;

static wMenu_p descPopupM;

static unsigned int editableLayerList[NUM_LAYERS];		/**< list of non-frozen layers */
static int * layerValue;								/**pointer to current Layer (int *) */

static paramFloatRange_t rdata = { 0, 0, 100, PDO_NORANGECHECK_HIGH|PDO_NORANGECHECK_LOW };
static paramIntegerRange_t idata = { 0, 0, 100, PDO_NORANGECHECK_HIGH|PDO_NORANGECHECK_LOW };
static paramTextData_t tdata = { 300, 150 };
static char * pivotLabels[] = { N_("First"), N_("Middle"), N_("End"), NULL };
static char * boxLabels[] = { "", NULL };
static paramData_t describePLs[] = {
#define I_FLOAT_0		(0)
    { PD_FLOAT, NULL, "F1", 0, &rdata },
    { PD_FLOAT, NULL, "F2", 0, &rdata },
    { PD_FLOAT, NULL, "F3", 0, &rdata },
    { PD_FLOAT, NULL, "F4", 0, &rdata },
    { PD_FLOAT, NULL, "F5", 0, &rdata },
    { PD_FLOAT, NULL, "F6", 0, &rdata },
    { PD_FLOAT, NULL, "F7", 0, &rdata },
    { PD_FLOAT, NULL, "F8", 0, &rdata },
    { PD_FLOAT, NULL, "F9", 0, &rdata },
    { PD_FLOAT, NULL, "F10", 0, &rdata },
    { PD_FLOAT, NULL, "F11", 0, &rdata },
    { PD_FLOAT, NULL, "F12", 0, &rdata },
    { PD_FLOAT, NULL, "F13", 0, &rdata },
    { PD_FLOAT, NULL, "F14", 0, &rdata },
    { PD_FLOAT, NULL, "F15", 0, &rdata },
    { PD_FLOAT, NULL, "F16", 0, &rdata },
    { PD_FLOAT, NULL, "F17", 0, &rdata },
    { PD_FLOAT, NULL, "F18", 0, &rdata },
    { PD_FLOAT, NULL, "F19", 0, &rdata },
    { PD_FLOAT, NULL, "F20", 0, &rdata },
	{ PD_FLOAT, NULL, "F21", 0, &rdata },
    { PD_FLOAT, NULL, "F22", 0, &rdata },
	{ PD_FLOAT, NULL, "F23", 0, &rdata },
	{ PD_FLOAT, NULL, "F24", 0, &rdata },
	{ PD_FLOAT, NULL, "F25", 0, &rdata },
	{ PD_FLOAT, NULL, "F26", 0, &rdata },
	{ PD_FLOAT, NULL, "F27", 0, &rdata },
	{ PD_FLOAT, NULL, "F28", 0, &rdata },
	{ PD_FLOAT, NULL, "F29", 0, &rdata },
	{ PD_FLOAT, NULL, "F30", 0, &rdata },
	{ PD_FLOAT, NULL, "F31", 0, &rdata },
	{ PD_FLOAT, NULL, "F32", 0, &rdata },
    { PD_FLOAT, NULL, "F33", 0, &rdata },
	{ PD_FLOAT, NULL, "F34", 0, &rdata },
	{ PD_FLOAT, NULL, "F35", 0, &rdata },
	{ PD_FLOAT, NULL, "F36", 0, &rdata },
	{ PD_FLOAT, NULL, "F37", 0, &rdata },
	{ PD_FLOAT, NULL, "F38", 0, &rdata },
	{ PD_FLOAT, NULL, "F39", 0, &rdata },
	{ PD_FLOAT, NULL, "F40", 0, &rdata },
#define I_FLOAT_N		I_FLOAT_0+40

#define I_LONG_0		I_FLOAT_N
    { PD_LONG, NULL, "I1", 0, &idata },
    { PD_LONG, NULL, "I2", 0, &idata },
    { PD_LONG, NULL, "I3", 0, &idata },
    { PD_LONG, NULL, "I4", 0, &idata },
    { PD_LONG, NULL, "I5", 0, &idata },
#define I_LONG_N		I_LONG_0+5

#define I_STRING_0		I_LONG_N
    { PD_STRING, NULL, "S1", 0, (void*)300 },
    { PD_STRING, NULL, "S2", 0, (void*)300 },
    { PD_STRING, NULL, "S3", 0, (void*)300 },
    { PD_STRING, NULL, "S4", 0, (void*)300 },
#define I_STRING_N		I_STRING_0+4

#define I_LAYER_0		I_STRING_N
    { PD_DROPLIST, NULL, "Y1", 0, (void*)150, NULL, 0 },
#define I_LAYER_N		I_LAYER_0+1

#define I_COLOR_0		I_LAYER_N
    { PD_COLORLIST, NULL, "C1", PDO_NOPREF, NULL, N_("Color"), BC_HORZ|BC_NOBORDER },
#define I_COLOR_N		I_COLOR_0+1

#define I_LIST_0		I_COLOR_N
    { PD_DROPLIST, NULL, "L1", 0, (void*)150, NULL, 0 },
    { PD_DROPLIST, NULL, "L2", 0, (void*)150, NULL, 0 },
	{ PD_DROPLIST, NULL, "L3", 0, (void*)150, NULL, 0 },
	{ PD_DROPLIST, NULL, "L4", 0, (void*)150, NULL, 0 },
#define I_LIST_N		I_LIST_0+4

#define I_EDITLIST_0	I_LIST_N
    { PD_DROPLIST, NULL, "LE1", 0, (void*)150, NULL, BL_EDITABLE },
#define I_EDITLIST_N	I_EDITLIST_0+1

#define I_TEXT_0		I_EDITLIST_N
    { PD_TEXT, NULL, "T1", 0, &tdata, NULL, BT_HSCROLL },
#define I_TEXT_N		I_TEXT_0+1

#define I_PIVOT_0		I_TEXT_N
    { PD_RADIO, NULL, "P1", 0, pivotLabels, N_("Pivot"), BC_HORZ|BC_NOBORDER, 0 },
#define I_PIVOT_N		I_PIVOT_0+1

#define I_TOGGLE_0      I_PIVOT_N
    { PD_TOGGLE, NULL, "boxed1", PDO_NOPREF|PDO_DLGHORZ, boxLabels, N_("Boxed"), BC_HORZ|BC_NOBORDER },
	{ PD_TOGGLE, NULL, "boxed2", PDO_NOPREF|PDO_DLGHORZ, boxLabels, N_("Boxed"), BC_HORZ|BC_NOBORDER },
	{ PD_TOGGLE, NULL, "boxed3", PDO_NOPREF|PDO_DLGHORZ, boxLabels, N_("Boxed"), BC_HORZ|BC_NOBORDER },
	{ PD_TOGGLE, NULL, "boxed4", PDO_NOPREF|PDO_DLGHORZ, boxLabels, N_("Boxed"), BC_HORZ|BC_NOBORDER },
#define I_TOGGLE_N 		I_TOGGLE_0+4
};

static paramGroup_t describePG = { "describe", 0, describePLs, sizeof describePLs/sizeof describePLs[0] };

/**
 * A mapping table is used to map the index in the dropdown list to the layer
 * number. While usually this is a 1:1 translation, the values differ if there
 * are frozen layer. Frozen layers are not shown in the dropdown list.
 */

void
CreateEditableLayersList()
{
    int i = 0;
    int j = 0;

    while (i < NUM_LAYERS) {
        if (!GetLayerFrozen(i)) {
            editableLayerList[j++] = i;
        }

        i++;
    }
}

/**
 * Search a layer in the list of editable layers.
 *
 * \param IN layer layer to search
 * \return the index into the list
 */

static int
SearchEditableLayerList(unsigned int layer)
{
    int i;

    for (i = 0; i < NUM_LAYERS; i++) {
        if (editableLayerList[i] == layer) {
            return (i);
        }
    }

    return (-1);
}

static void DrawDescHilite(BOOL_T selected)
{
    wPos_t x, y, w, h;

    if (descNeedDrawHilite == FALSE) {
        return;
    }

    if (descColor==0) {
        descColor = wDrawColorGray(87);
    }

    w = (wPos_t)((descSize.x/mainD.scale)*mainD.dpi+0.5);
    h = (wPos_t)((descSize.y/mainD.scale)*mainD.dpi+0.5);
    mainD.CoOrd2Pix(&mainD,descOrig,&x,&y);
    wDrawFilledRectangle(tempD.d, x, y, w, h, selected?descColor:wDrawColorBlue, wDrawOptTemp|wDrawOptTransparent);
}



static void DescribeUpdate(
    paramGroup_p pg,
    int inx,
    void * data)
{
    coOrd hi, lo;
    descData_p ddp;

    if (inx < 0) {
        return;
    }

    ddp = (descData_p)pg->paramPtr[inx].context;

    if ((ddp->mode&(DESC_RO|DESC_IGNORE)) != 0) {
        return;
    }

    if (ddp->type == DESC_PIVOT) {
        return;
    }

    if (!descUndoStarted) {
        UndoStart(_("Change Track"), "Change Track");
        descUndoStarted = TRUE;
    }

    if (!descTrk) {
        return;    // In case timer pops after OK
    }

    UndoModify(descTrk);
    descUpdateFunc(descTrk, ddp-descData, descData, FALSE);

    if (descTrk) {
        GetBoundingBox(descTrk, &hi, &lo);

        if (OFF_D(mapD.orig, mapD.size, descOrig, descSize)) {
            ErrorMessage(MSG_MOVE_OUT_OF_BOUNDS);
        }
    }

    if ((ddp->mode&DESC_NOREDRAW) == 0) {
        descOrig = lo;
        descSize = hi;
        descOrig.x -= descBorder;
        descOrig.y -= descBorder;
        descSize.x -= descOrig.x-descBorder;
        descSize.y -= descOrig.y-descBorder;
    }

    for (inx = 0; inx < sizeof describePLs/sizeof describePLs[0]; inx++) {
        if ((describePLs[inx].option & PDO_DLGIGNORE) != 0) {
            continue;
        }

        ddp = (descData_p)describePLs[inx].context;

        if ((ddp->mode&DESC_IGNORE) != 0) {
            continue;
        }

        if ((ddp->mode&DESC_CHANGE) == 0) {
        	if ((ddp->mode&DESC_CHANGE2) == 0)
        		continue;
        }

        ddp->mode &= ~DESC_CHANGE;
        if (ddp->type == DESC_POS) {			//POS Has two fields
        	if (ddp->mode&DESC_CHANGE2) {
        		ddp->mode &= ~DESC_CHANGE2;		//Second time
        	} else {
        		ddp->mode |= DESC_CHANGE2;		//First time
        	}
        }
        ParamLoadControl(&describePG, inx);
    }
}


static void DescOk(void * junk)
{
    wHide(describePG.win);

    if (layerValue && *layerValue>=0) {
    	SetTrkLayer(descTrk, editableLayerList[*layerValue]);  //int found that is really in the parm controls.
    }
    layerValue = NULL; 									   // wipe out reference
    descUpdateFunc(descTrk, -1, descData, !descUndoStarted);
    descTrk = NULL;

    if (descUndoStarted) {
        UndoEnd();
        descUndoStarted = FALSE;
    }

    descNeedDrawHilite = FALSE;
    Reset(); // DescOk
}


static struct {
    parameterType pd_type;
    long option;
    int first;
    int last;
} descTypeMap[] = {
    /*NULL*/		{ 0, 0 },
    /*POS*/			{ PD_FLOAT, PDO_DIM,   I_FLOAT_0, I_FLOAT_N },
    /*FLOAT*/		{ PD_FLOAT, 0,		   I_FLOAT_0, I_FLOAT_N },
    /*ANGLE*/		{ PD_FLOAT, PDO_ANGLE, I_FLOAT_0, I_FLOAT_N },
    /*LONG*/		{ PD_LONG,	0,		   I_LONG_0, I_LONG_N },
    /*COLOR*/		{ PD_LONG,	0,		   I_COLOR_0, I_COLOR_N },
    /*DIM*/			{ PD_FLOAT, PDO_DIM,   I_FLOAT_0, I_FLOAT_N },
    /*PIVOT*/		{ PD_RADIO, 0,		   I_PIVOT_0, I_PIVOT_N },
    /*LAYER*/		{ PD_DROPLIST,PDO_LISTINDEX,	   I_LAYER_0, I_LAYER_N },
    /*STRING*/		{ PD_STRING,0,		   I_STRING_0, I_STRING_N },
    /*TEXT*/		{ PD_TEXT,	PDO_DLGNOLABELALIGN, I_TEXT_0, I_TEXT_N },
    /*LIST*/		{ PD_DROPLIST, PDO_LISTINDEX,	   I_LIST_0, I_LIST_N },
    /*EDITABLELIST*/{ PD_DROPLIST, 0,	   I_EDITLIST_0, I_EDITLIST_N },
	/*BOXED*/      	{ PD_TOGGLE, 0,	       I_TOGGLE_0, I_TOGGLE_N },
};

/**
 * An unused param element is selected from the list of pre-defined param elements and initialized
 * for an element specific param. 
 * 
 * \param ddp Element specific param
 * \param valueP the value pointer used by the element
 * \param label the label assigned by the element
 * \param sep ?
 * \return the selected widget
 */
 
static wControl_p AssignParamToDescribeDialog(descData_p ddp, void * valueP, char * label,
                               wPos_t sep)
{
    int inx;

    for (inx = descTypeMap[ddp->type].first; inx<descTypeMap[ddp->type].last;
            inx++) {
        if ((describePLs[inx].option & PDO_DLGIGNORE) != 0) {
            describePLs[inx].option = descTypeMap[ddp->type].option;

            if (describeW_posy > describeCmdButtonEnd) {
                describePLs[inx].option |= PDO_DLGUNDERCMDBUTT;
            }

            if (sep)
            	describeW_posy += wControlGetHeight(describePLs[inx].control) + sep;
            describePLs[inx].context = ddp;
            describePLs[inx].valueP = valueP;
            if ((ddp->type == DESC_STRING) && ddp->max_string) {
            	describePLs[inx].max_string = ddp->max_string;
            	describePLs[inx].option |= PDO_STRINGLIMITLENGTH;
            }

            if (label && ddp->type != DESC_TEXT) {
                wControlSetLabel(describePLs[inx].control, label);
                describePLs[inx].winLabel = label;
            } else {
            	wControlSetLabel(describePLs[inx].control, "");
            	describePLs[inx].winLabel = "";
            }

            return describePLs[inx].control;
        }
    }

    AbortProg("AssignParamToDescribeDialog: can't find %d", ddp->type);
    return NULL;
}


static void DescribeLayout(
    paramData_t * pd,
    int inx,
    wPos_t colX,
    wPos_t * x,
    wPos_t * y)
{
    descData_p ddp;
    wPos_t w, h;

    if (inx < 0) {
        return;
    }

    if (pd->context == NULL) {
        return;
    }

    ddp = (descData_p)pd->context;
    *y = ddp->posy;

    if (ddp->type == DESC_POS &&
            ddp->control0 != pd->control) {
        *x += wControlGetWidth(pd->control) + 3;
    } else if (ddp->type == DESC_TEXT) {
        w = tdata.width;
        h = tdata.height;
        wTextSetSize((wText_p)pd->control, w, h);
    }

    wControlShow(pd->control, TRUE);
}


/**
 * Creation and modification of the Describe dialog box is handled here. As the number
 * of values for a track element depends on the specific type, this dialog is dynamically
 * updated to hsow the changable parameters only
 *
 * \param IN title Description of the selected part, shown in window title bar
 * \param IN trk Track element to be described
 * \param IN data
 * \param IN update
 *
 */

static wList_p setLayerL;
void DoDescribe(char * title, track_p trk, descData_p data, descUpdate_t update)
{
    int inx;
    descData_p ddp;
    char * label;
    int ro_mode;

    if (!inDescribeCmd) {
        return;
    }

    CreateEditableLayersList();
    descTrk = trk;
    descData = data;
    descUpdateFunc = update;
    describeW_posy = 0;

    if (describePG.win == NULL) {
        /* SDB 5.13.2005 */
        ParamCreateDialog(&describePG, _("Description"), _("Done"), DescOk,
                          (paramActionCancelProc) DescribeCancel,
                          TRUE, DescribeLayout, F_RECALLPOS,
                          DescribeUpdate);
        describeCmdButtonEnd = wControlBelow((wControl_p)describePG.helpB);
    }

    for (inx=0; inx<sizeof describePLs/sizeof describePLs[0]; inx++) {
        describePLs[inx].option = PDO_DLGIGNORE;
        wControlShow(describePLs[inx].control, FALSE);
    }

    ro_mode = (GetLayerFrozen(GetTrkLayer(trk))?DESC_RO:0);

    if (ro_mode)
        for (ddp=data; ddp->type != DESC_NULL; ddp++) {
            if (ddp->mode&DESC_IGNORE) {
                continue;
            }

            ddp->mode |= ro_mode;
        }

    for (ddp=data; ddp->type != DESC_NULL; ddp++) {
        if (ddp->mode&DESC_IGNORE) {
            continue;
        }

        label = _(ddp->label);
        ddp->posy = describeW_posy;
        ddp->control0 = AssignParamToDescribeDialog(ddp, ddp->valueP, label,
                                     (ddp->type == DESC_POS?3:3));
        wControlActive(ddp->control0, ((ddp->mode|ro_mode)&DESC_RO)==0);

        switch (ddp->type) {
        case DESC_POS:
            ddp->control1 = AssignParamToDescribeDialog(ddp,
                                         &((coOrd*)(ddp->valueP))->y,
                                         NULL,
                                         0);
            wControlActive(ddp->control1, ((ddp->mode|ro_mode)&DESC_RO)==0);
            break;

        case DESC_LAYER:
            wListClear((wList_p)ddp->control0);  // Rebuild list on each invovation

            for (inx = 0; inx<NUM_LAYERS; inx++) {
                char *layerFormattedName;
                layerFormattedName = FormatLayerName(editableLayerList[inx]);
                wListAddValue((wList_p)ddp->control0, layerFormattedName, NULL, (void*)(long)inx);
                free(layerFormattedName);
            }

            *(int *)(ddp->valueP) = SearchEditableLayerList(*(int *)(ddp->valueP));
            layerValue = (int *)(ddp->valueP);
            break;

        default:
            break;
        }
    }

    ParamLayoutDialog(&describePG);
    ParamLoadControls(&describePG);
    sprintf(message, "%s (T%d)", title, GetTrkIndex(trk));
    wWinSetTitle(describePG.win, message);
    wShow(describePG.win);
}


static void DescChange(long changes)
{
    if ((changes&CHANGE_UNITS) && describePG.win && wWinIsVisible(describePG.win)) {
        ParamLoadControls(&describePG);
    }
}

/*****************************************************************************
 *
 * SIMPLE DESCRIPTION
 *
 */


EXPORT void DescribeCancel(void)
{
    if (describePG.win && wWinIsVisible(describePG.win)) {
        if (descTrk) {
        	if (!IsTrackDeleted(descTrk))
        		descUpdateFunc(descTrk, -1, descData, TRUE);
        	descTrk = NULL;

        }

        wHide(describePG.win);

        if (descUndoStarted) {
            UndoEnd();
            descUndoStarted = FALSE;
        }
    }

    descNeedDrawHilite = FALSE;
}


EXPORT STATUS_T CmdDescribe(wAction_t action, coOrd pos)
{
    static track_p trk;
    char msg[STR_SIZE];

    switch (action) {
    case C_START:
        InfoMessage(_("Select track to describe"));
        wSetCursor(mainD.d,wCursorQuestion);
        descUndoStarted = FALSE;
        trk = NULL;
        return C_CONTINUE;

    case wActionMove:
    	return C_CONTINUE;


    case C_DOWN:
        if ((trk = OnTrack(&pos, FALSE, FALSE)) != NULL) {
            if (describePG.win && wWinIsVisible(describePG.win) && descTrk) {
                descUpdateFunc(descTrk, -1, descData, TRUE);
                descTrk = NULL;
            }

            descBorder = mainD.scale*0.1;

            if (descBorder < trackGauge) {
                descBorder = trackGauge;
            }

            inDescribeCmd = TRUE;
            GetBoundingBox(trk, &descSize, &descOrig);
            descOrig.x -= descBorder;
            descOrig.y -= descBorder;
            descSize.x -= descOrig.x-descBorder;
            descSize.y -= descOrig.y-descBorder;
            descNeedDrawHilite = TRUE;
            DescribeTrack(trk, msg, 255);
            inDescribeCmd = FALSE;
            InfoMessage(msg);
        } else {
            InfoMessage("");
        }

        return C_CONTINUE;

    case C_REDRAW:

        if (describePG.win && wWinIsVisible(describePG.win) && descTrk) {
            DrawDescHilite(TRUE);
            if (descTrk && QueryTrack(descTrk, Q_IS_DRAW)) {
				DrawOriginAnchor(descTrk);
			}
        } else if (trk){
        	DrawTrack(trk,&tempD,wDrawColorBlue);
        }


        break;

    case C_CANCEL:
        DescribeCancel();
        wSetCursor(mainD.d,defaultCursor);
        return C_CONTINUE;

    case C_CMDMENU:
    	if (!trk) wMenuPopupShow(descPopupM);
    	return C_CONTINUE;
    }


    return C_CONTINUE;
}



#include "bitmaps/describe.xpm"

extern wIndex_t selectCmdInx;
extern wIndex_t modifyCmdInx;
extern wIndex_t panCmdInx;

void InitCmdDescribe(wMenu_p menu)
{
    describeCmdInx = AddMenuButton(menu, CmdDescribe, "cmdDescribe",
                                   _("Properties"), wIconCreatePixMap(describe_xpm),
                                   LEVEL0, IC_CANCEL|IC_POPUP|IC_WANT_MOVE|IC_CMDMENU, ACCL_DESCRIBE, NULL);
    RegisterChangeNotification(DescChange);
    ParamRegister(&describePG);
}
void InitCmdDescribe2(wMenu_p menu)
{
    descPopupM = MenuRegister( "Describe Context Menu" );
    wMenuPushCreate(descPopupM, "cmdSelectMode", GetBalloonHelpStr("cmdSelectMode"), 0, DoCommandB, (void*) (intptr_t) selectCmdInx);
    wMenuPushCreate(descPopupM, "cmdModifyMode", GetBalloonHelpStr("cmdModifyMode"), 0, DoCommandB, (void*) (intptr_t) modifyCmdInx);
    wMenuPushCreate(descPopupM, "cmdPanMode", GetBalloonHelpStr("cmdPanMode"), 0, DoCommandB, (void*) (intptr_t) panCmdInx);

}
