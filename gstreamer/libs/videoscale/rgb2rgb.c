/* Gnome-Streamer
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

//#define DEBUG_ENABLED

#include <gst/gst.h>
#include <gstcolorspace.h>

static GstBuffer *gst_colorspace_rgb24_to_bgr24(GstBuffer *src, GstColorSpaceParameters *params);
static GstBuffer *gst_colorspace_rgb_to_rgb_identity(GstBuffer *src, GstColorSpaceParameters *params);

GstColorSpaceConverter gst_colorspace_rgb2rgb_get_converter(GstColorSpace src, GstColorSpace dest) {
  switch(src) {
    case GST_COLORSPACE_RGB24:
      switch(dest) {
        case GST_COLORSPACE_RGB24:
          return gst_colorspace_rgb_to_rgb_identity;
        case GST_COLORSPACE_BGR24:
          return gst_colorspace_rgb24_to_bgr24;
	default:
	  break;
      }
      break;
    case GST_COLORSPACE_BGR24:
      switch(dest) {
        case GST_COLORSPACE_RGB24:
          return gst_colorspace_rgb24_to_bgr24;
        case GST_COLORSPACE_BGR24:
          return gst_colorspace_rgb_to_rgb_identity;
	default:
	  break;
      }
      break;
    default:
      break;
  }
  g_print("gst_colorspace: conversion not supported\n");
  return NULL;
}

static GstBuffer *gst_colorspace_rgb_to_rgb_identity(GstBuffer *src, GstColorSpaceParameters *params) {
  return src;
}

static GstBuffer *gst_colorspace_rgb24_to_bgr24(GstBuffer *src, GstColorSpaceParameters *params) {
  gint size;
  gchar temp;
  gchar *data;

  GST_DEBUG (0,"gst_colorspace_rgb24_to_bgr24 %d\n", GST_BUFFER_SIZE(src));

  size = GST_BUFFER_SIZE(src)/3;

  if (params != NULL && params->outbuf != NULL) {
    data = params->outbuf;
    GST_DEBUG (0,"gst_colorspace: to buffer %p\n", data);
  }
  else {
    data = GST_BUFFER_DATA(src);
  }

  while (size--) {
    temp = data[0];
    data[0] = data[2];
    data[2] = temp;
    data+=3;
  }
  GST_DEBUG (0,"gst_colorspace_rgb24_to_bgr24 end %d\n", GST_BUFFER_SIZE(src));

  return src;
}

