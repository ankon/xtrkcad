/** \file message.c
 * Message line
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

#include <stdlib.h>

#define GTK_DISABLE_SINGLE_INCLUDES
#define GDK_DISABLE_DEPRECATED
#define GTK_DISABLE_DEPRECATED
#define GSEAL_ENABLE

#include <gtk/gtk.h>
#include <gdk/gdk.h>

#include "gtkint.h"

/*
 *****************************************************************************
 *
 * Message Boxes
 *
 *****************************************************************************
 */

struct wMessage_t {
    WOBJ_COMMON
    GtkWidget * labelWidget;
    const char * message;
    wPos_t labelWidth;
};

/**
 * Set the message text
 *
 * \param b IN widget
 * \param arg IN new text
 * \return
 */

void wMessageSetValue(
    wMessage_p b,
    const char * arg)
{
    if (b->widget == 0) {
        abort();
    }

    gtk_label_set_text(GTK_LABEL(b->labelWidget), wlibConvertInput(arg));
}

/**
 * Set the width of the widget
 *
 * \param b IN widget
 * \param width IN  new width
 * \return
 */

void wMessageSetWidth(
    wMessage_p b,
    wPos_t width)
{
    b->labelWidth = width;
    gtk_widget_set_size_request(b->widget, width, -1);
}

/**
 * Get height of message text
 *
 * \param flags IN text properties (large or small size)
 * \return text height
 */
static int fonts_set = 0;

wPos_t wMessageGetHeight(
    long flags)

{

	GtkWidget * temp = gtk_combo_box_text_new();   //to get max size of an object in infoBar
	if (wMessageSetFont(flags))	{
		if (!fonts_set) {
			GtkStyleContext *context;
			GtkCssProvider *smallProvider = gtk_css_provider_new();
			GtkCssProvider *largeProvider = gtk_css_provider_new();
			/* get the current font descriptor */
		   context = gtk_widget_get_style_context(temp);

		   static const char smallStyle[] = "style \"smallLabel\" { font-size = 70% } widget \"*.smallLabel\" style \"smallLabel\"  ";

		   gtk_css_provider_load_from_data (smallProvider,
											smallStyle, -1, NULL);
		   gtk_style_context_add_provider(context,
											GTK_STYLE_PROVIDER(smallProvider),
											GTK_STYLE_PROVIDER_PRIORITY_APPLICATION);

		   static const char largeStyle[] = "style \"largeLabel\" { font-size = 140% } widget \"*.largeLabel\" style \"largeLabel\"  ";

		   gtk_css_provider_load_from_data (largeProvider,
											   largeStyle, -1, NULL);
		   gtk_style_context_add_provider(context,
										   GTK_STYLE_PROVIDER(largeProvider),
										   GTK_STYLE_PROVIDER_PRIORITY_APPLICATION);


		   fonts_set = 1;

		}

	   /* set the new font size */
	   if (flags & BM_LARGE) {
	       gtk_widget_class_set_css_name(GTK_WIDGET_CLASS(temp), "largeLabel");
	   } else {
	       gtk_widget_class_set_css_name(GTK_WIDGET_CLASS(temp), "smallLabel");
	   }




	}
	gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(temp),"Test");
	GtkRequisition min_requisition,natural_requisition;
	gtk_widget_get_preferred_size (temp,&min_requisition,&natural_requisition);
	gtk_widget_destroy(temp);
    return natural_requisition.height;
}

/**
 * Create a window for a simple text.
 *
 * \param IN parent Handle of parent window
 * \param IN x position in x direction
 * \param IN y position in y direction
 * \param IN labelStr ???
 * \param IN width horizontal size of window
 * \param IN message message to display ( null terminated )
 * \param IN flags display options
 * \return handle for created window
 */

 wMessage_p wMessageCreateEx(
    wWin_p	parent,
    wPos_t	x,
    wPos_t	y,
    const char 	* labelStr,
    wPos_t	width,
    const char	*message,
    long flags)
{
    wMessage_p b;
    GtkRequisition requisition;
    GtkStyle *stylecontext;
    PangoFontDescription *fontDesc;
    int fontSize;
    b = (wMessage_p)wlibAlloc(parent, B_MESSAGE, x, y, NULL, sizeof *b, NULL);
    wlibComputePos((wControl_p)b);
    b->message = message;
    b->labelWidth = width;
    b->labelWidget = gtk_label_new(message?wlibConvertInput(message):"");

    /* do we need to set a special font? */
    if (wMessageSetFont(flags))	{
		if (!fonts_set) {
			GtkStyleContext *context;
			GtkCssProvider *smallProvider = gtk_css_provider_new();
			GtkCssProvider *largeProvider = gtk_css_provider_new();
			/* get the current font descriptor */
		   context = gtk_widget_get_style_context(b->labelWidget);

		   static const char smallStyle[] = "style \"smallLabel\" { font-size = 70% } widget \"*.smallLabel\" style \"smallLabel\"  ";

		   gtk_css_provider_load_from_data (smallProvider,
											smallStyle, -1, NULL);
		   gtk_style_context_add_provider(context,
											GTK_STYLE_PROVIDER(smallProvider),
											GTK_STYLE_PROVIDER_PRIORITY_APPLICATION);

		   static const char largeStyle[] = "style \"largeLabel\" { font-size = 140% } widget \"*.largeLabel\" style \"largeLabel\"  ";

		   gtk_css_provider_load_from_data (largeProvider,
											   largeStyle, -1, NULL);
		   gtk_style_context_add_provider(context,
										   GTK_STYLE_PROVIDER(largeProvider),
										   GTK_STYLE_PROVIDER_PRIORITY_APPLICATION);
		   fonts_set = 1;
		}
	   /* set the new font size */
	   if (flags & BM_LARGE) {
		   gtk_widget_class_set_css_name(GTK_WIDGET_CLASS(b->labelWidget), "largeLabel");
	   } else {
		   gtk_widget_class_set_css_name(GTK_WIDGET_CLASS(b->labelWidget), "smallLabel");
	   }
    }

    b->widget = gtk_fixed_new();
    GtkRequisition min_requisition,natural_requisition;
    gtk_widget_get_preferred_size (b->labelWidget,&min_requisition,&natural_requisition);
    gtk_container_add(GTK_CONTAINER(b->widget), b->labelWidget);
    gtk_widget_set_size_request(b->widget, width?width:natural_requisition.width,
                                natural_requisition.height);
    wlibControlGetSize((wControl_p)b);
    gtk_fixed_put(GTK_FIXED(parent->widget), b->widget, b->realX, b->realY);
    gtk_widget_show(b->widget);
    gtk_widget_show(b->labelWidget);
    wlibAddButton((wControl_p)b);

    return b;
}
