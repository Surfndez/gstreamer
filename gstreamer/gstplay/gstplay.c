#include <config.h>

#include "gstplay.h"
#include "gstplayprivate.h"
#include "full-screen.h"

static void gst_play_class_init       (GstPlayClass *klass);
static void gst_play_init             (GstPlay *play);

static void gst_play_set_arg	      (GtkObject *object, GtkArg *arg, guint id);
static void gst_play_get_arg	      (GtkObject *object, GtkArg *arg, guint id);

static void gst_play_realize	      (GtkWidget *play);

static void gst_play_frame_displayed  (GstElement *element, GstPlay *play);
static void gst_play_have_size 	      (GstElement *element, guint width, guint height, GstPlay *play);
static void gst_play_audio_handoff    (GstElement *element, GstPlay *play);

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
static guint gst_play_signals[LAST_SIGNAL] = {0};

GtkType
gst_play_get_type (void)
{
	static GtkType play_type = 0;

	if (!play_type) {
		static const GtkTypeInfo play_info = {
			"GstPlay",
			sizeof (GstPlay),
			sizeof (GstPlayClass),
			(GtkClassInitFunc) gst_play_class_init,
			(GtkObjectInitFunc) gst_play_init,
			NULL,
			NULL,
			(GtkClassInitFunc)NULL,
		};
		play_type = gtk_type_unique (gtk_hbox_get_type (), &play_info);
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
		gtk_signal_new ("playing_state_changed", GTK_RUN_FIRST, object_class->type,
				GTK_SIGNAL_OFFSET (GstPlayClass, state_changed),
				gtk_marshal_NONE__INT, GTK_TYPE_NONE, 1,
				GTK_TYPE_INT);

	gst_play_signals[SIGNAL_FRAME_DISPLAYED] =
		gtk_signal_new ("frame_displayed",GTK_RUN_FIRST, object_class->type,
				GTK_SIGNAL_OFFSET (GstPlayClass, frame_displayed),
				gtk_marshal_NONE__NONE, GTK_TYPE_NONE, 0);

	gst_play_signals[SIGNAL_AUDIO_PLAYED] =
		gtk_signal_new ("audio_played",GTK_RUN_FIRST, object_class->type,
				GTK_SIGNAL_OFFSET (GstPlayClass, audio_played),
				gtk_marshal_NONE__NONE, GTK_TYPE_NONE, 0);

	gtk_object_class_add_signals (object_class, gst_play_signals, LAST_SIGNAL);
	gtk_object_add_arg_type ("GstPlay::uri", GTK_TYPE_STRING,
				 GTK_ARG_READABLE, ARG_URI);
	gtk_object_add_arg_type ("GstPlay::mute", GTK_TYPE_BOOL,
				 GTK_ARG_READWRITE, ARG_MUTE);
	gtk_object_add_arg_type ("GstPlay::state", GTK_TYPE_INT,
				 GTK_ARG_READABLE, ARG_STATE);
	gtk_object_add_arg_type ("GstPlay::media_size", GTK_TYPE_ULONG,
				 GTK_ARG_READABLE, ARG_MEDIA_SIZE);
	gtk_object_add_arg_type ("GstPlay::media_offset", GTK_TYPE_ULONG,
				 GTK_ARG_READABLE, ARG_MEDIA_OFFSET);
	gtk_object_add_arg_type ("GstPlay::media_total_time", GTK_TYPE_ULONG,
				 GTK_ARG_READABLE, ARG_MEDIA_TOTAL_TIME);
	gtk_object_add_arg_type ("GstPlay::media_current_time", GTK_TYPE_ULONG,
				 GTK_ARG_READABLE, ARG_MEDIA_CURRENT_TIME);

	object_class->set_arg = gst_play_set_arg;
	object_class->get_arg = gst_play_get_arg;

	widget_class->realize = gst_play_realize;

}

static void
gst_play_init (GstPlay *play)
{
	GstPlayPrivate *priv = g_new0 (GstPlayPrivate, 1);
	GstElement *colorspace;

	play->priv = priv;

	/* create a new bin to hold the elements */
	priv->thread = gst_thread_new ("main_thread");
	g_assert (priv->thread != NULL);
	priv->bin = gst_bin_new ("main_bin");
	g_assert (priv->bin != NULL);

	priv->audio_element = gst_elementfactory_make ("osssink", "play_audio");
	g_return_if_fail (priv->audio_element != NULL);
	gtk_signal_connect (GTK_OBJECT (priv->audio_element), "handoff",
			    GTK_SIGNAL_FUNC (gst_play_audio_handoff), play);

	priv->video_element = gst_elementfactory_make ("bin", "video_bin");
  
	priv->video_show = gst_elementfactory_make ("xvideosink", "show");
	g_return_if_fail (priv->video_show != NULL);
	//gtk_object_set (GTK_OBJECT (priv->video_element), "xv_enabled", FALSE, NULL);
	gtk_signal_connect (GTK_OBJECT (priv->video_show), "frame_displayed",
			    GTK_SIGNAL_FUNC (gst_play_frame_displayed), play);
	gtk_signal_connect (GTK_OBJECT (priv->video_show), "have_size",
			    GTK_SIGNAL_FUNC (gst_play_have_size), play);

	colorspace = gst_elementfactory_make ("colorspace", "colorspace");
	gst_bin_add (GST_BIN (priv->video_element), colorspace);
	gst_bin_add (GST_BIN (priv->video_element), priv->video_show);

	gst_element_connect (colorspace, "src", priv->video_show, "sink");
	gst_element_add_ghost_pad (priv->video_element, 
				   gst_element_get_pad (colorspace, "sink"),
				   "sink");

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
	GST_DEBUG(0, "gstplay: eos reached\n");
	gst_play_stop (play);
}

static void
gst_play_have_size (GstElement *element, guint width, guint height,
		    GstPlay *play)
{
	GstPlayPrivate *priv;

	priv = (GstPlayPrivate *) play->priv;

	priv->source_width = width;
	priv->source_height = height;

	gtk_widget_set_usize (priv->video_widget, width, height);
}

static void
gst_play_frame_displayed (GstElement *element, GstPlay *play)
{
	GstPlayPrivate *priv;
	static int stolen = FALSE;

	priv = (GstPlayPrivate *)play->priv;

	gdk_threads_enter ();
	if (!stolen) {
		gtk_widget_realize (priv->video_widget);
		gtk_socket_steal (GTK_SOCKET (priv->video_widget),
				  gst_util_get_int_arg (GTK_OBJECT (priv->video_show), "xid"));
		gtk_widget_show (priv->video_widget);
		stolen = TRUE;
	}
	gdk_threads_leave ();

	gtk_signal_emit (GTK_OBJECT (play), gst_play_signals[SIGNAL_FRAME_DISPLAYED],
			 NULL);
}

static void
gst_play_audio_handoff (GstElement *element, GstPlay *play)
{
	gtk_signal_emit (GTK_OBJECT (play), gst_play_signals[SIGNAL_AUDIO_PLAYED],
			 NULL);
}

static void
gst_play_object_introspect (GstObject *object, const gchar *property, GstElement **target)
{
	gchar *info;
	GtkArgInfo *arg;
	GstElement *element;

	if (!GST_IS_ELEMENT (object))
		return;

	element = GST_ELEMENT (object);

	info = gtk_object_arg_get_info (GTK_OBJECT_TYPE (element), property, &arg);

	if (info) {
		g_free(info);
	}
	else {
		*target = element;
		GST_DEBUG(0, "gstplay: using element \"%s\" for %s property\n",
			  gst_element_get_name(element), property);
	}
}

/* Dumb introspection of the interface...
 * this will change with glib 1.4
 * */
static void
gst_play_object_added (GstAutoplug* autoplug, GstObject *object, GstPlay *play)
{
	GstPlayPrivate *priv;

	g_return_if_fail (play != NULL);

	priv = (GstPlayPrivate *)play->priv;

	if (GST_FLAG_IS_SET (object, GST_ELEMENT_NO_SEEK)) {
		priv->can_seek = FALSE;
	}

	if (GST_IS_BIN (object)) {
		//gtk_signal_connect (GTK_OBJECT (object), "object_added", gst_play_object_added, play);
	}
	else {
		// first come first serve here...
		if (!priv->offset_element)
			gst_play_object_introspect (object, "offset", &priv->offset_element);
		if (!priv->bit_rate_element)
			gst_play_object_introspect (object, "bit_rate", &priv->bit_rate_element);
		if (!priv->media_time_element)
			gst_play_object_introspect (object, "media_time", &priv->media_time_element);
		if (!priv->current_time_element)
			gst_play_object_introspect (object, "current_time", &priv->current_time_element);
	}
}

static void
gst_play_have_type (GstElement *sink, GstElement *sink2, gpointer data)
{
	GST_DEBUG (0,"GstPipeline: play have type %p\n", (gboolean *)data);

	*(gboolean *)data = TRUE;
}

static GstCaps*
gst_play_typefind (GstBin *bin, GstElement *element)
{
	gboolean found = FALSE;
	GstElement *typefind;
	GstCaps *caps = NULL;

	GST_DEBUG (0, "GstPipeline: typefind for element \"%s\" %p\n",
		   GST_ELEMENT_NAME (element), &found);

	typefind = gst_elementfactory_make ("typefind", "typefind");
	g_return_val_if_fail (typefind != NULL, FALSE);

	gtk_signal_connect (GTK_OBJECT (typefind), "have_type",
			    GTK_SIGNAL_FUNC (gst_play_have_type), &found);

	gst_pad_connect (gst_element_get_pad (element, "src"),
			 gst_element_get_pad (typefind, "sink"));

	gst_bin_add (bin, typefind);

	gst_element_set_state (GST_ELEMENT (bin), GST_STATE_PLAYING);

	// push a buffer... the have_type signal handler will set the found flag
	gst_bin_iterate (bin);

	gst_element_set_state (GST_ELEMENT (bin), GST_STATE_NULL);

	if (found) {
		caps = gst_util_get_pointer_arg (GTK_OBJECT (typefind), "caps");

		gst_pad_set_caps (gst_element_get_pad (element, "src"), caps);
	}

	gst_pad_disconnect (gst_element_get_pad (element, "src"),
			    gst_element_get_pad (typefind, "sink"));
	gst_bin_remove (bin, typefind);
	gst_object_unref (GST_OBJECT (typefind));

	return caps;
}

static gboolean
connect_pads (GstElement *new_element, GstElement *target, gboolean add)
{
	GList *pads = gst_element_get_pad_list (new_element);
	GstPad *targetpad = gst_element_get_pad (target, "sink");

	while (pads) {
		GstPad *pad = GST_PAD (pads->data);

		if (gst_pad_check_compatibility (pad, targetpad)) {
			if (add) {
				gst_bin_add (GST_BIN (gst_element_get_parent (
					GST_ELEMENT (gst_pad_get_real_parent (pad)))),
					     target);
			}
			gst_pad_connect (pad, targetpad);
			return TRUE;
		}
		pads = g_list_next (pads);
	}
	return FALSE;
}

GstPlayReturn
gst_play_set_uri (GstPlay *play, const guchar *uri)
{
	GstPlayPrivate *priv;
	GstCaps *src_caps;
	GstElement *new_element;
	GstAutoplug *autoplug;

	g_return_val_if_fail (play != NULL, GST_PLAY_ERROR);
	g_return_val_if_fail (GST_IS_PLAY (play), GST_PLAY_ERROR);
	g_return_val_if_fail (uri != NULL, GST_PLAY_ERROR);

	priv = (GstPlayPrivate *)play->priv;

	if (priv->uri)
		g_free (priv->uri);

	priv->uri = g_strdup (uri);

	priv->src = gst_elementfactory_make ("disksrc", "disk_src");
	//priv->src = gst_elementfactory_make ("dvdsrc", "disk_src");
	priv->offset_element = priv->src;

	g_return_val_if_fail (priv->src != NULL, -1);
	gtk_object_set (GTK_OBJECT (priv->src), "location", uri, NULL);

	gst_bin_add (GST_BIN (priv->bin), priv->src);

	src_caps = gst_play_typefind (GST_BIN (priv->bin), priv->src);

	if (!src_caps) {
		return GST_PLAY_UNKNOWN_MEDIA;
	}

	autoplug = gst_autoplugfactory_make ("staticrender");
	g_assert (autoplug != NULL);

	gtk_signal_connect (GTK_OBJECT (autoplug), "new_object", gst_play_object_added, play);

	new_element = gst_autoplug_to_renderers (autoplug,
						 gst_pad_get_caps (gst_element_get_pad (priv->src, "src")),
						 priv->video_element,
						 priv->audio_element,
						 NULL);

	if (!new_element) {
		return GST_PLAY_CANNOT_PLAY;
	}

	gst_bin_remove (GST_BIN (priv->bin), priv->src);
	gst_bin_add (GST_BIN (priv->thread), priv->src);

	gst_bin_add (GST_BIN (priv->bin), new_element);

	gst_element_connect (priv->src, "src", new_element, "sink");

	gst_bin_add (GST_BIN (priv->thread), priv->bin);
	gtk_signal_connect (GTK_OBJECT (priv->thread), "eos", GTK_SIGNAL_FUNC (gst_play_eos), play);

	return GST_PLAY_OK;
}

static void
gst_play_realize (GtkWidget *widget)
{
	GstPlay *play;
	GstPlayPrivate *priv;

	g_return_if_fail (GST_IS_PLAY (widget));

	//g_print ("realize\n");

	play = GST_PLAY (widget);
	priv = (GstPlayPrivate *)play->priv;

	priv->video_widget = gtk_socket_new ();

	gtk_container_add (GTK_CONTAINER (widget), priv->video_widget);

	if (GTK_WIDGET_CLASS (parent_class)->realize) {
		GTK_WIDGET_CLASS (parent_class)->realize (widget);
	}

	//gtk_socket_steal (GTK_SOCKET (priv->video_widget),
	//             gst_util_get_int_arg (GTK_OBJECT(priv->video_element), "xid"));

	//gtk_widget_realize (priv->video_widget);
	//gtk_socket_steal (GTK_SOCKET (priv->video_widget),
	//             gst_util_get_int_arg (GTK_OBJECT(priv->video_element), "xid"));
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

	// FIXME until state changes are handled properly
	gst_element_set_state (GST_ELEMENT (priv->thread),GST_STATE_READY);
	gtk_object_set (GTK_OBJECT (priv->src),"offset",0,NULL);
	//gst_element_set_state (GST_ELEMENT (priv->thread),GST_STATE_NULL);

	play->state = GST_PLAY_STOPPED;

	gtk_signal_emit (GTK_OBJECT (play), gst_play_signals[SIGNAL_STATE_CHANGED],
			 play->state);
}

void
gst_play_set_display_size (GstPlay *play, gint display_preference)
{
	GstPlayPrivate *priv;

	g_return_if_fail (play != NULL);
	g_return_if_fail (GST_IS_PLAY (play));

	priv = (GstPlayPrivate *)play->priv;
  
	if (display_preference == 0) {
		gtk_widget_set_usize (GTK_WIDGET (priv->video_widget), priv->source_width, priv->source_height);
	}
	else if (display_preference == 1) {
		gtk_widget_set_usize (GTK_WIDGET (priv->video_widget), priv->source_width * 2, priv->source_height * 2);
	}
	else if (display_preference == 2) {
		GtkWidget *fs;
		GstPlay *fs_play;
	  
		fs = full_screen_new ();

		fs_play = full_screen_get_gst_play (FULL_SCREEN (fs));

		if (priv->uri != NULL) {
			//gst_play_stop (play);
			full_screen_set_uri (FULL_SCREEN (fs), fs_play, priv->uri);

			gtk_widget_show (fs);
		}
	}
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

	if (priv->offset_element)
		return gst_util_get_long_arg (GTK_OBJECT (priv->offset_element), "offset");
	else
		return 0;
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
gst_play_media_seek (GstPlay *play, gulong offset)
{
	GstPlayPrivate *priv;

	g_return_if_fail (play != NULL);
	g_return_if_fail (GST_IS_PLAY (play));

	priv = (GstPlayPrivate *)play->priv;

	gtk_object_set (GTK_OBJECT (priv->src), "offset", offset, NULL);
}

GstElement*
gst_play_get_pipeline (GstPlay *play)
{
	GstPlayPrivate *priv;

	g_return_val_if_fail (play != NULL, NULL);
	g_return_val_if_fail (GST_IS_PLAY (play), NULL);

	priv = (GstPlayPrivate *)play->priv;

	return GST_ELEMENT (priv->bin);
}

static void
gst_play_set_arg (GtkObject *object, GtkArg *arg, guint id)
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
gst_play_get_arg (GtkObject *object, GtkArg *arg, guint id)
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
