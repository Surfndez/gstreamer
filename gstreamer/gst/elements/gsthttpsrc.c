/* GStreamer
 * Copyright (C) 1999,2000 Erik Walthinsen <omega@cse.ogi.edu>
 *                    2000 Wim Taymans <wtay@chello.be>
 *
 * gsthttpsrc.c: 
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 */

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <stdlib.h>
#include <unistd.h>

#include <gsthttpsrc.h>


GstElementDetails gst_httpsrc_details = {
  "HTTP Source",
  "Source/Network",
  "Read data from an HTTP stream",
  VERSION,
  "Erik Walthinsen <omega@cse.ogi.edu>",
  "(C) 1999",
};

/* HttpSrc signals and args */
enum {
  /* FILL ME */
  LAST_SIGNAL
};

enum {
  ARG_0,
  ARG_LOCATION,
  ARG_BYTESPERREAD,
  ARG_OFFSET
};

static void 			gst_httpsrc_class_init		(GstHttpSrcClass *klass);
static void 			gst_httpsrc_init		(GstHttpSrc *httpsrc);

static void 			gst_httpsrc_set_property	(GObject *object, guint prop_id, 
								 const GValue *value, GParamSpec *pspec);
static void 			gst_httpsrc_get_property	(GObject *object, guint prop_id, 
								 GValue *value, GParamSpec *pspec);
static GstElementStateReturn	gst_httpsrc_change_state	(GstElement *element);

static GstBuffer *		gst_httpsrc_get			(GstPad *pad);

static gboolean			gst_httpsrc_open_url		(GstHttpSrc *src);
static void			gst_httpsrc_close_url		(GstHttpSrc *src);


static GstElementClass *parent_class = NULL;
//static guint gst_httpsrc_signals[LAST_SIGNAL] = { 0 };

GType
gst_httpsrc_get_type (void) 
{
  static GType httpsrc_type = 0;

  if (!httpsrc_type) {
    static const GTypeInfo httpsrc_info = {
      sizeof(GstHttpSrcClass),      NULL,
      NULL,
      (GClassInitFunc)gst_httpsrc_class_init,
      NULL,
      NULL,
      sizeof(GstHttpSrc),
      0,
      (GInstanceInitFunc)gst_httpsrc_init,
    };
    httpsrc_type = g_type_register_static (GST_TYPE_ELEMENT, "GstHttpSrc", &httpsrc_info, 0);
  }
  return httpsrc_type;
}

static void
gst_httpsrc_class_init (GstHttpSrcClass *klass) 
{
  GObjectClass *gobject_class;
  GstElementClass *gstelement_class;

  gobject_class = (GObjectClass*)klass;
  gstelement_class = (GstElementClass*)klass;

  parent_class = g_type_class_ref (GST_TYPE_ELEMENT);


  g_object_class_install_property(G_OBJECT_CLASS(klass), ARG_LOCATION,
    g_param_spec_string("location","location","location",
                        NULL, G_PARAM_READWRITE)); // CHECKME
  g_object_class_install_property(G_OBJECT_CLASS(klass), ARG_BYTESPERREAD,
    g_param_spec_int("bytesperread","bytesperread","bytesperread",
                     G_MININT,G_MAXINT,0,G_PARAM_READWRITE)); // CHECKME

  gobject_class->set_property = gst_httpsrc_set_property;
  gobject_class->get_property = gst_httpsrc_get_property;

  gstelement_class->change_state = gst_httpsrc_change_state;
}

static void gst_httpsrc_init(GstHttpSrc *httpsrc) {
  httpsrc->srcpad = gst_pad_new("src",GST_PAD_SRC);
  gst_pad_set_get_function(httpsrc->srcpad,gst_httpsrc_get);
  gst_element_add_pad(GST_ELEMENT(httpsrc),httpsrc->srcpad);

  httpsrc->url = NULL;
  httpsrc->request = NULL;
  httpsrc->fd = 0;
  httpsrc->curoffset = 0;
  httpsrc->bytes_per_read = 4096;
}

static GstBuffer *
gst_httpsrc_get(GstPad *pad)
{
  GstHttpSrc *src;
  GstBuffer *buf;
  glong readbytes;

  g_return_val_if_fail (pad != NULL, NULL);
  src = GST_HTTPSRC(gst_pad_get_parent (pad));

  buf = gst_buffer_new();
  GST_BUFFER_DATA(buf) = (gpointer)malloc(src->bytes_per_read);
  readbytes = read(src->fd,GST_BUFFER_DATA(buf),src->bytes_per_read);

  if (readbytes == 0) {
    gst_element_signal_eos(GST_ELEMENT(src));
    return NULL;
  }

  if (readbytes < src->bytes_per_read) {
    // FIXME: set the buffer's EOF bit here
  }
  GST_BUFFER_OFFSET(buf) = src->curoffset;
  GST_BUFFER_SIZE(buf) = readbytes;
  src->curoffset += readbytes;

  return buf;
}

static gboolean 
gst_httpsrc_open_url (GstHttpSrc *httpsrc) 
{
  gint status;

  g_return_val_if_fail (!GST_FLAG_IS_SET (httpsrc, GST_HTTPSRC_OPEN), FALSE);
  g_return_val_if_fail (httpsrc->url != NULL, FALSE);

  httpsrc->request = ghttp_request_new ();
  
  ghttp_set_uri (httpsrc->request, httpsrc->url);
  ghttp_set_sync (httpsrc->request, ghttp_async);
  ghttp_set_header (httpsrc->request, "User-Agent", "GstHttpSrc");
  ghttp_prepare (httpsrc->request);

  /* process everything up to the actual data stream */
  /* FIXME: should be in preroll, but hey */
  status = 0;
  while ((ghttp_get_status (httpsrc->request).proc != ghttp_proc_response)
         && (status >= 0)) {
    status = ghttp_process (httpsrc->request);
  }

  /* get the fd so we can read data ourselves */
  httpsrc->fd = ghttp_get_socket (httpsrc->request);
  
  GST_FLAG_SET (httpsrc, GST_HTTPSRC_OPEN);
  
  return TRUE;
}

/* unmap and close the file */
static void 
gst_httpsrc_close_url (GstHttpSrc *src) 
{  
  g_return_if_fail (GST_FLAG_IS_SET (src, GST_HTTPSRC_OPEN));
  g_return_if_fail (src->fd > 0);
 
  close(src->fd);
  src->fd = 0;

  GST_FLAG_UNSET (src, GST_HTTPSRC_OPEN);
}

static void 
gst_httpsrc_set_property (GObject *object, guint prop_id, const GValue *value, GParamSpec *pspec) 
{
  GstHttpSrc *src;

  /* it's not null if we got it, but it might not be ours */
  g_return_if_fail (GST_IS_HTTPSRC (object));
  
  src = GST_HTTPSRC (object);

  switch (prop_id) {
    case ARG_LOCATION:
      /* the element must not be playing in order to do this */
      g_return_if_fail (GST_STATE (src) < GST_STATE_PLAYING);

      if (src->url) g_free (src->url);
      /* clear the url if we get a NULL (is that possible?) */
      if (g_value_get_string (value) == NULL) {
        gst_element_set_state (GST_ELEMENT (object),GST_STATE_NULL);
        src->url = NULL;
      /* otherwise set the new url */
      } else {
        src->url = g_strdup (g_value_get_string (value));
      }
      break;
    case ARG_BYTESPERREAD:
      src->bytes_per_read = g_value_get_int (value);
      break;
    default:
      break;
  }
}

static void 
gst_httpsrc_get_property (GObject *object, guint prop_id, GValue *value, GParamSpec *pspec) 
{
  GstHttpSrc *httpsrc;

  /* it's not null if we got it, but it might not be ours */
  g_return_if_fail (GST_IS_HTTPSRC (object));
  
  httpsrc = GST_HTTPSRC (object);
                          
  switch (prop_id) {
    case ARG_LOCATION:
      g_value_set_string (value, httpsrc->url);
      break;
    case ARG_BYTESPERREAD:
      g_value_set_int (value, httpsrc->bytes_per_read);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
  }
}

static GstElementStateReturn 
gst_httpsrc_change_state (GstElement *element) 
{
  g_return_val_if_fail (GST_IS_HTTPSRC (element), GST_STATE_FAILURE);

  if (GST_STATE_PENDING (element) == GST_STATE_NULL) {
    if (GST_FLAG_IS_SET (element, GST_HTTPSRC_OPEN))
      gst_httpsrc_close_url (GST_HTTPSRC (element));
  } else {
    if (!GST_FLAG_IS_SET (element, GST_HTTPSRC_OPEN)) {
      if (!gst_httpsrc_open_url (GST_HTTPSRC (element)))
        return GST_STATE_FAILURE;
    }
  }
 
  if (GST_ELEMENT_CLASS (parent_class)->change_state)
    return GST_ELEMENT_CLASS (parent_class)->change_state (element);
  
  return GST_STATE_SUCCESS;
}
