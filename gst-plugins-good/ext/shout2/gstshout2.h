/* GStreamer
 * Copyright (C) <1999> Erik Walthinsen <omega@cse.ogi.edu>
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


#ifndef __GST_SHOUT2SEND_H__
#define __GST_SHOUT2SEND_H__

#include <gst/gst.h>
#include <shout/shout.h>

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

  /* Protocol type enum */
typedef enum {
  SHOUT2SEND_PROTOCOL_XAUDIOCAST = 1,
  SHOUT2SEND_PROTOCOL_ICY,
  SHOUT2SEND_PROTOCOL_HTTP
} GstShout2SendProtocol;


/* Definition of structure storing data for this element. */
typedef struct _GstShout2send GstShout2send;
struct _GstShout2send {
  GstElement element;

  GstPad *sinkpad,*srcpad;

  GstShout2SendProtocol protocol;

  shout_t *conn;

  gchar *ip;
  guint port;
  gchar *password;
  gchar *name;
  gchar *description;
  gchar *genre;
  gchar *mount;
  gchar *url;
  gboolean sync;
  gboolean started;

  guint16 audio_format;

  GstTagList* tags;

  GstClock 	*clock;

};



/* Standard definition defining a class for this element. */
typedef struct _GstShout2sendClass GstShout2sendClass;
struct _GstShout2sendClass {
  GstElementClass parent_class;

  /* signal callbacks */
  void (*connection_problem) (GstElement *element,guint errno);
};

/* Standard macros for defining types for this element.  */
#define GST_TYPE_SHOUT2SEND \
  (gst_shout2send_get_type())
#define GST_SHOUT2SEND(obj) \
  (G_TYPE_CHECK_INSTANCE_CAST((obj),GST_TYPE_SHOUT2SEND,GstShout2send))
#define GST_SHOUT2SEND_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_CAST((klass),GST_TYPE_SHOUT2SEND,GstShout2send))
#define GST_IS_SHOUT2SEND(obj) \
  (G_TYPE_CHECK_INSTANCE_TYPE((obj),GST_TYPE_SHOUT2SEND))
#define GST_IS_SHOUT2SEND_CLASS(obj) \
  (G_TYPE_CHECK_CLASS_TYPE((klass),GST_TYPE_SHOUT2SEND))

/* Standard function returning type information. */
GType gst_shout2send_get_type(void);


#ifdef __cplusplus
}
#endif /* __cplusplus */


#endif /* __GST_SHOUT2SEND_H__ */
