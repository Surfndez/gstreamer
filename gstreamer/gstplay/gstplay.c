/*
 * Initial main.c file generated by Glade. Edit as required.
 * Glade will not overwrite this file.
 */

#include <config.h>

#include "gstplay.h"
#include "gstplayprivate.h"

static void gst_play_class_init         (GstPlayClass *klass);
static void gst_play_init               (GstPlay *play);

static void gst_play_set_arg		(GtkObject *object,GtkArg *arg,guint id);
static void gst_play_get_arg		(GtkObject *object,GtkArg *arg,guint id);

static void gst_play_realize		(GtkWidget *play);

static void gst_play_frame_displayed	(GstElement *element, GstPlay *play);
static void gst_play_audio_handoff	(GstElement *element, GstPlay *play);

/* signals and args */
enum {
  SIGNAL_STATE_CHANGED,
  SIGNAL_FRAME_DISPLAYED,
  SIGNAL_AUDIO_PLAYED,
  LAST_SIGNAL
};

enum {
  ARG_0,
  ARG_URI,
  ARG_MUTE,
  ARG_STATE,
  ARG_MEDIA_SIZE,
  ARG_MEDIA_OFFSET,
  ARG_MEDIA_TOTAL_TIME,
  ARG_MEDIA_CURRENT_TIME,
};

static GtkObject *parent_class = NULL;
static guint gst_play_signals[LAST_SIGNAL] = { 0 };

GtkType 
gst_play_get_type (void) 
{
  static GtkType play_type = 0;

  if (!play_type) {
    static const GtkTypeInfo play_info = {
      "GstPlay",
      sizeof(GstPlay),
      sizeof(GstPlayClass),
      (GtkClassInitFunc)gst_play_class_init,
      (GtkObjectInitFunc)gst_play_init,
      NULL,
      NULL,
      (GtkClassInitFunc)NULL,
    };
    play_type = gtk_type_unique (gtk_hbox_get_type (),&play_info);
  }
  return play_type;
}

static void 
gst_play_class_init (GstPlayClass *klass) 
{
  GtkObjectClass *object_class;
  GtkWidgetClass *widget_class;

  parent_class = gtk_type_class (gtk_hbox_get_type ());

  object_class = (GtkObjectClass*)klass;
  widget_class = (GtkWidgetClass*)klass;

  gst_play_signals[SIGNAL_STATE_CHANGED] =
    gtk_signal_new ("playing_state_changed",GTK_RUN_FIRST,object_class->type,
                    GTK_SIGNAL_OFFSET (GstPlayClass,state_changed),
                    gtk_marshal_NONE__INT,GTK_TYPE_NONE,1,
                    GTK_TYPE_INT);

  gst_play_signals[SIGNAL_FRAME_DISPLAYED] =
    gtk_signal_new ("frame_displayed",GTK_RUN_FIRST,object_class->type,
                    GTK_SIGNAL_OFFSET (GstPlayClass,frame_displayed),
                    gtk_marshal_NONE__NONE,GTK_TYPE_NONE,0);

  gst_play_signals[SIGNAL_AUDIO_PLAYED] =
    gtk_signal_new ("audio_played",GTK_RUN_FIRST,object_class->type,
                    GTK_SIGNAL_OFFSET (GstPlayClass,audio_played),
                    gtk_marshal_NONE__NONE,GTK_TYPE_NONE,0);

  gtk_object_class_add_signals (object_class,gst_play_signals,LAST_SIGNAL);
  gtk_object_add_arg_type ("GstPlay::uri",GTK_TYPE_STRING,
                           GTK_ARG_READABLE, ARG_URI);
  gtk_object_add_arg_type ("GstPlay::mute",GTK_TYPE_BOOL,
                           GTK_ARG_READWRITE, ARG_MUTE);
  gtk_object_add_arg_type ("GstPlay::state",GTK_TYPE_INT,
                           GTK_ARG_READABLE, ARG_STATE);
  gtk_object_add_arg_type ("GstPlay::media_size",GTK_TYPE_ULONG,
                           GTK_ARG_READABLE, ARG_MEDIA_SIZE);
  gtk_object_add_arg_type ("GstPlay::media_offset",GTK_TYPE_ULONG,
                           GTK_ARG_READABLE, ARG_MEDIA_OFFSET);
  gtk_object_add_arg_type ("GstPlay::media_total_time",GTK_TYPE_ULONG,
                           GTK_ARG_READABLE, ARG_MEDIA_TOTAL_TIME);
  gtk_object_add_arg_type ("GstPlay::media_current_time",GTK_TYPE_ULONG,
                           GTK_ARG_READABLE, ARG_MEDIA_CURRENT_TIME);

  object_class->set_arg = gst_play_set_arg;
  object_class->get_arg = gst_play_get_arg;

  widget_class->realize = gst_play_realize; 

}

static void 
gst_play_init (GstPlay *play) 
{

  GstPlayPrivate *priv = g_new0 (GstPlayPrivate, 1);

  play->priv = priv;

  /* create a new bin to hold the elements */
  priv->thread = gst_thread_new ("main_thread");
  g_assert (priv->thread != NULL);
  priv->pipeline = gst_pipeline_new ("main_pipeline");
  g_assert (priv->pipeline != NULL);

  /* and an audio sink */
  priv->audio_play = gst_elementfactory_make ("audiosink","play_audio");
  g_return_if_fail (priv->audio_play != NULL);
  gtk_signal_connect (GTK_OBJECT (priv->audio_play), "handoff", 
		  GTK_SIGNAL_FUNC (gst_play_audio_handoff), play);

  /* and a video sink */
  priv->video_show = gst_elementfactory_make ("videosink","show");
  g_return_if_fail (priv->video_show != NULL);
  gtk_object_set (GTK_OBJECT (priv->video_show),"xv_enabled",FALSE,NULL);
  gtk_signal_connect (GTK_OBJECT (priv->video_show), "frame_displayed", 
		  GTK_SIGNAL_FUNC (gst_play_frame_displayed), play);

  gst_pipeline_add_sink (GST_PIPELINE (priv->pipeline), priv->audio_play);
  gst_pipeline_add_sink (GST_PIPELINE (priv->pipeline), priv->video_show);

  gst_bin_add (GST_BIN (priv->thread), priv->pipeline);

  play->state = GST_PLAY_STOPPED;
  play->flags = 0;

  priv->src = NULL;
  priv->muted = FALSE;
  priv->can_seek = TRUE;
  priv->uri = NULL;
  priv->offset_element = NULL;
  priv->bit_rate_element = NULL;
  priv->media_time_element = NULL;
}

GstPlay *
gst_play_new () 
{
  return GST_PLAY (gtk_type_new (GST_TYPE_PLAY));
}

static void 
gst_play_eos (GstElement *element, 
	      GstPlay *play) 
{
  g_print("gstplay: eos reached\n");
  gst_play_stop(play);
}

static void 
gst_play_frame_displayed (GstElement *element, 
		          GstPlay *play) 
{
  gtk_signal_emit (GTK_OBJECT (play), gst_play_signals[SIGNAL_FRAME_DISPLAYED],
		  NULL);
}

static void 
gst_play_audio_handoff (GstElement *element, 
		        GstPlay *play) 
{
  gtk_signal_emit (GTK_OBJECT (play), gst_play_signals[SIGNAL_AUDIO_PLAYED],
		  NULL);
}

static void 
gst_play_object_introspect (GstElement *element, 
		            const gchar *property,
			    GstElement **target)
{
  gchar *info;
  GtkArgInfo *arg;

  info = gtk_object_arg_get_info( GTK_OBJECT_TYPE(element), property, &arg);

  if (info) {
    g_free(info);
  }
  else {
    *target = element;
    g_print("gstplay: using element \"%s\" for %s property\n", 
		    gst_element_get_name(element), property);
  }
}

/* Dumb introspection of the interface...
 * this will change with glib 1.4 
 * */
static void 
gst_play_object_added (GstElement *pipeline, 
		       GstElement *element,
		       GstPlay *play) 
{
  GstPlayPrivate *priv;
  
  g_return_if_fail (play != NULL);

  priv = (GstPlayPrivate *)play->priv;

  if (GST_FLAGS (element) & GST_ELEMENT_NO_SEEK) {
    priv->can_seek = FALSE;
  }

  if (GST_IS_BIN (element)) {
    gtk_signal_connect (GTK_OBJECT (element), "object_added", gst_play_object_added, play);
  }
  else {
    // first come first serve here...
    if (!priv->offset_element) 
      gst_play_object_introspect (element, "offset", &priv->offset_element);
    if (!priv->bit_rate_element) 
      gst_play_object_introspect (element, "bit_rate", &priv->bit_rate_element);
    if (!priv->media_time_element)
      gst_play_object_introspect (element, "media_time", &priv->media_time_element);
    if (!priv->current_time_element)
      gst_play_object_introspect (element, "current_time", &priv->current_time_element);
  }
}

GstPlayReturn 
gst_play_set_uri (GstPlay *play, 
		  const guchar *uri) 
{
  GstPlayPrivate *priv;

  g_return_val_if_fail (play != NULL, GST_PLAY_ERROR);
  g_return_val_if_fail (GST_IS_PLAY (play), GST_PLAY_ERROR);
  g_return_val_if_fail (uri != NULL, GST_PLAY_ERROR);
  
  priv = (GstPlayPrivate *)play->priv;

  if (priv->src) {
  }

  if (priv->uri) g_free (priv->uri);

  priv->uri = g_strdup (uri);

  priv->src = gst_elementfactory_make ("asyncdisksrc", "disk_src");
  g_return_val_if_fail (priv->src != NULL, -1);
  gtk_object_set (GTK_OBJECT (priv->src),"location",uri,NULL);
  gtk_signal_connect (GTK_OBJECT (priv->src), "eos", GTK_SIGNAL_FUNC (gst_play_eos), play);

  gtk_signal_connect (GTK_OBJECT (priv->pipeline), "object_added", gst_play_object_added, play);

  gst_pipeline_add_src (GST_PIPELINE (priv->pipeline),GST_ELEMENT (priv->src));

  if (!gst_pipeline_autoplug (GST_PIPELINE (priv->pipeline))) {
    return GST_PLAY_UNKNOWN_MEDIA;
  }

  if (GST_PAD_CONNECTED (gst_element_get_pad (priv->video_show, "sink"))) {
    play->flags |= GST_PLAY_TYPE_VIDEO;
  }
  if (GST_PAD_CONNECTED (gst_element_get_pad (priv->audio_play, "sink"))) {
    play->flags |= GST_PLAY_TYPE_AUDIO;
  }

  return GST_PLAY_OK;
}

static void 
gst_play_realize (GtkWidget *widget) 
{
  GstPlay *play;
  GtkWidget *video_widget;
  GstPlayPrivate *priv;
  
  g_return_if_fail (GST_IS_PLAY (widget));

  play = GST_PLAY (widget);
  priv = (GstPlayPrivate *)play->priv;

  video_widget = gst_util_get_widget_arg (GTK_OBJECT (priv->video_show),"widget");

  if (video_widget) {
    gtk_container_add (GTK_CONTAINER (widget), video_widget);
    gtk_widget_show (video_widget);
  }

  if (GTK_WIDGET_CLASS (parent_class)->realize) {
    GTK_WIDGET_CLASS (parent_class)->realize (widget);
  }
}

void 
gst_play_play (GstPlay *play) 
{
  GstPlayPrivate *priv;

  g_return_if_fail (play != NULL);
  g_return_if_fail (GST_IS_PLAY (play));

  priv = (GstPlayPrivate *)play->priv;

  if (play->state == GST_PLAY_PLAYING) return;

  if (play->state == GST_PLAY_STOPPED)
    gst_element_set_state (GST_ELEMENT (priv->thread),GST_STATE_READY);
  gst_element_set_state (GST_ELEMENT (priv->thread),GST_STATE_PLAYING);

  play->state = GST_PLAY_PLAYING;

  gtk_signal_emit (GTK_OBJECT (play), gst_play_signals[SIGNAL_STATE_CHANGED],
		  play->state);
}

void 
gst_play_pause (GstPlay *play) 
{
  GstPlayPrivate *priv;

  g_return_if_fail (play != NULL);
  g_return_if_fail (GST_IS_PLAY (play));

  priv = (GstPlayPrivate *)play->priv;

  if (play->state != GST_PLAY_PLAYING) return;

  gst_element_set_state (GST_ELEMENT (priv->thread),GST_STATE_PAUSED);

  play->state = GST_PLAY_PAUSED;

  gtk_signal_emit (GTK_OBJECT (play), gst_play_signals[SIGNAL_STATE_CHANGED],
		  play->state);
}

void 
gst_play_stop (GstPlay *play) 
{
  GstPlayPrivate *priv;

  g_return_if_fail (play != NULL);
  g_return_if_fail (GST_IS_PLAY (play));

  if (play->state == GST_PLAY_STOPPED) return;

  priv = (GstPlayPrivate *)play->priv;

  gst_element_set_state (GST_ELEMENT (priv->thread),GST_STATE_NULL);
  gtk_object_set (GTK_OBJECT (priv->src),"offset",0,NULL);

  play->state = GST_PLAY_STOPPED;

  gtk_signal_emit (GTK_OBJECT (play), gst_play_signals[SIGNAL_STATE_CHANGED],
		  play->state);
}

gulong          
gst_play_get_media_size (GstPlay *play) 
{
  GstPlayPrivate *priv;

  g_return_val_if_fail (play != NULL, 0);
  g_return_val_if_fail (GST_IS_PLAY (play), 0);

  priv = (GstPlayPrivate *)play->priv;

  return gst_util_get_long_arg (GTK_OBJECT (priv->src), "size");
}

gulong          
gst_play_get_media_offset (GstPlay *play)
{
  GstPlayPrivate *priv;

  g_return_val_if_fail (play != NULL, 0);
  g_return_val_if_fail (GST_IS_PLAY (play), 0);

  priv = (GstPlayPrivate *)play->priv;

  return gst_util_get_long_arg (GTK_OBJECT (priv->offset_element), "offset");
}

gulong          
gst_play_get_media_total_time (GstPlay *play)
{
  gulong total_time, bit_rate;
  GstPlayPrivate *priv;
  
  g_return_val_if_fail (play != NULL, 0);
  g_return_val_if_fail (GST_IS_PLAY (play), 0);

  priv = (GstPlayPrivate *)play->priv;

  if (priv->media_time_element) {
    return gst_util_get_long_arg (GTK_OBJECT (priv->media_time_element), "media_time");
  }

  if (priv->bit_rate_element == NULL) return 0;

  bit_rate = gst_util_get_long_arg (GTK_OBJECT (priv->bit_rate_element), "bit_rate");

  if (bit_rate) 
    total_time = (gst_play_get_media_size (play) * 8) / bit_rate;
  else
    total_time = 0;

  return total_time;
}

gulong          
gst_play_get_media_current_time (GstPlay *play)
{
  gulong current_time, bit_rate;
  GstPlayPrivate *priv;
	
  g_return_val_if_fail (play != NULL, 0);
  g_return_val_if_fail (GST_IS_PLAY (play), 0);

  priv = (GstPlayPrivate *)play->priv;

  if (priv->current_time_element) {
    return gst_util_get_long_arg (GTK_OBJECT (priv->current_time_element), "current_time");
  }

  if (priv->bit_rate_element == NULL) return 0;

  bit_rate = gst_util_get_long_arg (GTK_OBJECT (priv->bit_rate_element), "bit_rate");

  if (bit_rate) 
    current_time = (gst_play_get_media_offset (play) * 8) / bit_rate;
  else
    current_time = 0;

  return current_time;
}

gboolean        
gst_play_media_can_seek (GstPlay *play)
{
  GstPlayPrivate *priv;

  g_return_val_if_fail (play != NULL, FALSE);
  g_return_val_if_fail (GST_IS_PLAY (play), FALSE);

  priv = (GstPlayPrivate *)play->priv;

  return priv->can_seek;
}

void            
gst_play_media_seek (GstPlay *play, 
		     gulong offset)
{
  GstPlayPrivate *priv;

  g_return_if_fail (play != NULL);
  g_return_if_fail (GST_IS_PLAY (play));

  priv = (GstPlayPrivate *)play->priv;

  gtk_object_set (GTK_OBJECT (priv->src), "offset", offset, NULL);
}


static void 
gst_play_set_arg (GtkObject *object,
		  GtkArg *arg,
		  guint id) 
{
  GstPlay *play;

  g_return_if_fail (object != NULL);
  g_return_if_fail (arg != NULL);

  play = GST_PLAY (object);

  switch (id) {
    case ARG_MUTE:
      break;
    default:
      g_warning ("GstPlay: unknown arg!");
      break;
  }
}

static void 
gst_play_get_arg (GtkObject *object,
		  GtkArg *arg,
		  guint id) 
{
  GstPlay *play;
  GstPlayPrivate *priv;

  g_return_if_fail (object != NULL);
  g_return_if_fail (arg != NULL);

  play = GST_PLAY (object);
  priv = (GstPlayPrivate *)play->priv;

  switch (id) {
    case ARG_URI:
      GTK_VALUE_STRING (*arg) = priv->uri;
      break;
    case ARG_MUTE:
      GTK_VALUE_BOOL (*arg) = priv->muted;
      break;
    case ARG_STATE:
      GTK_VALUE_INT (*arg) = play->state;
      break;
    case ARG_MEDIA_SIZE:
      GTK_VALUE_LONG (*arg) = gst_play_get_media_size(play);
      break;
    case ARG_MEDIA_OFFSET:
      GTK_VALUE_LONG (*arg) = gst_play_get_media_offset(play);
      break;
    case ARG_MEDIA_TOTAL_TIME:
      break;
    case ARG_MEDIA_CURRENT_TIME:
      break;
    default:
      arg->type = GTK_TYPE_INVALID;
      break;
  }
}

