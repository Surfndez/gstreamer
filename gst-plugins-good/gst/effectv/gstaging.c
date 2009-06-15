/* GStreamer
 * Copyright (C) <1999> Erik Walthinsen <omega@cse.ogi.edu>
 * Copyright (C) <2003> David Schleef <ds@schleef.org>
 * Copyright (C) <2009> Sebastian Dröge <sebastian.droege@collabora.co.uk>
 *
 * EffecTV - Realtime Digital Video Effector
 * Copyright (C) 2001-2002 FUKUCHI Kentarou
 *
 * AgingTV - film-aging effect.
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

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <gst/video/gstvideofilter.h>

#include <string.h>
#include <math.h>

#include <gst/video/video.h>

#define GST_TYPE_AGINGTV \
  (gst_agingtv_get_type())
#define GST_AGINGTV(obj) \
  (G_TYPE_CHECK_INSTANCE_CAST((obj),GST_TYPE_AGINGTV,GstAgingTV))
#define GST_AGINGTV_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_CAST((klass),GST_TYPE_AGINGTV,GstAgingTVClass))
#define GST_IS_AGINGTV(obj) \
  (G_TYPE_CHECK_INSTANCE_TYPE((obj),GST_TYPE_AGINGTV))
#define GST_IS_AGINGTV_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_TYPE((klass),GST_TYPE_AGINGTV))

#define SCRATCH_MAX 20
typedef struct _scratch
{
  gint life;
  gint x;
  gint dx;
  gint init;
} scratch;

static const gint dx[8] = { 1, 1, 0, -1, -1, -1, 0, 1 };
static const gint dy[8] = { 0, -1, -1, -1, 0, 1, 1, 1 };

enum
{
  PROP_0 = 0,
  PROP_SCRATCH_LINES
};

typedef struct _GstAgingTV GstAgingTV;
typedef struct _GstAgingTVClass GstAgingTVClass;

struct _GstAgingTV
{
  GstVideoFilter videofilter;

  gint width, height;

  gint coloraging_state;

  scratch scratches[SCRATCH_MAX];
  gint scratch_lines;

  gint dust_interval;
  gint pits_interval;

};

struct _GstAgingTVClass
{
  GstVideoFilterClass parent_class;
};

static GstStaticPadTemplate gst_agingtv_src_template =
GST_STATIC_PAD_TEMPLATE ("src",
    GST_PAD_SRC,
    GST_PAD_ALWAYS,
    GST_STATIC_CAPS (GST_VIDEO_CAPS_BGRx)
    );

static GstStaticPadTemplate gst_agingtv_sink_template =
GST_STATIC_PAD_TEMPLATE ("sink",
    GST_PAD_SINK,
    GST_PAD_ALWAYS,
    GST_STATIC_CAPS (GST_VIDEO_CAPS_BGRx)
    );

GST_BOILERPLATE (GstAgingTV, gst_agingtv, GstVideoFilter,
    GST_TYPE_VIDEO_FILTER);

static gboolean
gst_agingtv_set_caps (GstBaseTransform * btrans, GstCaps * incaps,
    GstCaps * outcaps)
{
  GstAgingTV *filter = GST_AGINGTV (btrans);
  GstStructure *structure;
  gboolean ret = FALSE;

  structure = gst_caps_get_structure (incaps, 0);

  if (gst_structure_get_int (structure, "width", &filter->width) &&
      gst_structure_get_int (structure, "height", &filter->height)) {
    ret = TRUE;
  }

  return ret;
}

static gboolean
gst_agingtv_get_unit_size (GstBaseTransform * btrans, GstCaps * caps,
    guint * size)
{
  GstAgingTV *filter = GST_AGINGTV (btrans);
  GstStructure *structure;
  gboolean ret = FALSE;
  gint width, height;

  structure = gst_caps_get_structure (caps, 0);

  if (gst_structure_get_int (structure, "width", &width) &&
      gst_structure_get_int (structure, "height", &height)) {
    *size = width * height * 32 / 8;
    ret = TRUE;
    GST_DEBUG_OBJECT (filter, "our frame size is %d bytes (%dx%d)", *size,
        width, height);
  }

  return ret;
}

static inline guint
fastrand (void)
{
  static guint fastrand_val;

  return (fastrand_val = fastrand_val * 1103515245 + 12345);
}


static void
coloraging (guint32 * src, guint32 * dest, gint video_area, gint * c)
{
  guint32 a, b;
  gint i;
  gint c_tmp = *c;

  c_tmp -= (gint) (fastrand ()) >> 28;
  if (c_tmp < 0)
    c_tmp = 0;
  if (c_tmp > 0x18)
    c_tmp = 0x18;

  for (i = 0; i < video_area; i++) {
    a = *src++;
    b = (a & 0xfcfcfc) >> 2;
    *dest++ =
        a - b + (c_tmp | (c_tmp << 8) | (c_tmp << 16)) +
        ((fastrand () >> 8) & 0x101010);
  }
  *c = c_tmp;
}


static void
scratching (scratch * scratches, gint scratch_lines, guint32 * dest, gint width,
    gint height)
{
  gint i, y, y1, y2;
  guint32 *p, a, b;
  scratch *scratch;

  for (i = 0; i < scratch_lines; i++) {
    scratch = &scratches[i];

    if (scratch->life) {
      scratch->x = scratch->x + scratch->dx;

      if (scratch->x < 0 || scratch->x > width * 256) {
        scratch->life = 0;
        break;
      }
      p = dest + (scratch->x >> 8);
      if (scratch->init) {
        y1 = scratch->init;
        scratch->init = 0;
      } else {
        y1 = 0;
      }
      scratch->life--;
      if (scratch->life) {
        y2 = height;
      } else {
        y2 = fastrand () % height;
      }
      for (y = y1; y < y2; y++) {
        a = *p & 0xfefeff;
        a += 0x202020;
        b = a & 0x1010100;
        *p = a | (b - (b >> 8));
        p += width;
      }
    } else {
      if ((fastrand () & 0xf0000000) == 0) {
        scratch->life = 2 + (fastrand () >> 27);
        scratch->x = fastrand () % (width * 256);
        scratch->dx = ((int) fastrand ()) >> 23;
        scratch->init = (fastrand () % (height - 1)) + 1;
      }
    }
  }
}

static void
dusts (guint32 * dest, gint width, gint height, gint * dust_interval,
    gint area_scale)
{
  gint i, j;
  gint dnum;
  gint d, len;
  guint x, y;

  if (*dust_interval == 0) {
    if ((fastrand () & 0xf0000000) == 0) {
      *dust_interval = fastrand () >> 29;
    }
    return;
  }
  dnum = area_scale * 4 + (fastrand () >> 27);

  for (i = 0; i < dnum; i++) {
    x = fastrand () % width;
    y = fastrand () % height;
    d = fastrand () >> 29;
    len = fastrand () % area_scale + 5;
    for (j = 0; j < len; j++) {
      dest[y * width + x] = 0x101010;
      y += dy[d];
      x += dx[d];

      if (y >= height || x >= width)
        break;

      d = (d + fastrand () % 3 - 1) & 7;
    }
  }
  *dust_interval = *dust_interval - 1;
}

static void
pits (guint32 * dest, gint width, gint height, gint area_scale,
    gint * pits_interval)
{
  gint i, j;
  gint pnum, size, pnumscale;
  guint x, y;

  pnumscale = area_scale * 2;
  if (*pits_interval) {
    pnum = pnumscale + (fastrand () % pnumscale);

    *pits_interval = *pits_interval - 1;
  } else {
    pnum = fastrand () % pnumscale;

    if ((fastrand () & 0xf8000000) == 0) {
      *pits_interval = (fastrand () >> 28) + 20;
    }
  }
  for (i = 0; i < pnum; i++) {
    x = fastrand () % (width - 1);
    y = fastrand () % (height - 1);

    size = fastrand () >> 28;

    for (j = 0; j < size; j++) {
      x = x + fastrand () % 3 - 1;
      y = y + fastrand () % 3 - 1;

      if (y >= height || x >= width)
        break;

      dest[y * width + x] = 0xc0c0c0;
    }
  }
}

static void
gst_agingtv_get_property (GObject * object, guint prop_id,
    GValue * value, GParamSpec * pspec)
{
  GstAgingTV *agingtv = GST_AGINGTV (object);

  switch (prop_id) {
    case PROP_SCRATCH_LINES:
      g_value_set_uint (value, agingtv->scratch_lines);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
  }
}

static void
gst_agingtv_set_property (GObject * object, guint prop_id,
    const GValue * value, GParamSpec * pspec)
{
  GstAgingTV *agingtv = GST_AGINGTV (object);

  switch (prop_id) {
    case PROP_SCRATCH_LINES:
      agingtv->scratch_lines = g_value_get_uint (value);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
  }
}

static gboolean
gst_agingtv_start (GstBaseTransform * trans)
{
  GstAgingTV *agingtv = GST_AGINGTV (trans);

  agingtv->coloraging_state = 0x18;
  agingtv->dust_interval = 0;
  agingtv->pits_interval = 0;

  memset (agingtv->scratches, 0, sizeof (agingtv->scratches));

  return TRUE;
}

static GstFlowReturn
gst_agingtv_transform (GstBaseTransform * trans, GstBuffer * in,
    GstBuffer * out)
{
  GstAgingTV *agingtv = GST_AGINGTV (trans);
  gint width = agingtv->width;
  gint height = agingtv->height;
  gint video_size = width * height;
  guint32 *src = (guint32 *) GST_BUFFER_DATA (in);
  guint32 *dest = (guint32 *) GST_BUFFER_DATA (out);
  gint area_scale = width * height / 64 / 480;
  GstFlowReturn ret = GST_FLOW_OK;

  if (area_scale <= 0)
    area_scale = 1;

  coloraging (src, dest, video_size, &agingtv->coloraging_state);
  scratching (agingtv->scratches, agingtv->scratch_lines, dest, width, height);
  pits (dest, width, height, area_scale, &agingtv->pits_interval);
  if (area_scale > 1)
    dusts (dest, width, height, &agingtv->dust_interval, area_scale);

  return ret;
}

static void
gst_agingtv_base_init (gpointer g_class)
{
  GstElementClass *element_class = GST_ELEMENT_CLASS (g_class);

  gst_element_class_set_details_simple (element_class, "AgingTV effect",
      "Filter/Effect/Video",
      "AgingTV adds age to video input using scratches and dust",
      "Sam Lantinga <slouken@devolution.com>");

  gst_element_class_add_pad_template (element_class,
      gst_static_pad_template_get (&gst_agingtv_sink_template));
  gst_element_class_add_pad_template (element_class,
      gst_static_pad_template_get (&gst_agingtv_src_template));
}

static void
gst_agingtv_class_init (GstAgingTVClass * klass)
{
  GObjectClass *gobject_class = (GObjectClass *) klass;
  GstBaseTransformClass *trans_class = (GstBaseTransformClass *) klass;

  gobject_class->set_property = gst_agingtv_set_property;
  gobject_class->get_property = gst_agingtv_get_property;

  g_object_class_install_property (gobject_class, PROP_SCRATCH_LINES,
      g_param_spec_uint ("scratch-lines", "Scratch Lines",
          "Number of scratch lines", 0, SCRATCH_MAX, 7,
          G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS | GST_PARAM_CONTROLLABLE));

  trans_class->set_caps = GST_DEBUG_FUNCPTR (gst_agingtv_set_caps);
  trans_class->get_unit_size = GST_DEBUG_FUNCPTR (gst_agingtv_get_unit_size);
  trans_class->transform = GST_DEBUG_FUNCPTR (gst_agingtv_transform);
  trans_class->start = GST_DEBUG_FUNCPTR (gst_agingtv_start);
}

static void
gst_agingtv_init (GstAgingTV * agingtv, GstAgingTVClass * klass)
{
  agingtv->scratch_lines = 7;
}
