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
#include <string.h>
#include <math.h>

/* First, include the header file for the plugin, to bring in the
 * object definition and other useful things.
 */
#include "gstdvdec.h"

#define NTSC_HEIGHT 480
#define NTSC_BUFFER 120000
#define NTSC_FRAMERATE 30000/1001.

#define PAL_HEIGHT 576
#define PAL_BUFFER 144000
#define PAL_FRAMERATE 25.0

#define PAL_NORMAL_PAR_X	16
#define PAL_NORMAL_PAR_Y	15
#define PAL_WIDE_PAR_X		64
#define PAL_WIDE_PAR_Y		45

#define NTSC_NORMAL_PAR_X	80
#define NTSC_NORMAL_PAR_Y	89
#define NTSC_WIDE_PAR_X		320
#define NTSC_WIDE_PAR_Y		267

/* The ElementDetails structure gives a human-readable description
 * of the plugin, as well as author and version data.
 */
static GstElementDetails dvdec_details =
GST_ELEMENT_DETAILS ("DV (smpte314) decoder plugin",
    "Codec/Decoder/Video",
    "Uses libdv to decode DV video (libdv.sourceforge.net)",
    "Erik Walthinsen <omega@cse.ogi.edu>\n" "Wim Taymans <wim.taymans@tvd.be>");


/* These are the signals that this element can fire.  They are zero-
 * based because the numbers themselves are private to the object.
 * LAST_SIGNAL is used for initialization of the signal array.
 */
enum
{
  /* FILL ME */
  LAST_SIGNAL
};

#define DV_DEFAULT_QUALITY DV_QUALITY_BEST
#define DV_DEFAULT_DECODE_NTH 1

/* Arguments are identified the same way, but cannot be zero, so you
 * must leave the ARG_0 entry in as a placeholder.
 */
enum
{
  ARG_0,
  ARG_CLAMP_LUMA,
  ARG_CLAMP_CHROMA,
  ARG_QUALITY,
  ARG_DECODE_NTH
      /* FILL ME */
};

const gint qualities[] = {
  DV_QUALITY_DC,
  DV_QUALITY_AC_1,
  DV_QUALITY_AC_2,
  DV_QUALITY_DC | DV_QUALITY_COLOR,
  DV_QUALITY_AC_1 | DV_QUALITY_COLOR,
  DV_QUALITY_AC_2 | DV_QUALITY_COLOR
};

/* The PadFactory structures describe what pads the element has or
 * can have.  They can be quite complex, but for this dvdec plugin
 * they are rather simple.
 */
static GstStaticPadTemplate sink_temp = GST_STATIC_PAD_TEMPLATE ("sink",
    GST_PAD_SINK,
    GST_PAD_ALWAYS,
    GST_STATIC_CAPS ("video/x-dv, systemstream = (boolean) true")
    );

static GstStaticPadTemplate video_src_temp = GST_STATIC_PAD_TEMPLATE ("video",
    GST_PAD_SRC,
    GST_PAD_ALWAYS,
    GST_STATIC_CAPS ("video/x-raw-yuv, "
        "format = (fourcc) YUY2, "
        "width = (int) 720, "
        "height = (int) { "
        G_STRINGIFY (NTSC_HEIGHT) ", " G_STRINGIFY (PAL_HEIGHT)
        " }, "
        "pixel-aspect-ratio=(fraction) { "
        G_STRINGIFY (PAL_NORMAL_PAR_X) "/" G_STRINGIFY (PAL_NORMAL_PAR_Y) ","
        G_STRINGIFY (PAL_WIDE_PAR_X) "/" G_STRINGIFY (PAL_WIDE_PAR_Y) ","
        G_STRINGIFY (NTSC_NORMAL_PAR_X) "/" G_STRINGIFY (NTSC_NORMAL_PAR_Y) ","
        G_STRINGIFY (NTSC_WIDE_PAR_X) "/" G_STRINGIFY (NTSC_WIDE_PAR_Y) "},"
        "framerate = (double) [ 1.0, 60.0 ];"
        //"framerate = (double) { "
        //G_STRINGIFY (PAL_FRAMERATE) ", " G_STRINGIFY (NTSC_FRAMERATE)
        //" }; "
        "video/x-raw-rgb, "
        "bpp = (int) 32, "
        "depth = (int) 24, "
        "endianness = (int) " G_STRINGIFY (G_BIG_ENDIAN) ", "
        "red_mask =   (int) 0x0000ff00, "
        "green_mask = (int) 0x00ff0000, "
        "blue_mask =  (int) 0xff000000, "
        "width = (int) 720, "
        "height = (int) { "
        G_STRINGIFY (NTSC_HEIGHT) ", " G_STRINGIFY (PAL_HEIGHT)
        " }, "
        "pixel-aspect-ratio=(fraction) { "
        G_STRINGIFY (PAL_NORMAL_PAR_X) "/" G_STRINGIFY (PAL_NORMAL_PAR_Y) ","
        G_STRINGIFY (PAL_WIDE_PAR_X) "/" G_STRINGIFY (PAL_WIDE_PAR_Y) ","
        G_STRINGIFY (NTSC_NORMAL_PAR_X) "/" G_STRINGIFY (NTSC_NORMAL_PAR_Y) ","
        G_STRINGIFY (NTSC_WIDE_PAR_X) "/" G_STRINGIFY (NTSC_WIDE_PAR_Y) "},"
        "framerate = (double) [ 1.0, 60.0 ];"
        //"framerate = (double) { "
        //G_STRINGIFY (PAL_FRAMERATE) ", " G_STRINGIFY (NTSC_FRAMERATE)
        //" }; "
        "video/x-raw-rgb, "
        "bpp = (int) 24, "
        "depth = (int) 24, "
        "endianness = (int) " G_STRINGIFY (G_BIG_ENDIAN) ", "
        "red_mask =   (int) 0x00ff0000, "
        "green_mask = (int) 0x0000ff00, "
        "blue_mask =  (int) 0x000000ff, "
        "width = (int) 720, "
        "height = (int) { "
        G_STRINGIFY (NTSC_HEIGHT) ", " G_STRINGIFY (PAL_HEIGHT)
        " }, "
        "pixel-aspect-ratio=(fraction) { "
        G_STRINGIFY (PAL_NORMAL_PAR_X) "/" G_STRINGIFY (PAL_NORMAL_PAR_Y) ","
        G_STRINGIFY (PAL_WIDE_PAR_X) "/" G_STRINGIFY (PAL_WIDE_PAR_Y) ","
        G_STRINGIFY (NTSC_NORMAL_PAR_X) "/" G_STRINGIFY (NTSC_NORMAL_PAR_Y) ","
        G_STRINGIFY (NTSC_WIDE_PAR_X) "/" G_STRINGIFY (NTSC_WIDE_PAR_Y) "},"
        "framerate = (double) [ 1.0, 60.0 ]"
        //"framerate = (double) { "
        //G_STRINGIFY (PAL_FRAMERATE) ", " G_STRINGIFY (NTSC_FRAMERATE)
        //" }"
    )
    );

static GstStaticPadTemplate audio_src_temp = GST_STATIC_PAD_TEMPLATE ("audio",
    GST_PAD_SRC,
    GST_PAD_ALWAYS,
    GST_STATIC_CAPS ("audio/x-raw-int, "
        "depth = (int) 16, "
        "width = (int) 16, "
        "signed = (boolean) TRUE, "
        "channels = (int) {2, 4}, "
        "endianness = (int) " G_STRINGIFY (G_LITTLE_ENDIAN) ", "
        "rate = (int) { 32000, 44100, 48000 }")
    );

#define GST_TYPE_DVDEC_QUALITY (gst_dvdec_quality_get_type())
GType
gst_dvdec_quality_get_type (void)
{
  static GType qtype = 0;

  if (qtype == 0) {
    static const GEnumValue values[] = {
      {0, "DV_QUALITY_FASTEST", "Fastest decoding, low-quality mono"},
      {1, "DV_QUALITY_AC_1", "Mono decoding using the first AC coefficient"},
      {2, "DV_QUALITY_AC_2", "Highest quality mono decoding"},
      {3, "DV_QUALITY_DC|DV_QUALITY_COLOUR", "Fastest colour decoding"},
      {4, "DV_QUALITY_AC_1|DV_QUALITY_COLOUR",
          "Colour, using only the first AC coefficient"},
      {5, "DV_QUALITY_BEST", "Highest quality colour decoding"},
      {0, NULL, NULL},
    };

    qtype = g_enum_register_static ("GstDVDecQualityEnum", values);
  }
  return qtype;
}

/* A number of functon prototypes are given so we can refer to them later. */
static void gst_dvdec_base_init (gpointer g_class);
static void gst_dvdec_class_init (GstDVDecClass * klass);
static void gst_dvdec_init (GstDVDec * dvdec);

static const GstQueryType *gst_dvdec_get_src_query_types (GstPad * pad);
static gboolean gst_dvdec_src_query (GstPad * pad, GstQueryType type,
    GstFormat * format, gint64 * value);
static const GstFormat *gst_dvdec_get_formats (GstPad * pad);
static gboolean gst_dvdec_sink_convert (GstPad * pad, GstFormat src_format,
    gint64 src_value, GstFormat * dest_format, gint64 * dest_value);
static gboolean gst_dvdec_src_convert (GstPad * pad, GstFormat src_format,
    gint64 src_value, GstFormat * dest_format, gint64 * dest_value);

static GstPadLinkReturn gst_dvdec_video_link (GstPad * pad,
    const GstCaps * caps);
static GstCaps *gst_dvdec_video_getcaps (GstPad * pad);

static const GstEventMask *gst_dvdec_get_event_masks (GstPad * pad);
static gboolean gst_dvdec_handle_src_event (GstPad * pad, GstEvent * event);

static void gst_dvdec_loop (GstElement * element);

static GstElementStateReturn gst_dvdec_change_state (GstElement * element);

static void gst_dvdec_set_property (GObject * object, guint prop_id,
    const GValue * value, GParamSpec * pspec);
static void gst_dvdec_get_property (GObject * object, guint prop_id,
    GValue * value, GParamSpec * pspec);

/* The parent class pointer needs to be kept around for some object
 * operations.
 */
static GstElementClass *parent_class = NULL;

/* This array holds the ids of the signals registered for this object.
 * The array indexes are based on the enum up above.
 */
/*static guint gst_dvdec_signals[LAST_SIGNAL] = { 0 }; */

/* This function is used to register and subsequently return the type
 * identifier for this object class.  On first invocation, it will
 * register the type, providing the name of the class, struct sizes,
 * and pointers to the various functions that define the class.
 */
GType
gst_dvdec_get_type (void)
{
  static GType dvdec_type = 0;

  if (!dvdec_type) {
    static const GTypeInfo dvdec_info = {
      sizeof (GstDVDecClass),
      gst_dvdec_base_init,
      NULL,
      (GClassInitFunc) gst_dvdec_class_init,
      NULL,
      NULL,
      sizeof (GstDVDec),
      0,
      (GInstanceInitFunc) gst_dvdec_init,
    };

    dvdec_type =
        g_type_register_static (GST_TYPE_ELEMENT, "GstDVDec", &dvdec_info, 0);
  }
  return dvdec_type;
}

static void
gst_dvdec_base_init (gpointer g_class)
{
  GstElementClass *element_class = GST_ELEMENT_CLASS (g_class);

  /* The pad templates can be easily generated from the factories above,
   * and then added to the list of padtemplates for the elementfactory.
   * Note that the generated padtemplates are stored in static global
   * variables, for the gst_dvdec_init function to use later on.
   */
  gst_element_class_add_pad_template (element_class,
      gst_static_pad_template_get (&sink_temp));
  gst_element_class_add_pad_template (element_class,
      gst_static_pad_template_get (&video_src_temp));
  gst_element_class_add_pad_template (element_class,
      gst_static_pad_template_get (&audio_src_temp));

  gst_element_class_set_details (element_class, &dvdec_details);
}

/* In order to create an instance of an object, the class must be
 * initialized by this function.  GObject will take care of running
 * it, based on the pointer to the function provided above.
 */
static void
gst_dvdec_class_init (GstDVDecClass * klass)
{
  /* Class pointers are needed to supply pointers to the private
   * implementations of parent class methods.
   */
  GObjectClass *gobject_class;
  GstElementClass *gstelement_class;

  /* Since the dvdec class contains the parent classes, you can simply
   * cast the pointer to get access to the parent classes.
   */
  gobject_class = (GObjectClass *) klass;
  gstelement_class = (GstElementClass *) klass;

  /* The parent class is needed for class method overrides. */
  parent_class = g_type_class_ref (GST_TYPE_ELEMENT);

  g_object_class_install_property (G_OBJECT_CLASS (klass), ARG_CLAMP_LUMA,
      g_param_spec_boolean ("clamp_luma", "Clamp luma", "Clamp luma",
          FALSE, G_PARAM_READWRITE));
  g_object_class_install_property (G_OBJECT_CLASS (klass), ARG_CLAMP_CHROMA,
      g_param_spec_boolean ("clamp_chroma", "Clamp chroma", "Clamp chroma",
          FALSE, G_PARAM_READWRITE));
  g_object_class_install_property (G_OBJECT_CLASS (klass), ARG_QUALITY,
      g_param_spec_enum ("quality", "Quality", "Decoding quality",
          GST_TYPE_DVDEC_QUALITY, DV_DEFAULT_QUALITY, G_PARAM_READWRITE));
  g_object_class_install_property (G_OBJECT_CLASS (klass), ARG_DECODE_NTH,
      g_param_spec_int ("drop-factor", "Drop Factor", "Only decode Nth frame",
          1, G_MAXINT, DV_DEFAULT_DECODE_NTH, G_PARAM_READWRITE));

  gobject_class->set_property = gst_dvdec_set_property;
  gobject_class->get_property = gst_dvdec_get_property;

  gstelement_class->change_state = gst_dvdec_change_state;

  /* table initialization, only do once */
  dv_init (0, 0);
}

/* This function is responsible for initializing a specific instance of
 * the plugin.
 */
static void
gst_dvdec_init (GstDVDec * dvdec)
{
  gint i;

  dvdec->found_header = FALSE;

  dvdec->sinkpad =
      gst_pad_new_from_template (gst_static_pad_template_get (&sink_temp),
      "sink");
  gst_pad_set_query_function (dvdec->sinkpad, NULL);
  gst_pad_set_convert_function (dvdec->sinkpad,
      GST_DEBUG_FUNCPTR (gst_dvdec_sink_convert));
  gst_pad_set_formats_function (dvdec->sinkpad,
      GST_DEBUG_FUNCPTR (gst_dvdec_get_formats));
  gst_element_add_pad (GST_ELEMENT (dvdec), dvdec->sinkpad);

  dvdec->videosrcpad =
      gst_pad_new_from_template (gst_static_pad_template_get (&video_src_temp),
      "video");
  gst_pad_set_query_function (dvdec->videosrcpad,
      GST_DEBUG_FUNCPTR (gst_dvdec_src_query));
  gst_pad_set_query_type_function (dvdec->videosrcpad,
      GST_DEBUG_FUNCPTR (gst_dvdec_get_src_query_types));
  gst_pad_set_event_function (dvdec->videosrcpad,
      GST_DEBUG_FUNCPTR (gst_dvdec_handle_src_event));
  gst_pad_set_event_mask_function (dvdec->videosrcpad,
      GST_DEBUG_FUNCPTR (gst_dvdec_get_event_masks));
  gst_pad_set_convert_function (dvdec->videosrcpad,
      GST_DEBUG_FUNCPTR (gst_dvdec_src_convert));
  gst_pad_set_formats_function (dvdec->videosrcpad,
      GST_DEBUG_FUNCPTR (gst_dvdec_get_formats));
  gst_pad_set_getcaps_function (dvdec->videosrcpad,
      GST_DEBUG_FUNCPTR (gst_dvdec_video_getcaps));
  gst_pad_set_link_function (dvdec->videosrcpad,
      GST_DEBUG_FUNCPTR (gst_dvdec_video_link));
  gst_element_add_pad (GST_ELEMENT (dvdec), dvdec->videosrcpad);

  dvdec->audiosrcpad =
      gst_pad_new_from_template (gst_static_pad_template_get (&audio_src_temp),
      "audio");
  gst_pad_set_query_function (dvdec->audiosrcpad,
      GST_DEBUG_FUNCPTR (gst_dvdec_src_query));
  gst_pad_set_query_type_function (dvdec->audiosrcpad,
      GST_DEBUG_FUNCPTR (gst_dvdec_get_src_query_types));
  gst_pad_set_event_function (dvdec->audiosrcpad,
      GST_DEBUG_FUNCPTR (gst_dvdec_handle_src_event));
  gst_pad_set_event_mask_function (dvdec->audiosrcpad,
      GST_DEBUG_FUNCPTR (gst_dvdec_get_event_masks));
  gst_pad_set_convert_function (dvdec->audiosrcpad,
      GST_DEBUG_FUNCPTR (gst_dvdec_src_convert));
  gst_pad_set_formats_function (dvdec->audiosrcpad,
      GST_DEBUG_FUNCPTR (gst_dvdec_get_formats));
  gst_pad_use_explicit_caps (dvdec->audiosrcpad);
  gst_element_add_pad (GST_ELEMENT (dvdec), dvdec->audiosrcpad);

  gst_element_set_loop_function (GST_ELEMENT (dvdec), gst_dvdec_loop);

  dvdec->bs = NULL;
  dvdec->length = 0;
  dvdec->next_ts = 0LL;
  dvdec->end_position = -1LL;
  dvdec->need_discont = FALSE;
  dvdec->new_media = FALSE;
  dvdec->framerate = 0;
  dvdec->height = 0;
  dvdec->frequency = 0;
  dvdec->channels = 0;
  dvdec->wide = FALSE;
  dvdec->drop_factor = 1;

  dvdec->clamp_luma = FALSE;
  dvdec->clamp_chroma = FALSE;
  dvdec->quality = DV_DEFAULT_QUALITY;
  dvdec->loop = FALSE;

  for (i = 0; i < 4; i++) {
    dvdec->audio_buffers[i] =
        (gint16 *) g_malloc (DV_AUDIO_MAX_SAMPLES * sizeof (gint16));
  }
}

static const GstFormat *
gst_dvdec_get_formats (GstPad * pad)
{
  static const GstFormat src_formats[] = {
    GST_FORMAT_BYTES,
    GST_FORMAT_DEFAULT,
    GST_FORMAT_TIME,
    0
  };
  static const GstFormat sink_formats[] = {
    GST_FORMAT_BYTES,
    GST_FORMAT_TIME,
    GST_FORMAT_DEFAULT,
    0
  };

  return (GST_PAD_IS_SRC (pad) ? src_formats : sink_formats);
}

static gboolean
gst_dvdec_src_convert (GstPad * pad, GstFormat src_format, gint64 src_value,
    GstFormat * dest_format, gint64 * dest_value)
{
  gboolean res = TRUE;
  GstDVDec *dvdec;

  dvdec = GST_DVDEC (gst_pad_get_parent (pad));
  if (dvdec->length == 0)
    return FALSE;

  if (dvdec->decoder == NULL)
    return FALSE;

  switch (src_format) {
    case GST_FORMAT_BYTES:
      switch (*dest_format) {
        case GST_FORMAT_BYTES:
          *dest_value = src_value;
          break;
        case GST_FORMAT_DEFAULT:
        case GST_FORMAT_TIME:
          *dest_format = GST_FORMAT_TIME;
          if (pad == dvdec->videosrcpad)
            *dest_value = src_value * GST_SECOND /
                (720 * dvdec->height * dvdec->bpp * dvdec->framerate /
                GST_SECOND);
          else if (pad == dvdec->audiosrcpad)
            *dest_value = src_value * GST_SECOND /
                (2 * dvdec->frequency * dvdec->channels);
          break;
        default:
          res = FALSE;
      }
      break;
    case GST_FORMAT_TIME:
      switch (*dest_format) {
        case GST_FORMAT_BYTES:
          if (pad == dvdec->videosrcpad)
            *dest_value = src_value * 720 * dvdec->height * dvdec->bpp *
                dvdec->framerate / GST_SECOND;
          else if (pad == dvdec->audiosrcpad)
            *dest_value = 2 * src_value * dvdec->frequency *
                dvdec->channels / GST_SECOND;
          break;
        case GST_FORMAT_TIME:
        case GST_FORMAT_DEFAULT:
          *dest_format = GST_FORMAT_TIME;
          *dest_value = src_value;
          break;
        default:
          res = FALSE;
      }
      break;
    default:
      res = FALSE;
  }
  return res;
}

static gboolean
gst_dvdec_sink_convert (GstPad * pad, GstFormat src_format, gint64 src_value,
    GstFormat * dest_format, gint64 * dest_value)
{
  gboolean res = TRUE;
  GstDVDec *dvdec;

  dvdec = GST_DVDEC (gst_pad_get_parent (pad));

  if (dvdec->length == 0)
    return FALSE;

  if (*dest_format == GST_FORMAT_DEFAULT)
    *dest_format = GST_FORMAT_TIME;

  switch (src_format) {
    case GST_FORMAT_BYTES:
      switch (*dest_format) {
        case GST_FORMAT_TIME:
        {
          guint64 frame;

          /* get frame number */
          frame = src_value / dvdec->length;

          *dest_value = (frame * GST_SECOND) / dvdec->framerate;
          break;
        }
        case GST_FORMAT_BYTES:
          *dest_value = src_value;
          break;
        default:
          res = FALSE;
      }
      break;
    case GST_FORMAT_TIME:
      switch (*dest_format) {
        case GST_FORMAT_BYTES:
        {
          guint64 frame;

          /* calculate the frame */
          frame = src_value * dvdec->framerate / GST_SECOND;
          /* calculate the offset */
          *dest_value = frame * dvdec->length;
          break;
        }
        case GST_FORMAT_TIME:
          *dest_value = src_value;
          break;
        default:
          res = FALSE;
      }
      break;
    default:
      res = FALSE;
  }
  return res;
}

static const GstQueryType *
gst_dvdec_get_src_query_types (GstPad * pad)
{
  static const GstQueryType src_query_types[] = {
    GST_QUERY_TOTAL,
    GST_QUERY_POSITION,
    0
  };

  return src_query_types;
}

static gboolean
gst_dvdec_src_query (GstPad * pad, GstQueryType type,
    GstFormat * format, gint64 * value)
{
  gboolean res = TRUE;
  GstDVDec *dvdec;

  dvdec = GST_DVDEC (gst_pad_get_parent (pad));

  switch (type) {
    case GST_QUERY_TOTAL:
      switch (*format) {
        default:
        {
          guint64 len;
          GstFormat tmp_format;

          if (!dvdec->bs)
            return FALSE;

          len = gst_bytestream_length (dvdec->bs);
          tmp_format = GST_FORMAT_TIME;
          if (len == -1 || !gst_pad_convert (dvdec->sinkpad,
                  GST_FORMAT_BYTES, len, &tmp_format, value)) {
            return FALSE;
          }
          if (!gst_pad_convert (pad, GST_FORMAT_TIME, *value, format, value)) {
            return FALSE;
          }
          break;
        }
      }
      break;
    case GST_QUERY_POSITION:
      switch (*format) {
        default:
          res =
              gst_pad_convert (pad, GST_FORMAT_TIME, dvdec->next_ts, format,
              value);
          break;
      }
      break;
    default:
      res = FALSE;
      break;
  }
  return res;
}

static const GstEventMask *
gst_dvdec_get_event_masks (GstPad * pad)
{
  static const GstEventMask src_event_masks[] = {
    {GST_EVENT_SEEK, GST_SEEK_METHOD_SET | GST_SEEK_FLAG_FLUSH},
    {0,}
  };
  static const GstEventMask sink_event_masks[] = {
    {GST_EVENT_EOS, 0},
    {GST_EVENT_DISCONTINUOUS, 0},
    {GST_EVENT_FLUSH, 0},
    {0,}
  };

  return (GST_PAD_IS_SRC (pad) ? src_event_masks : sink_event_masks);
}

static gboolean
gst_dvdec_handle_sink_event (GstDVDec * dvdec)
{
  guint32 remaining;
  GstEvent *event;
  GstEventType type;

  gst_bytestream_get_status (dvdec->bs, &remaining, &event);

  type = event ? GST_EVENT_TYPE (event) : GST_EVENT_UNKNOWN;

  switch (type) {
    case GST_EVENT_FLUSH:
    case GST_EVENT_EOS:
    case GST_EVENT_FILLER:
    {
      /* Forward the event to output sinks */
      if (GST_PAD_IS_LINKED (dvdec->videosrcpad)) {
        gst_event_ref (event);
        gst_pad_push (dvdec->videosrcpad, GST_DATA (event));
      }
      if (GST_PAD_IS_LINKED (dvdec->audiosrcpad)) {
        gst_event_ref (event);
        gst_pad_push (dvdec->audiosrcpad, GST_DATA (event));
      }
      if (type == GST_EVENT_EOS) {
        gst_element_set_eos (GST_ELEMENT (dvdec));
      }
      break;
    }
    case GST_EVENT_DISCONTINUOUS:
    {
      gint i;
      gboolean found = FALSE;
      GstFormat format;

      format = GST_FORMAT_TIME;
      /* try to get a timestamp from the discont formats */
      for (i = 0; i < GST_EVENT_DISCONT_OFFSET_LEN (event); i++) {
        if (gst_pad_convert (dvdec->sinkpad,
                GST_EVENT_DISCONT_OFFSET (event, i).format,
                GST_EVENT_DISCONT_OFFSET (event, i).value,
                &format, &dvdec->next_ts)) {
          found = TRUE;
          break;
        }
      }
      /* assume 0 then */
      if (!found) {
        dvdec->next_ts = 0LL;
      }
      dvdec->need_discont = TRUE;
      break;
    }
    default:
      return gst_pad_event_default (dvdec->sinkpad, event);
      break;
  }
  gst_event_unref (event);
  return TRUE;
}

static gboolean
gst_dvdec_handle_src_event (GstPad * pad, GstEvent * event)
{
  gboolean res = TRUE;
  GstDVDec *dvdec;

  dvdec = GST_DVDEC (gst_pad_get_parent (pad));

  switch (GST_EVENT_TYPE (event)) {
    case GST_EVENT_SEEK_SEGMENT:
    {
      gint64 position;
      GstFormat format;

      /* first bring the format to time */
      format = GST_FORMAT_TIME;
      if (!gst_pad_convert (pad,
              GST_EVENT_SEEK_FORMAT (event),
              GST_EVENT_SEEK_ENDOFFSET (event), &format, &position)) {
        /* could not convert seek format to time offset */
        res = FALSE;
        break;
      }

      dvdec->end_position = position;
      dvdec->loop = GST_EVENT_SEEK_TYPE (event) & GST_SEEK_FLAG_SEGMENT_LOOP;
    }
    case GST_EVENT_SEEK:
    {
      gint64 position;
      GstFormat format;

      /* first bring the format to time */
      format = GST_FORMAT_TIME;
      if (!gst_pad_convert (pad,
              GST_EVENT_SEEK_FORMAT (event),
              GST_EVENT_SEEK_OFFSET (event), &format, &position)) {
        /* could not convert seek format to time offset */
        res = FALSE;
        break;
      }
      dvdec->next_ts = position;
      /* then try to figure out the byteoffset for this time */
      format = GST_FORMAT_BYTES;
      if (!gst_pad_convert (dvdec->sinkpad, GST_FORMAT_TIME, position,
              &format, &position)) {
        /* could not convert seek format to byte offset */
        res = FALSE;
        break;
      }
      /* seek to offset */
      if (!gst_bytestream_seek (dvdec->bs, position, GST_SEEK_METHOD_SET)) {
        res = FALSE;
      }
      if (GST_EVENT_TYPE (event) != GST_EVENT_SEEK_SEGMENT)
        dvdec->end_position = -1;
      break;
    }
    default:
      res = FALSE;
      break;
  }
  gst_event_unref (event);
  return res;
}

static GstCaps *
gst_dvdec_video_getcaps (GstPad * pad)
{
  GstDVDec *dvdec;
  GstCaps *caps;
  GstPadTemplate *src_pad_template;

  dvdec = GST_DVDEC (gst_pad_get_parent (pad));
  src_pad_template = gst_static_pad_template_get (&video_src_temp);
  caps = gst_caps_copy (gst_pad_template_get_caps (src_pad_template));

  if (dvdec->found_header) {
    int i;
    gint par_x, par_y;

    if (dvdec->PAL) {
      if (dvdec->wide) {
        par_x = PAL_WIDE_PAR_X;
        par_y = PAL_WIDE_PAR_Y;
      } else {
        par_x = PAL_NORMAL_PAR_X;
        par_y = PAL_NORMAL_PAR_Y;
      }
    } else {
      if (dvdec->wide) {
        par_x = NTSC_WIDE_PAR_X;
        par_y = NTSC_WIDE_PAR_Y;
      } else {
        par_x = NTSC_NORMAL_PAR_X;
        par_y = NTSC_NORMAL_PAR_Y;
      }
    }
    /* set the height */
    for (i = 0; i < gst_caps_get_size (caps); i++) {
      GstStructure *structure = gst_caps_get_structure (caps, i);

      gst_structure_set (structure,
          "height", G_TYPE_INT, dvdec->height,
          "framerate", G_TYPE_DOUBLE, dvdec->framerate / dvdec->drop_factor,
          "pixel-aspect-ratio", GST_TYPE_FRACTION, par_x, par_y, NULL);
    }
  }

  return caps;
}

static GstPadLinkReturn
gst_dvdec_video_link (GstPad * pad, const GstCaps * caps)
{
  GstDVDec *dvdec;
  GstStructure *structure;
  guint32 fourcc;
  gint height;
  gdouble framerate;

  dvdec = GST_DVDEC (gst_pad_get_parent (pad));

  /* if we did not find a header yet, return delayed */
  if (!dvdec->found_header) {
    return GST_PAD_LINK_DELAYED;
  }

  structure = gst_caps_get_structure (caps, 0);

  if (!gst_structure_get_int (structure, "height", &height) ||
      !gst_structure_get_double (structure, "framerate", &framerate))
    return GST_PAD_LINK_REFUSED;

  /* allow a margin of error for the framerate caused by float rounding errors */
  if ((height != dvdec->height) ||
      (fabs (framerate - (dvdec->framerate / dvdec->drop_factor)) > 0.00000001))
    return GST_PAD_LINK_REFUSED;

  if (strcmp (gst_structure_get_name (structure), "video/x-raw-rgb") == 0) {
    gint bpp;

    gst_structure_get_int (structure, "bpp", &bpp);
    if (bpp == 24) {
      dvdec->space = e_dv_color_rgb;
      dvdec->bpp = 3;
    } else {
      dvdec->space = e_dv_color_bgr0;
      dvdec->bpp = 4;
    }
  } else {
    if (!gst_structure_get_fourcc (structure, "format", &fourcc))
      return GST_PAD_LINK_REFUSED;

    dvdec->space = e_dv_color_yuv;
    dvdec->bpp = 2;
  }

  return GST_PAD_LINK_OK;
}

static void
gst_dvdec_push (GstDVDec * dvdec, GstBuffer * outbuf, GstPad * pad,
    GstClockTime ts)
{
  if ((dvdec->need_discont) || (dvdec->new_media)) {
    GstEvent *discont;

    discont = gst_event_new_discontinuous (FALSE, GST_FORMAT_TIME, ts, NULL);
    GST_EVENT_DISCONT_NEW_MEDIA (discont) = dvdec->new_media;

    gst_pad_push (pad, GST_DATA (discont));
  }

  gst_pad_push (pad, GST_DATA (outbuf));

  if ((dvdec->end_position != -1) && (dvdec->next_ts >= dvdec->end_position)) {
    if (dvdec->loop)
      gst_pad_push (pad, GST_DATA (gst_event_new (GST_EVENT_SEGMENT_DONE)));
    else
      gst_pad_push (pad, GST_DATA (gst_event_new (GST_EVENT_EOS)));
  }
}

static void
gst_dvdec_loop (GstElement * element)
{
  GstDVDec *dvdec;
  GstBuffer *buf, *outbuf;
  guint8 *inframe;
  gint height;
  guint32 length, got_bytes;
  GstClockTime ts, duration;
  gdouble fps;
  gboolean wide;

  dvdec = GST_DVDEC (element);

  /*
   * Apparently dv_parse_header can read from the body of the frame
   * too, so it needs more than header_size bytes. Wacky!
   */
  if (dvdec->found_header)
    length = (dvdec->PAL ? PAL_BUFFER : NTSC_BUFFER);
  else
    length = NTSC_BUFFER;

  /* first read enough bytes to parse the header */
  got_bytes = gst_bytestream_peek_bytes (dvdec->bs, &inframe, length);
  if (got_bytes < length) {
    gst_dvdec_handle_sink_event (dvdec);
    return;
  }
  if (dv_parse_header (dvdec->decoder, inframe) < 0) {
    GST_ELEMENT_ERROR (dvdec, STREAM, DECODE, (NULL), (NULL));
    return;
  }

  /* after parsing the header we know the length of the data */
  dvdec->PAL = dv_system_50_fields (dvdec->decoder);
  dvdec->found_header = TRUE;

  fps = (dvdec->PAL ? PAL_FRAMERATE : NTSC_FRAMERATE);
  height = (dvdec->PAL ? PAL_HEIGHT : NTSC_HEIGHT);
  length = (dvdec->PAL ? PAL_BUFFER : NTSC_BUFFER);
  wide = dv_format_wide (dvdec->decoder);

  if (length != dvdec->length) {
    dvdec->length = length;
    gst_bytestream_size_hint (dvdec->bs, length);
  }

  /* then read the read data */
  got_bytes = gst_bytestream_read (dvdec->bs, &buf, length);
  if (got_bytes < length) {
    gst_dvdec_handle_sink_event (dvdec);
    return;
  }

  ts = dvdec->next_ts;
  dvdec->next_ts += GST_SECOND / fps;
  duration = dvdec->next_ts - ts;

  dv_parse_packs (dvdec->decoder, GST_BUFFER_DATA (buf));
  if (dv_is_new_recording (dvdec->decoder, GST_BUFFER_DATA (buf)))
    dvdec->new_media = TRUE;
  if (GST_PAD_IS_LINKED (dvdec->audiosrcpad)) {
    gint num_samples;

    dv_decode_full_audio (dvdec->decoder, GST_BUFFER_DATA (buf),
        dvdec->audio_buffers);

    if ((dv_get_frequency (dvdec->decoder) != dvdec->frequency) ||
        (dv_get_num_channels (dvdec->decoder) != dvdec->channels)) {
      if (!gst_pad_set_explicit_caps (dvdec->audiosrcpad,
              gst_caps_new_simple ("audio/x-raw-int",
                  "rate", G_TYPE_INT, dv_get_frequency (dvdec->decoder),
                  "depth", G_TYPE_INT, 16,
                  "width", G_TYPE_INT, 16,
                  "signed", G_TYPE_BOOLEAN, TRUE,
                  "channels", G_TYPE_INT, dv_get_num_channels (dvdec->decoder),
                  "endianness", G_TYPE_INT, G_LITTLE_ENDIAN, NULL))) {
        gst_buffer_unref (buf);
        GST_ELEMENT_ERROR (dvdec, CORE, NEGOTIATION, (NULL),
            ("Failed to negotiate audio parameters for the DV audio stream"));
        return;
      }

      dvdec->frequency = dv_get_frequency (dvdec->decoder);
      dvdec->channels = dv_get_num_channels (dvdec->decoder);
    }

    num_samples = dv_get_num_samples (dvdec->decoder);

    if (num_samples) {
      gint16 *a_ptr;
      gint i, j;

      outbuf = gst_buffer_new ();
      GST_BUFFER_SIZE (outbuf) = dv_get_num_samples (dvdec->decoder) *
          sizeof (gint16) * dv_get_num_channels (dvdec->decoder);
      GST_BUFFER_DATA (outbuf) = g_malloc (GST_BUFFER_SIZE (outbuf));

      a_ptr = (gint16 *) GST_BUFFER_DATA (outbuf);

      for (i = 0; i < num_samples; i++) {
        for (j = 0; j < dv_get_num_channels (dvdec->decoder); j++) {
          *(a_ptr++) = dvdec->audio_buffers[j][i];
        }
      }

      GST_BUFFER_TIMESTAMP (outbuf) = ts;
      GST_BUFFER_DURATION (outbuf) = duration;
      GST_BUFFER_OFFSET (outbuf) = dvdec->audio_offset;
      dvdec->audio_offset += num_samples;
      GST_BUFFER_OFFSET_END (outbuf) = dvdec->audio_offset;

      gst_dvdec_push (dvdec, outbuf, dvdec->audiosrcpad, ts);
    }
  } else {
    dvdec->frequency = dv_get_frequency (dvdec->decoder);
    dvdec->channels = dv_get_num_channels (dvdec->decoder);
  }

  if (GST_PAD_IS_LINKED (dvdec->videosrcpad)) {
    guint8 *outframe;
    guint8 *outframe_ptrs[3];
    gint outframe_pitches[3];

    dvdec->framecount++;
    if (dvdec->framecount < dvdec->drop_factor) {
      /* don't decode */
      goto end;
    }
    dvdec->framecount = 0;

    if ((dvdec->framerate != fps) || (dvdec->height != height)
        || dvdec->wide != wide) {
      dvdec->height = height;
      dvdec->framerate = fps;
      dvdec->wide = wide;

      if (GST_PAD_LINK_FAILED (gst_pad_renegotiate (dvdec->videosrcpad))) {
        GST_ELEMENT_ERROR (dvdec, CORE, NEGOTIATION, (NULL), (NULL));
        return;
      }
    }

    outbuf = gst_buffer_new_and_alloc ((720 * height) * dvdec->bpp);

    outframe = GST_BUFFER_DATA (outbuf);

    outframe_ptrs[0] = outframe;
    outframe_pitches[0] = 720 * dvdec->bpp;

    /* the rest only matters for YUY2 */
    if (dvdec->bpp < 3) {
      outframe_ptrs[1] = outframe_ptrs[0] + 720 * height;
      outframe_ptrs[2] = outframe_ptrs[1] + 360 * height;

      outframe_pitches[1] = height / 2;
      outframe_pitches[2] = outframe_pitches[1];
    }

    dv_decode_full_frame (dvdec->decoder, GST_BUFFER_DATA (buf),
        dvdec->space, outframe_ptrs, outframe_pitches);

    GST_BUFFER_TIMESTAMP (outbuf) = ts;
    GST_BUFFER_DURATION (outbuf) = duration * dvdec->drop_factor;

    gst_dvdec_push (dvdec, outbuf, dvdec->videosrcpad, ts);
  } else {
    dvdec->height = height;
    dvdec->framerate = fps;
    dvdec->wide = wide;
  }

end:
  if ((dvdec->end_position != -1) &&
      (dvdec->next_ts >= dvdec->end_position) && !dvdec->loop) {
    gst_element_set_eos (GST_ELEMENT (dvdec));
  }

  dvdec->need_discont = FALSE;
  dvdec->new_media = FALSE;

  gst_buffer_unref (buf);
}

static GstElementStateReturn
gst_dvdec_change_state (GstElement * element)
{
  GstDVDec *dvdec = GST_DVDEC (element);

  switch (GST_STATE_TRANSITION (element)) {
    case GST_STATE_NULL_TO_READY:
      break;
    case GST_STATE_READY_TO_PAUSED:
      dvdec->bs = gst_bytestream_new (dvdec->sinkpad);
      dvdec->decoder =
          dv_decoder_new (0, dvdec->clamp_luma, dvdec->clamp_chroma);
      dvdec->decoder->quality = qualities[dvdec->quality];
      dvdec->audio_offset = 0;
      dvdec->framecount = 0;
      /* 
       * Enable this function call when libdv2 0.100 or higher is more
       * common
       */
      /* dv_set_quality (dvdec->decoder, qualities [dvdec->quality]); */
      break;
    case GST_STATE_PAUSED_TO_PLAYING:
      break;
    case GST_STATE_PLAYING_TO_PAUSED:
      break;
    case GST_STATE_PAUSED_TO_READY:
      dv_decoder_free (dvdec->decoder);
      dvdec->decoder = NULL;
      dvdec->found_header = FALSE;
      gst_bytestream_destroy (dvdec->bs);
      dvdec->bs = NULL;
      break;
    case GST_STATE_READY_TO_NULL:
      break;
    default:
      break;
  }

  return parent_class->change_state (element);
}

/* Arguments are part of the Gtk+ object system, and these functions
 * enable the element to respond to various arguments.
 */
static void
gst_dvdec_set_property (GObject * object, guint prop_id, const GValue * value,
    GParamSpec * pspec)
{
  GstDVDec *dvdec;

  /* It's not null if we got it, but it might not be ours */
  g_return_if_fail (GST_IS_DVDEC (object));

  /* Get a pointer of the right type. */
  dvdec = GST_DVDEC (object);

  /* Check the argument id to see which argument we're setting. */
  switch (prop_id) {
    case ARG_CLAMP_LUMA:
      dvdec->clamp_luma = g_value_get_boolean (value);
      break;
    case ARG_CLAMP_CHROMA:
      dvdec->clamp_chroma = g_value_get_boolean (value);
      break;
    case ARG_QUALITY:
      dvdec->quality = g_value_get_enum (value);
      if ((dvdec->quality < 0) || (dvdec->quality > 5))
        dvdec->quality = 0;
      break;
    case ARG_DECODE_NTH:
      dvdec->drop_factor = g_value_get_int (value);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
  }
}

/* The set function is simply the inverse of the get fuction. */
static void
gst_dvdec_get_property (GObject * object, guint prop_id, GValue * value,
    GParamSpec * pspec)
{
  GstDVDec *dvdec;

  /* It's not null if we got it, but it might not be ours */
  g_return_if_fail (GST_IS_DVDEC (object));
  dvdec = GST_DVDEC (object);

  switch (prop_id) {
    case ARG_CLAMP_LUMA:
      g_value_set_boolean (value, dvdec->clamp_luma);
      break;
    case ARG_CLAMP_CHROMA:
      g_value_set_boolean (value, dvdec->clamp_chroma);
      break;
    case ARG_QUALITY:
      g_value_set_enum (value, dvdec->quality);
      break;
    case ARG_DECODE_NTH:
      g_value_set_int (value, dvdec->drop_factor);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
  }
}

/* This is the entry into the plugin itself.  When the plugin loads,
 * this function is called to register everything that the plugin provides.
 */
static gboolean
plugin_init (GstPlugin * plugin)
{
  if (!gst_library_load ("gstbytestream"))
    return FALSE;

  if (!gst_element_register (plugin, "dvdec", GST_RANK_PRIMARY,
          gst_dvdec_get_type ()))
    return FALSE;

  return TRUE;
}

GST_PLUGIN_DEFINE (GST_VERSION_MAJOR,
    GST_VERSION_MINOR,
    "dvdec",
    "Uses libdv to decode DV video (libdv.sourceforge.net)",
    plugin_init, VERSION, "LGPL", GST_PACKAGE, GST_ORIGIN);
