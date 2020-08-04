/** \file helphelper.c
 * use OSX Help system 
 */

/*  XTrkCad - Model Railroad CAD
 *  Copyright (C) 2015 Martin Fischer
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

#import <CoreFoundation/CoreFoundation.h>
#import <Carbon/Carbon.h>

#include <stdio.h>
#include <stdlib.h>
#include <dirent.h>
#include <sys/time.h>
#include <signal.h>
#include <unistd.h>
#include <string.h>
#include <ctype.h>
#include <assert.h>
#include <errno.h>
#include <fcntl.h>

#include <stdint.h>

#define HELPCOMMANDPIPE "/tmp/helppipe"
#define EXITCOMMAND "##exit##"


OSStatus MyGoToHelpPage (CFStringRef pagePath, CFStringRef anchorName)
{
    CFBundleRef myApplicationBundle = NULL;
    CFStringRef myBookName = NULL;
    OSStatus err = noErr;
    printf("HelpHelper: Started to look for help\n");
 
    myApplicationBundle = CFBundleGetMainBundle();
    if (myApplicationBundle == NULL) {
    	printf("HelpHelper: Error - No Application Bundle\n");
    	err = fnfErr; 
    	return err;
    }
    printf("HelpHelper: Application Bundle Found\n");
 
    myBookName = CFBundleGetValueForInfoDictionaryKey(
                    myApplicationBundle,
                    CFSTR("CFBundleHelpBookName"));
    if (myBookName == NULL) {
    	myBookName = CFStringCreateWithCString(NULL, "XTrackCAD Help", kCFStringEncodingMacRoman);
    	printf("HelpHelper: Defaulting to 'XTrackCAD Help'\n" );  
    	err = fnfErr; 
    	return err;
    }
    printf("HelpHelper: BookName dictionary name %s found\n",CFStringGetCStringPtr(myBookName, kCFStringEncodingMacRoman));
 
    if (CFGetTypeID(myBookName) != CFStringGetTypeID()) {
    	printf("HelpHelper: Error - BookName is not a string\n" );
        err = paramErr;
    }
 
    if (err == noErr) {
    	err = AHGotoPage (myBookName, pagePath, anchorName);
    	if (err != noErr) {
    		printf("HelpHelper: Error in AHGoToPage('%s','%s')\n",CFStringGetCStringPtr(myBookName, kCFStringEncodingMacRoman), CFStringGetCStringPtr(pagePath, kCFStringEncodingMacRoman));
    	}
    }
    	
    return err;
 
};

int displayHelp(char* name) {
	CFStringRef str = CFStringCreateWithCString(NULL,name,kCFStringEncodingMacRoman);
	OSStatus err = MyGoToHelpPage(str, NULL);
	if (err != noErr) printf("HelpHelper: MyGoToHelpPage had error %d\n", err); 
	return err;

};

int
main( int argc, char **argv )
{
	int handleOfPipe = 0;
	char buffer[ 100 ];
	char issue[ 100 ];
	
	int len;
	int finished = 0;
	int numBytes = 0;
	int numBytes2 = 0;
	
	printf( "HelpHelper: starting!\n" );
	
	handleOfPipe = open( HELPCOMMANDPIPE, O_RDONLY );
	if( handleOfPipe ) {
		printf( "HelpHelper: opened pipe for reading\n" );
		while( !finished ) {
			printf( "HelpHelper: reading from pipe...\n" );
			numBytes = read( handleOfPipe, &len, sizeof( int ));
			
			if( numBytes > 0 )
				printf( "HelpHelper: read %d bytes\n", numBytes );
			if( numBytes == sizeof(int)) {
				printf( "HelpHelper: Expecting %d bytes\n", len );
				numBytes2 = read( handleOfPipe, buffer, len + 1 );
				buffer[numBytes2] = '\0';
				if (numBytes2 > 0) 
					printf( "HelpHelper: Display help on: %s\n", buffer );
				
				if( !strcmp(buffer, EXITCOMMAND )) {
					finished = 1;
				} else {
					if (displayHelp(buffer) != 0) 
						printf( "HelpHelper: Error\n");
				}				
			}
			if( numBytes <= 0 ) {
				printf( "HelpHelper: exiting on pipe error\n" );
				exit( 1 );
			}		
		}
	} else {
		printf( "HelpHelper: Could not open pipe for reading\n" );
	}	
	
	printf( "HelpHelper: exiting!" );
	
	exit( 0 );
};



	
