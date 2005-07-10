/* GStreamer
 * Copyright (C) 1999,2000 Erik Walthinsen <omega@cse.ogi.edu>
 *                    2000 Wim Taymans <wtay@chello.be>
 *
 * gstosssrc.c: 
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
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <errno.h>
#include <unistd.h>
#include <string.h>

#ifdef HAVE_OSS_INCLUDE_IN_SYS
#include <sys/soundcard.h>
#else

#ifdef HAVE_OSS_INCLUDE_IN_ROOT
#include <soundcard.h>
#else

#include <machine/soundcard.h>

#endif /* HAVE_OSS_INCLUDE_IN_ROOT */

#endif /* HAVE_OSS_INCLUDE_IN_SYS */

#include <gstosssrc.h>
#include <gstosselement.h>
#include <gst/audio/audioclock.h>

/* elementfactory information */
static GstElementDetails gst_oss_src_details =
GST_ELEMENT_DETAILS ("Audio Source (OSS)",
    "Source/Audio",
    "Read from the sound card",
    "Erik Walthinsen <omega@cse.ogi.edu>");


/* OssSrc signals and args */
enum
{
  /* FILL ME */
  LAST_SIGNAL
};

enum
{
  ARG_0,
  ARG_BUFFERSIZE,
  ARG_FRAGMENT
};

static GstStaticPadTemplate osssrc_src_factory = GST_STATIC_PAD_TEMPLATE ("src",
    GST_PAD_SRC,
    GST_PAD_ALWAYS,
    GST_STATIC_CAPS ("audio/x-raw-int, "
        "endianness = (int) { LITTLE_ENDIAN, BIG_ENDIAN }, "
        "signed = (boolean) { TRUE, FALSE }, "
        "width = (int) 16, "
        "depth = (int) { 8, 16 }, "
        "rate = (int) [ 1, MAX ], " "channels = (int) [ 1, 2 ]; "
        "audio/x-raw-int, "
        "signed = (boolean) { TRUE, FALSE }, "
        "width = (int) 8, "
        "depth = (int) 8, "
        "rate = (int) [ 1, MAX ], " "channels = (int) [ 1, 2 ]")
    );

static void gst_oss_src_base_init (gpointer g_class);
static void gst_oss_src_class_init (GstOssSrcClass * klass);
static void gst_oss_src_init (GstOssSrc * osssrc);
static void gst_oss_src_dispose (GObject * object);

static GstPadLinkReturn gst_oss_src_src_link (GstPad * pad, GstPad * peer);
static GstCaps *gst_oss_src_getcaps (GstPad * pad);
static const GstFormat *gst_oss_src_get_formats (GstPad * pad);
static gboolean gst_oss_src_convert (GstPad * pad,
    GstFormat src_format, gint64 src_value,
    GstFormat * dest_format, gint64 * dest_value);

static void gst_oss_src_set_property (GObject * object, guint prop_id,
    const GValue * value, GParamSpec * pspec);
static void gst_oss_src_get_property (GObject * object, guint prop_id,
    GValue * value, GParamSpec * pspec);
static GstElementStateReturn gst_oss_src_change_state (GstElement * element);

static void gst_oss_src_set_clock (GstElement * element, GstClock * clock);
static GstClock *gst_oss_src_get_clock (GstElement * element);
static GstClockTime gst_oss_src_get_time (GstClock * clock, gpointer data);

static const GstEventMask *gst_oss_src_get_event_masks (GstPad * pad);
static gboolean gst_oss_src_src_event (GstPad * pad, GstEvent * event);
static gboolean gst_oss_src_send_event (GstElement * element, GstEvent * event);
static const GstQueryType *gst_oss_src_get_query_types (GstPad * pad);
static gboolean gst_oss_src_src_query (GstPad * pad, GstQueryType type,
    GstFormat * format, gint64 * value);

static void gst_oss_src_loop (GstPad * pad);

static GstElementClass *parent_class = NULL;

/*static guint gst_oss_src_signals[LAST_SIGNAL] = { 0 }; */

GType
gst_oss_src_get_type (void)
{
  static GType osssrc_type = 0;

  if (!osssrc_type) {
    static const GTypeInfo osssrc_info = {
      sizeof (GstOssSrcClass),
      gst_oss_src_base_init,
      NULL,
      (GClassInitFunc) gst_oss_src_class_init,
      NULL,
      NULL,
      sizeof (GstOssSrc),
      0,
      (GInstanceInitFunc) gst_oss_src_init,
    };

    osssrc_type =
        g_type_register_static (GST_TYPE_OSSELEMENT, "GstOssSrc", &osssrc_info,
        0);
  }
  return osssrc_type;
}

static void
gst_oss_src_base_init (gpointer g_class)
{
  GstElementClass *element_class = GST_ELEMENT_CLASS (g_class);

  gst_element_class_set_details (element_class, &gst_oss_src_details);
  gst_element_class_add_pad_template (element_class,
      gst_static_pad_template_get (&osssrc_src_factory));
}
static void
gst_oss_src_class_init (GstOssSrcClass * klass)
{
  GObjectClass *gobject_class;
  GstElementClass *gstelement_class;

  gobject_class = (GObjectClass *) klass;
  gstelement_class = (GstElementClass *) klass;

  parent_class = g_type_class_ref (GST_TYPE_OSSELEMENT);

  gobject_class->set_property = gst_oss_src_set_property;
  gobject_class->get_property = gst_oss_src_get_property;

  g_object_class_install_property (G_OBJECT_CLASS (klass), ARG_BUFFERSIZE,
      g_param_spec_ulong ("buffersize", "Buffer Size",
          "The size of the buffers with samples", 0, G_MAXULONG, 0,
          G_PARAM_READWRITE));
  g_object_class_install_property (G_OBJECT_CLASS (klass), ARG_FRAGMENT,
      g_param_spec_int ("fragment", "Fragment",
          "The fragment as 0xMMMMSSSS (MMMM = total fragments, 2^SSSS = fragment size)",
          0, G_MAXINT, 6, G_PARAM_READWRITE));

  gobject_class->dispose = gst_oss_src_dispose;

  gstelement_class->change_state = gst_oss_src_change_state;
  gstelement_class->send_event = gst_oss_src_send_event;

  gstelement_class->set_clock = gst_oss_src_set_clock;
  gstelement_class->get_clock = gst_oss_src_get_clock;
}

static void
gst_oss_src_init (GstOssSrc * osssrc)
{
  osssrc->srcpad =
      gst_pad_new_from_template (gst_static_pad_template_get
      (&osssrc_src_factory), "src");
  gst_pad_set_loop_function (osssrc->srcpad, gst_oss_src_loop);
  gst_pad_set_getcaps_function (osssrc->srcpad, gst_oss_src_getcaps);
  gst_pad_set_link_function (osssrc->srcpad, gst_oss_src_src_link);
  gst_pad_set_convert_function (osssrc->srcpad, gst_oss_src_convert);
  gst_pad_set_formats_function (osssrc->srcpad, gst_oss_src_get_formats);
  gst_pad_set_event_function (osssrc->srcpad, gst_oss_src_src_event);
  gst_pad_set_event_mask_function (osssrc->srcpad, gst_oss_src_get_event_masks);
  gst_pad_set_query_function (osssrc->srcpad, gst_oss_src_src_query);
  gst_pad_set_query_type_function (osssrc->srcpad, gst_oss_src_get_query_types);

  gst_element_add_pad (GST_ELEMENT (osssrc), osssrc->srcpad);

  osssrc->buffersize = 4096;
  osssrc->curoffset = 0;

  osssrc->provided_clock =
      gst_audio_clock_new ("ossclock", gst_oss_src_get_time, osssrc);
  gst_object_set_parent (GST_OBJECT (osssrc->provided_clock),
      GST_OBJECT (osssrc));

  osssrc->clock = NULL;
}

static void
gst_oss_src_dispose (GObject * object)
{
  GstOssSrc *osssrc = (GstOssSrc *) object;

  if (osssrc->provided_clock != NULL) {
    gst_object_unparent (GST_OBJECT (osssrc->provided_clock));
    osssrc->provided_clock = NULL;
  }

  G_OBJECT_CLASS (parent_class)->dispose (object);
}

static GstCaps *
gst_oss_src_getcaps (GstPad * pad)
{
  GstOssSrc *src;
  GstCaps *caps;

  src = GST_OSSSRC (GST_PAD_PARENT (pad));

  gst_osselement_probe_caps (GST_OSSELEMENT (src));

  if (GST_OSSELEMENT (src)->probed_caps == NULL) {
    caps = gst_caps_copy (gst_pad_get_pad_template_caps (pad));
  } else {
    caps = gst_caps_copy (GST_OSSELEMENT (src)->probed_caps);
  }

  return caps;
}

static GstPadLinkReturn
gst_oss_src_src_link (GstPad * pad, GstPad * peer)
{
  return GST_RPAD_LINKFUNC (peer) (peer, pad);
}

static gboolean
gst_oss_src_negotiate (GstPad * pad)
{
  GstOssSrc *src;
  GstCaps *allowed;

  src = GST_OSSSRC (GST_PAD_PARENT (pad));

  //allowed = gst_pad_get_allowed_caps (pad);
  allowed = NULL;

  if (!gst_osselement_merge_fixed_caps (GST_OSSELEMENT (src), allowed))
    return FALSE;

  if (!gst_osselement_sync_parms (GST_OSSELEMENT (src)))
    return FALSE;

  /* set caps on src pad */
  GST_PAD_CAPS (src->srcpad) =
      gst_caps_new_simple ("audio/x-raw-int",
      "endianness", G_TYPE_INT, GST_OSSELEMENT (src)->endianness,
      "signed", G_TYPE_BOOLEAN, GST_OSSELEMENT (src)->sign,
      "width", G_TYPE_INT, GST_OSSELEMENT (src)->width,
      "depth", G_TYPE_INT, GST_OSSELEMENT (src)->depth,
      "rate", G_TYPE_INT, GST_OSSELEMENT (src)->rate,
      "channels", G_TYPE_INT, GST_OSSELEMENT (src)->channels, NULL);

  return TRUE;
}

static GstClockTime
gst_oss_src_get_time (GstClock * clock, gpointer data)
{
  GstOssSrc *osssrc = GST_OSSSRC (data);
  audio_buf_info info;

  if (!GST_OSSELEMENT (osssrc)->bps)
    return 0;

  if (ioctl (GST_OSSELEMENT (osssrc)->fd, SNDCTL_DSP_GETISPACE, &info) < 0)
    return 0;

  return (osssrc->curoffset * GST_OSSELEMENT (osssrc)->sample_width +
      info.bytes) * GST_SECOND / GST_OSSELEMENT (osssrc)->bps;
}

static GstClock *
gst_oss_src_get_clock (GstElement * element)
{
  GstOssSrc *osssrc;

  osssrc = GST_OSSSRC (element);

  return GST_CLOCK (osssrc->provided_clock);
}

static void
gst_oss_src_set_clock (GstElement * element, GstClock * clock)
{
  GstOssSrc *osssrc;

  osssrc = GST_OSSSRC (element);

  osssrc->clock = clock;
}

static void
gst_oss_src_loop (GstPad * pad)
{
  GstOssSrc *src;
  GstBuffer *buf;
  glong readbytes;
  glong readsamples;

  src = GST_OSSSRC (GST_PAD_PARENT (pad));

  GST_DEBUG ("attempting to read something from the soundcard");

  if (src->need_eos) {
    src->need_eos = FALSE;
    gst_pad_push_event (pad, gst_event_new (GST_EVENT_EOS));
    return;
  }

  buf = gst_buffer_new_and_alloc (src->buffersize);

  if (!GST_PAD_CAPS (pad)) {
    /* nothing was negotiated, we can decide on a format */
    if (!gst_oss_src_negotiate (pad)) {
      gst_buffer_unref (buf);
      GST_ELEMENT_ERROR (src, CORE, NEGOTIATION, (NULL), (NULL));
      return;
    }
  }
  if (GST_OSSELEMENT (src)->bps == 0) {
    gst_buffer_unref (buf);
    GST_ELEMENT_ERROR (src, CORE, NEGOTIATION, (NULL),
        ("format wasn't negotiated before chain function"));
    return;
  }

  readbytes = read (GST_OSSELEMENT (src)->fd, GST_BUFFER_DATA (buf),
      src->buffersize);
  if (readbytes < 0) {
    gst_buffer_unref (buf);
    GST_ELEMENT_ERROR (src, RESOURCE, READ, (NULL), GST_ERROR_SYSTEM);
    return;
  }

  if (readbytes == 0) {
    gst_buffer_unref (buf);
    gst_pad_push_event (pad, gst_event_new (GST_EVENT_EOS));
    return;
  }

  readsamples = readbytes * GST_OSSELEMENT (src)->rate /
      GST_OSSELEMENT (src)->bps;

  GST_BUFFER_SIZE (buf) = readbytes;
  GST_BUFFER_OFFSET (buf) = src->curoffset;
  GST_BUFFER_OFFSET_END (buf) = src->curoffset + readsamples;
  GST_BUFFER_DURATION (buf) =
      readsamples * GST_SECOND / GST_OSSELEMENT (src)->rate;

  /* if we have a clock */
  if (src->clock) {
    if (src->clock == src->provided_clock) {
      /* if it's our own clock, we can be very accurate */
      GST_BUFFER_TIMESTAMP (buf) =
          src->curoffset * GST_SECOND / GST_OSSELEMENT (src)->rate;
    } else {
      /* somebody elses clock, timestamp with that clock, no discontinuity in
       * the stream since the OFFSET is updated correctly. Elements can stretch
       * to match timestamps */
      GST_BUFFER_TIMESTAMP (buf) =
          gst_element_get_time (GST_ELEMENT (src)) - GST_BUFFER_DURATION (buf);
    }
  } else {
    /* no clock, no timestamp */
    GST_BUFFER_TIMESTAMP (buf) = GST_CLOCK_TIME_NONE;
  }

  src->curoffset += readsamples;

  GST_DEBUG ("pushed buffer from soundcard of %ld bytes, timestamp %"
      G_GINT64_FORMAT, readbytes, GST_BUFFER_TIMESTAMP (buf));

  gst_pad_push (pad, buf);

  return;
}

static void
gst_oss_src_set_property (GObject * object, guint prop_id, const GValue * value,
    GParamSpec * pspec)
{
  GstOssSrc *src;

  src = GST_OSSSRC (object);

  switch (prop_id) {
    case ARG_BUFFERSIZE:
      src->buffersize = g_value_get_ulong (value);
      break;
    case ARG_FRAGMENT:
      GST_OSSELEMENT (src)->fragment = g_value_get_int (value);
      gst_osselement_sync_parms (GST_OSSELEMENT (src));
      break;
    default:
      break;
  }
}

static void
gst_oss_src_get_property (GObject * object, guint prop_id, GValue * value,
    GParamSpec * pspec)
{
  GstOssSrc *src;

  src = GST_OSSSRC (object);

  switch (prop_id) {
    case ARG_BUFFERSIZE:
      g_value_set_ulong (value, src->buffersize);
      break;
    case ARG_FRAGMENT:
      g_value_set_int (value, GST_OSSELEMENT (src)->fragment);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
  }
}

static GstElementStateReturn
gst_oss_src_change_state (GstElement * element)
{
  GstOssSrc *osssrc = GST_OSSSRC (element);

  GST_DEBUG ("osssrc: state change");

  switch (GST_STATE_TRANSITION (element)) {
    case GST_STATE_READY_TO_PAUSED:
      osssrc->curoffset = 0;
      break;
    case GST_STATE_PAUSED_TO_PLAYING:
      gst_audio_clock_set_active (GST_AUDIO_CLOCK (osssrc->provided_clock),
          TRUE);
      break;
    case GST_STATE_PLAYING_TO_PAUSED:
      gst_audio_clock_set_active (GST_AUDIO_CLOCK (osssrc->provided_clock),
          FALSE);
      break;
    case GST_STATE_PAUSED_TO_READY:
      if (GST_FLAG_IS_SET (element, GST_OSSSRC_OPEN))
        ioctl (GST_OSSELEMENT (osssrc)->fd, SNDCTL_DSP_RESET, 0);
      break;
    default:
      break;
  }

  if (GST_ELEMENT_CLASS (parent_class)->change_state)
    return GST_ELEMENT_CLASS (parent_class)->change_state (element);

  return GST_STATE_SUCCESS;
}

static const GstFormat *
gst_oss_src_get_formats (GstPad * pad)
{
  static const GstFormat formats[] = {
    GST_FORMAT_TIME,
    GST_FORMAT_DEFAULT,
    GST_FORMAT_BYTES,
    0
  };

  return formats;
}

static gboolean
gst_oss_src_convert (GstPad * pad, GstFormat src_format, gint64 src_value,
    GstFormat * dest_format, gint64 * dest_value)
{
  GstOssSrc *osssrc;

  osssrc = GST_OSSSRC (GST_PAD_PARENT (pad));

  return gst_osselement_convert (GST_OSSELEMENT (osssrc), src_format, src_value,
      dest_format, dest_value);
}

static const GstEventMask *
gst_oss_src_get_event_masks (GstPad * pad)
{
  static const GstEventMask gst_oss_src_src_event_masks[] = {
    {GST_EVENT_EOS, 0},
    {GST_EVENT_SIZE, 0},
    {0,}
  };

  return gst_oss_src_src_event_masks;
}

static gboolean
gst_oss_src_src_event (GstPad * pad, GstEvent * event)
{
  GstOssSrc *osssrc;
  gboolean retval = FALSE;

  osssrc = GST_OSSSRC (GST_PAD_PARENT (pad));

  switch (GST_EVENT_TYPE (event)) {
    case GST_EVENT_EOS:
      osssrc->need_eos = TRUE;
      retval = TRUE;
      break;
    case GST_EVENT_SIZE:
    {
      GstFormat format;
      gint64 value;

      format = GST_FORMAT_BYTES;

      /* convert to bytes */
      if (gst_osselement_convert (GST_OSSELEMENT (osssrc),
              GST_EVENT_SIZE_FORMAT (event),
              GST_EVENT_SIZE_VALUE (event), &format, &value)) {
        osssrc->buffersize = GST_EVENT_SIZE_VALUE (event);
        g_object_notify (G_OBJECT (osssrc), "buffersize");
        retval = TRUE;
      }
    }
    default:
      break;
  }
  gst_event_unref (event);
  return retval;
}

static gboolean
gst_oss_src_send_event (GstElement * element, GstEvent * event)
{
  GstOssSrc *osssrc = GST_OSSSRC (element);

  return gst_oss_src_src_event (osssrc->srcpad, event);
}

static const GstQueryType *
gst_oss_src_get_query_types (GstPad * pad)
{
  static const GstQueryType query_types[] = {
    GST_QUERY_POSITION,
    0,
  };

  return query_types;
}

static gboolean
gst_oss_src_src_query (GstPad * pad, GstQueryType type, GstFormat * format,
    gint64 * value)
{
  gboolean res = FALSE;
  GstOssSrc *osssrc;

  osssrc = GST_OSSSRC (GST_PAD_PARENT (pad));

  switch (type) {
    case GST_QUERY_POSITION:
      res = gst_osselement_convert (GST_OSSELEMENT (osssrc),
          GST_FORMAT_DEFAULT, osssrc->curoffset, format, value);
      break;
    default:
      break;
  }
  return res;
}
