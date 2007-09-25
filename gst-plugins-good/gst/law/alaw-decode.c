/* GStreamer
 * Copyright (C) 1999 Erik Walthinsen <omega@cse.ogi.edu>
 * PCM - A-Law conversion
 *   Copyright (C) 2000 by Abramo Bagnara <abramo@alsa-project.org>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
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
#include <gst/gst.h>
#include "alaw-decode.h"

extern GstPadTemplate *alawdec_src_template, *alawdec_sink_template;

/* Stereo signals and args */
enum
{
  /* FILL ME */
  LAST_SIGNAL
};

enum
{
  ARG_0
};

static void gst_alawdec_class_init (GstALawDecClass * klass);
static void gst_alawdec_base_init (GstALawDecClass * klass);
static void gst_alawdec_init (GstALawDec * alawdec);
static GstStateChangeReturn
gst_alawdec_change_state (GstElement * element, GstStateChange transition);

static GstFlowReturn gst_alawdec_chain (GstPad * pad, GstBuffer * buffer);

static GstElementClass *parent_class = NULL;

/*static guint gst_stereo_signals[LAST_SIGNAL] = { 0 }; */

/* some day we might have defines in gstconfig.h that tell us about the
 * desired cpu/memory/binary size trade-offs */
#define GST_ALAW_DEC_USE_TABLE

#ifdef GST_ALAW_DEC_USE_TABLE

static const gint alaw_to_s16_table[256] = {
  -5504, -5248, -6016, -5760, -4480, -4224, -4992, -4736,
  -7552, -7296, -8064, -7808, -6528, -6272, -7040, -6784,
  -2752, -2624, -3008, -2880, -2240, -2112, -2496, -2368,
  -3776, -3648, -4032, -3904, -3264, -3136, -3520, -3392,
  -22016, -20992, -24064, -23040, -17920, -16896, -19968, -18944,
  -30208, -29184, -32256, -31232, -26112, -25088, -28160, -27136,
  -11008, -10496, -12032, -11520, -8960, -8448, -9984, -9472,
  -15104, -14592, -16128, -15616, -13056, -12544, -14080, -13568,
  -344, -328, -376, -360, -280, -264, -312, -296,
  -472, -456, -504, -488, -408, -392, -440, -424,
  -88, -72, -120, -104, -24, -8, -56, -40,
  -216, -200, -248, -232, -152, -136, -184, -168,
  -1376, -1312, -1504, -1440, -1120, -1056, -1248, -1184,
  -1888, -1824, -2016, -1952, -1632, -1568, -1760, -1696,
  -688, -656, -752, -720, -560, -528, -624, -592,
  -944, -912, -1008, -976, -816, -784, -880, -848,
  5504, 5248, 6016, 5760, 4480, 4224, 4992, 4736,
  7552, 7296, 8064, 7808, 6528, 6272, 7040, 6784,
  2752, 2624, 3008, 2880, 2240, 2112, 2496, 2368,
  3776, 3648, 4032, 3904, 3264, 3136, 3520, 3392,
  22016, 20992, 24064, 23040, 17920, 16896, 19968, 18944,
  30208, 29184, 32256, 31232, 26112, 25088, 28160, 27136,
  11008, 10496, 12032, 11520, 8960, 8448, 9984, 9472,
  15104, 14592, 16128, 15616, 13056, 12544, 14080, 13568,
  344, 328, 376, 360, 280, 264, 312, 296,
  472, 456, 504, 488, 408, 392, 440, 424,
  88, 72, 120, 104, 24, 8, 56, 40,
  216, 200, 248, 232, 152, 136, 184, 168,
  1376, 1312, 1504, 1440, 1120, 1056, 1248, 1184,
  1888, 1824, 2016, 1952, 1632, 1568, 1760, 1696,
  688, 656, 752, 720, 560, 528, 624, 592,
  944, 912, 1008, 976, 816, 784, 880, 848
};

static inline gint
alaw_to_s16 (guint8 a_val)
{
  return alaw_to_s16_table[a_val];
}

#else /* GST_ALAW_DEC_USE_TABLE */

static inline gint
alaw_to_s16 (guint8 a_val)
{
  gint t;
  gint seg;

  a_val ^= 0x55;
  t = a_val & 0x7f;
  if (t < 16)
    t = (t << 4) + 8;
  else {
    seg = (t >> 4) & 0x07;
    t = ((t & 0x0f) << 4) + 0x108;
    t <<= seg - 1;
  }
  return ((a_val & 0x80) ? t : -t);
}

#endif /* GST_ALAW_DEC_USE_TABLE */

static gboolean
alawdec_sink_setcaps (GstPad * pad, GstCaps * caps)
{
  GstALawDec *alawdec;
  GstStructure *structure;
  int rate, channels;
  gboolean ret;

  alawdec = GST_ALAWDEC (GST_PAD_PARENT (pad));

  structure = gst_caps_get_structure (caps, 0);
  ret = gst_structure_get_int (structure, "rate", &rate);
  ret = ret && gst_structure_get_int (structure, "channels", &channels);
  if (!ret)
    return FALSE;

  if (alawdec->srccaps)
    gst_caps_unref (alawdec->srccaps);
  alawdec->srccaps = gst_caps_new_simple ("audio/x-raw-int",
      "width", G_TYPE_INT, 16,
      "depth", G_TYPE_INT, 16,
      "endianness", G_TYPE_INT, G_BYTE_ORDER,
      "signed", G_TYPE_BOOLEAN, TRUE,
      "rate", G_TYPE_INT, rate, "channels", G_TYPE_INT, channels, NULL);

  return TRUE;
}

GType
gst_alawdec_get_type (void)
{
  static GType alawdec_type = 0;

  if (!alawdec_type) {
    static const GTypeInfo alawdec_info = {
      sizeof (GstALawDecClass),
      (GBaseInitFunc) gst_alawdec_base_init,
      NULL,
      (GClassInitFunc) gst_alawdec_class_init,
      NULL,
      NULL,
      sizeof (GstALawDec),
      0,
      (GInstanceInitFunc) gst_alawdec_init,
    };

    alawdec_type =
        g_type_register_static (GST_TYPE_ELEMENT, "GstALawDec", &alawdec_info,
        0);
  }
  return alawdec_type;
}

static void
gst_alawdec_base_init (GstALawDecClass * klass)
{
  GstElementClass *element_class = GST_ELEMENT_CLASS (klass);
  const GstElementDetails alawdec_details =
      GST_ELEMENT_DETAILS ("A Law audio decoder",
      "Codec/Decoder/Audio",
      "Convert 8bit A law to 16bit PCM",
      "Zaheer Abbas Merali <zaheerabbas at merali dot org>");

  gst_element_class_add_pad_template (element_class, alawdec_src_template);
  gst_element_class_add_pad_template (element_class, alawdec_sink_template);
  gst_element_class_set_details (element_class, &alawdec_details);
}

static void
gst_alawdec_class_init (GstALawDecClass * klass)
{
  GstElementClass *element_class = (GstElementClass *) klass;

  parent_class = g_type_class_peek_parent (klass);

  element_class->change_state = GST_DEBUG_FUNCPTR (gst_alawdec_change_state);
}

static void
gst_alawdec_init (GstALawDec * alawdec)
{
  alawdec->sinkpad = gst_pad_new_from_template (alawdec_sink_template, "sink");
  gst_pad_set_setcaps_function (alawdec->sinkpad, alawdec_sink_setcaps);
  gst_pad_set_chain_function (alawdec->sinkpad, gst_alawdec_chain);
  gst_element_add_pad (GST_ELEMENT (alawdec), alawdec->sinkpad);

  alawdec->srcpad = gst_pad_new_from_template (alawdec_src_template, "src");
  gst_pad_use_fixed_caps (alawdec->srcpad);
  gst_element_add_pad (GST_ELEMENT (alawdec), alawdec->srcpad);
}

static GstFlowReturn
gst_alawdec_chain (GstPad * pad, GstBuffer * buffer)
{
  GstALawDec *alawdec;
  gint16 *linear_data;
  guint8 *alaw_data;
  guint alaw_size;
  GstBuffer *outbuf;
  gint i;
  GstFlowReturn ret;

  alawdec = GST_ALAWDEC (gst_pad_get_parent (pad));

  alaw_data = GST_BUFFER_DATA (buffer);
  alaw_size = GST_BUFFER_SIZE (buffer);

  outbuf = gst_buffer_new_and_alloc (alaw_size * 2);
  linear_data = (gint16 *) GST_BUFFER_DATA (outbuf);

  /* copy discont flag */
  if (GST_BUFFER_FLAG_IS_SET (buffer, GST_BUFFER_FLAG_DISCONT))
    GST_BUFFER_FLAG_SET (outbuf, GST_BUFFER_FLAG_DISCONT);

  GST_BUFFER_TIMESTAMP (outbuf) = GST_BUFFER_TIMESTAMP (buffer);
  GST_BUFFER_DURATION (outbuf) = GST_BUFFER_DURATION (buffer);
  gst_buffer_set_caps (outbuf, alawdec->srccaps);

  for (i = 0; i < alaw_size; i++) {
    linear_data[i] = alaw_to_s16 (alaw_data[i]);
  }
  gst_buffer_unref (buffer);

  ret = gst_pad_push (alawdec->srcpad, outbuf);

  gst_object_unref (alawdec);

  return ret;
}

static GstStateChangeReturn
gst_alawdec_change_state (GstElement * element, GstStateChange transition)
{
  GstStateChangeReturn ret;
  GstALawDec *dec = GST_ALAWDEC (element);

  switch (transition) {
    default:
      break;
  }

  ret = GST_ELEMENT_CLASS (parent_class)->change_state (element, transition);
  if (ret != GST_STATE_CHANGE_SUCCESS)
    return ret;

  switch (transition) {
    case GST_STATE_CHANGE_PAUSED_TO_READY:
      if (dec->srccaps) {
        gst_caps_unref (dec->srccaps);
        dec->srccaps = NULL;
      }
      break;
    default:
      break;
  }

  return ret;
}
