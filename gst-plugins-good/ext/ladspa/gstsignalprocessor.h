/* GStreamer
 * Copyright (C) 1999,2000 Erik Walthinsen <omega@cse.ogi.edu>
 *                    2005 Wim Taymans <wim@fluendo.com>
 *
 * gstsignalprocessor.h:
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


#ifndef __GST_SIGNAL_PROCESSOR_H__
#define __GST_SIGNAL_PROCESSOR_H__

#include <gst/gst.h>

G_BEGIN_DECLS


#define GST_TYPE_SIGNAL_PROCESSOR	  (gst_signal_processor_get_type())
#define GST_SIGNAL_PROCESSOR(obj)	  (G_TYPE_CHECK_INSTANCE_CAST((obj),GST_TYPE_SIGNAL_PROCESSOR,GstSignalProcessor))
#define GST_SIGNAL_PROCESSOR_CLASS(klass) (G_TYPE_CHECK_CLASS_CAST((klass),GST_TYPE_SIGNAL_PROCESSOR,GstSignalProcessorClass))
#define GST_SIGNAL_PROCESSOR_GET_CLASS(obj) \
	(G_TYPE_INSTANCE_GET_CLASS ((obj),GST_TYPE_SIGNAL_PROCESSOR,GstSignalProcessorClass))
#define GST_IS_SIGNAL_PROCESSOR(obj)	  (G_TYPE_CHECK_INSTANCE_TYPE((obj),GST_TYPE_SIGNAL_PROCESSOR))
#define GST_IS_SIGNAL_PROCESSOR_CLASS(obj)(G_TYPE_CHECK_CLASS_TYPE((klass),GST_TYPE_SIGNAL_PROCESSOR))


typedef struct _GstSignalProcessor GstSignalProcessor;
typedef struct _GstSignalProcessorClass GstSignalProcessorClass;


struct _GstSignalProcessor {
  GstElement	 element;

  GstCaps *caps;

  guint sample_rate;
  guint buffer_frames;

  GstFlowReturn state;

  GstActivateMode mode;

  guint pending_in;
  guint pending_out;

  gfloat *control_in;
  gfloat **audio_in;
  gfloat *control_out;
  gfloat **audio_out;
};

struct _GstSignalProcessorClass {
  GstElementClass parent_class;

  /*< public >*/
  guint num_control_in;
  guint num_audio_in;
  guint num_control_out;
  guint num_audio_out;

  /* virtual methods for subclasses */

  gboolean	(*setup)        (GstSignalProcessor *self, guint sample_rate,
                                 guint buffer_frames);
  gboolean      (*start)        (GstSignalProcessor *self);
  gboolean      (*stop)         (GstSignalProcessor *self);
  gboolean      (*process)      (GstSignalProcessor *self, guint num_frames);
  gboolean      (*event)        (GstSignalProcessor *self, GstEvent *event);
};


GType gst_signal_processor_get_type (void);
void gst_signal_processor_class_add_pad_template (GstSignalProcessorClass *klass,
    const gchar *name, GstPadDirection direction, guint index);



G_END_DECLS


#endif /* __GST_SIGNAL_PROCESSOR_H__ */
