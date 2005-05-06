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

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif
#include <gst/gst.h>
#include "mulaw-encode.h"
#include "mulaw-conversion.h"

extern GstPadTemplate *mulawenc_src_template, *mulawenc_sink_template;

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

static void gst_mulawenc_class_init (GstMuLawEncClass * klass);
static void gst_mulawenc_base_init (GstMuLawEncClass * klass);
static void gst_mulawenc_init (GstMuLawEnc * mulawenc);

static GstFlowReturn gst_mulawenc_chain (GstPad * pad, GstBuffer * buffer);

static GstElementClass *parent_class = NULL;

/*static guint gst_stereo_signals[LAST_SIGNAL] = { 0 }; */

static GstCaps *
mulawenc_getcaps (GstPad * pad)
{
  GstMuLawEnc *mulawenc;
  GstPad *otherpad;
  GstCaps *base_caps, *othercaps;

  mulawenc = GST_MULAWENC (GST_PAD_PARENT (pad));

  base_caps = gst_caps_copy (gst_pad_get_pad_template_caps (pad));

  if (pad == mulawenc->srcpad) {
    otherpad = mulawenc->sinkpad;
  } else {
    otherpad = mulawenc->srcpad;
  }
  othercaps = gst_pad_peer_get_caps (otherpad);
  if (othercaps) {
    GstStructure *structure;
    const GValue *orate, *ochans;
    const GValue *rate, *chans;
    GValue irate = { 0 }, ichans = {
    0};

    structure = gst_caps_get_structure (othercaps, 0);
    orate = gst_structure_get_value (structure, "rate");
    ochans = gst_structure_get_value (structure, "channels");
    if (!rate || !chans)
      goto done;

    structure = gst_caps_get_structure (base_caps, 0);
    rate = gst_structure_get_value (structure, "rate");
    chans = gst_structure_get_value (structure, "channels");
    if (!rate || !chans)
      goto done;

    gst_value_intersect (&irate, orate, rate);
    gst_value_intersect (&ichans, ochans, chans);

    /* Set the samplerate/channels on the to-be-returned caps */
    structure = gst_caps_get_structure (base_caps, 0);
    gst_structure_set_value (structure, "rate", &irate);
    gst_structure_set_value (structure, "channels", &ichans);

    gst_caps_unref (othercaps);
  }
done:
  return base_caps;
}

static gboolean
mulawenc_setcaps (GstPad * pad, GstCaps * caps)
{
  GstMuLawEnc *mulawenc;
  GstPad *otherpad;
  GstStructure *structure;
  const GValue *rate, *chans;
  GstCaps *base_caps;

  mulawenc = GST_MULAWENC (gst_pad_get_parent (pad));

  structure = gst_caps_get_structure (caps, 0);
  rate = gst_structure_get_value (structure, "rate");
  chans = gst_structure_get_value (structure, "channels");
  if (!rate || !chans)
    return FALSE;

  if (pad == mulawenc->sinkpad) {
    otherpad = mulawenc->srcpad;
  } else {
    otherpad = mulawenc->sinkpad;
  }
  base_caps = gst_caps_copy (gst_pad_get_pad_template_caps (otherpad));

  structure = gst_caps_get_structure (base_caps, 0);
  gst_structure_set_value (structure, "rate", rate);
  gst_structure_set_value (structure, "channels", chans);

  gst_pad_set_caps (otherpad, base_caps);
  gst_caps_unref (base_caps);

  return TRUE;
}

GType
gst_mulawenc_get_type (void)
{
  static GType mulawenc_type = 0;

  if (!mulawenc_type) {
    static const GTypeInfo mulawenc_info = {
      sizeof (GstMuLawEncClass),
      (GBaseInitFunc) gst_mulawenc_base_init,
      NULL,
      (GClassInitFunc) gst_mulawenc_class_init,
      NULL,
      NULL,
      sizeof (GstMuLawEnc),
      0,
      (GInstanceInitFunc) gst_mulawenc_init,
    };

    mulawenc_type =
        g_type_register_static (GST_TYPE_ELEMENT, "GstMuLawEnc", &mulawenc_info,
        0);
  }
  return mulawenc_type;
}

static void
gst_mulawenc_base_init (GstMuLawEncClass * klass)
{
  GstElementClass *element_class = GST_ELEMENT_CLASS (klass);
  GstElementDetails mulawenc_details = {
    "PCM to Mu Law conversion",
    "Codec/Encoder/Audio",
    "Convert 16bit PCM to 8bit mu law",
    "Zaheer Abbas Merali <zaheerabbas at merali dot org>"
  };

  gst_element_class_add_pad_template (element_class, mulawenc_src_template);
  gst_element_class_add_pad_template (element_class, mulawenc_sink_template);
  gst_element_class_set_details (element_class, &mulawenc_details);
}

static void
gst_mulawenc_class_init (GstMuLawEncClass * klass)
{
  parent_class = g_type_class_ref (GST_TYPE_ELEMENT);
}

static void
gst_mulawenc_init (GstMuLawEnc * mulawenc)
{
  mulawenc->sinkpad =
      gst_pad_new_from_template (mulawenc_sink_template, "sink");
  gst_pad_set_setcaps_function (mulawenc->sinkpad, mulawenc_setcaps);
  gst_pad_set_getcaps_function (mulawenc->sinkpad, mulawenc_getcaps);
  gst_pad_set_chain_function (mulawenc->sinkpad, gst_mulawenc_chain);
  gst_element_add_pad (GST_ELEMENT (mulawenc), mulawenc->sinkpad);

  mulawenc->srcpad = gst_pad_new_from_template (mulawenc_src_template, "src");
  gst_pad_set_setcaps_function (mulawenc->srcpad, mulawenc_setcaps);
  gst_pad_set_getcaps_function (mulawenc->srcpad, mulawenc_getcaps);
  gst_element_add_pad (GST_ELEMENT (mulawenc), mulawenc->srcpad);
}

static GstFlowReturn
gst_mulawenc_chain (GstPad * pad, GstBuffer * buffer)
{
  GstMuLawEnc *mulawenc;
  gint16 *linear_data;
  guint8 *mulaw_data;
  GstBuffer *outbuf;

  mulawenc = GST_MULAWENC (GST_OBJECT_PARENT (pad));

  linear_data = (gint16 *) GST_BUFFER_DATA (buffer);
  outbuf = gst_buffer_new_and_alloc (GST_BUFFER_SIZE (buffer) / 2);
  GST_BUFFER_TIMESTAMP (outbuf) = GST_BUFFER_TIMESTAMP (buffer);
  GST_BUFFER_DURATION (outbuf) = GST_BUFFER_DURATION (buffer);
  gst_buffer_set_caps (outbuf, GST_PAD_CAPS (mulawenc->srcpad));
  mulaw_data = (gint8 *) GST_BUFFER_DATA (outbuf);

  mulaw_encode (linear_data, mulaw_data, GST_BUFFER_SIZE (outbuf));

  gst_buffer_unref (buffer);

  return gst_pad_push (mulawenc->srcpad, outbuf);
}
