/* GStreamer
 * Copyright (C) 1999,2000 Erik Walthinsen <omega@cse.ogi.edu>
 *                    2000 Wim Taymans <wim.taymans@chello.be>
 *
 * gstpad.h: Header for GstPad object
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


#ifndef __GST_QUERY_H__
#define __GST_QUERY_H__

#include <gst/gstconfig.h>

G_BEGIN_DECLS

typedef enum {
  GST_QUERY_NONE = 0,
  GST_QUERY_TOTAL,
  GST_QUERY_POSITION,
  GST_QUERY_LATENCY,
  GST_QUERY_JITTER,
  GST_QUERY_START,
  GST_QUERY_SEGMENT_END,
  GST_QUERY_RATE
} GstQueryType;

#ifdef G_HAVE_ISO_VARARGS
#define GST_QUERY_TYPE_FUNCTION(type, functionname, ...)  	\
static const GstQueryType*                           	\
functionname (type object)                         	\
{                                                       \
  static const GstQueryType types[] = {              	\
    __VA_ARGS__,                                        \
    0                                              	\
  };                                                    \
  return types;                                         \
}
#elif defined(G_HAVE_GNUC_VARARGS)
#define GST_QUERY_TYPE_FUNCTION(type, functionname, a...) 	\
static const GstQueryType*                           	\
functionname (type object)                          	\
{                                                       \
  static const GstQueryType types[] = {              	\
    a,                                                  \
    0                                              	\
  };                                                    \
  return types;                                         \
}
#endif

G_END_DECLS

#endif /* __GST_QUERY_H__ */

