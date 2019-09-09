/** \file single.c
 * Single line entry field for text
 */

/*  XTrkCad - Model Railroad CAD
 *  Copyright (C) 2005 Dave Bullis, 2012 Martin Fischer
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

#define GTK_DISABLE_SINGLE_INCLUDES
#define GDK_DISABLE_DEPRECATED
// #define GTK_DISABLE_DEPRECATED
#define GSEAL_ENABLE

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

#include <gtk/gtk.h>
#include <glib-object.h>

#include "gtkint.h"

#define TIMEOUT_INACTIVITY (500)  	/**< timeout for entry fields in millisecs */

/*
 *****************************************************************************
 *
 * Text Boxes
 *
 *****************************************************************************
 */

struct wString_t {
	WOBJ_COMMON
	char *valueP;			/**< pointer to result buffer */
	wIndex_t valueL;	 	/**< maximum length */
	wStringCallBack_p action;  	/**< callback for changes */
	wBool_t notice_activate; /** if flag set to observe enter key **/
	wBool_t enter_pressed;	/**< flag if enter was pressed */
	wBool_t hasSignal;		/** needs signal to be suppressed */
	int count;				/** number of 100ms since last entry **/
	guint	timer;			/**< timer source for inactivity timer */
};

/**
 * Set the string value in a string entry field
 *
 * \param b 	IN widget to be updated
 * \param arg 	IN new string value
 * \return 
 */

void wStringSetValue(
    wString_p b,
    const char *arg) 
{
	if (b->widget == NULL)
		abort();
	
	// the contents should not be changed programatically while
	// the user is editing it
	if( !(gtk_widget_has_focus(b->widget))) {
		if (b->hasSignal) 
	    	gtk_signal_handler_block_by_data(GTK_OBJECT(b->widget), b);
		gtk_entry_set_text(GTK_ENTRY(b->widget), arg);
		if (b->hasSignal)
			gtk_signal_handler_unblock_by_data(GTK_OBJECT(b->widget), b);
	}
}

/**
 * Set the width of the entry field
 *
 * \param b 	IN widget to be updated
 * \param w 	IN new width
 * \return 
 */

void wStringSetWidth(
    wString_p b,
    wPos_t w) 
{
	gtk_widget_set_size_request(b->widget, w, -1);
	b->w = w;
}

/**
 * Return the entered value
 *
 * \param b IN entry field
 * \return   the entered text
 */

const char *wStringGetValue(
    wString_p b) 
{
	if ( !b->widget ) 
		abort();
	
	return gtk_entry_get_text(GTK_ENTRY(b->widget));
}

/**
 * Kill an active timer
 *
 * \param b IN entry field
 * \return   the entered text
 */

static gboolean killTimer(
    GtkEntry *widget,
	GdkEvent *event,
    wString_p b) 
{

	// remove all timers related to this widget	
	while( g_source_remove_by_user_data( b ))
		;
	b->timer = 0;
	
	if (b->action) {
		const char *s;
		
		s = gtk_entry_get_text(GTK_ENTRY(b->widget));
		b->action(s, b->data);
	}
	gtk_editable_select_region( GTK_EDITABLE( widget ), 0, 0 );
	return( FALSE );
}	

/**
 *	Timer handler for string activity. This timer checks the input if the user
 * 	doesn't change an entry value for the preset time (0.5s).
 */

static gboolean
timeoutString( wString_p bs ) 
{
	const char *new_value;
	if ( !bs )
		return( FALSE );
	if (bs->widget == 0) 
		abort();
	
	bs->count--;

	if (bs->count==0) {
		// get the currently entered value
	    new_value = wStringGetValue(bs);
		if (bs->valueP != NULL)
			strcpy(bs->valueP, new_value);

		if (bs->action) {
			bs->enter_pressed = FALSE;     //Normal input
			if ( new_value )
				bs->action(new_value,bs->data);
		}
	}
	if (bs->count<=0) {
		bs->timer = 0;
		return( FALSE );   //Stop timer
	} else {
		return TRUE;       //Wait 100ms
	}
}

/**
 * Signal handler for 'activate' signal: enter pressed - callback with the current value and then
 * select the whole default value
 *
 * \param widget 	IN the edit field
 * \param b 		IN the widget data structure
 * \return 
 */

static gboolean stringActivated(
    GtkEntry *widget,
    wString_p b) 
{
	const char *s;
	const char * output = "\n";

	if ( !b )
		return( FALSE );
	
	s = wStringGetValue(b);

	if (b->valueP)
		strcpy(b->valueP, s);

	if (b->action) {
		b->enter_pressed = TRUE;
		b->action( output, b->data);
	}
	
	// select the complete default value to make editing it easier
	gtk_editable_select_region( GTK_EDITABLE( widget ), 0, -1 );
	return( TRUE );
}

static gboolean stringExposed(GtkWidget* widget, GdkEventExpose * event, gpointer g )
{
	wControl_p b = (wControl_p)g;
	return wControlExpose(widget,event,b);
}


/**
 * Signal handler for changes in an entry field
 *
 * \param widget 		IN
 * \param entry field 	IN
 * \return 
 */

static void stringChanged(
    GtkEntry *widget,
    wString_p b) 
{
	const char *new_value;

	if ( !b  )
		return;

	b->count = 5;              /* set ~500 ms from now */

	// get the entered value
	//new_value = wStringGetValue(b);
	//if (b->valueP != NULL)
	//	strcpy(b->valueP, new_value);
	//
	// 
	if (b->action){
		// if one exists, remove the inactivity timer
		if( !b->timer ) {
			//g_source_remove( b->timer );
		
		// create a new timer
			b->timer = g_timeout_add( TIMEOUT_INACTIVITY/5,
								  (GSourceFunc)timeoutString, 
								  	  b );
		}
	}	
	return;
}

/**
 * Create a single line entry field for a string value
 *
 * \param 	parent	IN	parent widget
 * \param 	x		IN	x position
 * \param 	y		IN	y position
 * \param 	helpStr	IN	help anchor
 * \param 	labelStr IN label
 * \param	option	IN	option (supported BO_READONLY )
 * \param	width	IN	width of entry field	
 * \param	valueP	IN	default value
 * \param	valueL	IN 	maximum length of entry 
 * \param	action	IN	application callback function
 * \param 	data	IN	application data
 * \return  the created widget
 */

wString_p wStringCreate(
    wWin_p	parent,
    wPos_t	x,
    wPos_t	y,
    const char 	 *helpStr,
    const char	 *labelStr,
    long	option,
    wPos_t	width,
    char	*valueP,
    wIndex_t valueL,
    wStringCallBack_p action,
    void 	*data) 
{
	wString_p b;

	// create and initialize the widget	
	b = (wString_p)wlibAlloc(parent, B_TEXT, x, y, labelStr, sizeof *b, data);
	b->valueP = valueP;
	b->action = action;
	b->option = option;
	b->valueL = valueL;
	b->timer = 0;
	b->hasSignal = 0;
	wlibComputePos((wControl_p)b);

	// create the gtk entry field and set maximum length if desired	
	b->widget = (GtkWidget *)gtk_entry_new();
	if (b->widget == NULL) abort();

	if( valueL )
		gtk_entry_set_max_length( GTK_ENTRY( b->widget ), valueL );
	
	// it is assumed that the parent is a fixed layout widget and the entry can
	// be placed at a specific position
	gtk_fixed_put(GTK_FIXED(parent->widget), b->widget, b->realX, b->realY);
	
	// set minimum size for widget	
	if (width)
		gtk_widget_set_size_request(b->widget, width, -1);
	
	// get the resulting size
	wlibControlGetSize((wControl_p)b);

	// if desired, place a label in front of the created widget
	if (labelStr)
		b->labelW = wlibAddLabel((wControl_p)b, labelStr);
	
	if (option & BO_READONLY)
		gtk_editable_set_editable(GTK_EDITABLE(b->widget), FALSE);
	
	// set the default text	and select it to make replacing it easier
	if (b->valueP) {
		wStringSetValue(b, b->valueP);
		// select the text only if text is editable
	}
	
	// show
	gtk_widget_show(b->widget);
	
	// add the new widget to the list of created widgets
	wlibAddButton((wControl_p)b);
	
	// link into help 
	wlibAddHelpString(b->widget, helpStr);
	
	//g_signal_connect(GTK_OBJECT(b->widget), "changed", G_CALLBACK(stringChanged), b);
	//if (option&BO_ENTER)
		g_signal_connect(GTK_OBJECT(b->widget), "activate", G_CALLBACK(stringActivated), b);
	b->hasSignal = 1;
		g_signal_connect_after(GTK_OBJECT(b->widget), "expose-event",
	    							G_CALLBACK(stringExposed), b);
	
	// set the default text	and select it to make replacing it easier
	if (b->valueP) {
		wStringSetValue(b, b->valueP);
		// select the text only if text is editable
	}

	gtk_widget_add_events( b->widget, GDK_FOCUS_CHANGE_MASK );
	g_signal_connect(GTK_OBJECT(b->widget), "focus-out-event", G_CALLBACK(killTimer), b);
	
	return b;
}
