/*
 * Initial main.c file generated by Glade. Edit as required.
 * Glade will not overwrite this file.
 */

#include <config.h>

#include <gnome.h>
#include "gstmediaplay.h"
#include "callbacks.h"

static void gst_media_play_class_init (GstMediaPlayClass *klass);
static void gst_media_play_init       (GstMediaPlay *play);

static void gst_media_play_set_arg    (GtkObject *object,GtkArg *arg,guint id);
static void gst_media_play_get_arg    (GtkObject *object,GtkArg *arg,guint id);

static void update_buttons            (GstMediaPlay *mplay, GstPlayState state);
static void update_slider             (GtkAdjustment *adjustment, gfloat value);

/* signals and args */
enum {
  LAST_SIGNAL
};

enum {
  ARG_0,
};

static void
target_drag_data_received  (GtkWidget          *widget,
                            GdkDragContext     *context,
                            gint                x,
                            gint                y,
                            GtkSelectionData   *data,
                            guint               info,
                            guint               time)
{
  if (strstr (data->data, "file:")) {
    g_print ("Got: %s\n",data->data);
  }
}

static GtkTargetEntry target_table[] = {
  { "text/plain", 0, 0 }
};

static GtkObject *parent_class = NULL;
static guint gst_media_play_signals[LAST_SIGNAL] = { 0 };

GtkType 
gst_media_play_get_type(void) 
{
  static GtkType play_type = 0;

  if (!play_type) {
    static const GtkTypeInfo play_info = {
      "GstMediaPlay",
      sizeof(GstMediaPlay),
      sizeof(GstMediaPlayClass),
      (GtkClassInitFunc)gst_media_play_class_init,
      (GtkObjectInitFunc)gst_media_play_init,
      NULL,
      NULL,
      (GtkClassInitFunc)NULL,
    };
    play_type = gtk_type_unique(gtk_object_get_type(),&play_info);
  }
  return play_type;
}

static void 
gst_media_play_class_init (GstMediaPlayClass *klass) 
{
  GtkObjectClass *object_class;

  parent_class = gtk_type_class (gtk_object_get_type ());

  object_class = (GtkObjectClass*)klass;

  object_class->set_arg = gst_media_play_set_arg;
  object_class->get_arg = gst_media_play_get_arg;
}

typedef struct {
  GstMediaPlay *play;
  GModule *symbols;
} connect_struct;

/* we need more control here so... */
static void 
gst_media_play_connect_func (const gchar *handler_name,
                             GtkObject *object,
                             const gchar *signal_name,
                             const gchar *signal_data,
                             GtkObject *connect_object,
                             gboolean after,
                             gpointer user_data)
{
  GtkSignalFunc func;
  connect_struct *data = (connect_struct *)user_data;

  if (!g_module_symbol (data->symbols, handler_name, (gpointer *)&func))
    g_warning("gsteditorproperty: could not find signal handler '%s'.", handler_name);
  else {
    if (after)
      gtk_signal_connect_after (object, signal_name, func, (gpointer) data->play);
    else
      gtk_signal_connect (object, signal_name, func, (gpointer) data->play);
  }
}


static void 
gst_media_play_init(GstMediaPlay *mplay) 
{
  GtkWidget *gstplay;
  GModule *symbols;
  connect_struct data;

  glade_init();
  glade_gnome_init();

  g_print("using %s\n", DATADIR"gstmediaplay.glade");
  /* load the interface */
  mplay->xml = glade_xml_new (DATADIR "gstmediaplay.glade", "gstplay");

  mplay->play_button = glade_xml_get_widget (mplay->xml, "toggle_play");
  mplay->pause_button = glade_xml_get_widget (mplay->xml, "toggle_pause");
  mplay->stop_button = glade_xml_get_widget (mplay->xml, "toggle_stop");

  gstplay = glade_xml_get_widget (mplay->xml, "gstplay");
  gtk_drag_dest_set (gstplay,
                     GTK_DEST_DEFAULT_ALL,
	             target_table, 1,
	             GDK_ACTION_COPY);
  gtk_signal_connect (GTK_OBJECT (gstplay), "drag_data_received",
	              GTK_SIGNAL_FUNC (target_drag_data_received),
	              NULL);

  mplay->play = gst_play_new();

  gnome_dock_set_client_area (GNOME_DOCK (glade_xml_get_widget(mplay->xml, "dock1")),
		  GTK_WIDGET (mplay->play));

  gtk_widget_show (GTK_WIDGET (mplay->play));

  mplay->status = gst_status_area_new();
  gst_status_area_set_state (mplay->status, GST_STATUS_AREA_STATE_INIT);
  gst_status_area_set_playtime (mplay->status, "00:00 / 00:00");

  symbols = g_module_open (NULL, 0);

  data.play = mplay;
  data.symbols = symbols;

  glade_xml_signal_autoconnect_full (mplay->xml, gst_media_play_connect_func, &data);

  gtk_container_add (GTK_CONTAINER (glade_xml_get_widget (mplay->xml, "dockitem4")),
		  GTK_WIDGET (mplay->status));
  gtk_widget_show (GTK_WIDGET (mplay->status));

}

GstMediaPlay *
gst_media_play_new () 
{
  return GST_MEDIA_PLAY (gtk_type_new (GST_TYPE_MEDIA_PLAY));
}

void 
gst_media_play_start_uri (GstMediaPlay *play, 
		          const guchar *uri) 
{
  GstPlayReturn ret;

  g_return_if_fail (play != NULL);
  g_return_if_fail (GST_IS_MEDIA_PLAY (play));

  if (uri != NULL) {
    ret = gst_play_set_uri (play->play, uri);
    gst_status_area_set_state (play->status, GST_STATUS_AREA_STATE_PLAYING);
    gst_play_play (play->play);
    update_buttons (play, GST_PLAY_STATE (play->play));
  }
}

static void 
gst_media_play_set_arg (GtkObject *object,
		        GtkArg *arg,
		        guint id) 
{
  GstMediaPlay *play;
  play = GST_MEDIA_PLAY (object);

  switch (id) {
    default:
      g_warning("GstMediaPlay: unknown arg!");
      break;
  }
}

static void 
gst_media_play_get_arg (GtkObject *object,
		        GtkArg *arg,
		        guint id) 
{
  GstMediaPlay *play;

  play = GST_MEDIA_PLAY (object);

  switch (id) {
    default:
      arg->type = GTK_TYPE_INVALID;
      break;
  }
}

void
on_toggle_play_toggled (GtkToggleButton *togglebutton,
                        GstMediaPlay    *play)
{
  gst_status_area_set_state (play->status, GST_STATUS_AREA_STATE_PLAYING);
  gst_play_play (play->play);
  update_buttons (play, GST_PLAY_STATE (play->play));
}

void
on_toggle_pause_toggled (GtkToggleButton *togglebutton,
                         GstMediaPlay    *play)
{
  gst_status_area_set_state (play->status, GST_STATUS_AREA_STATE_PAUSED);
  gst_play_pause (play->play);
  update_buttons (play, GST_PLAY_STATE (play->play));
}

void
on_toggle_stop_toggled (GtkToggleButton *togglebutton,
                        GstMediaPlay    *play)
{
  gst_status_area_set_state (play->status, GST_STATUS_AREA_STATE_STOPPED);
  gst_play_stop (play->play);
  update_buttons (play, GST_PLAY_STATE (play->play));
}

static void 
update_buttons (GstMediaPlay *mplay, 
		GstPlayState state) 
{
  gtk_signal_handler_block_by_func (GTK_OBJECT (mplay->play_button),
                         GTK_SIGNAL_FUNC (on_toggle_play_toggled),
	                 mplay);
  gtk_signal_handler_block_by_func (GTK_OBJECT (mplay->pause_button),
                         GTK_SIGNAL_FUNC (on_toggle_pause_toggled),
	                 mplay);
  gtk_signal_handler_block_by_func (GTK_OBJECT (mplay->stop_button),
                         GTK_SIGNAL_FUNC (on_toggle_stop_toggled),
	                 mplay);

  gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (mplay->play_button), FALSE);
  gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (mplay->pause_button), FALSE);
  gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (mplay->stop_button), FALSE);

  if (state == GST_PLAY_PLAYING) {
    gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (mplay->play_button), TRUE);
  }
  else if (state == GST_PLAY_PAUSED) {
    gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (mplay->pause_button), TRUE);
  }
  else if (state == GST_PLAY_STOPPED) {
    gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (mplay->stop_button), TRUE);
  }

  gtk_signal_handler_unblock_by_func (GTK_OBJECT (mplay->play_button),
                         GTK_SIGNAL_FUNC (on_toggle_play_toggled),
	                 mplay);
  gtk_signal_handler_unblock_by_func (GTK_OBJECT (mplay->pause_button),
                         GTK_SIGNAL_FUNC (on_toggle_pause_toggled),
	                 mplay);
  gtk_signal_handler_unblock_by_func (GTK_OBJECT (mplay->stop_button),
                         GTK_SIGNAL_FUNC (on_toggle_stop_toggled),
	                 mplay);
}

static void 
update_slider (GtkAdjustment *adjustment, 
	       gfloat value) 
{
  gtk_signal_handler_block_by_func (GTK_OBJECT (adjustment),
                         GTK_SIGNAL_FUNC (on_hscale1_value_changed),
	                 NULL);
  gtk_adjustment_set_value (adjustment, value);
  gtk_signal_handler_unblock_by_func (GTK_OBJECT (adjustment),
                         GTK_SIGNAL_FUNC (on_hscale1_value_changed),
	                 NULL);
}

