#ifndef __GST_MEDIA_PLAY_H__
#define __GST_MEDIA_PLAY_H__

#include <glade/glade.h>
#include "gstplay.h"
#include "gststatusarea.h"

#define GST_TYPE_MEDIA_PLAY           (gst_media_play_get_type())
#define GST_MEDIA_PLAY(obj)           (GTK_CHECK_CAST ((obj), GST_TYPE_MEDIA_PLAY, GstMediaPlay))
#define GST_MEDIA_PLAY_CLASS(klass)   (GTK_CHECK_CLASS_CAST ((klass), GST_TYPE_MEDIA_PLAY, GstMediaPlayClass))
#define GST_IS_MEDIA_PLAY(obj)        (GTK_CHECK_TYPE ((obj), GST_TYPE_MEDIA_PLAY))
#define GST_IS_MEDIA_PLAY_CLASS(obj)  (GTK_CHECK_CLASS_TYPE ((klass), GST_TYPE_MEDIA_PLAY))

typedef struct _GstMediaPlay GstMediaPlay;
typedef struct _GstMediaPlayClass GstMediaPlayClass;

struct _GstMediaPlay {
	GtkObject parent;

	GladeXML *xml;
	GladeXML *playlist_xml;
	GstPlay *play;
	
	GtkWidget *play_button;
	GtkWidget *pause_button;
	GtkWidget *stop_button;
	GtkWidget *window;
	
	GstStatusArea *status;
	
	// the slider
	GtkAdjustment *adjustment;
	GtkWidget *slider;

	// the playlist
	GtkWidget *playlist_window;
	GtkWidget *playlist_clist;

	guint fullscreen_connection_id;
	
	gulong last_time;

	gint x, y, width, height;
};

struct _GstMediaPlayClass {
	GtkObjectClass parent_class;
};


GtkType 	gst_media_play_get_type		(void);

/* setup the player */
GstMediaPlay*	gst_media_play_new		  (void);

void 		gst_media_play_start_uri	  (GstMediaPlay *play, const guchar *uri);

void            gst_media_play_show_playlist      (GstMediaPlay *mplay);
void            gst_media_play_addto_playlist     (GstMediaPlay *mplay, char *uri);

void            gst_media_play_set_original_size  (GstMediaPlay *mplay);
void            gst_media_play_set_double_size    (GstMediaPlay *mplay);
void            gst_media_play_set_fullscreen     (GstMediaPlay *mplay);

#endif /* __GST_MEDIA_PLAY_H__ */
