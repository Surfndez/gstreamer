/*
 * Copyright (C) 2001 CodeFactory AB
 * Copyright (C) 2001 Thomas Nyberg <thomas@codefactory.se>
 * Copyright (C) 2001-2002 Andy Wingo <apwingo@eos.ncsu.edu>
 * Copyright (C) 2003 Benjamin Otte <in7y118@public.uni-hamburg.de>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public
 * License along with this library; if not, write to the Free
 * Software Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */

#ifndef __GST_ALSA_H__
#define __GST_ALSA_H__

#define ALSA_PCM_NEW_HW_PARAMS_API
#define ALSA_PCM_NEW_SW_PARAMS_API

#include <alsa/asoundlib.h>
#include <gst/gst.h>
#include <gst/bytestream/bytestream.h>
#include <glib.h>

#define GST_ALSA_MAX_CHANNELS 64 /* we don't support more than 64 channels */
#define GST_ALSA_MIN_RATE 8000
#define GST_ALSA_MAX_RATE 192000

#define GST_ALSA(obj) G_TYPE_CHECK_INSTANCE_CAST(obj, GST_TYPE_ALSA, GstAlsa)
#define GST_ALSA_CLASS(klass) G_TYPE_CHECK_CLASS_CAST(klass, GST_TYPE_ALSA, GstAlsaClass)
#define GST_IS_ALSA(obj) G_TYPE_CHECK_INSTANCE_TYPE(obj, GST_TYPE_ALSA)
#define GST_IS_ALSA_CLASS(klass) G_TYPE_CHECK_CLASS_TYPE(klass, GST_TYPE_ALSA)
#define GST_TYPE_ALSA gst_alsa_get_type()

#define GST_ALSA_SINK(obj) G_TYPE_CHECK_INSTANCE_CAST(obj, GST_TYPE_ALSA_SINK, GstAlsaSink)
#define GST_ALSA_SINK_CLASS(klass) G_TYPE_CHECK_CLASS_CAST(klass, GST_TYPE_ALSA_SINK, GstAlsaSinkClass)
#define GST_IS_ALSA_SINK(obj) G_TYPE_CHECK_INSTANCE_TYPE(obj, GST_TYPE_ALSA_SINK)
#define GST_IS_ALSA_SINK_CLASS(klass) G_TYPE_CHECK_CLASS_TYPE(klass, GST_TYPE_ALSA_SINK)
#define GST_TYPE_ALSA_SINK gst_alsa_sink_get_type()

#define GST_ALSA_SRC(obj) G_TYPE_CHECK_INSTANCE_CAST(obj, GST_TYPE_ALSA_SRC, GstAlsaSrc)
#define GST_ALSA_SRC_CLASS(klass) G_TYPE_CHECK_CLASS_CAST(klass, GST_TYPE_ALSA_SRC, GstAlsaSrcClass)
#define GST_IS_ALSA_SRC(obj) G_TYPE_CHECK_INSTANCE_TYPE(obj, GST_TYPE_ALSA_SRC)
#define GST_IS_ALSA_SRC_CLASS(klass) G_TYPE_CHECK_CLASS_TYPE(klass, GST_TYPE_ALSA_SRC)
#define GST_TYPE_ALSA_SRC gst_alsa_src_get_type()

/* I would have preferred to avoid this variety of trickery, but without it i
 * can't tell whether I'm a source or a sink upon creation. */

typedef struct _GstAlsa GstAlsa;
typedef struct _GstAlsaClass GstAlsaClass;
typedef GstAlsa GstAlsaSink;
typedef GstAlsaClass GstAlsaSinkClass;
typedef GstAlsa GstAlsaSrc;
typedef GstAlsaClass GstAlsaSrcClass;

enum {
  GST_ALSA_OPEN = GST_ELEMENT_FLAG_LAST,
  GST_ALSA_RUNNING,
  GST_ALSA_CAPS_NEGO,
  GST_ALSA_FLAG_LAST = GST_ELEMENT_FLAG_LAST + 3,
};
typedef enum {
  GST_ALSA_CAPS_PAUSE = 0,
  GST_ALSA_CAPS_RESUME,
  GST_ALSA_CAPS_SYNC_START
  /* add more */
} GstAlsaPcmCaps;
#define GST_ALSA_CAPS_IS_SET(obj, flag)		(GST_ALSA (obj)->pcm_caps & (1<<(flag)))
#define GST_ALSA_CAPS_SET(obj, flag, set)	G_STMT_START{  \
  if (set) { \
    (GST_ALSA (obj)->pcm_caps |= (1<<(flag))); \
  } else { \
    (GST_ALSA (obj)->pcm_caps &= ~(1<<(flag))); \
  } \
}G_STMT_END

typedef int (*GstAlsaTransmitFunction) (GstAlsa *this, snd_pcm_sframes_t *avail);

typedef struct {
  GstPad *pad;
  GstByteStream *bs;
  guint8 *data;
  guint8 offset;
} GstAlsaPad;
typedef struct {
  snd_pcm_format_t format;
  guint rate;
  gint channels;  
} GstAlsaFormat;
struct _GstAlsa {
  GstElement parent;

  /* array of GstAlsaPads */
  GstAlsaPad pads[GST_ALSA_MAX_CHANNELS];

  gchar *device;
  snd_pcm_stream_t stream;
  snd_pcm_t *handle;
  guint pcm_caps; /* capabilities of the pcm device */
  snd_output_t *out;

  GstAlsaFormat *format; /* NULL if undefined */
  gboolean mmap; /* use mmap transmit (fast) or read/write (sloooow) */
  GstAlsaTransmitFunction transmit;

  /* latency / performance parameters */
  snd_pcm_uframes_t period_size;
  unsigned int period_count;

  gboolean autorecover;
};

struct _GstAlsaClass {
  GstElementClass parent_class;
};

GType gst_alsa_get_type (void);
GType gst_alsa_sink_get_type (void);
GType gst_alsa_src_get_type (void);

#endif /* __ALSA_H__ */
