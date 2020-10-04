/* \file cprofile.c
 * Track profile
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
#include <stdbool.h>

#include "custom.h"
#include "cselect.h"
#include "cundo.h"
#include "i18n.h"
#include "layout.h"
#include "messages.h"
#include "param.h"
#include "shrtpath.h"
#include "track.h"


/*

   PROFILE COMMAND TEST CASE
(use 0testprof.xtc - 6 tracks connected 0:0:1 1:0:1 2:0:1 3:0:1 4:0:1 5:0:1 6:0:1)

		PreCond Action  PostCond

/ empty -> creating single pt
A1      -  -    10      10 -
A2      -  -    20      20 11
A3      -  -    11      11 20

/ single pt -> delete
B1      10 -    10      -  -
B2      20 11   20      -  -
B3      20 11   11      -  -

/ single pt at EOT - extend
C1      10 -    11      10 11 {1}
C2      10 -    20      10 11 {1}
C3      10 -    41      10 41 {1234}
C4      10 -    50      10 41 {1234}

/ single pt at mid track - extend
D1      31 40   11      31 20 {32}
D2      31 40   20      31 20 {32}
D3      31 40   51      40 51 {45}
D4      31 40   61      40 61 {456}
D5      31 40   10      31 10 {321}

/ length=2, delete end
E1      30 41   30      40 41 {4}
E2      30 41   21      40 41 {4}
E3      30 41   41      30 31 {3}
E4      30 41   50      30 31 {3}

/ length=1, delete end
F1      30 31   30      31 -
F2      30 31   21      31 -
F3      30 31   31      30 -
F4      30 31   40      30 -

/ length=1, extend
G1      30 31   11      20 31 {23}
G2      30 31   10      10 31 {123}
G3      30 31   51      30 51 {345}
G4      30 31   60      30 51 {345}
G5      30 31   61      30 61 {3456}

/ length=2, extend
H1      30 41   11      20 41 {234}
H2      30 41   10      10 41 {1234}
H3      30 41   51      30 51 {345}
H4      30 41   60      30 51 {345}
H5      30 41   61      30 61 {3456}
*/

/*****************************************************************************
 *
 * PROFILE WINDOW
 *
 */

static wDrawColor profileColorDefinedProfile;
static wDrawColor profileColorUndefinedProfile;
static wDrawColor profileColorFill;
static wFontSize_t screenProfileFontSize = 12;
static wFontSize_t printProfileFontSize = 6;
static BOOL_T printVert = TRUE;
static wMenu_p profilePopupM;
static track_p profilePopupTrk;
static EPINX_T profilePopupEp;
static wMenuToggle_p profilePopupToggles[3]; 

static int log_profile = 0; 

#define LABELH (labelH*fontSize/screenProfileFontSize)
#define LABELW (labelW*fontSize/screenProfileFontSize)
#define PBB(FS) (2.0*(labelH*(FS)/screenProfileFontSize+3.0/mainD.dpi))
#define PBT (10.0/mainD.dpi)
#define PBR(FS) (1.0*(labelW*(FS)/screenProfileFontSize+3.0/mainD.dpi))
#define PBL(FS) (1.0*(labelW*(FS)/screenProfileFontSize+3.0/mainD.dpi))
static FLOAT_T labelH;
static FLOAT_T labelW;	


track_p pathStartTrk; 
EPINX_T pathStartEp;
track_p pathEndTrk;
EPINX_T pathEndEp;

#define PASSERT( F, X, R ) if ( ! (X) ) { ErrorMessage( MSG_PASSERT, F, __LINE__, #X ); return R; }
#define NOP

typedef struct {
    track_p trk;
    EPINX_T ep;
    DIST_T elev;
    DIST_T dist;
    BOOL_T defined;			/* from prev PE to current */
} profElem_t, *profElem_p;

static dynArr_t profElem_da; 
static profElem_p copyOfprofElem;

#define profElem(N) DYNARR_N( profElem_t, profElem_da, N )  

typedef struct {
    DIST_T dist;
    char * name;
} station_t, *station_p;
static dynArr_t station_da; 

#define station(N) DYNARR_N( station_t, station_da, N )



struct {
    DIST_T totalD, minE;
    int minC, maxC, incrC;
    DIST_T scaleX, scaleY;
} prof;			


/**
 * Creates a copy of profile elements
 */

static void
CreateCopyProfileElements()
{
    if (copyOfprofElem) {
        MyFree(copyOfprofElem);
    }

    copyOfprofElem = MyMalloc(profElem_da.cnt * sizeof(profElem_t));
    if (!copyOfprofElem) {
        AbortProg("Couldn't allocate memory for profile copy\n");
    }
    for (int i = 0; i < profElem_da.cnt; i++) {
        copyOfprofElem[i] = profElem(i);
    }
}

/**
 * Destroys the copy of profile elements
 */

static void
DestroyCopyOfProfileElements()
{
    if (copyOfprofElem) {
        MyFree(copyOfprofElem);
        copyOfprofElem = NULL;
    }
}


/**
 * Draw profile
 *
 * \param  D		 The drawCmd_p to use.
 * \param  fontSize  Size of the font.
 * \param  printVert print vertical.
 */

static void DrawProfile(drawCmd_p D, wFontSize_t fontSize, BOOL_T printVert)
{
    coOrd pl, pt, pb;
    int inx;
    DIST_T grade;
    wFont_p fp;
    static dynArr_t points_da;
#define points(N) DYNARR_N( coOrd, points_da, N )
    wDrawWidth lw;
    station_p ps;
    coOrd textsize;

    lw = (wDrawWidth)(D->dpi*1.0/mainD.dpi);
    fp = wStandardFont(F_HELV, FALSE, FALSE);
    DYNARR_RESET(pts_t, points_da);

    pb.x = pt.x = 0;
    pb.y = prof.minE;
    pt.y = GetDim(prof.maxC);
    DrawLine(D, pb, pt, 0, snapGridColor);
    pb.x = pt.x = prof.totalD;
    DrawLine(D, pb, pt, 0, snapGridColor);
    pb.x = 0;
    pt.x = prof.totalD;

    // Draw horizontal grid and y scale
    for (inx=prof.minC; inx<=prof.maxC; inx+=prof.incrC) {
        coOrd textsize;
        // grid line
        pt.y = pb.y = GetDim(inx);
        DrawLine(D, pb, pt, 0, snapGridColor);
        // scale
        sprintf(message, "%d", inx);
        DrawTextSize(&mainD, message, wStandardFont(F_HELV, FALSE, FALSE),
                     screenProfileFontSize, FALSE, &textsize);
        pl.x = ((-3.0/mainD.dpi) - textsize.y*0.5 - textsize.x) / prof.scaleX*D->scale;
        pl.y = pb.y-LABELH/2/prof.scaleY*D->scale;

        DrawString(D, pl, 0.0, message, fp, fontSize*D->scale, borderColor);
    }

    // show the measurement units
    sprintf(message, "%s", units == UNITS_ENGLISH ? "in." : "cm");
    DrawTextSize(&mainD, message, wStandardFont(F_HELV, FALSE, FALSE),
                 screenProfileFontSize, FALSE, &textsize);
    pl.x = ((-3.0 / mainD.dpi) - textsize.y*0.5 - textsize.x) /
           prof.scaleX*D->scale;
    pl.y += LABELH * 1.5  / prof.scaleY*D->scale;
    DrawString(D, pl, 0.0, message, fp, fontSize*D->scale, borderColor);

    if (profElem_da.cnt <= 0) {
        return;
    }

    for (inx=0; inx<profElem_da.cnt; inx++) {
        pt.y = profElem(inx).elev;
        pt.x = profElem(inx).dist;
        DYNARR_APPEND(pts_t, points_da, 10);
        points(points_da.cnt-1) = pt;
    }
    pb.y = pt.y = prof.minE;
    if (points_da.cnt > 1) {
        DYNARR_APPEND(coOrd, points_da, 10);
        pt.x = prof.totalD;
        points(points_da.cnt-1) = pt;
        DYNARR_APPEND(pts_t, points_da, 10);
        pb.x = 0;
        points(points_da.cnt-1) = pb;
        DrawPoly(D, points_da.cnt, points_da.ptr, NULL, profileColorFill, 1, 1, 0);
    }

    pt.y = prof.minE-(2*LABELH+3.0/mainD.dpi)/prof.scaleY*D->scale;
    for (inx=0; inx<station_da.cnt; inx++) {
        ps = &station(inx);
        DrawTextSize(&mainD, ps->name, fp, fontSize, FALSE, &textsize);
        pt.x = ps->dist - textsize.x/2.0/prof.scaleX*D->scale;
        if (pt.x < -PBR(screenProfileFontSize)) {
            pt.x = -(PBR(screenProfileFontSize)-3/mainD.dpi)/prof.scaleX*D->scale;
        } else if (pt.x+textsize.x > prof.totalD) {
            pt.x = prof.totalD-(textsize.x-3/mainD.dpi)/prof.scaleX*D->scale;
        }
        DrawString(D, pt, 0.0, ps->name, fp, fontSize*D->scale, borderColor);
    }

    pb.x = 0.0;
    pb.y = prof.minE;

	// mark the starting point for the profile
    pt = points(0);
    DrawLine(D, pb, pt, lw, snapGridColor);
	DrawArc(D, pt, 0.05, 0, 360, TRUE, 2, wDrawColorGrey40);
    if (units==UNITS_ENGLISH) {
        sprintf(message, "%0.1f", PutDim(profElem(0).elev)+0.05);
    } else {
        sprintf(message, "%0.1f", PutDim(profElem(0).elev)+0.05);
    }
    if (printVert) {
        pl.x = pt.x + LABELH/2.0/prof.scaleX*D->scale;
        pl.y = pt.y + 2.0/mainD.dpi/prof.scaleY*D->scale + GetDim(prof.incrC) / 16;;
        DrawString(D, pl, 270.0, message, fp, fontSize*D->scale, borderColor);
    } else {
        pl.x = pt.x+2.0/mainD.dpi/prof.scaleX*D->scale + GetDim(prof.incrC) / 16;;
        pl.y = pt.y;
        if (profElem_da.cnt>1 && profElem(0).elev < profElem(1).elev) {
            pl.y -= LABELH/prof.scaleY*D->scale;
        }
        DrawString(D, pl, 0.0, message, fp, fontSize*D->scale, borderColor);
    }
    pl = pt;

    for (inx=1; inx<profElem_da.cnt; inx++) {
        pt.y = profElem(inx).elev;
        pb.x = pt.x = profElem(inx).dist;
        pt = points(inx);

		// draw line to x-axis for intermediate elevation points
		if (inx != profElem_da.cnt - 1) {
			unsigned long oldOptions = D->options;

			D->options = D->options | DC_DOT;
			DrawLine(D, pb, pt, lw, borderColor);
			D->options = oldOptions;
		}

		// draw grade line
		DrawLine(D, pl, pt, lw*2, (profElem(inx).defined ? profileColorDefinedProfile :
			profileColorUndefinedProfile));
	    // draw the markers   
        DrawArc(D, pt, 0.05, 0, 360, TRUE, 2, wDrawColorGrey40);
    
    if (profElem(inx).dist > 0.1) {
            grade = fabs(profElem(inx).elev-profElem(inx-1).elev)/
                    (profElem(inx).dist-profElem(inx-1).dist);
            sprintf(message, "%0.1f%%", round(grade*1000.0)/10.0);
            DrawTextSize(&mainD, message, fp, fontSize, FALSE, &textsize);
            pl.x = (points(inx).x+points(inx-1).x)/2.0;
            pl.y = (points(inx).y+points(inx-1).y)/2.0;
            if (printVert) {
                pl.x += (LABELH/2)/prof.scaleX*D->scale;
                pl.y += ((LABELH/2)*grade/prof.scaleX + 2.0/mainD.dpi/prof.scaleY)*D->scale;
                DrawString(D, pl, 270.0, message, fp, fontSize*D->scale, borderColor);
            } else {
                pl.x -= (textsize.x/2)/prof.scaleX*D->scale;
                pl.y += (textsize.x/2)*grade/prof.scaleX*D->scale;
                DrawString(D, pl, 0.0, message, fp, fontSize*D->scale, borderColor);
            }
        }
        if (units==UNITS_ENGLISH) {
            if (prof.totalD > 240) {
                sprintf(message, "%0.1f'", (round((profElem(inx).dist/12.0)*10.0)/10.0));
            } else {
                sprintf(message, "%d'%0.1f\"", (int)floor((profElem(inx).dist)/12.0),
                        round(fmod(profElem(inx).dist,12.0)*10.0)/10.0);
            }
        } else {
            if (PutDim(prof.totalD) > 10000) {
                sprintf(message, "%0.1fm", (round(PutDim(profElem(inx).dist)/10.0)/10.0));
            } else if (PutDim(prof.totalD) > 100) {
                sprintf(message, "%0.2fm", (round(PutDim(profElem(inx).dist))/100.0));
            } else {
                sprintf(message, "%0.1fcm", round(PutDim(profElem(inx).dist)+0.5));
            }
        }
        DrawTextSize(&mainD, message, fp, fontSize, FALSE, &textsize);
        pl.x = pb.x-(textsize.x/2)/prof.scaleX*D->scale;
        pl.y = prof.minE-(LABELH+3.0/mainD.dpi)/prof.scaleY*D->scale;
        DrawString(D, pl, 0.0, message, fp, fontSize*D->scale, borderColor);
        sprintf(message, "%0.1f", round(PutDim(profElem(inx).elev)*100.0)/100.0);
        if (printVert) {
            pl.x = pt.x + LABELH/2.0/prof.scaleX*D->scale;
            pl.y = pt.y + 2.0/mainD.dpi/prof.scaleY*D->scale+GetDim(prof.incrC) / 16;
            DrawString(D, pl, 270.0, message, fp, fontSize*D->scale, borderColor);
        } else {
            pl.x = pt.x + 2.0/mainD.dpi/prof.scaleX*D->scale + GetDim(prof.incrC) / 16;
            pl.y = pt.y;
            if (inx != profElem_da.cnt-1 && profElem(inx).elev < profElem(inx+1).elev) {
                pl.y -= LABELH/prof.scaleY*D->scale;
            }
            DrawString(D, pl, 0.0, message, fp, fontSize*D->scale, borderColor);
        }
        pl = pt;
    }
}



static void ProfilePix2CoOrd(drawCmd_p, wPos_t, wPos_t, coOrd *);
static void ProfileCoOrd2Pix(drawCmd_p, coOrd, wPos_t*, wPos_t*);
static drawCmd_t screenProfileD = {
    NULL,
    &screenDrawFuncs,
	DC_NOCLIP,
    1.0,
    0.0,
    {0.0,0.0}, {0.0,0.0},
    ProfilePix2CoOrd, ProfileCoOrd2Pix
};

static void ProfilePix2CoOrd(
    drawCmd_p d,
    wPos_t xx,
    wPos_t yy,
    coOrd * pos)
{
    pos->x = (xx/d->dpi+d->orig.x)/prof.scaleX;
    pos->y = (yy/d->dpi+d->orig.y)/prof.scaleY+prof.minE;
}


static void ProfileCoOrd2Pix(
    drawCmd_p d,
    coOrd pos,
    wPos_t *xx,
    wPos_t *yy)
{
    wPos_t x, y;
    x = (wPos_t)((((pos.x*prof.scaleX)/d->scale-d->orig.x)*d->dpi+0.5));
    y = (wPos_t)(((((pos.y-prof.minE)*prof.scaleY)/d->scale-d->orig.y)*d->dpi+0.5));
    if (d->angle == 0) {
        *xx = x;
        *yy = y;
    } else if (d->angle == -90.0) {
        /* L->P */
        *xx = y;
        *yy = -x;
    } else {
        /* P->L */
        *xx = -y;
        *yy = x;
    }
}

/** 
 * Redraw profile window 
 */

static void RedrawProfileW(void)
{
    wPos_t ww, hh;
    coOrd size;
    int divC;
    DIST_T maxE, rngE;
    profElem_t *p;
    wFont_p fp;
    POS_T w;
    coOrd textsize;
    char *pTestString;

    wDrawDelayUpdate(screenProfileD.d, TRUE); 
	wDrawClear(screenProfileD.d);

	// get the size of the window area in pixels and convert to inches
    wDrawGetSize(screenProfileD.d, &ww, &hh);
    screenProfileD.size.x = (ww)/screenProfileD.dpi;
    screenProfileD.size.y = (hh)/screenProfileD.dpi;

	// calculate positions for labels???
	fp = wStandardFont(F_HELV, FALSE, FALSE);
	screenProfileD.orig.x = -PBL(screenProfileFontSize);
    screenProfileD.orig.y = -PBB(screenProfileFontSize);

    /* Calculate usable dimension of canvas in inches */
    size = screenProfileD.size;
    size.x -= (PBL(screenProfileFontSize));
    size.y -= (PBB(screenProfileFontSize));

    /* make sure there is enough space to show the rightmost coordinate value*/
    if (units == UNITS_ENGLISH) {
        if (prof.totalD > 240.0) {
            pTestString = "9999'";
        } else {
            pTestString = "999'11\"";
        }
    } else {
        if (PutDim(prof.totalD) > 10000.0) {
            pTestString = "999m";
        } else {
            if (PutDim(prof.totalD) > 100.0) {
                pTestString = "99.9m";
            } else {
                pTestString = "9.99m";
            }
        }
    }
    DrawTextSize(&mainD, pTestString, fp, screenProfileFontSize, FALSE, &textsize);
    size.x -= textsize.x / 2;
    size.y -= textsize.y * 1.5 ;


	// now we have the size of the profile area
#ifdef WINDOWS
    if (printVert) {
        size.x -= PBR(screenProfileFontSize)/4.0;
        size.y -= PBT;
    } else
#endif
    {
        size.x -= PBR(screenProfileFontSize);
        size.y -= PBT;
    }

    if (size.x < 0.1 || size.y < 0.1) {
        wDrawDelayUpdate(screenProfileD.d, FALSE);
        return;
    }

    /* Calculate range of data values */
    if (profElem_da.cnt<=0) {
        prof.totalD = 0.0;
        prof.minE = 0.0;
        maxE = 1.0;
    } else {
        maxE = prof.minE = profElem(0).elev;
        prof.totalD = profElem(profElem_da.cnt-1).dist;
        for (int inx=1; inx<profElem_da.cnt; inx++) {
            p = &profElem(inx);
            if (p->elev<prof.minE) {
                prof.minE = p->elev;
            }
            if (p->elev>maxE) {
                maxE = p->elev;
            }
        }
    }

    /* Calculate number of grid lines */
    prof.minC = (int)floor(PutDim(prof.minE));
    prof.maxC = (int)ceil(PutDim(maxE));
    if (prof.maxC-prof.minC <= 0) {
        prof.maxC = prof.minC+1;
    }
    divC = (int)floor(size.y/labelH);
    if (divC < 1) {
        divC = 1;
    }
    prof.incrC = (prof.maxC-prof.minC+divC-1)/divC;
    if (prof.incrC < 1) {
        prof.incrC = 1;
    }
    prof.maxC = prof.minC + (prof.maxC-prof.minC+prof.incrC-1)/prof.incrC *
                prof.incrC;

    /* Reset bounds based on intergal values */
    prof.minE = GetDim(prof.minC);
    rngE = GetDim(prof.maxC) - prof.minE;
    if (rngE < 1.0) {
        rngE = 1.0;
    }

    /* Compute vert scale */
    prof.scaleY = size.y/rngE;
    sprintf(message, "%0.2f", maxE);

    DrawTextSize(&mainD, message, fp, screenProfileFontSize, FALSE, &textsize);
    w = textsize.x;
    w -= PBT;
    w += 4.0/screenProfileD.dpi;
    w -= (GetDim(prof.maxC)-maxE)*prof.scaleY;
    if (w > 0) {
        size.y -= w;
        prof.scaleY = size.y/rngE;
    }

    /* Compute horz scale */
    if (prof.totalD <= 0.1) {
        prof.totalD = size.x;
    }
    prof.scaleX = size.x/prof.totalD;

    DrawProfile(&screenProfileD, screenProfileFontSize,
#ifdef WINDOWS
                printVert
#else
                FALSE
#endif
               );
    wDrawDelayUpdate(screenProfileD.d, FALSE);
}



static drawCmd_t printProfileD = {
    NULL,
    &printDrawFuncs,
	DC_PRINT | DC_NOCLIP,
    1.0,
    0.0,
    {0.0,0.0}, {1.0,1.0},
    ProfilePix2CoOrd, ProfileCoOrd2Pix
};

/**
 * This is the print function for the track height profile. The paper
 * orientation is based in on the orientation of the display windows.
 * Eg. is the windows is wider than high, the printout will be in
 * landscape.
 * \todo Rework the layout of the printout
 *
 * \param junk IN
 * \return
 */

static void DoProfilePrint(void * junk)
{
    coOrd size, p[4];
    int copies;
    WDOUBLE_T w, h, screenRatio, printRatio, titleH;
    wFont_p fp;
    coOrd screenSize;
    coOrd textsize;

    if (!wPrintDocStart(_("Profile"), 1, &copies)) {
        return;
    }
    printProfileD.d = wPrintPageStart();
    if (printProfileD.d == NULL) {
        return;
    }
    printProfileD.dpi = wDrawGetDPI(printProfileD.d);
    wPrintGetPageSize(&w, &h);
    printProfileD.orig.x = -PBL(printProfileFontSize);
    printProfileD.orig.y = -PBB(printProfileFontSize);
    printProfileD.angle = 0.0;
    screenRatio = screenProfileD.size.y/screenProfileD.size.x;
    screenSize.x = prof.totalD*prof.scaleX;
    screenSize.y = GetDim(prof.maxC-prof.minC)*prof.scaleY;
    screenRatio = screenSize.y/screenSize.x;
    printProfileD.size.x = w;
    printProfileD.size.y = h;
    sprintf(message, _("%s Profile: %s"), sProdName, GetLayoutTitle());
    fp = wStandardFont(F_TIMES, FALSE, FALSE);
    DrawTextSize(&mainD, message, fp, 24, FALSE, &textsize);
    titleH = textsize.y + 6.0/mainD.dpi;
    if (screenRatio < 1.0 && w < h) {
        /* Landscape -> Portrait */
        printProfileD.angle = -90.0;
        printProfileD.orig.x += h;
        size.x = h;
        size.y = w;
    } else if (screenRatio > 1.0 && w > h) {
        /* Portrait -> Landscape */
        printProfileD.angle = 90.0;
        printProfileD.orig.y += w;
        size.x = h;
        size.y = w;
    } else {
        size.x = w;
        size.y = h;
    }
    size.y -= titleH+(printVert?PBT*2:PBT)+PBB(printProfileFontSize);
    size.x -= 4.0/mainD.dpi+PBL(printProfileFontSize)+(printVert?PBR(
                  printProfileFontSize)/4.0:PBR(printProfileFontSize));
    printRatio = size.y/size.x;
    if (printRatio < screenRatio) {
        printProfileD.scale = screenSize.y/size.y;
        size.x = screenSize.x/printProfileD.scale;
    } else {
        printProfileD.scale = screenSize.x/size.x;
        printProfileD.orig.y -= size.y;
        size.y = screenSize.y/printProfileD.scale;
        printProfileD.orig.y += size.y;
    }
#define PRINT_ABS2PAGEX(X) (((X)*printProfileD.scale)/prof.scaleX)
#define PRINT_ABS2PAGEY(Y) (((Y)*printProfileD.scale)/prof.scaleY+prof.minE)
    p[0].y = PRINT_ABS2PAGEY(size.y+(printVert?PBT*2:PBT)+0.05);
    p[0].x = PRINT_ABS2PAGEX((size.x-textsize.x)/2.0);
    if (p[0].x < 0) {
        p[0].x = 0;
    }
    DrawString(&printProfileD, p[0], 0, message, fp, 24*printProfileD.scale,
               borderColor);
    p[0].x = p[3].x = PRINT_ABS2PAGEX((-PBL(printProfileFontSize))+2.0/mainD.dpi);
    p[0].y = p[1].y = PRINT_ABS2PAGEY(-PBB(printProfileFontSize));
    p[1].x = p[2].x = PRINT_ABS2PAGEX(size.x+(printVert?PBR(
                                          printProfileFontSize)/4.0:PBR(printProfileFontSize)));
    p[2].y = p[3].y = PRINT_ABS2PAGEY(size.y+(printVert?PBT*2:PBT));
    DrawLine(&printProfileD, p[0], p[1], 0, drawColorBlack);
    DrawLine(&printProfileD, p[1], p[2], 0, drawColorBlack);
    DrawLine(&printProfileD, p[2], p[3], 0, drawColorBlack);
    DrawLine(&printProfileD, p[3], p[0], 0, drawColorBlack);

    DrawProfile(&printProfileD, printProfileFontSize, printVert);
    wPrintPageEnd(printProfileD.d);
    wPrintDocEnd();
}

/**************************************************************************
 *
 *  Window Handlers
 *
 **************************************************************************/

static wWin_p profileW;

static BOOL_T profileUndo = FALSE;
static void DoProfileChange(void *junk);
static void DoProfileReset(void *junk);
static void DoProfileDone(void *);
static void DoProfileClear(void *);
static void DoProfilePrint(void *);
static void DoProfileChangeMode(void *);
static void SelProfileW(wIndex_t, coOrd);
static void CloseProfileWindow(paramGroup_p pg, int event, void *data);

static paramDrawData_t profileDrawData = { 300, 150, (wDrawRedrawCallBack_p)RedrawProfileW, SelProfileW, &screenProfileD };
static paramData_t profilePLs[] = {
    {	PD_DRAW, NULL, "canvas", PDO_DLGRESIZE, &profileDrawData },
#define I_PROFILEMSG			(1)
    {	PD_MESSAGE, NULL, NULL, PDO_DLGIGNOREX, (void*)300 },
#define I_CHANGEBUTTON 2
    {	PD_BUTTON, (void*)DoProfileChange, "change", PDO_DLGCMDBUTTON, NULL, N_("Change") },
#define I_RESETBUTTON 3
    {	PD_BUTTON, (void*)DoProfileReset, "reset", PDO_DLGCMDBUTTON, NULL, N_("Reset") },
#define I_CLEARBUTTON 4
    {	PD_BUTTON, (void*)DoProfileClear, "clear", PDO_DLGCMDBUTTON, NULL, N_("Clear") },
#define I_PRINTBUTTON 5
    {	PD_BUTTON, (void*)DoProfilePrint, "print", 0, NULL, N_("Print") }
};
static paramGroup_t profilePG = { "profile", PGO_DIALOGTEMPLATE, profilePLs, sizeof profilePLs/sizeof profilePLs[0] };

#define CHANGEBUTTON  ((wButton_p)profilePLs[I_CHANGEBUTTON].control)
#define RESETBUTTON  ((wButton_p)profilePLs[I_RESETBUTTON].control)
#define CLEARBUTTON  ((wButton_p)profilePLs[I_CLEARBUTTON].control)
#define PRINTBUTTON  ((wButton_p)profilePLs[I_PRINTBUTTON].control)

static void SelProfileW(
    wIndex_t action,
    coOrd pos)
{
    DIST_T dist;
    static DIST_T oldElev;
    static int inx;
    DIST_T elev;

    if (profElem_da.cnt <= 0) {
        return;
    }

    dist = pos.x;
    elev = pos.y;

    switch (action&0xFF) {
    case C_DOWN:
        for (inx=0; inx<profElem_da.cnt; inx++) {
            if (dist <= profElem(inx).dist) {
                if (inx!=0 && profElem(inx).dist-dist > dist-profElem(inx-1).dist) {
                    inx--;
                }
                break;
            }
        }
        if (inx >= profElem_da.cnt) {
            inx = profElem_da.cnt-1;
        }
        sprintf(message, _("Elev = %0.1f"), round(PutDim(elev)*10.0)/10.0);
        ParamLoadMessage(&profilePG, I_PROFILEMSG, message);
        oldElev = elev;
        RedrawProfileW();
        break;
    case C_MOVE:
        if (inx < 0) {
            break;
        }
        if (profElem_da.cnt == 1) {
            sprintf(message, _("Elev = %0.1f"), round(PutDim(elev)*10.0)/10.0);
        } else if (inx == 0) {
            sprintf(message, _("Elev=%0.2f %0.1f%%"),
                    round(PutDim(elev)*100.0)/100.0,
                    round(fabs(((profElem(inx+1).elev-elev) / (profElem(inx+1).dist-profElem(
                                    inx).dist)) * 1000.0))/10.0);
        } else if (inx == profElem_da.cnt-1) {
            sprintf(message, _("%0.1f%% Elev = %0.2f"),
                    round(fabs(((profElem(inx-1).elev-elev) / (profElem(inx).dist-profElem(
                                    inx-1).dist)) * 1000.0))/10.0,
                    round(PutDim(elev)*100.0)/100.0);
        } else {
            sprintf(message, _("%0.1f%% Elev = %0.2f %0.1f%%"),
                    round(fabs(((profElem(inx-1).elev-elev) / (profElem(inx).dist-profElem(
                                    inx-1).dist)) * 1000.0))/10.0,
                    round(PutDim(elev)*100.0)/100.0,
                    round(fabs((profElem(inx+1).elev-elev) / (profElem(inx+1).dist-profElem(
                                   inx).dist)) * 1000.0)/10.0);
        }
        ParamLoadMessage(&profilePG, I_PROFILEMSG, message);
        oldElev = elev;
        profElem(inx).elev = oldElev;
        RedrawProfileW();
		wPause(500l);
		break;
    case C_UP:
        if (profileUndo == FALSE) {
            UndoStart(_("Profile Command"), "Profile - set elevation");
            profileUndo = TRUE;
        }
        if (profElem(inx).trk) {
            UpdateTrkEndElev(profElem(inx).trk, profElem(inx).ep, ELEV_DEF|ELEV_VISIBLE,
                             oldElev, NULL);
        }
        profElem(inx).elev = oldElev;
        RedrawProfileW();
        ParamLoadMessage(&profilePG, I_PROFILEMSG, _("Drag to change Elevation"));
        inx = -1;
        break;
    default:
        break;
    }
}

static void HilightProfileElevations(BOOL_T show)
{
    /*if ( profElem_da.cnt <= 0 ) {*/
    HilightElevations(show);
    /*} else {
    }*/
}

/**
 *
 *
 * \param 		   pg    The page.
 * \param 		   event The event.
 * \param [in,out] data  If non-null, the data.
 */
void
CloseProfileWindow(paramGroup_p pg, int event, void *data)
{
    Reset();
    return;
}


/** 
 * Undo the changes made in the profile window to the layout.
 */
 
static void
ResetChanges()
{
	if (copyOfprofElem) {
		for (int i = 0; i < profElem_da.cnt; i++) {
			profElem(i) = copyOfprofElem[i];
			if (profElem(i).trk) {
				UpdateTrkEndElev(profElem(i).trk, profElem(i).ep, ELEV_DEF | ELEV_VISIBLE,
					copyOfprofElem[i].elev, NULL);
			}
		}
	}
}

/**
 * Executes the profile reset operation. All elevations are copied from the
 * backup, the main drawing area and the profile window are updated
 *
 * \param [in,out] junk
 */

static void
DoProfileReset(void *junk)
{
	if (profileUndo == 0) {
		profileUndo = TRUE;
		UndoStart(_("Profile Command"), "Profile");
	}
	ResetChanges();
	RedrawProfileW();
    TempRedraw();
}

/**
 * Confirm the changes made in the profile window
 *
 * \param [in,out] junk If non-null, the junk.
 */

static void
DoProfileChange(void *junk)
{
	DestroyCopyOfProfileElements();
    TempRedraw();
}


static void DoProfileDone(void * junk)
{
    Reset();
}


static void DoProfileClear(void * junk)
{
	ResetChanges();
    profElem_da.cnt = 0;
    station_da.cnt = 0;
    ClrAllTrkBitsRedraw(TB_PROFILEPATH, TRUE);
    pathStartTrk = pathEndTrk = NULL;
    RedrawProfileW();
}


static void DoProfileChangeMode(void * junk)
{
    if (profElem_da.cnt<=0) {
        InfoMessage(_("Select a Defined Elevation to start Profile"));
    } else {
        InfoMessage(_("Select a Defined Elevation to extend Profile"));
    }
}

/**************************************************************************
 *
 *  Find Shortest Path
 *
 **************************************************************************/

static BOOL_T PathListEmpty(void)
{
    return pathStartTrk == NULL;
}

static BOOL_T PathListSingle(void)
{
    return pathStartTrk != NULL &&
           (pathEndTrk == NULL ||
            (GetTrkEndTrk(pathEndTrk,pathEndEp) == pathStartTrk &&
             GetTrkEndTrk(pathStartTrk,pathStartEp) == pathEndTrk));
}


static int profileShortestPathMatch;
static DIST_T profileShortestPathDist;

static int ProfileShortestPathFunc(
    SPTF_CMD cmd,
    track_p trk,
    EPINX_T ep,
    EPINX_T ep0,
    DIST_T dist,
    void * data)
{
    int rc0=0;
    int pathMatch;

    switch (cmd) {
        track_p trkN;
    case SPTC_TERMINATE:
        rc0 = 1;
        break;

    case SPTC_MATCH:
        if (EndPtIsIgnoredElev(trk,ep)) {
            break;
        }
        if (PathListSingle()) {
            if (trk == pathStartTrk && ep == pathStartEp) {
                pathMatch = 2;
            } else if (trk == pathEndTrk && ep == pathEndEp) {
                pathMatch = 3;
            } else {
                break;
            }
        } else if ((trkN = GetTrkEndTrk(trk,ep)) == NULL) {
            break;
        } else {
            EPINX_T epN;
            epN = GetEndPtConnectedToMe(trkN, trk);
            if (trkN == pathStartTrk && epN == pathStartEp) {
                pathMatch = 1;
            } else if (trkN == pathEndTrk && epN == pathEndEp) {
                pathMatch = 2;
            } else if (trkN == pathStartTrk && trkN == pathEndTrk) {
                pathMatch = 2;
            } else if (trkN == pathStartTrk) {
                pathMatch = 1;
            } else if (trkN == pathEndTrk) {
                pathMatch = 2;
            } else {
                break;
            }
        }
        if (profileShortestPathMatch < 0 || profileShortestPathDist > dist) {
            LOG(log_shortPath, 4, (" Match=%d", pathMatch))
            profileShortestPathMatch = pathMatch;
            profileShortestPathDist = dist;
        }
        rc0 = 1;
        break;

    case SPTC_MATCHANY:
        rc0 = -1;
        break;

    case SPTC_IGNNXTTRK:
        if (EndPtIsIgnoredElev(trk,ep)) {
            rc0 = 1;
        } else if ((GetTrkBits(trk)&TB_PROFILEPATH)!=0) {
            rc0 = 1;
        } else if ((!EndPtIsDefinedElev(trk,ep)) && GetTrkEndTrk(trk,ep)==NULL) {
            rc0 = 1;
        } else {
            rc0 = 0;
        }
        break;

    case SPTC_ADD_TRK:
        if (log_shortPath<=0||
                logTable(log_shortPath).level<4) LOG(log_profile, 4, ("    ADD_TRK T%d:%d",
                            GetTrkIndex(trk), ep))
            SetTrkBits(trk, TB_PROFILEPATH);
        DrawTrack(trk, &mainD, profilePathColor);
        rc0 = 0;
        break;

    case SPTC_VALID:
        rc0 = 1;
        break;

    default:
        break;
    }
    return rc0;
}

static int FindProfileShortestPath(
    track_p trkN,
    EPINX_T epN)
{
    LOG(log_profile, 4, ("Searching from T%d:%d to T%d:%d or T%d:%d\n",
                         GetTrkIndex(trkN), epN,
                         pathStartTrk?GetTrkIndex(pathStartTrk):-1, pathStartTrk?pathStartEp:-1,
                         pathEndTrk?GetTrkIndex(pathEndTrk):-1, pathEndTrk?pathEndEp:-1))
    profileShortestPathMatch = -1;
    return FindShortestPath(trkN, epN, TRUE, ProfileShortestPathFunc, NULL);
}


/**************************************************************************
 *
 *  Main Window Handler
 *
 **************************************************************************/


#define ONPATH_NOT		(1<<0)
#define ONPATH_END		(1<<1)
#define ONPATH_MID		(1<<2)
#define ONPATH_BRANCH	(1<<3)
static int OnPath(track_p trk, EPINX_T ep)
{
    if (GetTrkBits(trk)&TB_PROFILEPATH) {
        track_p trk0;
        trk0 = GetTrkEndTrk(profilePopupTrk, profilePopupEp);
        if (trk0 && (GetTrkBits(trk0)&TB_PROFILEPATH)) {
            return ONPATH_MID;
        }
        if (trk == pathStartTrk && ep == pathStartEp) {
            return ONPATH_END;
        }
        return ONPATH_BRANCH;
    }
    return ONPATH_NOT;
}


static BOOL_T PathListCheck(void)
{
    track_p trk;
    if (PathListEmpty() || PathListSingle()) {
        return TRUE;
    }
    if (!(GetTrkBits(pathStartTrk)&TB_PROFILEPATH)) {
        ErrorMessage(MSG_PST_NOT_ON_PATH);
        return FALSE;
    }
    if (!(GetTrkBits(pathEndTrk)&TB_PROFILEPATH)) {
        ErrorMessage(MSG_PET_NOT_ON_PATH);
        return FALSE;
    }
    trk = GetTrkEndTrk(pathStartTrk,pathStartEp);
    if (trk && (GetTrkBits(trk)&TB_PROFILEPATH)) {
        ErrorMessage(MSG_INV_PST_ON_PATH);
        return FALSE;
    }
    trk = GetTrkEndTrk(pathEndTrk,pathEndEp);
    if (trk && (GetTrkBits(trk)&TB_PROFILEPATH)) {
        ErrorMessage(MSG_INV_PET_ON_PATH);
        return FALSE;
    }
    return TRUE;
}


static void RemoveTracksFromPath(
    track_p *Rtrk,
    EPINX_T *Rep,
    track_p trkEnd,
    EPINX_T epEnd)
{
    track_p trk = *Rtrk, trkN;
    EPINX_T ep = *Rep;

    PASSERT("removeTracksFromPath", trk, NOP);
    PASSERT("removeTracksFromPath", !PathListSingle(), NOP);
    while (1) {
        EPINX_T ep2;
        DrawTrack(trk, &mainD, drawColorWhite);
        ClrTrkBits(trk, TB_PROFILEPATH);
        DrawTrack(trk, &mainD, drawColorBlack);

        if (trk == trkEnd) {
            pathStartTrk = trkEnd;
            pathStartEp = epEnd;
            pathEndTrk = GetTrkEndTrk(pathStartTrk,pathStartEp);
            if (pathEndTrk) {
                pathEndEp = GetEndPtConnectedToMe(pathEndTrk,pathStartTrk);
            }
            return;
        }

        ep2 = GetNextTrkOnPath(trk, ep);
        PASSERT("removeTracksFromPath", ep2 >= 0,NOP);
        trkN = GetTrkEndTrk(trk,ep2);
        PASSERT("removeTracksFromPath", trkN != NULL, NOP);
        ep = GetEndPtConnectedToMe(trkN,trk);
        trk = trkN;
        if (EndPtIsDefinedElev(trk,ep)) {
            *Rtrk = trk;
            *Rep = ep;
            return;
        }
    }
}


static void ChkElev(track_p trk, EPINX_T ep, EPINX_T ep2, DIST_T dist,
                    BOOL_T * defined)
{
    profElem_p p;
    station_p s;
    EPINX_T epDefElev = -1;
    int mode;
    BOOL_T undefined;

    mode = GetTrkEndElevMode(trk, ep);
    if (mode == ELEV_DEF) {
        epDefElev = ep;
    } else if (mode == ELEV_STATION) {
        DYNARR_APPEND(station_t, station_da, 10);
        s = &station(station_da.cnt-1);
        s->dist = dist;
        s->name = GetTrkEndElevStation(trk,ep);
    }
    undefined = FALSE;
    if (epDefElev<0) {
        if ((trk == pathStartTrk && ep == pathStartEp) ||
                (trk == pathEndTrk && ep == pathEndEp)) {
            epDefElev = ep;
        }
    }
    if (epDefElev<0) {
        if (ep == ep2 ||
                GetTrkEndElevMode(trk,ep2) != ELEV_DEF)
            for (EPINX_T ep1=0; ep1<GetTrkEndPtCnt(trk); ep1++) {
                if (ep1==ep || ep1==ep2) {
                    continue;
                }
                if (EndPtIsDefinedElev(trk,ep1)) {
                    epDefElev = ep1;
                    dist -= GetTrkLength(trk, ep, ep1);
                    break;
                }
                if (GetTrkEndTrk(trk,ep1)) {
                    if (!EndPtIsIgnoredElev(trk,ep1)) {
                        undefined = TRUE;
                    }
                }
            }
    }

    if (epDefElev>=0) {
        DYNARR_APPEND(profElem_t, profElem_da, 10);
        p = &profElem(profElem_da.cnt-1);
        p->trk = trk;
        p->ep = epDefElev;
        p->dist = dist;
        if (GetTrkEndElevMode(trk,epDefElev) == ELEV_DEF) {
            p->elev = GetTrkEndElevHeight(trk,epDefElev);
        } else {
            ComputeElev(trk, epDefElev, TRUE, &p->elev, NULL, TRUE);
        }
        p->defined = *defined;
        *defined = TRUE;
    } else if (undefined) {
        *defined = FALSE;
    }
}


static void ComputeProfElem(void)
{
    track_p trk = pathStartTrk, trkN;
    EPINX_T ep = pathStartEp, ep2;
    BOOL_T go;
    DIST_T dist;
    BOOL_T defined;

    profElem_da.cnt = 0;
    station_da.cnt = 0;
    dist = 0;
    defined = TRUE;
    if (PathListEmpty()) {
        return;
    }
    ChkElev(trk, ep, ep, dist, &defined);
    if (PathListSingle()) {
        return;
    }
    go = TRUE;
    while (go) {
        if (trk == pathEndTrk) {
            go = FALSE;
            ep2 = pathEndEp;
        } else {
            ep2 = GetNextTrkOnPath(trk, ep);
            //PASSERT( "computeProfElem", ep2 >= 0, NOP );
        }
        dist += GetTrkLength(trk, ep, ep2);
        ChkElev(trk, ep2, ep, dist, &defined);
        if (!go) {
            break;
        }
        trkN = GetTrkEndTrk(trk,ep2);
        ep = GetEndPtConnectedToMe(trkN,trk);
        trk = trkN;
    }
}


static void DumpProfElems(void)
{
    track_p trk, trkN;
    EPINX_T ep;
    BOOL_T go;

    trk = pathStartTrk;
    ep = pathStartEp;

    if (pathStartTrk==NULL) {
        lprintf("s--:- e--:-");
    } else if (pathEndTrk == NULL) {
        lprintf("sT%d:%d e--:-", GetTrkIndex(pathStartTrk), pathStartEp);
    } else {
        lprintf("sT%d:%d eT%d:%d", GetTrkIndex(pathStartTrk), pathStartEp,
                GetTrkIndex(pathEndTrk), pathEndEp);
    }
    lprintf(" { ");
    go = TRUE;
    if (!PathListSingle())
        while (trk) {
            EPINX_T ep2;
            if (trk==pathEndTrk) {
                ep2 = pathEndEp;
                go = FALSE;
            } else {
                ep2 = GetNextTrkOnPath(trk, ep);
                PASSERT("computeProfElem", ep2 >= 0, NOP);
            }
            lprintf("T%d:%d:%d ", GetTrkIndex(trk), ep, ep2);
            if (!go) {
                break;
            }
            trkN = GetTrkEndTrk(trk,ep2);
            ep = GetEndPtConnectedToMe(trkN,trk);
            trk = trkN;
        }
    lprintf("}");
}


static void ProfileSelect(track_p trkN, EPINX_T epN)
{
    track_p trkP;
    EPINX_T epP=-1;
    int rc;

    if (log_profile>=1) {
        DumpProfElems();
        lprintf("  @ T%d:%d ", GetTrkIndex(trkN), epN);
        if (log_profile>=2) {
            lprintf("\n");
        }
    }

    trkP = GetTrkEndTrk(trkN, epN);
    if (trkP) {
        epP = GetEndPtConnectedToMe(trkP, trkN);
    }

    if (!PathListCheck()) {
        return;
    }

    if (PathListEmpty()) {
        pathStartTrk = trkN;
        pathStartEp = epN;
        pathEndTrk = trkP;
        pathEndEp = epP;
        LOG(log_profile, 2, ("Adding first element\n"))

    } else if (PathListSingle() &&
               ((trkN == pathStartTrk && epN == pathStartEp) ||
                (trkP && trkP == pathStartTrk && epP == pathStartEp))) {
        pathStartTrk = pathEndTrk = NULL;
        LOG(log_profile, 2, ("Clearing list\n"))

    } else if ((trkN == pathStartTrk && epN == pathStartEp) ||
               (trkP && trkP == pathStartTrk && epP == pathStartEp)) {
        RemoveTracksFromPath(&pathStartTrk, &pathStartEp, pathEndTrk, pathEndEp);
        LOG(log_profile, 2, ("Removing first element\n"))

    } else if ((trkN == pathEndTrk && epN == pathEndEp) ||
               (trkP && trkP == pathEndTrk && epP == pathEndEp)) {
        RemoveTracksFromPath(&pathEndTrk, &pathEndEp, pathStartTrk, pathStartEp);
        LOG(log_profile, 2, ("Removing last element\n"))

    } else if ((GetTrkBits(trkN)&TB_PROFILEPATH) || (trkP &&
               (GetTrkBits(trkP)&TB_PROFILEPATH))) {
        ErrorMessage(MSG_EP_ON_PATH);
        return;

    } else if ((rc = FindProfileShortestPath(trkN, epN)) > 0) {
        if (!(GetTrkBits(trkN)&TB_PROFILEPATH)) {
            PASSERT("profileSelect", trkP != NULL, NOP);
            trkN = trkP;
            epN = epP;
            LOG(log_profile, 2, ("Invert selected EP\n"))
        }

        switch (profileShortestPathMatch) {
        case 1:
            /* extend Start */
            pathStartTrk = trkN;
            pathStartEp = epN;
            LOG(log_profile, 2, ("Prepending Path\n"))
            break;
        case 2:
            /* extend End */
            pathEndTrk = trkN;
            pathEndEp = epN;
            LOG(log_profile, 2, ("Appending Path\n"))
            break;
        case 3:
            /* need to flip */
            pathStartTrk = pathEndTrk;
            pathStartEp = pathEndEp;
            pathEndTrk = trkN;
            pathEndEp = epN;
            LOG(log_profile, 2, ("Flip/Appending Path\n"))
            break;
        default:
            AbortProg("findPaths:1");
        }

    } else {
        ErrorMessage(MSG_NO_PATH_TO_EP);
        return;
    }

    DestroyCopyOfProfileElements();
    ComputeProfElem();
    CreateCopyProfileElements();

    RedrawProfileW();
    DoProfileChangeMode(NULL);
    if (log_profile>=1) {
        lprintf(" = ");
        DumpProfElems();
        lprintf("\n");
    }
    PathListCheck();
}



static void ProfileSubCommand(wBool_t set, void* pcmd)
{
    long cmd = (long)pcmd;
    int mode;
    coOrd pos = oldMarker;
    DIST_T elev;
    DIST_T radius;

    if ((profilePopupTrk = OnTrack(&pos, TRUE, TRUE)) == NULL ||
            (profilePopupEp = PickEndPoint(pos, profilePopupTrk)) < 0) {
        return;
    }
    if (profileUndo==0) {
        profileUndo = TRUE;
        UndoStart(_("Profile Command"), "Profile");
    }
    radius = 0.05*mainD.scale;
    if (radius < trackGauge/2.0) {
        radius = trackGauge/2.0;
    }
    pos = GetTrkEndPos(profilePopupTrk, profilePopupEp);
    mode = GetTrkEndElevMode(profilePopupTrk, profilePopupEp);

    elev = 0.0;
    switch (cmd) {
    case 0:
        /* define */
        ComputeElev(profilePopupTrk, profilePopupEp, TRUE, &elev, NULL, TRUE);
        mode = ELEV_DEF|ELEV_VISIBLE;
        break;
    case 1:
        /* ignore */
        mode = ELEV_IGNORE|ELEV_VISIBLE;
        break;
    case 2:
    default:
        /* none */
        mode = ELEV_NONE;
        break;
    }
    UpdateTrkEndElev(profilePopupTrk, profilePopupEp, mode, elev, NULL);
    ComputeProfElem();
    RedrawProfileW();
    TempRedraw(); // ProfileSubCommand
}


static STATUS_T CmdProfile(wAction_t action, coOrd pos)
{
    track_p trk0;
    coOrd textsize;

    switch (action) {
    case C_START:
        if (profileW == NULL) {
            profileColorDefinedProfile = drawColorBlue;
            profileColorUndefinedProfile = drawColorRed;
            profileColorFill = drawColorGrey80;
            DrawTextSize(&mainD, "999.9", wStandardFont(F_HELV, FALSE, FALSE),
                         screenProfileFontSize, FALSE, &textsize);
            labelH = textsize.y;
            labelW = textsize.x;
            profileW = ParamCreateDialog(&profilePG, MakeWindowTitle(_("Profile")), NULL,
                                         NULL, wHide, TRUE, NULL, F_RESIZE, CloseProfileWindow);
        }
        ParamLoadControls(&profilePG);
        ParamGroupRecord(&profilePG);
        wShow(profileW);
        ParamLoadMessage(&profilePG, I_PROFILEMSG, _("Drag to change Elevation"));
        profElem_da.cnt = 0;
        station_da.cnt = 0;
        RedrawProfileW();
        ClrAllTrkBitsRedraw(TB_PROFILEPATH, TRUE);
        pathStartTrk = NULL;
        SetAllTrackSelect(FALSE);
        profileUndo = FALSE;
        InfoMessage(_("Select a Defined Elevation to start profile"));
        TempRedraw(); // CmdProfile C_START
        return C_CONTINUE;
    case C_LCLICK:
        InfoMessage("");
        if ((trk0 = OnTrack(&pos, TRUE, TRUE)) != NULL) {
            EPINX_T ep0;
            ep0 = PickEndPoint(pos, trk0);
            if (ep0 >= 0) {
                ProfileSelect(trk0, ep0);
            }
        }
        return C_CONTINUE;
    case C_CMDMENU:
        if ((profilePopupTrk = OnTrack(&pos, TRUE, TRUE)) != NULL) {
            profilePopupEp = PickEndPoint(pos, profilePopupTrk);
            if (profilePopupEp >= 0) {
                int mode;
                mode = GetTrkEndElevMode(profilePopupTrk, profilePopupEp);
                if (mode != ELEV_DEF && mode != ELEV_IGNORE && mode != ELEV_NONE) {
                    ErrorMessage(MSG_CHANGE_ELEV_MODE);
                } else {
                    wMenuToggleEnable(profilePopupToggles[1], TRUE);
                    if (OnPath(profilePopupTrk, profilePopupEp) & (ONPATH_END|ONPATH_MID)) {
                        wMenuToggleEnable(profilePopupToggles[1], FALSE);
                    }
                    wMenuToggleSet(profilePopupToggles[0], mode == ELEV_DEF);
                    wMenuToggleSet(profilePopupToggles[1], mode == ELEV_IGNORE);
                    wMenuToggleSet(profilePopupToggles[2], mode == ELEV_NONE);
                    menuPos = pos;
                    wMenuPopupShow(profilePopupM);
                }
            }
        }
        return C_CONTINUE;
    case C_OK:
        DoProfileDone(NULL);
        return C_TERMINATE;
    case C_CANCEL:
        wHide(profileW);
        ClrAllTrkBitsRedraw(TB_PROFILEPATH, TRUE);
        return C_TERMINATE;
    case C_REDRAW:
        if (wWinIsVisible(profileW)) {
            HilightProfileElevations(wWinIsVisible(profileW));
        }
        return C_CONTINUE;
    }
    return C_CONTINUE;
}


static void ProfileChange(long changes)
{
    if ((changes & CHANGE_UNITS) && screenProfileD.d) {
        RedrawProfileW();
    }
}

#include "bitmaps/profile.xpm"

EXPORT void InitCmdProfile(wMenu_p menu)
{
    log_profile = LogFindIndex("profile");
    ParamRegister(&profilePG);

    AddMenuButton(menu, CmdProfile, "cmdProfile", _("Profile"),
                  wIconCreatePixMap(profile_xpm), LEVEL0_50, IC_LCLICK|IC_CMDMENU|IC_POPUP3,
                  ACCL_PROFILE, NULL);
    profilePopupM = MenuRegister("Profile Mode");
    profilePopupToggles[0] = wMenuToggleCreate(profilePopupM, "", _("Define"), 0,
                             FALSE, ProfileSubCommand, (void*)0);
    profilePopupToggles[1] = wMenuToggleCreate(profilePopupM, "", _("Ignore"), 0,
                             FALSE, ProfileSubCommand, (void*)1);
    profilePopupToggles[2] = wMenuToggleCreate(profilePopupM, "", _("None"), 0,
                             FALSE, ProfileSubCommand, (void*)2);
    RegisterChangeNotification(ProfileChange);
}
