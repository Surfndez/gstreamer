/* GStreamer Wavpack encoder plugin
 * Copyright (c) 2006 Sebastian Dröge <slomo@circular-chaos.org>
 *
 * gstwavpackdec.c: Wavpack audio encoder
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

/**
 * SECTION:element-wavpackenc
 *
 * <refsect2>
 * WavpackEnc encodes raw audio into a framed Wavpack stream.
 * <ulink url="http://www.wavpack.com/">Wavpack</ulink> is an open-source
 * audio codec that features both lossless and lossy encoding.
 * <title>Example launch line</title>
 * <para>
 * <programlisting>
 * gst-launch audiotestsrc num-buffers=500 ! wavpackenc ! filesink location=sinewave.wv
 * </programlisting>
 * This pipeline encodes audio from audiotestsrc into a Wavpack file.
 * </para>
 * <para>
 * <programlisting>
 * gst-launch cdda://1 ! wavpackenc ! filesink location=track1.wv
 * </programlisting>
 * This pipeline encodes audio from an audio CD into a Wavpack file using
 * lossless encoding (the file output will be fairly large).
 * </para>
 * <para>
 * <programlisting>
 * gst-launch cdda://1 ! wavpackenc bitrate=128000 ! filesink location=track1.wv
 * </programlisting>
 * This pipeline encodes audio from an audio CD into a Wavpack file using
 * lossy encoding at a certain bitrate (the file will be fairly small).
 * </para>
 * </refsect2>
 */

/*
 * TODO: - add multichannel handling. channel_mask is:
 *                  front left
 *                  front right
 *                  center
 *                  LFE
 *                  back left
 *                  back right
 *                  front left center
 *                  front right center
 *                  back left
 *                  back center
 *                  side left
 *                  side right
 *                  ...
 *        - add 32 bit float mode. CONFIG_FLOAT_DATA
 */

#include <string.h>
#include <gst/gst.h>
#include <glib/gprintf.h>

#include <wavpack/wavpack.h>
#include "gstwavpackenc.h"
#include "gstwavpackcommon.h"
#include "md5.h"

static GstFlowReturn gst_wavpack_enc_chain (GstPad * pad, GstBuffer * buffer);
static gboolean gst_wavpack_enc_sink_set_caps (GstPad * pad, GstCaps * caps);
static int gst_wavpack_enc_push_block (void *id, void *data, int32_t count);
static gboolean gst_wavpack_enc_sink_event (GstPad * pad, GstEvent * event);
static GstStateChangeReturn gst_wavpack_enc_change_state (GstElement * element,
    GstStateChange transition);
static void gst_wavpack_enc_finalize (GObject * object);
static void gst_wavpack_enc_set_property (GObject * object, guint prop_id,
    const GValue * value, GParamSpec * pspec);
static void gst_wavpack_enc_get_property (GObject * object, guint prop_id,
    GValue * value, GParamSpec * pspec);

enum
{
  ARG_0,
  ARG_MODE,
  ARG_BITRATE,
  ARG_BITSPERSAMPLE,
  ARG_CORRECTION_MODE,
  ARG_MD5,
  ARG_EXTRA_PROCESSING,
  ARG_JOINT_STEREO_MODE
};

GST_DEBUG_CATEGORY_STATIC (gst_wavpack_enc_debug);
#define GST_CAT_DEFAULT gst_wavpack_enc_debug

static GstStaticPadTemplate sink_factory = GST_STATIC_PAD_TEMPLATE ("sink",
    GST_PAD_SINK,
    GST_PAD_ALWAYS,
    GST_STATIC_CAPS ("audio/x-raw-int, "
        "width = (int) 32, "
        "depth = (int) 32, "
        "endianness = (int) LITTLE_ENDIAN, "
        "channels = (int) [ 1, 2 ], "
        "rate = (int) [ 6000, 192000 ]," "signed = (boolean) TRUE;"
        "audio/x-raw-int, "
        "width = (int) 24, "
        "depth = (int) 24, "
        "endianness = (int) LITTLE_ENDIAN, "
        "channels = (int) [ 1, 2 ], "
        "rate = (int) [ 6000, 192000 ]," "signed = (boolean) TRUE;"
        "audio/x-raw-int, "
        "width = (int) 16, "
        "depth = (int) 16, "
        "endianness = (int) LITTLE_ENDIAN, "
        "channels = (int) [ 1, 2 ], "
        "rate = (int) [ 6000, 192000 ]," "signed = (boolean) TRUE;"
        "audio/x-raw-int, "
        "width = (int) 8, "
        "depth = (int) 8, "
        "endianness = (int) LITTLE_ENDIAN, "
        "channels = (int) [ 1, 2 ], "
        "rate = (int) [ 6000, 192000 ]," "signed = (boolean) TRUE")
    );

static GstStaticPadTemplate src_factory = GST_STATIC_PAD_TEMPLATE ("src",
    GST_PAD_SRC,
    GST_PAD_ALWAYS,
    GST_STATIC_CAPS ("audio/x-wavpack, "
        "width = (int) { 8, 16, 24, 32 }, "
        "channels = (int) [ 1, 2 ], "
        "rate = (int) [ 6000, 192000 ], " "framed = (boolean) TRUE")
    );

static GstStaticPadTemplate wvcsrc_factory = GST_STATIC_PAD_TEMPLATE ("wvcsrc",
    GST_PAD_SRC,
    GST_PAD_SOMETIMES,
    GST_STATIC_CAPS ("audio/x-wavpack-correction, " "framed = (boolean) TRUE")
    );

enum
{
  GST_WAVPACK_ENC_MODE_VERY_FAST = 0,
  GST_WAVPACK_ENC_MODE_FAST,
  GST_WAVPACK_ENC_MODE_DEFAULT,
  GST_WAVPACK_ENC_MODE_HIGH,
  GST_WAVPACK_ENC_MODE_VERY_HIGH
};

#define GST_TYPE_WAVPACK_ENC_MODE (gst_wavpack_enc_mode_get_type ())
static GType
gst_wavpack_enc_mode_get_type (void)
{
  static GType qtype = 0;

  if (qtype == 0) {
    static const GEnumValue values[] = {
#if 0
      /* Very Fast Compression is not supported yet, but will be supported
       * in future wavpack versions */
      {0, "Very Fast Compression", "veryfast"},
#endif
      {1, "Fast Compression", "fast"},
      {2, "Normal Compression", "normal"},
      {3, "High Compression", "high"},
#ifndef WAVPACK_OLD_API
      {4, "Very High Compression", "veryhigh"},
#endif
      {0, NULL, NULL}
    };

    qtype = g_enum_register_static ("GstWavpackEncMode", values);
  }
  return qtype;
}

enum
{
  GST_WAVPACK_CORRECTION_MODE_OFF = 0,
  GST_WAVPACK_CORRECTION_MODE_ON,
  GST_WAVPACK_CORRECTION_MODE_OPTIMIZED
};

#define GST_TYPE_WAVPACK_ENC_CORRECTION_MODE (gst_wavpack_enc_correction_mode_get_type ())
static GType
gst_wavpack_enc_correction_mode_get_type (void)
{
  static GType qtype = 0;

  if (qtype == 0) {
    static const GEnumValue values[] = {
      {0, "Create no correction file", "off"},
      {1, "Create correction file", "on"},
      {2, "Create optimized correction file", "optimized"},
      {0, NULL, NULL}
    };

    qtype = g_enum_register_static ("GstWavpackEncCorrectionMode", values);
  }
  return qtype;
}

enum
{
  GST_WAVPACK_JS_MODE_AUTO = 0,
  GST_WAVPACK_JS_MODE_LEFT_RIGHT,
  GST_WAVPACK_JS_MODE_MID_SIDE
};

#define GST_TYPE_WAVPACK_ENC_JOINT_STEREO_MODE (gst_wavpack_enc_joint_stereo_mode_get_type ())
static GType
gst_wavpack_enc_joint_stereo_mode_get_type (void)
{
  static GType qtype = 0;

  if (qtype == 0) {
    static const GEnumValue values[] = {
      {0, "auto", "auto"},
      {1, "left/right", "leftright"},
      {2, "mid/side", "midside"},
      {0, NULL, NULL}
    };

    qtype = g_enum_register_static ("GstWavpackEncJSMode", values);
  }
  return qtype;
}

GST_BOILERPLATE (GstWavpackEnc, gst_wavpack_enc, GstElement, GST_TYPE_ELEMENT);

static void
gst_wavpack_enc_base_init (gpointer klass)
{
  static const GstElementDetails element_details = {
    "Wavpack audio encoder",
    "Codec/Encoder/Audio",
    "Encodes audio with the Wavpack lossless/lossy audio codec",
    "Sebastian Dröge <slomo@circular-chaos.org>"
  };
  GstElementClass *element_class = GST_ELEMENT_CLASS (klass);

  /* add pad templates */
  gst_element_class_add_pad_template (element_class,
      gst_static_pad_template_get (&sink_factory));
  gst_element_class_add_pad_template (element_class,
      gst_static_pad_template_get (&src_factory));
  gst_element_class_add_pad_template (element_class,
      gst_static_pad_template_get (&wvcsrc_factory));

  /* set element details */
  gst_element_class_set_details (element_class, &element_details);
}


static void
gst_wavpack_enc_class_init (GstWavpackEncClass * klass)
{
  GObjectClass *gobject_class = (GObjectClass *) klass;
  GstElementClass *gstelement_class = (GstElementClass *) klass;

  parent_class = g_type_class_peek_parent (klass);

  /* set state change handler */
  gstelement_class->change_state =
      GST_DEBUG_FUNCPTR (gst_wavpack_enc_change_state);
  gobject_class->finalize = GST_DEBUG_FUNCPTR (gst_wavpack_enc_finalize);

  /* set property handlers */
  gobject_class->set_property =
      GST_DEBUG_FUNCPTR (gst_wavpack_enc_set_property);
  gobject_class->get_property =
      GST_DEBUG_FUNCPTR (gst_wavpack_enc_get_property);

  /* install all properties */
  g_object_class_install_property (gobject_class, ARG_MODE,
      g_param_spec_enum ("mode", "Encoding mode",
          "Speed versus compression tradeoff.",
          GST_TYPE_WAVPACK_ENC_MODE, GST_WAVPACK_ENC_MODE_DEFAULT,
          G_PARAM_READWRITE));
  g_object_class_install_property (gobject_class, ARG_BITRATE,
      g_param_spec_double ("bitrate", "Bitrate",
          "Try to encode with this average bitrate (bits/sec). "
          "This enables lossy encoding! A value smaller than 24000.0 disables this.",
          0.0, 9600000.0, 0.0, G_PARAM_READWRITE));
  g_object_class_install_property (gobject_class, ARG_BITSPERSAMPLE,
      g_param_spec_double ("bits-per-sample", "Bits per sample",
          "Try to encode with this amount of bits per sample. "
          "This enables lossy encoding! A value smaller than 2.0 disables this.",
          0.0, 24.0, 0.0, G_PARAM_READWRITE));
  g_object_class_install_property (gobject_class, ARG_CORRECTION_MODE,
      g_param_spec_enum ("correction-mode", "Correction file mode",
          "Use this mode for correction file creation. Only works in lossy mode!",
          GST_TYPE_WAVPACK_ENC_CORRECTION_MODE, GST_WAVPACK_CORRECTION_MODE_OFF,
          G_PARAM_READWRITE));
  g_object_class_install_property (gobject_class, ARG_MD5,
      g_param_spec_boolean ("md5", "MD5",
          "Store MD5 hash of raw samples within the file.", FALSE,
          G_PARAM_READWRITE));
  g_object_class_install_property (gobject_class, ARG_EXTRA_PROCESSING,
      g_param_spec_boolean ("extra-processing", "Extra processing",
          "Extra encode processing.", FALSE, G_PARAM_READWRITE));
  g_object_class_install_property (gobject_class, ARG_JOINT_STEREO_MODE,
      g_param_spec_enum ("joint-stereo-mode", "Joint-Stereo mode",
          "Use this joint-stereo mode.", GST_TYPE_WAVPACK_ENC_JOINT_STEREO_MODE,
          GST_WAVPACK_JS_MODE_AUTO, G_PARAM_READWRITE));
}

static void
gst_wavpack_enc_reset (GstWavpackEnc * enc)
{
  /* close and free everything stream related if we already did something */
  if (enc->wp_context) {
    WavpackCloseFile (enc->wp_context);
    enc->wp_context = NULL;
  }
  if (enc->wp_config) {
    g_free (enc->wp_config);
    enc->wp_config = NULL;
  }
  if (enc->first_block) {
    g_free (enc->first_block);
    enc->first_block = NULL;
  }
  enc->first_block_size = 0;
  if (enc->md5_context) {
    g_free (enc->md5_context);
    enc->md5_context = NULL;
  }

  /* reset the last returns to GST_FLOW_OK. This is only set to something else
   * while WavpackPackSamples() or more specific gst_wavpack_enc_push_block()
   * so not valid anymore */
  enc->srcpad_last_return = enc->wvcsrcpad_last_return = GST_FLOW_OK;

  /* reset stream information */
  enc->samplerate = 0;
  enc->width = 0;
  enc->channels = 0;
}

static void
gst_wavpack_enc_init (GstWavpackEnc * enc, GstWavpackEncClass * gclass)
{
  enc->sinkpad = gst_pad_new_from_static_template (&sink_factory, "sink");
  gst_pad_set_setcaps_function (enc->sinkpad,
      GST_DEBUG_FUNCPTR (gst_wavpack_enc_sink_set_caps));
  gst_pad_set_chain_function (enc->sinkpad,
      GST_DEBUG_FUNCPTR (gst_wavpack_enc_chain));
  gst_pad_set_event_function (enc->sinkpad,
      GST_DEBUG_FUNCPTR (gst_wavpack_enc_sink_event));
  gst_element_add_pad (GST_ELEMENT (enc), enc->sinkpad);

  /* setup src pad */
  enc->srcpad = gst_pad_new_from_static_template (&src_factory, "src");
  gst_element_add_pad (GST_ELEMENT (enc), enc->srcpad);

  /* initialize object attributes */
  enc->wp_config = NULL;
  enc->wp_context = NULL;
  enc->first_block = NULL;
  enc->md5_context = NULL;
  gst_wavpack_enc_reset (enc);

  enc->wv_id = g_new0 (GstWavpackEncWriteID, 1);
  enc->wv_id->correction = FALSE;
  enc->wv_id->wavpack_enc = enc;
  enc->wvc_id = g_new0 (GstWavpackEncWriteID, 1);
  enc->wvc_id->correction = TRUE;
  enc->wvc_id->wavpack_enc = enc;

  /* set default values of params */
  enc->mode = GST_WAVPACK_ENC_MODE_DEFAULT;
  enc->bitrate = 0.0;
  enc->correction_mode = GST_WAVPACK_CORRECTION_MODE_OFF;
  enc->md5 = FALSE;
  enc->extra_processing = FALSE;
  enc->joint_stereo_mode = GST_WAVPACK_JS_MODE_AUTO;
}

static void
gst_wavpack_enc_finalize (GObject * object)
{
  GstWavpackEnc *enc = GST_WAVPACK_ENC (object);

  /* free the blockout helpers */
  g_free (enc->wv_id);
  g_free (enc->wvc_id);

  G_OBJECT_CLASS (parent_class)->finalize (object);
}

static gboolean
gst_wavpack_enc_sink_set_caps (GstPad * pad, GstCaps * caps)
{
  GstWavpackEnc *enc = GST_WAVPACK_ENC (gst_pad_get_parent (pad));
  GstStructure *structure = gst_caps_get_structure (caps, 0);
  int depth = 0;

  /* check caps and put relevant parts into our object attributes */
  if (!gst_structure_get_int (structure, "channels", &enc->channels) ||
      !gst_structure_get_int (structure, "rate", &enc->samplerate) ||
      !gst_structure_get_int (structure, "width", &enc->width) ||
      !(gst_structure_get_int (structure, "depth", &depth) ||
          depth != enc->width)) {
    GST_ELEMENT_ERROR (enc, LIBRARY, INIT, (NULL),
        ("got invalid caps: %" GST_PTR_FORMAT, caps));
    gst_object_unref (enc);
    return FALSE;
  }

  /* set fixed src pad caps now that we know what we will get */
  caps = gst_caps_new_simple ("audio/x-wavpack",
      "channels", G_TYPE_INT, enc->channels,
      "rate", G_TYPE_INT, enc->samplerate,
      "width", G_TYPE_INT, enc->width, "framed", G_TYPE_BOOLEAN, TRUE, NULL);

  if (!gst_pad_set_caps (enc->srcpad, caps)) {
    GST_ELEMENT_ERROR (enc, LIBRARY, INIT, (NULL),
        ("setting caps failed: %" GST_PTR_FORMAT, caps));
    gst_caps_unref (caps);
    gst_object_unref (enc);
    return FALSE;
  }
  gst_pad_use_fixed_caps (enc->srcpad);

  gst_caps_unref (caps);
  gst_object_unref (enc);
  return TRUE;
}

static void
gst_wavpack_enc_set_wp_config (GstWavpackEnc * enc)
{
  enc->wp_config = g_new0 (WavpackConfig, 1);
  /* set general stream informations in the WavpackConfig */
  enc->wp_config->bytes_per_sample = (enc->width + 7) >> 3;
  enc->wp_config->bits_per_sample = enc->width;
  enc->wp_config->num_channels = enc->channels;

  /* TODO: handle more than 2 channels correctly! */
  if (enc->channels == 1) {
    enc->wp_config->channel_mask = 0x4;
  } else if (enc->channels == 2) {
    enc->wp_config->channel_mask = 0x2 | 0x1;
  }
  enc->wp_config->sample_rate = enc->samplerate;

  /*
   * Set parameters in WavpackConfig
   */

  /* Encoding mode */
  switch (enc->mode) {
#if 0
    case GST_WAVPACK_ENC_MODE_VERY_FAST:
      enc->wp_config->flags |= CONFIG_VERY_FAST_FLAG;
      enc->wp_config->flags |= CONFIG_FAST_FLAG;
      break;
#endif
    case GST_WAVPACK_ENC_MODE_FAST:
      enc->wp_config->flags |= CONFIG_FAST_FLAG;
      break;
    case GST_WAVPACK_ENC_MODE_DEFAULT:
      break;
    case GST_WAVPACK_ENC_MODE_HIGH:
      enc->wp_config->flags |= CONFIG_HIGH_FLAG;
      break;
#ifndef WAVPACK_OLD_API
    case GST_WAVPACK_ENC_MODE_VERY_HIGH:
      enc->wp_config->flags |= CONFIG_HIGH_FLAG;
      enc->wp_config->flags |= CONFIG_VERY_HIGH_FLAG;
      break;
#endif
  }

  /* Bitrate, enables lossy mode */
  if (enc->bitrate >= 2.0) {
    enc->wp_config->flags |= CONFIG_HYBRID_FLAG;
    if (enc->bitrate >= 24000.0) {
      enc->wp_config->bitrate = enc->bitrate / 1000.0;
      enc->wp_config->flags |= CONFIG_BITRATE_KBPS;
    } else {
      enc->wp_config->bitrate = enc->bitrate;
    }
  }

  /* Correction Mode, only in lossy mode */
  if (enc->wp_config->flags & CONFIG_HYBRID_FLAG) {
    if (enc->correction_mode > GST_WAVPACK_CORRECTION_MODE_OFF) {
      enc->wvcsrcpad =
          gst_pad_new_from_static_template (&wvcsrc_factory, "wvcsrc");

      /* try to add correction src pad, don't set correction mode on failure */
      GstCaps *caps = gst_caps_new_simple ("audio/x-wavpack-correction",
          "framed", G_TYPE_BOOLEAN, TRUE, NULL);

      GST_DEBUG_OBJECT (enc, "Adding correction pad with caps %"
          GST_PTR_FORMAT, caps);
      if (!gst_pad_set_caps (enc->wvcsrcpad, caps)) {
        enc->correction_mode = 0;
        GST_WARNING_OBJECT (enc, "setting correction caps failed");
      } else {
        gst_pad_use_fixed_caps (enc->wvcsrcpad);
        gst_pad_set_active (enc->wvcsrcpad, TRUE);
        gst_element_add_pad (GST_ELEMENT (enc), enc->wvcsrcpad);
        enc->wp_config->flags |= CONFIG_CREATE_WVC;
        if (enc->correction_mode == GST_WAVPACK_CORRECTION_MODE_OPTIMIZED) {
          enc->wp_config->flags |= CONFIG_OPTIMIZE_WVC;
        }
      }
      gst_caps_unref (caps);
    }
  } else {
    if (enc->correction_mode > GST_WAVPACK_CORRECTION_MODE_OFF) {
      enc->correction_mode = 0;
      GST_WARNING_OBJECT (enc, "setting correction mode only has "
          "any effect if a bitrate is provided.");
    }
  }
  gst_element_no_more_pads (GST_ELEMENT (enc));

  /* MD5, setup MD5 context */
  if ((enc->md5) && !(enc->md5_context)) {
    enc->wp_config->flags |= CONFIG_MD5_CHECKSUM;
    enc->md5_context = g_new0 (MD5_CTX, 1);
    MD5Init (enc->md5_context);
  }

  /* Extra encode processing */
  if (enc->extra_processing) {
    enc->wp_config->flags |= CONFIG_EXTRA_MODE;
  }

  /* Joint stereo mode */
  switch (enc->joint_stereo_mode) {
    case GST_WAVPACK_JS_MODE_AUTO:
      break;
    case GST_WAVPACK_JS_MODE_LEFT_RIGHT:
      enc->wp_config->flags |= CONFIG_JOINT_OVERRIDE;
      enc->wp_config->flags &= ~CONFIG_JOINT_STEREO;
      break;
    case GST_WAVPACK_JS_MODE_MID_SIDE:
      enc->wp_config->flags |= (CONFIG_JOINT_OVERRIDE | CONFIG_JOINT_STEREO);
      break;
  }
}

static int32_t *
gst_wavpack_enc_format_samples (const uchar * src_data, uint32_t sample_count,
    guint width)
{
  int32_t *data = g_new0 (int32_t, sample_count);

  /* put all samples into an int32_t*, no matter what
   * width we have and convert them from little endian
   * to host byte order */

  switch (width) {
      int i;

    case 8:
      for (i = 0; i < sample_count; i++)
        data[i] = (int32_t) (int8_t) src_data[i];
      break;
    case 16:
      for (i = 0; i < sample_count; i++)
        data[i] = (int32_t) src_data[2 * i]
            | ((int32_t) (int8_t) src_data[2 * i + 1] << 8);
      break;
    case 24:
      for (i = 0; i < sample_count; i++)
        data[i] = (int32_t) src_data[3 * i]
            | ((int32_t) src_data[3 * i + 1] << 8)
            | ((int32_t) (int8_t) src_data[3 * i + 2] << 16);
      break;
    case 32:
      for (i = 0; i < sample_count; i++)
        data[i] = (int32_t) src_data[4 * i]
            | ((int32_t) src_data[4 * i + 1] << 8)
            | ((int32_t) src_data[4 * i + 2] << 16)
            | ((int32_t) (int8_t) src_data[4 * i + 3] << 24);
      break;
  }

  return data;
}

static int
gst_wavpack_enc_push_block (void *id, void *data, int32_t count)
{
  GstWavpackEncWriteID *wid = (GstWavpackEncWriteID *) id;
  GstWavpackEnc *enc = GST_WAVPACK_ENC (wid->wavpack_enc);
  GstFlowReturn *flow;
  GstBuffer *buffer;
  GstPad *pad;
  guchar *block = (guchar *) data;

  pad = (wid->correction) ? enc->wvcsrcpad : enc->srcpad;
  flow =
      (wid->correction) ? &enc->wvcsrcpad_last_return : &enc->
      srcpad_last_return;

  *flow = gst_pad_alloc_buffer_and_set_caps (pad, GST_BUFFER_OFFSET_NONE,
      count, GST_PAD_CAPS (pad), &buffer);

  if (*flow != GST_FLOW_OK) {
    GST_WARNING_OBJECT (enc, "flow on %s:%s = %s",
        GST_DEBUG_PAD_NAME (pad), gst_flow_get_name (*flow));
    return FALSE;
  }

  g_memmove (GST_BUFFER_DATA (buffer), block, count);

  if (count > sizeof (WavpackHeader) && memcmp (block, "wvpk", 4) == 0) {
    /* if it's a Wavpack block set buffer timestamp and duration, etc */
    WavpackHeader wph;

    GST_LOG_OBJECT (enc, "got %d bytes of encoded wavpack %sdata",
        count, (wid->correction) ? "correction " : "");

    gst_wavpack_read_header (&wph, block);

    /* if it's the first wavpack block, send a NEW_SEGMENT event */
    if (wph.block_index == 0) {
      gst_pad_push_event (pad,
          gst_event_new_new_segment (FALSE,
              1.0, GST_FORMAT_BYTES, 0, GST_BUFFER_OFFSET_NONE, 0));

      /* save header for later reference, so we can re-send it later on
       * EOS with fixed up values for total sample count etc. */
      if (enc->first_block == NULL && !wid->correction) {
        enc->first_block = g_memdup (block, count);
        enc->first_block_size = count;
      }
    }

    /* set buffer timestamp, duration, offset, offset_end from
     * the wavpack header */
    GST_BUFFER_TIMESTAMP (buffer) =
        gst_util_uint64_scale_int (GST_SECOND, wph.block_index,
        enc->samplerate);
    GST_BUFFER_DURATION (buffer) =
        gst_util_uint64_scale_int (GST_SECOND, wph.block_samples,
        enc->samplerate);
    GST_BUFFER_OFFSET (buffer) = wph.block_index;
    GST_BUFFER_OFFSET_END (buffer) = wph.block_index + wph.block_samples;
  } else {
    /* if it's something else set no timestamp and duration on the buffer */
    GST_DEBUG_OBJECT (enc, "got %d bytes of unknown data", count);

    GST_BUFFER_TIMESTAMP (buffer) = GST_CLOCK_TIME_NONE;
    GST_BUFFER_DURATION (buffer) = GST_CLOCK_TIME_NONE;
  }

  /* push the buffer and forward errors */
  *flow = gst_pad_push (pad, buffer);

  if (*flow != GST_FLOW_OK) {
    GST_WARNING_OBJECT (enc, "flow on %s:%s = %s",
        GST_DEBUG_PAD_NAME (pad), gst_flow_get_name (*flow));
    return FALSE;
  }

  return TRUE;
}

static GstFlowReturn
gst_wavpack_enc_chain (GstPad * pad, GstBuffer * buf)
{
  GstWavpackEnc *enc = GST_WAVPACK_ENC (gst_pad_get_parent (pad));
  uint32_t sample_count = GST_BUFFER_SIZE (buf) / ((enc->width + 7) >> 3);
  int32_t *data;
  GstFlowReturn ret;

  /* reset the last returns to GST_FLOW_OK. This is only set to something else
   * while WavpackPackSamples() or more specific gst_wavpack_enc_push_block()
   * so not valid anymore */
  enc->srcpad_last_return = enc->wvcsrcpad_last_return = GST_FLOW_OK;

  GST_DEBUG ("got %u raw samples", sample_count);

  /* check if we already have a valid WavpackContext, otherwise make one */
  if (!enc->wp_context) {
    /* create raw context */
    enc->wp_context =
        WavpackOpenFileOutput (gst_wavpack_enc_push_block, enc->wv_id,
        (enc->correction_mode > 0) ? enc->wvc_id : NULL);
    if (!enc->wp_context) {
      GST_ELEMENT_ERROR (enc, LIBRARY, INIT, (NULL),
          ("error creating Wavpack context"));
      gst_object_unref (enc);
      gst_buffer_unref (buf);
      return GST_FLOW_ERROR;
    }

    /* set the WavpackConfig according to our parameters */
    gst_wavpack_enc_set_wp_config (enc);

    /* set the configuration to the context now that we know everything
     * and initialize the encoder */
    if (!WavpackSetConfiguration (enc->wp_context,
            enc->wp_config, (uint32_t) (-1))
        || !WavpackPackInit (enc->wp_context)) {
      GST_ELEMENT_ERROR (enc, LIBRARY, SETTINGS, (NULL),
          ("error setting up wavpack encoding context"));
      WavpackCloseFile (enc->wp_context);
      gst_object_unref (enc);
      gst_buffer_unref (buf);
      return GST_FLOW_ERROR;
    }
    GST_DEBUG ("setup of encoding context successfull");
  }

  /* if we want to append the MD5 sum to the stream update it here
   * with the current raw samples */
  if (enc->md5) {
    MD5Update (enc->md5_context, GST_BUFFER_DATA (buf), GST_BUFFER_SIZE (buf));
  }

  /* put all samples into an int32_t*, no matter what
   * width we have and convert them from little endian
   * to host byte order */
  data =
      gst_wavpack_enc_format_samples (GST_BUFFER_DATA (buf), sample_count,
      enc->width);

  gst_buffer_unref (buf);

  /* encode and handle return values from encoding */
  if (WavpackPackSamples (enc->wp_context, data, sample_count / enc->channels)) {
    GST_DEBUG ("encoding samples successful");
    ret = GST_FLOW_OK;
  } else {
    if ((enc->srcpad_last_return == GST_FLOW_RESEND) ||
        (enc->wvcsrcpad_last_return == GST_FLOW_RESEND)) {
      ret = GST_FLOW_RESEND;
    } else if ((enc->srcpad_last_return == GST_FLOW_OK) ||
        (enc->wvcsrcpad_last_return == GST_FLOW_OK)) {
      ret = GST_FLOW_OK;
    } else if ((enc->srcpad_last_return == GST_FLOW_NOT_LINKED) &&
        (enc->wvcsrcpad_last_return == GST_FLOW_NOT_LINKED)) {
      ret = GST_FLOW_NOT_LINKED;
    } else if ((enc->srcpad_last_return == GST_FLOW_WRONG_STATE) &&
        (enc->wvcsrcpad_last_return == GST_FLOW_WRONG_STATE)) {
      ret = GST_FLOW_WRONG_STATE;
    } else {
      GST_ELEMENT_ERROR (enc, LIBRARY, ENCODE, (NULL),
          ("encoding samples failed"));
      ret = GST_FLOW_ERROR;
    }
  }

  g_free (data);
  gst_object_unref (enc);
  return ret;
}

static void
gst_wavpack_enc_rewrite_first_block (GstWavpackEnc * enc)
{
  GstEvent *event = gst_event_new_new_segment (TRUE, 1.0, GST_FORMAT_BYTES,
      0, GST_BUFFER_OFFSET_NONE, 0);
  gboolean ret;

  g_return_if_fail (enc);
  g_return_if_fail (enc->first_block);

  /* update the sample count in the first block */
  WavpackUpdateNumSamples (enc->wp_context, enc->first_block);

  /* try to seek to the beginning of the output */
  ret = gst_pad_push_event (enc->srcpad, event);
  if (ret) {
    /* try to rewrite the first block */
    GST_DEBUG_OBJECT (enc, "rewriting first block ...");
    ret = gst_wavpack_enc_push_block (enc->wv_id,
        enc->first_block, enc->first_block_size);
  } else {
    GST_WARNING_OBJECT (enc, "rewriting of first block failed. "
        "Seeking to first block failed!");
  }
}

static gboolean
gst_wavpack_enc_sink_event (GstPad * pad, GstEvent * event)
{
  GstWavpackEnc *enc = GST_WAVPACK_ENC (gst_pad_get_parent (pad));
  gboolean ret = TRUE;

  GST_DEBUG ("Received %s event on sinkpad", GST_EVENT_TYPE_NAME (event));

  switch (GST_EVENT_TYPE (event)) {
    case GST_EVENT_EOS:
      /* Encode all remaining samples and flush them to the src pads */
      WavpackFlushSamples (enc->wp_context);

      /* write the MD5 sum if we have to write one */
      if ((enc->md5) && (enc->md5_context)) {
        guchar md5_digest[16];

        MD5Final (md5_digest, enc->md5_context);
        WavpackStoreMD5Sum (enc->wp_context, md5_digest);
      }

      /* Try to rewrite the first frame with the correct sample number */
      if (enc->first_block)
        gst_wavpack_enc_rewrite_first_block (enc);

      /* close the context if not already happened */
      if (enc->wp_context) {
        WavpackCloseFile (enc->wp_context);
        enc->wp_context = NULL;
      }

      ret = gst_pad_event_default (pad, event);
      break;
    case GST_EVENT_NEWSEGMENT:
      if (enc->wp_context) {
        GST_WARNING_OBJECT (enc, "got NEWSEGMENT after encoding "
            "already started");
      }
      /* drop NEWSEGMENT events, we create our own when pushing
       * the first buffer to the pads */
      gst_event_unref (event);
      ret = TRUE;
      break;
    default:
      ret = gst_pad_event_default (pad, event);
      break;
  }

  gst_object_unref (enc);
  return ret;
}

static GstStateChangeReturn
gst_wavpack_enc_change_state (GstElement * element, GstStateChange transition)
{
  GstStateChangeReturn ret = GST_STATE_CHANGE_SUCCESS;
  GstWavpackEnc *enc = GST_WAVPACK_ENC (element);

  switch (transition) {
    case GST_STATE_CHANGE_NULL_TO_READY:
      /* set the last returned GstFlowReturns of the two pads to GST_FLOW_OK
       * as they're only set to something else in WavpackPackSamples() or more
       * specific gst_wavpack_enc_push_block() and nothing happened there yet */
      enc->srcpad_last_return = enc->wvcsrcpad_last_return = GST_FLOW_OK;
      break;
    case GST_STATE_CHANGE_READY_TO_PAUSED:
      break;
    case GST_STATE_CHANGE_PAUSED_TO_PLAYING:
    default:
      break;
  }

  ret = GST_ELEMENT_CLASS (parent_class)->change_state (element, transition);

  switch (transition) {
    case GST_STATE_CHANGE_PLAYING_TO_PAUSED:
      break;
    case GST_STATE_CHANGE_PAUSED_TO_READY:
      gst_wavpack_enc_reset (enc);
      break;
    case GST_STATE_CHANGE_READY_TO_NULL:
      break;
    default:
      break;
  }

  return ret;
}

static void
gst_wavpack_enc_set_property (GObject * object, guint prop_id,
    const GValue * value, GParamSpec * pspec)
{
  GstWavpackEnc *enc = GST_WAVPACK_ENC (object);

  switch (prop_id) {
    case ARG_MODE:
      enc->mode = g_value_get_enum (value);
      break;
    case ARG_BITRATE:{
      gdouble val = g_value_get_double (value);

      if ((val >= 24000.0) && (val <= 9600000.0)) {
        enc->bitrate = val;
      } else {
        enc->bitrate = 0.0;
      }
      break;
    }
    case ARG_BITSPERSAMPLE:{
      gdouble val = g_value_get_double (value);

      if ((val >= 2.0) && (val <= 24.0)) {
        enc->bitrate = val;
      } else {
        enc->bitrate = 0.0;
      }
      break;
    }
    case ARG_CORRECTION_MODE:
      enc->correction_mode = g_value_get_enum (value);
      break;
    case ARG_MD5:
      enc->md5 = g_value_get_boolean (value);
      break;
    case ARG_EXTRA_PROCESSING:
      enc->extra_processing = g_value_get_boolean (value);
      break;
    case ARG_JOINT_STEREO_MODE:
      enc->joint_stereo_mode = g_value_get_enum (value);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
  }
}

static void
gst_wavpack_enc_get_property (GObject * object, guint prop_id, GValue * value,
    GParamSpec * pspec)
{
  GstWavpackEnc *enc = GST_WAVPACK_ENC (object);

  switch (prop_id) {
    case ARG_MODE:
      g_value_set_enum (value, enc->mode);
      break;
    case ARG_BITRATE:
      if (enc->bitrate >= 24000.0) {
        g_value_set_double (value, enc->bitrate);
      } else {
        g_value_set_double (value, 0.0);
      }
      break;
    case ARG_BITSPERSAMPLE:
      if (enc->bitrate <= 24.0) {
        g_value_set_double (value, enc->bitrate);
      } else {
        g_value_set_double (value, 0.0);
      }
      break;
    case ARG_CORRECTION_MODE:
      g_value_set_enum (value, enc->correction_mode);
      break;
    case ARG_MD5:
      g_value_set_boolean (value, enc->md5);
      break;
    case ARG_EXTRA_PROCESSING:
      g_value_set_boolean (value, enc->extra_processing);
      break;
    case ARG_JOINT_STEREO_MODE:
      g_value_set_enum (value, enc->joint_stereo_mode);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
  }
}

gboolean
gst_wavpack_enc_plugin_init (GstPlugin * plugin)
{
  if (!gst_element_register (plugin, "wavpackenc",
          GST_RANK_NONE, GST_TYPE_WAVPACK_ENC))
    return FALSE;

  GST_DEBUG_CATEGORY_INIT (gst_wavpack_enc_debug, "wavpackenc", 0,
      "wavpack encoder");

  return TRUE;
}
