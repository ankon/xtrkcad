/** \file button.c
 * Toolbar button creation and handling
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

#include <stdio.h>
#include <stdlib.h>

#define GTK_DISABLE_SINGLE_INCLUDES
#define GDK_DISABLE_DEPRECATED
#define GTK_DISABLE_DEPRECATED
#define GSEAL_ENABLE

#include <gtk/gtk.h>
#include <gdk/gdk.h>

#include "gtkint.h"
#include "i18n.h"

#include "eggwrapbox.h"

#define MIN_BUTTON_WIDTH (80)
#define MIN_BUTTON_HEIGHT (30)
#define MIN_TOOLBAR_WIDTH (10)
#define MIN_TOOLBAR_HEIGHT (10)

/*
 *****************************************************************************
 *
 * Simple Buttons
 *
 *****************************************************************************
 */

/**
 * Set the state of the button
 *
 * \param bb IN the button
 * \param value IN TRUE for active, FALSE for inactive
 */

void wButtonSetBusy(wButton_p bb, int value)
{
    bb->recursion++;
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(bb->widget), value);
    bb->recursion--;
    bb->busy = value;
}

/**
 * Set the label of a button, does also allow to set an icon
 *
 * \param widget IN
 * \param option IN
 * \param labelStr IN
 * \param labelG IN
 * \param imageG IN
 */

void wlibSetLabel(
    GtkWidget *widget,
    long option,
    const char * labelStr,
    GtkLabel * * labelG,
    GtkWidget * * imageG)
{
    wIcon_p bm;
   // GdkBitmap * mask;

    if (widget == 0) {
        abort();
    }

    if (labelStr) {
        if (option&BO_ICON) {
            GdkPixbuf *pixbuf;

            bm = (wIcon_p)labelStr;

            if (bm->gtkIconType == gtkIcon_pixmap) {
                pixbuf = gdk_pixbuf_new_from_xpm_data((const char**)bm->bits);
            } else {
                pixbuf = wlibPixbufFromXBM( bm );
            }
            if (*imageG==NULL) {
                *imageG = gtk_image_new_from_pixbuf(pixbuf);
				gtk_container_add(GTK_CONTAINER(widget), *imageG);
				gtk_widget_show(*imageG);
            } else {
                gtk_image_set_from_pixbuf(GTK_IMAGE(*imageG), pixbuf);
            }
           g_object_unref((gpointer)pixbuf);
        } else {
            if (*labelG==NULL) {
                *labelG = (GtkLabel*)gtk_label_new(wlibConvertInput(labelStr));
				gtk_container_add(GTK_CONTAINER(widget), (GtkWidget*)*labelG);
				gtk_widget_show((GtkWidget*)*labelG);
            } else {
                gtk_label_set_text(*labelG, wlibConvertInput(labelStr));
            }
        }
    }
}

/**
 * Change only the text label of a button
 * \param bb IN button handle
 * \param labelStr IN new label string
 */

void wButtonSetLabel(wButton_p bb, const char * labelStr)
{
    if(!bb->fromTemplate) {
        wlibSetLabel(bb->widget, bb->option, labelStr, &bb->labelG, &bb->imageG);
    } else {
        gtk_button_set_label(GTK_BUTTON(bb->widget), labelStr );
    }    
}

/**
 * Perform the user callback function
 *
 * \param bb IN button handle
 */

void wlibButtonDoAction(
    wButton_p bb)
{
    if (bb->action) {
        bb->action(bb->data);
    }
}

/**
 * Signal handler for button push
 * \param widget IN the widget
 * \param value IN the button handle (same as widget???)
 */

static void pushButt(
    GtkWidget *widget,
    gpointer value)
{
    wButton_p b = (wButton_p)value;

    if (debugWindow >= 2) {
        printf("%s button pushed\n", b->labelStr?b->labelStr:"No label");
    }

    if (b->recursion) {
        return;
    }

    if (b->action) {
        b->action(b->data);
    }

    if (!b->busy && !b->fromTemplate) {
        b->recursion++;
        gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(b->widget), FALSE);
        b->recursion--;
    }
}

static dynArr_t toolbar_da;

typedef struct {
	wButton_p button;
} toolbar_t;

void wButtonToolBarRedraw(wWin_p win) {
	gtk_widget_queue_resize(win->toolbar);
}

static GtkWidget * last_inner_box;
/*
 * wButtonCreateForToolbar - Create button to got into a toolbar
 *
 */

wButton_p wButtonCreateForToolbar(
	wWin_p  w,
    wPos_t	x,
    wPos_t	y,
    const char 	* helpStr,
    const char	* labelStr,
    long 	option,
	wPos_t width,
    wButtonCallBack_p action,
    void 	* data) {


	wButton_p b;
	if (!w->toolbar) {
		GtkWidget * toolbar_box = wlibGetWidgetFromName(w, "main-toolbar","box", FALSE);
		DYNARR_RESET(toolbar_t,toolbar_da);
		w->toolbar = egg_wrap_box_new (EGG_WRAP_ALLOCATE_FREE,
									EGG_WRAP_BOX_SPREAD_START,
									EGG_WRAP_BOX_SPREAD_START,
									2, 2);
		egg_wrap_box_set_minimum_line_children (EGG_WRAP_BOX (w->toolbar), 15);
		egg_wrap_box_set_natural_line_children (EGG_WRAP_BOX (w->toolbar), 60);
		gtk_widget_set_name(w->toolbar, "toolbar");
		gtk_widget_set_hexpand(w->toolbar,TRUE);
		gtk_box_pack_start(GTK_BOX(toolbar_box),w->toolbar,FALSE,FALSE,0);
		gtk_widget_show_all(w->toolbar);
		GdkScreen * screen = gdk_screen_get_default();
		GtkCssProvider * provider = gtk_css_provider_new();
		GtkStyleContext * context = gtk_widget_get_style_context(w->toolbar);
		static const char style[] = ".image-button {min-width:5px } ";
		gtk_css_provider_load_from_data(provider, style, strlen(style), NULL);
		gtk_style_context_add_provider_for_screen(screen,
										GTK_STYLE_PROVIDER(provider),
										GTK_STYLE_PROVIDER_PRIORITY_APPLICATION);
	}

	b = wlibAlloc(w, B_BUTTON, x, y, labelStr, sizeof *b, data);

	DYNARR_APPEND(toolbar_t,toolbar_da,20);
	(((toolbar_t *)toolbar_da.ptr)[toolbar_da.cnt-1]).button = b;

	b->option = option;
	b->action = action;
	wlibComputePos((wControl_p)b);

	b->widget = (GtkWidget *)gtk_toggle_button_new();

	b->reveal = (GtkRevealer *)gtk_revealer_new();
	gtk_container_add(GTK_CONTAINER(b->reveal),b->widget);  /*Add a revealer to allow show/noshow */

	if ((option&BO_ABUT) && last_inner_box) { /* Add revealer into last Child Box if ABUT */
		gtk_box_pack_start(GTK_BOX(last_inner_box),GTK_WIDGET(b->reveal),FALSE, FALSE, 0);
	} else {
		last_inner_box = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 0); /*New Box*/
		if (option&BO_GAP) {
			b->separator = gtk_separator_new(GTK_ORIENTATION_HORIZONTAL);
			gtk_box_pack_start(GTK_BOX(last_inner_box),b->separator,FALSE,FALSE,0);
		}
		gtk_box_pack_start(GTK_BOX(last_inner_box),GTK_WIDGET(b->reveal),FALSE, FALSE, 0);
		egg_wrap_box_insert_child(EGG_WRAP_BOX(w->toolbar), last_inner_box, -1, 0 );
	}
	b->inToolbar = TRUE;

	if (option&BO_ICON) {
		GdkPixbuf *pixbuf;

		wIcon_p bm;

		bm = (wIcon_p)labelStr;

		if (bm->gtkIconType == gtkIcon_pixmap) {
			pixbuf = gdk_pixbuf_new_from_xpm_data((const char**)bm->bits);
		} else {
			pixbuf = wlibPixbufFromXBM( bm );
		}
	    b->imageG = gtk_image_new_from_pixbuf(pixbuf);
		gtk_button_set_image(GTK_BUTTON(b->widget),b->imageG);
	   	g_object_unref((gpointer)pixbuf);

	} else {

		gtk_button_set_label(GTK_BUTTON(b->widget),wlibConvertInput(labelStr));
	}

    if (option & BB_DEFAULT) {
		gtk_widget_set_can_default(b->widget, TRUE);
		gtk_widget_grab_default(b->widget);
		gtk_window_set_default(GTK_WINDOW(w->gtkwin), b->widget);
	}

    wlibAddHelpString(b->widget, helpStr);
    wlibAddButton((wControl_p)b);

    g_signal_connect(b->widget, "clicked",
                            G_CALLBACK(pushButt), b);

    if (b->inToolbar) {

    	//gtk_widget_set_size_request(b->widget, MIN_TOOLBAR_WIDTH, MIN_TOOLBAR_HEIGHT);
    } else
    	gtk_widget_set_size_request(b->widget, MIN_BUTTON_WIDTH, MIN_BUTTON_HEIGHT);

	return b;


}
/**
 * Create a button
 *
 * \param parent IN parent window
 * \param x IN X-position
 * \param y IN Y-position
 * \param helpStr IN Help string
 * \param labelStr IN Label
 * \param option IN Options
 * \param width IN Width of button
 * \param action IN Callback
 * \param data IN User data as context
 * \returns button widget
 */

wButton_p wButtonCreate(
    wWin_p	parent,
    wPos_t	x,
    wPos_t	y,
    const char 	* helpStr,
    const char	* labelStr,
    long 	option,
    wPos_t 	width,
    wButtonCallBack_p action,
    void 	* data)
{
    wButton_p b;
    b = wlibAlloc(parent, B_BUTTON, x, y, labelStr, sizeof *b, data);
    b->option = option;
    b->action = action;
    wlibComputePos((wControl_p)b);

    if(!(option & BO_USETEMPLATE ))
    {    
    	if (option & BO_TOOLBAR) {
    		b->widget = gtk_toggle_button_new();
    		b->inToolbar = TRUE;
    	} else
    		b->widget = gtk_toggle_button_new();

        if (width > 0) {
            gtk_widget_set_size_request(b->widget, width, -1);
        }
        if( labelStr ){
            wButtonSetLabel(b, labelStr);
        }

        if (!(option&BO_TOOLBAR)) {
        	gtk_fixed_put(GTK_FIXED(parent->widget), b->widget, b->realX, b->realY);

			if (option & BB_DEFAULT) {
				gtk_widget_set_can_default(b->widget, TRUE);
				gtk_widget_grab_default(b->widget);
				gtk_window_set_default(GTK_WINDOW(parent->gtkwin), b->widget);
			}
        }

        wlibControlGetSize((wControl_p)b);

        if (width == 0 && b->w < MIN_BUTTON_WIDTH && (b->option&BO_ICON)==0) {
            b->w = MIN_BUTTON_WIDTH;
            if (b->h<MIN_BUTTON_HEIGHT) b->h = MIN_BUTTON_HEIGHT;
            gtk_widget_set_size_request(b->widget, b->w, b->h);
        }
        gtk_widget_show(b->widget);

    } else {
    	wBool_t free_needed = FALSE;
        char *id;
        switch( option & (BB_DEFAULT|BB_CANCEL|BB_HELP) ) {
        case BB_DEFAULT:
            id="id-ok";
            break;
        case BB_CANCEL:
            id = "id-cancel";
            break;
        case BB_HELP:
            id="id-help";
            break;
        default:
        	if (helpStr) {
        		id = strdup(helpStr);
        		free_needed = TRUE;
        	} else
        		id="unknown-button";
        }
        if (!(id==NULL || *id==0)) {
        	b->widget = wlibWidgetFromIdWarn(parent, id);
        	if (b->widget)
        		b->fromTemplate = TRUE;
        	b->template_id =strdup(id);
        	/* Find if this widget is inside a revealer widget which will be named with .reveal at the end*/
        	    b->reveal = (GtkRevealer *)wlibGetWidgetFromName( b->parent, id, "reveal", TRUE );
        }
        if (free_needed) free(id);
    }
    wlibAddButton((wControl_p)b);
    
    g_signal_connect(b->widget, "clicked",
                         G_CALLBACK(pushButt), b);
    
    wlibAddHelpString(b->widget, helpStr);
    return b;
}


/*
 *****************************************************************************
 *
 * Choice Boxes
 *
 *****************************************************************************
 */

struct wChoice_t {
    WOBJ_COMMON
    long *valueP;
    wChoiceCallBack_p action;
    int recursion;
    char * helpStr;
};


/**
 * Get the state of a group of buttons. If the group consists of
 * radio buttons, the return value is the index of the selected button
 * or -1 for none. If toggle buttons are checked, a bit is set for each
 * button that is active.
 *
 * \param bc IN
 * \returns state of group
 */

static long choiceGetValue(
    wChoice_p bc)
{
    GList * child, * children;
    long value, inx;

    if (bc->type == B_TOGGLE) {
        value = 0;
    } else {
        value = -1;
    }

    for (children=child=gtk_container_get_children(GTK_CONTAINER(bc->widget)),inx=0;
            child; child=child->next,inx++) {
        if (gtk_toggle_button_get_active(child->data)) {
            if (bc->type == B_TOGGLE) {
                value |= (1<<inx);
            } else {
                value = inx;
            }
        }
    }

    if (children) {
        g_list_free(children);
    }

    return value;
}

/**
 * Set the active radio button in a group
 *
 * \param bc IN button group
 * \param value IN index of active button
 */

void wRadioSetValue(
    wChoice_p bc,		/* Radio box */
    long value)		/* Value */
{
    GList * child, * children;
    long inx;

    for (children=child=gtk_container_get_children(GTK_CONTAINER(bc->widget)),inx=0;
            child; child=child->next,inx++) {
        if (inx == value) {
            bc->recursion++;
            gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(child->data), TRUE);
            bc->recursion--;
        }
    }

    if (children) {
        g_list_free(children);
    }
}

/**
 * Get the active button from a group of radio buttons
 *
 * \param bc IN
 * \returns
 */

long wRadioGetValue(
    wChoice_p bc)		/* Radio box */
{
    return choiceGetValue(bc);
}

/**
 * Set a group of toggle buttons from a bitfield
 *
 * \param bc IN button group
 * \param value IN bitfield
 */

void wToggleSetValue(
    wChoice_p bc,		/* Toggle box */
    long value)		/* Values */
{
    GList * child, * children;
    long inx;
    bc->recursion++;

    for (children=child=gtk_container_get_children(GTK_CONTAINER(bc->widget)),inx=0;
            child; child=child->next,inx++) {
        gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(child->data),
                                     (value&(1<<inx))!=0);
    }

    if (children) {
        g_list_free(children);
    }

    bc->recursion--;
}


/**
 * Get the active buttons from a group of toggle buttons
 *
 * \param b IN
 * \returns
 */

long wToggleGetValue(
    wChoice_p b)		/* Toggle box */
{
    return choiceGetValue(b);
}

/**
 * Signal handler for button selection in radio buttons and toggle
 * button group
 *
 * \param widget IN the button group
 * \param b IN user data (button group????)
 * \returns always 1
 */

static int pushChoice(
    GtkWidget *widget,
    gpointer b)
{
    wChoice_p bc = (wChoice_p)b;
    long value = choiceGetValue(bc);

    if (debugWindow >= 2) {
        printf("%s choice pushed = %ld\n", bc->labelStr?bc->labelStr:"No label",
               value);
    }

    if (bc->type == B_RADIO &&
            !(gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(widget)))) {
        return 1;
    }

    if (bc->recursion) {
        return 1;
    }

    if (bc->valueP) {
        *bc->valueP = value;
    }

    if (bc->action) {
        bc->action(value, bc->data);
    }

    return 1;
}

/**
 * Signal handler used to draw a frame around a widget, used to visually
 * group several buttons together
 *
 * \param b IN  widget
 */

static void choiceRepaint(
    wControl_p b)
{
    wChoice_p bc = (wChoice_p)b;

    wBoxType_e boxType = wBoxBelow;

    if (gtk_widget_get_visible(b->widget)) {
        wlibDrawBox(bc->parent, boxType, bc->realX-1, bc->realY-1, bc->w+1, bc->h+1);
    }
}

/**
 * Create a group of radio buttons.
 *
 * \param parent IN parent window
 * \param x IN X-position
 * \param y IN Y-position
 * \param helpStr IN Help string
 * \param labelStr IN Label
 * \param option IN Options
 * \param labels IN Labels
 * \param valueP IN Selected value
 * \param action IN Callback
 * \param data IN User data as context
 * \returns radio button widget
 */

wChoice_p wRadioCreate(
    wWin_p	parent,
    wPos_t	x,
    wPos_t	y,
    const char 	* helpStr,
    const char	* labelStr,
    long	option,
    const char	**labels,
    long	*valueP,
    wChoiceCallBack_p action,
    void 	*data)
{
    wChoice_p b;
    const char ** label;
    GtkWidget *butt0=NULL, *butt;

    if ((option & BC_NOBORDER)==0) {
        if (x>=0) {
            x++;
        } else {
            x--;
        }

        if (y>=0) {
            y++;
        } else {
            y--;
        }
    }

    b = wlibAlloc(parent, B_RADIO, x, y, labelStr, sizeof *b, data);
    b->option = option;
    b->action = action;
    b->valueP = valueP;
    wlibComputePos((wControl_p)b);

    if(option&BO_USETEMPLATE) {
    	/* This type of button uses a box to encapsulate the options
    	 * the convention is that this will be the field name followed by ".box"
    	 */
    	b->widget = wlibGetWidgetFromName( parent, helpStr, "box", FALSE);
    	if (b->widget) b->fromTemplate = TRUE;
    	b->template_id = strdup(helpStr);
    } else {
		if (option&BC_HORZ) {
			b->widget = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 0);
		} else {
			b->widget = gtk_box_new (GTK_ORIENTATION_VERTICAL, 0);
		}
    }

    if (b->widget == 0) {
        abort();
    }

    if(b->fromTemplate) {
    	GList * child, * children;

		for (children=child=gtk_container_get_children(GTK_CONTAINER(b->widget));
					child; child=child->next) {
			g_signal_connect(child->data, "toggled",
							G_CALLBACK(pushChoice), b);
			wlibAddHelpString(child->data, helpStr);
		}
		if (children) g_list_free(children);

    } else {

		for (label=labels; *label; label++) {
				butt = gtk_radio_button_new_with_label(
					   butt0?gtk_radio_button_get_group(GTK_RADIO_BUTTON(butt0)):NULL, _(*label));
			if (butt0==NULL) {
				butt0 = butt;
			}
			gtk_box_pack_start(GTK_BOX(b->widget), butt, TRUE, TRUE, 0);
			g_signal_connect(butt, "toggled",
							 G_CALLBACK(pushChoice), b);
			wlibAddHelpString(butt, helpStr);
		}
	}
    if (option & BB_DEFAULT) {
        gtk_widget_set_can_default(b->widget, TRUE);
        gtk_widget_grab_default(b->widget);
    }

    if (valueP) {
        wRadioSetValue(b, *valueP);
    }

    if ((option & BC_NOBORDER)==0) {
        b->repaintProc = choiceRepaint;
        b->w += 2;
        b->h += 2;
    }

    if (!b->fromTemplate) {
    	 gtk_fixed_put(GTK_FIXED(parent->widget), b->widget, b->realX, b->realY);
    }
    wlibControlGetSize((wControl_p)b);

    if (labelStr) {
        b->labelW = wlibAddLabel((wControl_p)b, labelStr);
    }

    gtk_widget_show_all(b->widget);
    wlibAddButton((wControl_p)b);

    return b;
}


/**
 * Create a group of toggle buttons.
 *
 * \param parent IN parent window
 * \param x IN X-position
 * \param y IN Y-position
 * \param helpStr IN Help string
 * \param labelStr IN Label
 * \param option IN Options
 * \param labels IN Labels
 * \param valueP IN Selected value
 * \param action IN Callback
 * \param data IN User data as context
 * \returns toggle button widget
 */

wChoice_p wToggleCreate(
    wWin_p	parent,
    wPos_t	x,
    wPos_t	y,
    const char 	* helpStr,
    const char	* labelStr,
    long	option,
    const char	**labels,
    long	*valueP,
    wChoiceCallBack_p action,
    void 	*data)
{
    wChoice_p b;
    const char ** label;

    if ((option & BC_NOBORDER)==0) {
        if (x>=0) {
            x++;
        } else {
            x--;
        }

        if (y>=0) {
            y++;
        } else {
            y--;
        }
    }

    b = wlibAlloc(parent, B_TOGGLE, x, y, labelStr, sizeof *b, data);
    b->option = option;
    b->action = action;
    wlibComputePos((wControl_p)b);

    if (option&BO_USETEMPLATE ) {
        b->widget = wlibGetWidgetFromName(parent, helpStr, "box", FALSE);
    	if (b->widget) 
            b->fromTemplate = TRUE;
    	b->template_id = strdup(helpStr);
    } else {
		if (option&BC_HORZ) {
			b->widget = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 0);
		} else {
			b->widget = gtk_box_new (GTK_ORIENTATION_VERTICAL, 0);
		}
    }

    if (b->widget == 0) {
        abort();
    }

    if (b->fromTemplate && !(option&BC_REBUILDBUTTONS)) {
    	 GList * child, * children;

    	for (children=child=gtk_container_get_children(GTK_CONTAINER(b->widget));
    	            child; child=child->next) {
    		g_signal_connect(child->data, "toggled",
    						G_CALLBACK(pushChoice), b);
    		wlibAddHelpString(child->data, helpStr);

    	}
    	if (children) g_list_free(children);

    } else {

    	for (label=labels; *label; label++) {
			GtkWidget *butt;


			butt = gtk_check_button_new_with_label(_(*label));
			gtk_box_pack_start(GTK_BOX(b->widget), butt, TRUE, TRUE, 0);
			gtk_widget_show(butt);

			g_signal_connect(butt, "toggled",
							 G_CALLBACK(pushChoice), b);
			wlibAddHelpString(butt, helpStr);
    	}
    }

    if (valueP) {
        wToggleSetValue(b, *valueP);
    }

    if ((option & BC_NOBORDER)==0) {
        b->repaintProc = choiceRepaint;
        b->w += 2;
        b->h += 2;
    }

    if (!b->fromTemplate){
    	gtk_fixed_put(GTK_FIXED(parent->widget), b->widget, b->realX, b->realY);
    	wlibControlGetSize((wControl_p)b);
    }

    if (labelStr) {
        b->labelW = wlibAddLabel((wControl_p)b, labelStr);
    }

	gtk_widget_show(b->widget);
	wlibAddButton((wControl_p)b);

    return b;
}
