/* GStreamer
 * Copyright (C) 1999,2000 Erik Walthinsen <omega@cse.ogi.edu>
 *                    2005 Wim Taymans <wim@fluendo.com>
 *
 * gstbasetransform.h: 
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


#ifndef __GST_BASE_TRANSFORM_H__
#define __GST_BASE_TRANSFORM_H__

#include <gst/gst.h>

G_BEGIN_DECLS

#define GST_TYPE_BASE_TRANSFORM  	(gst_base_transform_get_type())
#define GST_BASE_TRANSFORM(obj)  	(G_TYPE_CHECK_INSTANCE_CAST((obj),GST_TYPE_BASE_TRANSFORM,GstBaseTransform))
#define GST_BASE_TRANSFORM_CLASS(klass) (G_TYPE_CHECK_CLASS_CAST((klass),GST_TYPE_BASE_TRANSFORM,GstBaseTransformClass))
#define GST_BASE_TRANSFORM_GET_CLASS(obj) \
	(G_TYPE_INSTANCE_GET_CLASS ((obj),GST_TYPE_BASE_TRANSFORM,GstBaseTransformClass))
#define GST_IS_BASE_TRANSFORM(obj)  	(G_TYPE_CHECK_INSTANCE_TYPE((obj),GST_TYPE_BASE_TRANSFORM))
#define GST_IS_BASE_TRANSFORM_CLASS(obj)(G_TYPE_CHECK_CLASS_TYPE((klass),GST_TYPE_BASE_TRANSFORM))

/* the names of the templates for the sink and source pads */
#define GST_BASE_TRANSFORM_SINK_NAME	"sink"
#define GST_BASE_TRANSFORM_SRC_NAME	"src"

typedef struct _GstBaseTransform GstBaseTransform;
typedef struct _GstBaseTransformClass GstBaseTransformClass;

struct _GstBaseTransform {
  GstElement 	 element;

  /* source and sink pads */
  GstPad 	*sinkpad;
  GstPad 	*srcpad;
};

struct _GstBaseTransformClass {
  GstElementClass parent_class;

  gboolean      (*set_caps)     (GstBaseTransform *trans, GstCaps *caps);

  gboolean      (*start)        (GstBaseTransform *trans);
  gboolean      (*stop)         (GstBaseTransform *trans);

  gboolean      (*event)        (GstBaseTransform *trans, GstEvent *event);
  GstFlowReturn (*transform)    (GstBaseTransform *trans, GstBuffer *inbuf, GstBuffer **outbuf);
};

GType gst_base_transform_get_type(void);

G_END_DECLS

#endif /* __GST_BASE_TRANSFORM_H__ */
