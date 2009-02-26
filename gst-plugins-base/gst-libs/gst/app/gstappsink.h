/* GStreamer
 * Copyright (C) 2007 David Schleef <ds@schleef.org>
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

#ifndef _GST_APP_SINK_H_
#define _GST_APP_SINK_H_

#include <gst/gst.h>
#include <gst/base/gstbasesink.h>

G_BEGIN_DECLS

#define GST_TYPE_APP_SINK \
  (gst_app_sink_get_type())
#define GST_APP_SINK(obj) \
  (G_TYPE_CHECK_INSTANCE_CAST((obj),GST_TYPE_APP_SINK,GstAppSink))
#define GST_APP_SINK_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_CAST((klass),GST_TYPE_APP_SINK,GstAppSinkClass))
#define GST_IS_APP_SINK(obj) \
  (G_TYPE_CHECK_INSTANCE_TYPE((obj),GST_TYPE_APP_SINK))
#define GST_IS_APP_SINK_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_TYPE((klass),GST_TYPE_APP_SINK))

typedef struct _GstAppSink GstAppSink;
typedef struct _GstAppSinkClass GstAppSinkClass;
typedef struct _GstAppSinkPrivate GstAppSinkPrivate;

/**
 * GstAppSinkCallbacks:
 * @eos: Signal that the end-of-stream has been reached. This signal is
 *       emited from the steaming thread.
 * @new_preroll: Signal that a new preroll buffer is available. 
 *       This signal is emited from the steaming thread.
 *       The new preroll buffer can be retrieved with
 *       gst_app_sink_pull_preroll() either from this callback
 *       or from any other thread.
 * @new_buffer: Signal that a new buffer is available. 
 *       This signal is emited from the steaming thread.
 *       The new buffer can be retrieved with
 *       gst_app_sink_pull_buffer() either from this callback
 *       or from any other thread.
 *
 * A set of callbacks that can be installed on the appsink with
 * gst_app_sink_set_callbacks().
 *
 * Since: 0.10.23
 */
typedef struct {
  void (*eos)           (GstAppSink *sink, gpointer user_data);
  void (*new_preroll)   (GstAppSink *sink, gpointer user_data);
  void (*new_buffer)    (GstAppSink *sink, gpointer user_data);

  /*< private >*/
  gpointer     _gst_reserved[GST_PADDING];
} GstAppSinkCallbacks;

struct _GstAppSink
{
  GstBaseSink basesink;

  /*< private >*/
  GstAppSinkPrivate *priv;

  /*< private >*/
  gpointer     _gst_reserved[GST_PADDING];
};

struct _GstAppSinkClass
{
  GstBaseSinkClass basesink_class;

  /* signals */
  void        (*eos)          (GstAppSink *sink);
  void        (*new_preroll)  (GstAppSink *sink);
  void        (*new_buffer)   (GstAppSink *sink);

  /* actions */
  GstBuffer * (*pull_preroll)  (GstAppSink *sink);
  GstBuffer * (*pull_buffer)   (GstAppSink *sink);

  /*< private >*/
  gpointer     _gst_reserved[GST_PADDING];
};

GType gst_app_sink_get_type(void);

void            gst_app_sink_set_caps         (GstAppSink *appsink, const GstCaps *caps);
GstCaps *       gst_app_sink_get_caps         (GstAppSink *appsink);

gboolean        gst_app_sink_is_eos           (GstAppSink *appsink);

void            gst_app_sink_set_emit_signals (GstAppSink *appsink, gboolean emit);
gboolean        gst_app_sink_get_emit_signals (GstAppSink *appsink);

void            gst_app_sink_set_max_buffers  (GstAppSink *appsink, guint max);
guint           gst_app_sink_get_max_buffers  (GstAppSink *appsink);

void            gst_app_sink_set_drop         (GstAppSink *appsink, gboolean drop);
gboolean        gst_app_sink_get_drop         (GstAppSink *appsink);

GstBuffer *     gst_app_sink_pull_preroll     (GstAppSink *appsink);
GstBuffer *     gst_app_sink_pull_buffer      (GstAppSink *appsink);

void            gst_app_sink_set_callbacks    (GstAppSink * appsink,
                                               GstAppSinkCallbacks *callbacks,
					       gpointer user_data,
					       GDestroyNotify notify);

G_END_DECLS

#endif

