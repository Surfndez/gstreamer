/* GStreamer
 * Copyright (C) 1999,2000 Erik Walthinsen <omega@cse.ogi.edu>
 *                    2000 Wim Taymans <wtay@chello.be>
 *
 * gsttee.h: Header for GstTee element
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


#ifndef __GST_TEE_H__
#define __GST_TEE_H__

#include <gst/gst.h>

G_BEGIN_DECLS
#define GST_TYPE_TEE \
  (gst_tee_get_type())
#define GST_TEE(obj) \
  (G_TYPE_CHECK_INSTANCE_CAST((obj),GST_TYPE_TEE,GstTee))
#define GST_TEE_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_CAST((klass),GST_TYPE_TEE,GstTeeClass))
#define GST_IS_TEE(obj) \
  (G_TYPE_CHECK_INSTANCE_TYPE((obj),GST_TYPE_TEE))
#define GST_IS_TEE_CLASS(obj) \
  (G_TYPE_CHECK_CLASS_TYPE((klass),GST_TYPE_TEE))
typedef struct _GstTee GstTee;
typedef struct _GstTeeClass GstTeeClass;

struct _GstTee
{
  GstElement element;

  GstPad *sinkpad;

  gboolean silent;
  gchar *last_message;
};

struct _GstTeeClass
{
  GstElementClass parent_class;
};

GType gst_tee_get_type (void);

gboolean gst_tee_factory_init (GstElementFactory * factory);

G_END_DECLS
#endif /* __GST_TEE_H__ */
