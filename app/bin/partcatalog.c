/** \file partcatalog.c
* Manage the catalog of track parameter files
*/
/*  XTrkCad - Model Railroad CAD
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

#include <ctype.h>
#include <malloc.h>
#include <search.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <sys/types.h>
#include <sys/stat.h>

#ifdef WINDOWS
    #include "include/dirent.h"
#endif

#include "include/readxtp.h"
#include "include/partcatalog.h"
#include "paths.h"

#if _MSC_VER > 1300
    #define stricmp _stricmp
    #define strnicmp _strnicmp
    #define strdup _strdup
#endif

/**
 * Create and initialize the linked list for the catalog entries
 *
 * \return pointer to first element
 */

static CatalogEntry *
InitList(void)
{
    CatalogEntry *head;
    CatalogEntry *tail;

    /* allocate two pseudo nodes for beginnung and end of list */
    head = (CatalogEntry *)malloc(sizeof(CatalogEntry));
    tail = (CatalogEntry *)malloc(sizeof(CatalogEntry));

    head->next = tail;
    tail->next = tail;

    return (head);
}

static CatalogEntry *
InsertIntoListAfter(CatalogEntry *entry)
{
    CatalogEntry *newEntry = (CatalogEntry *)malloc(sizeof(CatalogEntry));
    newEntry->next = entry->next;
    entry->next = newEntry;
    newEntry->files = 0;
    newEntry->contents = NULL;

    return (newEntry);
}

/**
 * Count the elements in the linked list ignoring dummy elements
 *
 * \param listHeader IN the linked list
 * \return the numberof elements
 */

static unsigned
CountListElements(CatalogEntry *listHeader)
{
    CatalogEntry *currentEntry = listHeader->next;
    unsigned count = 0;

    while (currentEntry != currentEntry->next) {
        count++;
        currentEntry = currentEntry->next;
    }
    return (count);
}

/**
 * Get the existing list element for a content
 *
 * \param listHeader IN start of list
 * \param contents IN  contents to search
 *
 * \return CatalogEntry if found, NULL otherwise
 */

static CatalogEntry *
IsExistingContents(CatalogEntry *listHeader, const char *contents)
{
    CatalogEntry *currentEntry = listHeader->next;

    while (currentEntry != currentEntry->next) {
        if (!stricmp(currentEntry->contents, contents)) {
            printf("%s already exists in %s\n", contents, currentEntry->fullFileName[0]);
            return (currentEntry);
        }
        currentEntry = currentEntry->next;
    }
    return (NULL);
}

/*!
 * Store information about a parameter file. The filename is added to the array
 * of files with identical content. If not already present, in addition
 * the content is stored
 *
 * \param entry existing entry to be updated
 * \param path filename to add
 * \param contents contents description
 */

static void
UpdateCatalogEntry(CatalogEntry *entry, char *path, char *contents)
{
    if (!entry->contents) {
        entry->contents = strdup(contents);
    }

    if (entry->files < MAXFILESPERCONTENT) {
        entry->fullFileName[entry->files++] = strdup(path);
    } else {
        abort(); // too many files for same contents
    }
}

/**
 * Create the list for the catalog entries
 *
 * \return
 */

static CatalogEntry *
CreateCatalog()
{
    CatalogEntry *catalog = InitList();

    return (catalog);
}

static IndexEntry *
CreateIndexTable(unsigned int capacity)
{
    IndexEntry *index = (IndexEntry *)malloc(capacity * sizeof(IndexEntry));

    return (index);
}

/**
 * Scan opened directory for the next parameter file
 *
 * \param dir IN opened directory handle
 * \param dirName IN name of directory
 * \param fileName OUT fully qualified filename
 *
 * \return TRUE if file found, FALSE if not
 */

static bool
GetNextParameterFile(DIR *dir, const char *dirName, char **fileName)
{
    bool done = false;
    bool res = false;

    /*
    * get all files from the directory
    */
    while (!done) {
        struct stat fileState;
        struct dirent *ent;

        ent = readdir(dir);

        if (ent) {
            if (!stricmp(FindFileExtension(ent->d_name), "xtp")) {
                /* create full file name and get the state for that file */
                MakeFullpath(fileName, dirName, ent->d_name, NULL);

                if (stat(*fileName, &fileState) == -1) {
                    fprintf(stderr, "Error getting file state for %s\n", *fileName);
                    continue;
                }

                /* ignore any directories */
                if (!(fileState.st_mode & S_IFDIR)) {
                    done = true;
                    res = true;
                }
            }
        } else {
            done = true;
            res = false;
        }
    }
    return (res);
}

/**
 * Scan a directory for parameter files. For each file found the CONTENTS is
 * read and added to the list *
 *
 * \param insertAfter IN starting point for the list of files
 * \param dirName IN directory to be scanned
 *
 * \return pointer to the last element(?)
 */

static CatalogEntry *
ScanDirectory(CatalogEntry *insertAfter, const char *dirName)
{
    DIR *d;

    d = opendir(dirName);
    if (d) {
        char *fileName = NULL;

        while (GetNextParameterFile(d, dirName, &fileName)) {
            CatalogEntry *newEntry;
            char *contents = GetParameterFileContent(fileName);
            if ((newEntry = IsExistingContents(insertAfter, contents))) {
                printf("Update existing parameter file %s\n", fileName);
            } else {
                newEntry = InsertIntoListAfter(insertAfter);
            }
            UpdateCatalogEntry(newEntry, fileName, contents);
            free(contents);
            free(fileName);
            fileName = NULL;
        }
        closedir(d);
    }

    return (insertAfter);
}

static int
CompareIndex(const IndexEntry *entry1, const IndexEntry *entry2)
{
    return (strcmp(entry1->keyWord, entry2->keyWord));
}

static unsigned
CreateContentsIndex(CatalogEntry *catalog, IndexEntry *index,
                    unsigned capacityOfIndex)
{
    CatalogEntry *currentEntry = catalog->next;
    unsigned totalMemory = 0;
    size_t wordCount = 0;
    char *wordList;
    char *wordListPtr;

    while (currentEntry != currentEntry->next) {
        totalMemory += strlen(currentEntry->contents) + 1;
        currentEntry = currentEntry->next;
    }

    wordList = malloc((totalMemory + 1) * sizeof(char));
    wordListPtr = wordList;
    currentEntry = catalog->next;

    while (currentEntry != currentEntry->next) {
        char *word;
        char *content = strdup(currentEntry->contents);

        word = strtok(content, " \t\n\r");
        while (word && wordCount < capacityOfIndex) {
            strcpy(wordListPtr, word);

            char *p = wordListPtr;
            for (; *p; ++p) {
                *p = tolower(*p);
            }

            index[wordCount].value = currentEntry;
            index[wordCount].keyWord = wordListPtr;
            wordListPtr += strlen(word) + 1;
            wordCount++;
            if (wordCount > capacityOfIndex) {
                abort();	/** TODO: Improve error handling when tto many words were used */
            }
            word = strtok(NULL, " \t\n\r");
        }
        free(content);
        currentEntry = currentEntry->next;
    }
    *wordListPtr = '\0';

    qsort((void*)index, wordCount, sizeof(IndexEntry), CompareIndex);

    return (wordCount);
}

/**
* A recursive binary search function. It returns location of x in
* given array arr[l..r] is present, otherwise -1
* Taken from http://www.geeksforgeeks.org/binary-search/ and modified
*
* \param arr IN array to search
* \param l IN starting index
* \param r IN highest index in array
* \param key IN key to search
* \return index if found, -1 otherwise
*/

static int SearchInIndex(IndexEntry arr[], int l, int r, char *key)
{
    if (r >= l) {
        int mid = l + (r - l) / 2;
        int res = strcmp(key, arr[mid].keyWord);

        // If the element is present at the middle itself
        if (!res) {
            return mid;
        }

        // If the array size is 1
        if (r == 0) {
            return -1;
        }

        // If element is smaller than mid, then it can only be present
        // in left subarray
        if (res < 0) {
            return SearchInIndex(arr, l, mid - 1, key);
        }

        // Else the element can only be present in right subarray
        return SearchInIndex(arr, mid + 1, r, key);
    }

    // We reach here when element is not present in array
    return -1;
}

/**
 * Inserts a key in arr[] of given capacity.  n is current
 * size of arr[]. This function returns n+1 if insertion
 * is successful, else n.
 * Taken from http ://www.geeksforgeeks.org/search-insert-and-delete-in-a-sorted-array/ and modified
 */

int InsertSorted(CatalogEntry *arr[], int n, CatalogEntry *key, int capacity)
{
    // Cannot insert more elements if n is already
    // more than or equal to capcity
    if (n >= capacity) {
        return n;
    }

    int i;
    for (i = n - 1; (i >= 0 && arr[i] > key); i--) {
        arr[i + 1] = arr[i];
    }

    arr[i + 1] = key;

    return (n + 1);
}

/**
 * Search the index for a keyword. The index is assumed to be sorted. So after one entry
 * is found, neighboring entries up and down are checked as well. The total result set
 * is placed into an array and returned. This array has to be free'd by the caller.
 *
 * \param index IN	index list
 * \param length IN number of entries index
 * \param search IN search string
 * \param resultCount OUT count of found entries
 * \return array of found entries, NULL if none found
 */

unsigned int
FindWord(IndexEntry *index, int length, char *search, CatalogEntry ***entries)
{
    CatalogEntry **result = NULL;
    int found;
    int foundElements = 0;

    found = SearchInIndex(index, 0, length, search);

    if (found >= 0) {
        int lower = found;
        int upper = found;
        int i;

        while (lower > 0 && !strcmp(index[lower-1].keyWord, search)) {
            lower--;
        }

        while (upper < length - 1 && !strcmp(index[upper + 1].keyWord, search)) {
            upper++;
        }

        foundElements = 1 + upper - lower;

//        printf("Successful search, found %d entries for %s\n", foundElements, search);

        result = malloc((foundElements) * sizeof(CatalogEntry *));

        for (i = 0; i < foundElements; i++) {
            InsertSorted(result, i, index[lower + i].value, foundElements);
        }

        *entries = result;
    } else {
//       printf("Unsuccessful search, no entries found for %s\n", search);
    }
    return (foundElements);
}

/**
 * Create and initialize the data structure for the track library
 *
 * \param trackLibrary OUT the newly allocated track library
 * \return TRUE on success
 */

bool
CreateLibrary(TrackLibrary **trackLibrary)
{
    TrackLibrary *trackLib = malloc(sizeof(TrackLibrary));

    if (trackLib) {
        trackLib->catalog = CreateCatalog();
        trackLib->index = NULL;
        trackLib->wordCount = 0;
        trackLib->trackTypeCount = 0;
    }
    *trackLibrary = trackLib;

    return (trackLib != NULL);
}

/**
 * Scan directory and all parameter files found to the catalog
 *
 * \param trackLib IN the catalog
 * \param directory IN directory to scan
 * \return number of files found
 */

bool
GetTrackFiles(TrackLibrary *trackLib, unsigned char *directory)
{
    ScanDirectory(trackLib->catalog, directory);
    trackLib->trackTypeCount = CountListElements(trackLib->catalog);

    return (trackLib->trackTypeCount);
}

/**
 * Create the search index for the whole catalog
 *
 * \param trackLib IN the catalog
 * \return the number of words indexed
 */

unsigned
CreateLibraryIndex(TrackLibrary *trackLib)
{
    trackLib->index = CreateIndexTable(trackLib->trackTypeCount *
                                       ESTIMATED_CONTENTS_WORDS);

    trackLib->wordCount = CreateContentsIndex(trackLib->catalog, trackLib->index,
                          ESTIMATED_CONTENTS_WORDS * trackLib->trackTypeCount);

    return (trackLib->wordCount);
}

unsigned
SearchLibrary(TrackLibrary *library, char *searchExpression,
              SearchResult **results)
{
    CatalogEntry **entries;
    unsigned entryCount;

    if (library->index == NULL || library->wordCount == 0) {
        return (0);
    }
    entryCount = FindWord(library->index, library->wordCount, searchExpression,
                          &entries);
    if (entryCount) {
        *results = (SearchResult *)malloc(sizeof(SearchResult));

        //     unsigned int i = 0;
        //     while (i < entryCount) {
        //         printf("%s found in %s (%lx)\n", searchExpression, entries[i]->contents,
        //                (unsigned long)entries[i]);
        //i++;
        //     }
        (*results)->entries = entries;
        (*results)->count = entryCount;
        return (entryCount);
    } else {
        *results = NULL;
        return (0);
    }
}

/**
 * Free the memory used for the search results
 *
 * \param results IN/OUT pointer to search results
 * \return true
 */

bool
FreeResults(SearchResult **results)
{
    if (*results) {
        if ((*results)->entries) {
            free((*results)->entries);
        }
        free(*results);
        *results = NULL;
    }
    return (true);
}