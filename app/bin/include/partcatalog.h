/** \file partcatalog.h
* Manage the catalog of track parameter files
*/
/*  XTrackCad - Model Railroad CAD
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

#ifndef HAVE_TRACKCATALOG_H
#define HAVE_TRACKCATALOG_H

#include <stdbool.h>
#include "include/utlist.h"

#define MAXFILESPERCONTENT	10					/**< count of files with the same content header */

struct sCatalogEntry {
    struct sCatalogEntry *next, *prev;
    unsigned files;								/**< current count of files */
    char *fullFileName[MAXFILESPERCONTENT];		/**< fully qualified file name */
    char *contents;								/**< content field of parameter file */
};
typedef struct sCatalogEntry CatalogEntry;

struct sCatalog {
    CatalogEntry *head;							/**< The entries */
};
typedef struct sCatalog Catalog;


/**
An index entry. This struct holds a keyword pointer and an array of pointers to
CatalogEntry
It is managed as a linked list
*/
struct sIndexEntry {
    struct sIndexEntry *next;
    struct sIndexEntry *prev;
    char *keyWord;								/**< keyword */
    dynArr_t *references;						/**< references to the CatalogEntry */
};
typedef struct sIndexEntry IndexEntry;


struct sParameterLib {
    Catalog *catalog;							/**< list of files cataloged */
    IndexEntry *index;							/**< Index for lookup */
    unsigned wordCount;							/**< How many words indexed */
    unsigned parameterFileCount;					/**< */
    char *words;
};
typedef struct sParameterLib
    ParameterLib;							/**< core data structure for the catalog */

enum WORDSTATE {
    SEARCHED,
    DISCARDED,
    NOTFOUND,
	CLOSE,
    STATE_COUNT
};

struct sSearchResult {
    Catalog subCatalog;
    unsigned totalFound;
    unsigned words;								/**< no of words in search string */
    struct sSingleResult {
        char *keyWord;
        unsigned count;
        enum WORDSTATE state;
    } *kw;
};
typedef struct sSearchResult SearchResult;

Catalog *InitCatalog(void);
ParameterLib *InitLibrary(void);
ParameterLib *CreateLibrary(char *directory);
void DestroyLibrary(ParameterLib *tracklib);
bool CreateCatalogFromDir(ParameterLib *trackLib, char *directory);
unsigned CreateLibraryIndex(ParameterLib *trackLib);
unsigned SearchLibrary(ParameterLib *library, char *searchExpression, SearchResult *totalResult);
char *SearchStatistics(SearchResult *result);
void SearchDiscardResult(SearchResult *res);
unsigned CountCatalogEntries(Catalog *catalog);
void DiscardCatalog(ParameterLib *library);
bool FilterKeyword(char *word);
#endif // !HAVE_TRACKCATALOG_H
