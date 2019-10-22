/** \file PathsTest.c
* Unit tests for the paths module
*/

#include <stdarg.h>
#include <stddef.h>
#include <string.h>
#include <stdio.h>
#include <setjmp.h>
#include <cmocka.h>

#include "common.h"

#include "../appdefaults.c"

struct appDefault tests[] = {
	{"akey"},
	{"hkey"},
	{"mkey"},
	{"zkey"}
};


const char *libDir ="Parameter/directory/";

/**
 * A dummy for the real MakePath function 
 */

void
MakeFullpath( char **result, ...)
{
    *result = libDir;
}

#define TESTARRAYSIZE (sizeof(tests) / sizeof(tests[0]) )

struct appDefault test1[] = {
	{ "akey" }
};

#define TEST1ARRAYSIZE (sizeof(test1) / sizeof(test1[0]) )


int
wPrefGetIntegerBasic(const char *section, const char *name, long *result, long defaultValue)
{
	*result = defaultValue;
	return(TRUE);
}

int
wPrefGetFloatBasic(const char *section, const char *name, double *result, double defaultValue)
{
	*result = defaultValue;
	return(TRUE);
}

char * wPrefGetStringBasic(const char *section, const char *name)
{
	return(NULL);
}

/* dummy to make the linker happy */
void 
wPrefSetInteger(const char *section, const char *name,  long value)
{
	return;
}
static void BinarySearch(void **state)
{
	int result;
	(void)state;

	result = binarySearch(tests, 0, TESTARRAYSIZE-1, "nokey");
	assert_int_equal(result, -1);

	result = binarySearch(tests, 0, TESTARRAYSIZE-1, "akey");
	assert_int_equal(result, 0);

	result = binarySearch(tests, 1, TESTARRAYSIZE-1, "mkey");
	assert_int_equal(result, 2);

	result = binarySearch(tests, 0, TESTARRAYSIZE-1, "zkey");
	assert_int_equal(result, 3);

	result = binarySearch(test1, 0, TEST1ARRAYSIZE-1, "akey");
	assert_int_equal(result, 0);

	result = binarySearch(test1, 0, TEST1ARRAYSIZE-1, "zkey");
	assert_int_equal(result, -1);

}

static void GetDefaults(void **state)
{
	double value = 0.0;
	long intValue = 0;
	(void)state;

	wPrefGetIntegerExt("DialogItem", "cmdopt-preselect", &intValue, 2);
	assert_int_equal(intValue, 1);

	wPrefGetIntegerExt("DialogItem", "cmdopt-preselect", &intValue, 2);
	assert_int_equal(intValue, 2);

}

int main(void)
{
    const struct CMUnitTest tests[] = {
		cmocka_unit_test(BinarySearch),
		cmocka_unit_test(GetDefaults)
    };
    return cmocka_run_group_tests(tests, NULL, NULL);
}
