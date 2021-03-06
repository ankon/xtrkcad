/** \file stringxtc.c
 *
 *  Copyright (C) 1991, 1992  Linus Torvalds
 */

 /*
  * stupid library routines.
  */

#include <ctype.h>
#include <stddef.h>  
#include <errno.h>
#include "include/stringxtc.h"

/**
 * strscpy - Copy a C-string into a sized buffer
 * @dest: Where to copy the string to
 * @src: Where to copy the string from
 * @count: Size of destination buffer
 *
 * Copy the string, or as much of it as fits, into the dest buffer.  The
 * behavior is undefined if the string buffers overlap.  The destination
 * buffer is always NUL terminated, unless it's zero-sized.
 *
 * Preferred to strlcpy() since the API doesn't require reading memory
 * from the src string beyond the specified "count" bytes, and since
 * the return value is easier to error-check than strlcpy()'s.
 * In addition, the implementation is robust to the string changing out
 * from underneath it, unlike the current strlcpy() implementation.
 *
 * Preferred to strncpy() since it always returns a valid string, and
 * doesn't unnecessarily force the tail of the destination buffer to be
 * zeroed.  If zeroing is desired please use strscpy_pad().
 *
 * Return: The number of characters copied (not including the trailing
 *         %NUL) or -E2BIG if the destination buffer wasn't big enough.
 */
 
size_t strscpy(char *dest, const char *src, size_t count)
{
	long res = 0;

	if (count == 0)
		return -E2BIG;

	while (count) {
		char c;

		c = src[res];
		dest[res] = c;
		if (!c)
			return res;
		res++;
		count--;
	}

	/* Hit buffer length without finding a NUL; force NUL-termination. */
	if (res)
		dest[res - 1] = '\0';

	return -E2BIG;
}

/**
 * Convert a string to lower case
 * Taken from https://stackoverflow.com/questions/23618316/undefined-reference-to-strlwr
 * 
 * \param str IN string to convert
 * \return pointer to converted string
 */

char *
XtcStrlwr(char *str)
{
	unsigned char *p = (unsigned char *)str;

	while (*p) {
		*p = tolower((unsigned char)*p);
		p++;
	}

	return str;
}

/**
 * Compare two strings case insensitive
 * Taken from https://stackoverflow.com/questions/30733786/c99-remove-stricmp-and-strnicmp
 * 
 * \param a, b IN strings to compare
  * \return 
 */
 
int
XtcStricmp(const char *a, const char *b)
{
    int ca, cb;
    do {
        ca = (unsigned char) *a++;
        cb = (unsigned char) *b++;
        ca = tolower(toupper(ca));
        cb = tolower(toupper(cb));
    } while ((ca == cb) && (ca != '\0'));
    return ca - cb;
}


