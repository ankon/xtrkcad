/** \file paramfilesearch_ui.c
 * Parameter File Search Dialog
 */

/*  XTrackCAD - Model Railroad CAD
 *  Copyright (C) 2019 Martin Fischer
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

#include <assert.h>
#include <ctype.h>
#include <stdbool.h>
#include <stdint.h>
#include <string.h>

#include "custom.h"
#include "dynstring.h"
#include "i18n.h"
#include "messages.h"
#include "param.h"
#include "include/partcatalog.h"
#include "paths.h"
#include "include/paramfilelist.h"
#include "fileio.h"
#include "directory.h"
#include "wlib.h"

static Catalog *catalogFileBrowse;			/**< current search results */
static ParameterLib *trackLibrary;			/**< Track Library          */
static Catalog *currentCat;					/**< catalog being shown    */

/* define the search / browse dialog */

static struct wFilSel_t *searchUi_fs;		/**< searchdialog for parameter files */

static void SearchUiBrowse(void *junk);
static void SearchUiDefault(void);
static void SearchUiApply(wWin_p junk);
static void SearchUiSelectAll(void *junk);
static void SearchUiDoSearch(void *junk);
static void SearchUiClearFilter(void *ptr);

static long searchUiMode = 0;
static paramListData_t searchUiListData = { 10, 370, 0 };
#define MAXQUERYLENGTH 250
static char searchUiQuery[MAXQUERYLENGTH];
static char * searchUiLabels[] = { N_("Show File Names"), NULL };

#define QUERYPROMPTSTRING "Enter at least one search word"

static paramData_t searchUiPLs[] = {
#define I_QUERYSTRING  (0)
    { PD_STRING, searchUiQuery, "query", PDO_ENTER | PDO_NOPREF | PDO_STRINGLIMITLENGTH | PDO_DLGRESIZE, (void*)(340), "", 0, 0, MAXQUERYLENGTH-1 },
#define I_SEARCHBUTTON (1)
    { PD_BUTTON, (void*)SearchUiDoSearch, "find", PDO_DLGHORZ, 0, NULL,  BO_ICON, (void *)NULL },
#define I_CLEARBUTTON (2)
    { PD_BUTTON, (void*)SearchUiClearFilter, "clearfilter", PDO_DLGHORZ, 0, NULL,  BO_ICON, (void *)NULL },
#define I_MESSAGE (3)
    { PD_MESSAGE, N_(QUERYPROMPTSTRING), NULL, 0, (void *)370 },
#define I_STATISTICS (4)
    { PD_MESSAGE, "", NULL, PDO_DLGBOXEND, (void *)370 },
#define I_RESULTLIST	(5)
    {	PD_LIST, NULL, "inx", PDO_NOPREF | PDO_DLGRESIZE, &searchUiListData, NULL, BL_DUP|BL_SETSTAY|BL_MANY },
#define I_MODETOGGLE	(6)
    {	PD_TOGGLE, &searchUiMode, "mode", PDO_DLGBOXEND, searchUiLabels, NULL, BC_HORZ|BC_NOBORDER },
#define I_APPLYBUTTON	(7)
    {	PD_BUTTON, (void *)SearchUiApply, "apply", PDO_DLGCMDBUTTON, NULL, N_("Add") },
#define I_SELECTALLBUTTON (8)
    {	PD_BUTTON, (void*)SearchUiSelectAll, "selectall", PDO_DLGCMDBUTTON, NULL, N_("Select all") },
};

#define SEARCHBUTTON ((wButton_p)searchUiPLs[I_SEARCHBUTTON].control)
#define CLEARBUTTON ((wButton_p)searchUiPLs[I_CLEARBUTTON].control)
#define RESULTLIST	 ((wList_p)searchUiPLs[I_RESULTLIST].control)
#define APPLYBUTTON  ((wButton_p)searchUiPLs[I_APPLYBUTTON].control)
#define SELECTALLBUTTON  ((wButton_p)searchUiPLs[I_SELECTALLBUTTON].control)
#define MESSAGETEXT ((wMessage_p)searchUiPLs[I_MESSAGE].control)
#define QUERYSTRING ((wString_p)searchUiPLs[I_QUERYSTRING].control)
#define SEARCHSTAT ((wMessage_p)searchUiPLs[I_STATISTICS].control)

static paramGroup_t searchUiPG = { "searchgui", 0, searchUiPLs, sizeof searchUiPLs/sizeof searchUiPLs[0] };
static wWin_p searchUiW;

#define FILESECTION "file"
#define PARAMDIRECTORY "paramdir"

/**
 * Reload the listbox showing the current catalog. The catalog is either the system
 * default library catalog or a search result
 *
 * \param [in] catalog the current catalog.
 */

static
void SearchFileListLoad(Catalog *catalog)

{
    CatalogEntry *head = catalog->head;
    CatalogEntry *catalogEntry;

    DynString description;
    DynStringMalloc(&description, STR_SHORT_SIZE);

    wControlShow((wControl_p)RESULTLIST, FALSE);
    wListClear(RESULTLIST);

    DL_FOREACH(head, catalogEntry) {
        for (unsigned int i=0; i<catalogEntry->files; i++) {
            DynStringClear(&description);
            DynStringCatCStr(&description,
                             ((!searchUiMode) && catalogEntry->contents) ?
                             catalogEntry->contents :
                             catalogEntry->fullFileName[i]);

            wListAddValue(RESULTLIST,
                          DynStringToCStr(&description),
                          NULL,
                          (void*)catalogEntry->fullFileName[i]);
        }
    }

    wControlShow((wControl_p)RESULTLIST, TRUE);
    wControlActive((wControl_p)SELECTALLBUTTON,
                   wListGetCount(RESULTLIST));

    DynStringFree(&description);

    currentCat = catalog;
}

/**
 * Find parameter files using the file selector
 *
 * \param junk
 */

static void SearchUiBrowse(void * junk)
{
    wFilSelect(searchUi_fs, GetParamFileDir());

    return;
}


/**
 * Reload just the system files into the searchable set
 */

static void SearchUiDefault()
{
	DynString dsSummary;

    SearchFileListLoad(trackLibrary->catalog);  //Start with system files
	wStringSetValue(QUERYSTRING, "");

	wMessageSetValue(MESSAGETEXT, _(QUERYPROMPTSTRING));
	DynStringMalloc(&dsSummary, 16);
	DynStringPrintf(&dsSummary, _("%u parameter files in library."), CountCatalogEntries(trackLibrary->catalog));
	wMessageSetValue(SEARCHSTAT, DynStringToCStr(&dsSummary));
	DynStringFree(&dsSummary);

	wControlActive((wControl_p)CLEARBUTTON, FALSE);
}

/**
 * Load the selected items of search results
 */

void static
SearchUILoadResults(void)
{
    int files = wListGetSelectedCount(RESULTLIST);

    if (files) {
        char **fileNames;
        int found = 0;
        fileNames = MyMalloc(sizeof(char *)*files);
        if (!fileNames) {
            AbortProg("Couldn't allocate memory for result list: %s (%d)", __FILE__,
                      __LINE__, NULL);
        }

        for (int inx = 0; found < files; inx++) {
            if (wListGetItemSelected(RESULTLIST, inx)) {
                fileNames[found++] = (char *)wListGetItemContext(RESULTLIST, inx);
            }
        }

        LoadParamFile(files, fileNames, NULL);
        MyFree(fileNames);
        SearchUiOk((void *) 0);
    }

}

/**
 * Update the action buttons.
 *
 * If there is at least one selected file, Apply is enabled
 * If there are entries in the list, Select All is enabled
 *
 * \return
 */

static void UpdateSearchUiButton(void)
{
    wIndex_t selCnt = wListGetSelectedCount(RESULTLIST);
    wIndex_t cnt = wListGetCount(RESULTLIST);

    wControlActive((wControl_p)APPLYBUTTON, selCnt > 0);
    wControlActive((wControl_p)SELECTALLBUTTON, cnt > 0);
}

/**
 * Return a pointer to the (shifted) trimmed string
 *
 * \param [in,out] s If non-null, a char to process.
 *
 * \returns pointer to the trimmed string
 */

char * StringTrim(char *s)
{
    char *original = s;
    size_t len = 0;

    while (isspace((unsigned char) *s)) {
        s++;
    }
    if (*s) {
        char *p = s;
        while (*p) {
            p++;
        }
        while (isspace((unsigned char) *(--p)));
        p[1] = '\0';
        len = (size_t)(p - s + 1);
    }

    return (s == original) ? s : memmove(original, s, len + 1);
}

/**
 * Perform the search. If successful, the results are loaded into the list
 *
 * \param [in,out] ptr ignored.
 */

static void SearchUiDoSearch(void * ptr)
{
    unsigned result;
    SearchResult *currentResults = MyMalloc(sizeof(SearchResult));
    char * search;

    search = StringTrim(searchUiQuery);

    if (catalogFileBrowse) {
        CatalogDiscard(catalogFileBrowse);
    } else {
        catalogFileBrowse = InitCatalog();
    }

	if (search[0]) {
		result = SearchLibrary(trackLibrary, search, currentResults);

		if (result) {
			DynString hitsMessage;
			char *statistics;
			DynStringMalloc(&hitsMessage, 16);
			DynStringPrintf(&hitsMessage, _("%d parameter files found."), result);
			wMessageSetValue(MESSAGETEXT, DynStringToCStr(&hitsMessage));
			DynStringFree(&hitsMessage);

			statistics = SearchStatistics(currentResults);
			wMessageSetValue(SEARCHSTAT, statistics);
			MyFree(statistics);

			SearchFileListLoad(&(currentResults->subCatalog));
			SearchDiscardResult(currentResults);
			wControlActive((wControl_p)CLEARBUTTON, TRUE);
		} else {
			wListClear(RESULTLIST);
			wControlActive((wControl_p)SELECTALLBUTTON, FALSE);
			wMessageSetValue(MESSAGETEXT, _("No matches found."));
		}

		MyFree((void *)currentResults);
	} else {
		SearchUiDefault();
	}
}

/**
 * Clear the current filter
 *
 * \param [in,out] ptr ignored
 */

static void
SearchUiClearFilter(void *ptr)
{
	SearchUiDefault();
}

/**
 * Select all files in the list
 *
 * \param junk IN ignored
 * \return
 */

static void SearchUiSelectAll(void *junk)
{
    wListSelectAll(RESULTLIST);
}

/**
 * Action handler for Done button. Hides the dialog.
 *
 * \param [in,out] junk ignored.
 */

void SearchUiOk(void * junk)
{
    if (searchUiW) {
        wHide(searchUiW);
    }
}

/**
 * Handle the Add button: a list of selected list elements is created and
 * passed to the parameter file list.
 *
 * \param junk IN/OUT ignored
 */

static void SearchUiApply(wWin_p junk)
{
    SearchUILoadResults();
}

/**
 * Event handling for the Search dialog. If the 'X' decoration is pressed the
 * dialog window is closed.
 *
 * \param pg IN ignored
 * \param inx IN ignored
 * \param valueP IN ignored
 */

static void SearchUiDlgUpdate(
    paramGroup_p pg,
    int inx,
    void * valueP)
{
    switch (inx) {
    case I_QUERYSTRING:
    	if (pg->paramPtr[inx].enter_pressed) {
    		strcpy( searchUiQuery, wStringGetValue((wString_p)pg->paramPtr[inx].control) );
    		SearchUiDoSearch(NULL);
    	}
    	break;
    case I_RESULTLIST:
        UpdateSearchUiButton();
        break;
    case I_MODETOGGLE:
        SearchFileListLoad(currentCat);
        break;
    case -1:
        SearchUiOk(valueP);
        break;
    }
}

/**
 * Get the system default directory for parameter files. First step is to
 * check the configuration file for a user specific setting. If that is not
 * found, the diretory is based derived from the installation directory.
 * The returned string has to be free'd() when no longer needed.
 *
 * \return   parameter file directory
 */

static char *
GetParamsPath()
{
    char * params_path;
    char *params_pref;
    params_pref = wPrefGetString(FILESECTION, PARAMDIRECTORY);

    if (!params_pref) {
        MakeFullpath(&params_path, wGetAppLibDir(), "params", NULL);
    } else {
        params_path = strdup(params_pref);
    }
    return (params_path);
}

#include "bitmaps/funnel.xpm"
#include "bitmaps/funnelclear.xpm"

/**
 * Create and open the search dialog.
 *
 * \param junk
 */

void DoSearchParams(void * junk)
{
    if (searchUiW == NULL) {
        catalogFileBrowse = InitCatalog();

        //Make the Find menu bound to the System Library initially
        char *paramsDir = GetParamsPath();
        trackLibrary = CreateLibrary(paramsDir);
        free(paramsDir);

        searchUiPLs[I_SEARCHBUTTON].winLabel = (char *)wIconCreatePixMap(funnel_xpm);
        searchUiPLs[I_CLEARBUTTON].winLabel = (char *)wIconCreatePixMap(
                funnelclear_xpm);

        ParamRegister(&searchUiPG);

        searchUiW = ParamCreateDialog(&searchUiPG,
                                      MakeWindowTitle(_("Choose parameter files")), _("Done"), NULL, wHide,
                                      TRUE, NULL, F_RESIZE | F_RECALLSIZE, SearchUiDlgUpdate);

        wControlActive((wControl_p)APPLYBUTTON, FALSE);
        wControlActive((wControl_p)SELECTALLBUTTON, FALSE);

        searchUi_fs = wFilSelCreate(searchUiW, FS_LOAD, FS_MULTIPLEFILES,
                                    _("Load Parameters"), _("Parameter files (*.xtp)|*.xtp"), GetParameterFileInfo,
                                    (void *)catalogFileBrowse);
    }

    ParamLoadControls(&searchUiPG);
    ParamGroupRecord(&searchUiPG);

    if (!trackLibrary) {
        wControlActive((wControl_p)SEARCHBUTTON, FALSE);
        wControlActive((wControl_p)QUERYSTRING, FALSE);
        wMessageSetValue(MESSAGETEXT,
                         _("No system parameter files found, search is disabled."));
    } else {
		SearchUiDefault();
    }

    wShow(searchUiW);
}


