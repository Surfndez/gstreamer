/*
 * DO NOT EDIT THIS FILE - it is generated by Glade.
 */

#ifdef HAVE_CONFIG_H
#  include <config.h>
#endif

#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <string.h>

#include <gnome.h>

#include "callbacks.h"
#include "interface.h"

extern GtkWidget *play_button;
extern GtkWidget *pause_button;
extern GtkWidget *stop_button;
extern guchar statusline[];
extern guchar *statustext;

void update_buttons(GstPlayState state) 
{
  gtk_signal_handler_block_by_func(GTK_OBJECT(play_button),
                         GTK_SIGNAL_FUNC (on_toggle_play_toggled),
	                 NULL);
  gtk_signal_handler_block_by_func(GTK_OBJECT(pause_button),
                         GTK_SIGNAL_FUNC (on_toggle_pause_toggled),
	                 NULL);
  gtk_signal_handler_block_by_func(GTK_OBJECT(stop_button),
                         GTK_SIGNAL_FUNC (on_toggle_stop_toggled),
	                 NULL);

  gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(play_button), FALSE);
  gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(pause_button), FALSE);
  gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(stop_button), FALSE);

  if (state == GST_PLAY_PLAYING) {
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(play_button), TRUE);
  }
  else if (state == GST_PLAY_PAUSED) {
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(pause_button), TRUE);
  }
  else if (state == GST_PLAY_STOPPED) {
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(stop_button), TRUE);
  }

  gtk_signal_handler_unblock_by_func(GTK_OBJECT(play_button),
                         GTK_SIGNAL_FUNC (on_toggle_play_toggled),
	                 NULL);
  gtk_signal_handler_unblock_by_func(GTK_OBJECT(pause_button),
                         GTK_SIGNAL_FUNC (on_toggle_pause_toggled),
	                 NULL);
  gtk_signal_handler_unblock_by_func(GTK_OBJECT(stop_button),
                         GTK_SIGNAL_FUNC (on_toggle_stop_toggled),
	                 NULL);
}

void update_slider(GtkAdjustment *adjustment, gfloat value) 
{
  gtk_signal_handler_block_by_func(GTK_OBJECT(adjustment),
                         GTK_SIGNAL_FUNC (on_hscale1_value_changed),
	                 NULL);
  gtk_adjustment_set_value(adjustment, value);
  gtk_signal_handler_unblock_by_func(GTK_OBJECT(adjustment),
                         GTK_SIGNAL_FUNC (on_hscale1_value_changed),
	                 NULL);
}

