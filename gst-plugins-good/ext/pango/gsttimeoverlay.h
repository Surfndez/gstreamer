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


#ifndef __GST_TIMEOVERLAY_H__
#define __GST_TIMEOVERLAY_H__


#include <config.h>
#include <gst/gst.h>
#include <pango/pango.h>

#include "gstvideofilter.h"


G_BEGIN_DECLS

#define GST_TYPE_TIMEOVERLAY \
  (gst_timeoverlay_get_type())
#define GST_TIMEOVERLAY(obj) \
  (G_TYPE_CHECK_INSTANCE_CAST((obj),GST_TYPE_TIMEOVERLAY,GstTimeoverlay))
#define GST_TIMEOVERLAY_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_CAST((klass),GST_TYPE_TIMEOVERLAY,GstTimeoverlayClass))
#define GST_IS_TIMEOVERLAY(obj) \
  (G_TYPE_CHECK_INSTANCE_TYPE((obj),GST_TYPE_TIMEOVERLAY))
#define GST_IS_TIMEOVERLAY_CLASS(obj) \
  (G_TYPE_CHECK_CLASS_TYPE((klass),GST_TYPE_TIMEOVERLAY))

typedef struct _GstTimeoverlay GstTimeoverlay;
typedef struct _GstTimeoverlayClass GstTimeoverlayClass;

struct _GstTimeoverlay {
  GstVideofilter videofilter;

  PangoFontDescription *font_description;
  PangoContext *context;

};

struct _GstTimeoverlayClass {
  GstVideofilterClass parent_class;
};

GType gst_timeoverlay_get_type(void);

G_END_DECLS

#endif /* __GST_TIMEOVERLAY_H__ */

