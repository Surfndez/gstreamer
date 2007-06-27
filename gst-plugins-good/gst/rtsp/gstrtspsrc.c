/* GStreamer
 * Copyright (C) <2005,2006> Wim Taymans <wim at fluendo dot com>
 *               <2006> Lutz Mueller <lutz at topfrose dot de>
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
/*
 * Unless otherwise indicated, Source Code is licensed under MIT license.
 * See further explanation attached in License Statement (distributed in the file
 * LICENSE).
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy of
 * this software and associated documentation files (the "Software"), to deal in
 * the Software without restriction, including without limitation the rights to
 * use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies
 * of the Software, and to permit persons to whom the Software is furnished to do
 * so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in all
 * copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */
/**
 * SECTION:element-rtspsrc
 *
 * <refsect2>
 * <para>
 * Makes a connection to an RTSP server and read the data.
 * rtspsrc strictly follows RFC 2326 and therefore does not (yet) support
 * RealMedia/Quicktime/Microsoft extensions.
 * </para>
 * <para>
 * RTSP supports transport over TCP or UDP in unicast or multicast mode. By
 * default rtspsrc will negotiate a connection in the following order:
 * UDP unicast/UDP multicast/TCP. The order cannot be changed but the allowed
 * protocols can be controlled with the "protocols" property.
 * </para>
 * <para>
 * rtspsrc currently understands SDP as the format of the session description.
 * For each stream listed in the SDP a new rtp_stream%d pad will be created
 * with caps derived from the SDP media description. This is a caps of mime type
 * "application/x-rtp" that can be connected to any available RTP depayloader
 * element. 
 * </para>
 * <para>
 * rtspsrc will internally instantiate an RTP session manager element
 * that will handle the RTCP messages to and from the server, jitter removal,
 * packet reordering along with providing a clock for the pipeline. 
 * This feature is however currently not yet implemented.
 * </para>
 * <para>
 * rtspsrc acts like a live source and will therefore only generate data in the 
 * PLAYING state.
 * </para>
 * <title>Example launch line</title>
 * <para>
 * <programlisting>
 * gst-launch rtspsrc location=rtsp://some.server/url ! fakesink
 * </programlisting>
 * Establish a connection to an RTSP server and send the raw RTP packets to a fakesink.
 * </para>
 * </refsect2>
 *
 * Last reviewed on 2006-08-18 (0.10.5)
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <unistd.h>
#include <stdlib.h>
#include <string.h>

#include "gstrtspsrc.h"
#include "sdp.h"
#include "rtsprange.h"

/* define for experimental real support */
#undef WITH_EXT_REAL

#include "rtspextwms.h"
#ifdef WITH_EXT_REAL
#include "rtspextreal.h"
#endif

GST_DEBUG_CATEGORY_STATIC (rtspsrc_debug);
#define GST_CAT_DEFAULT (rtspsrc_debug)

/* elementfactory information */
static const GstElementDetails gst_rtspsrc_details =
GST_ELEMENT_DETAILS ("RTSP packet receiver",
    "Source/Network",
    "Receive data over the network via RTSP (RFC 2326)",
    "Wim Taymans <wim@fluendo.com>\n"
    "Thijs Vermeir <thijs.vermeir@barco.com>\n"
    "Lutz Mueller <lutz@topfrose.de>");

static GstStaticPadTemplate rtptemplate = GST_STATIC_PAD_TEMPLATE ("stream%d",
    GST_PAD_SRC,
    GST_PAD_SOMETIMES,
    GST_STATIC_CAPS ("application/x-rtp; application/x-rdt"));

/* templates used internally */
static GstStaticPadTemplate anysrctemplate =
GST_STATIC_PAD_TEMPLATE ("internalsrc%d",
    GST_PAD_SRC,
    GST_PAD_SOMETIMES,
    GST_STATIC_CAPS_ANY);

static GstStaticPadTemplate anysinktemplate =
GST_STATIC_PAD_TEMPLATE ("internalsink%d",
    GST_PAD_SINK,
    GST_PAD_SOMETIMES,
    GST_STATIC_CAPS_ANY);

enum
{
  /* FILL ME */
  LAST_SIGNAL
};

#define DEFAULT_LOCATION        NULL
#define DEFAULT_PROTOCOLS       RTSP_LOWER_TRANS_UDP | RTSP_LOWER_TRANS_UDP_MCAST | RTSP_LOWER_TRANS_TCP
#define DEFAULT_DEBUG           FALSE
#define DEFAULT_RETRY           20
#define DEFAULT_TIMEOUT         5000000
#define DEFAULT_TCP_TIMEOUT     20000000
#define DEFAULT_LATENCY_MS      3000

enum
{
  PROP_0,
  PROP_LOCATION,
  PROP_PROTOCOLS,
  PROP_DEBUG,
  PROP_RETRY,
  PROP_TIMEOUT,
  PROP_TCP_TIMEOUT,
  PROP_LATENCY,
};

#define GST_TYPE_RTSP_LOWER_TRANS (gst_rtsp_lower_trans_get_type())
static GType
gst_rtsp_lower_trans_get_type (void)
{
  static GType rtsp_lower_trans_type = 0;
  static const GFlagsValue rtsp_lower_trans[] = {
    {RTSP_LOWER_TRANS_UDP, "UDP Unicast Mode", "udp-unicast"},
    {RTSP_LOWER_TRANS_UDP_MCAST, "UDP Multicast Mode", "udp-multicast"},
    {RTSP_LOWER_TRANS_TCP, "TCP interleaved mode", "tcp"},
    {0, NULL, NULL},
  };

  if (!rtsp_lower_trans_type) {
    rtsp_lower_trans_type =
        g_flags_register_static ("GstRTSPLowerTrans", rtsp_lower_trans);
  }
  return rtsp_lower_trans_type;
}

static void gst_rtspsrc_base_init (gpointer g_class);
static void gst_rtspsrc_finalize (GObject * object);

static void gst_rtspsrc_set_property (GObject * object, guint prop_id,
    const GValue * value, GParamSpec * pspec);
static void gst_rtspsrc_get_property (GObject * object, guint prop_id,
    GValue * value, GParamSpec * pspec);

static void gst_rtspsrc_uri_handler_init (gpointer g_iface,
    gpointer iface_data);
static GstCaps *gst_rtspsrc_media_to_caps (gint pt, SDPMedia * media);

static GstStateChangeReturn gst_rtspsrc_change_state (GstElement * element,
    GstStateChange transition);
static void gst_rtspsrc_handle_message (GstBin * bin, GstMessage * message);

static gboolean gst_rtspsrc_open (GstRTSPSrc * src);
static gboolean gst_rtspsrc_play (GstRTSPSrc * src);
static gboolean gst_rtspsrc_pause (GstRTSPSrc * src);
static gboolean gst_rtspsrc_close (GstRTSPSrc * src);

static gboolean gst_rtspsrc_uri_set_uri (GstURIHandler * handler,
    const gchar * uri);

static gboolean gst_rtspsrc_activate_streams (GstRTSPSrc * src);
static void gst_rtspsrc_loop (GstRTSPSrc * src);
static void gst_rtspsrc_push_event (GstRTSPSrc * src, GstEvent * event);

/* commands we send to out loop to notify it of events */
#define CMD_WAIT	0
#define CMD_RECONNECT	1
#define CMD_STOP	2

/*static guint gst_rtspsrc_signals[LAST_SIGNAL] = { 0 }; */

static void
_do_init (GType rtspsrc_type)
{
  static const GInterfaceInfo urihandler_info = {
    gst_rtspsrc_uri_handler_init,
    NULL,
    NULL
  };

  GST_DEBUG_CATEGORY_INIT (rtspsrc_debug, "rtspsrc", 0, "RTSP src");

  g_type_add_interface_static (rtspsrc_type, GST_TYPE_URI_HANDLER,
      &urihandler_info);
}

GST_BOILERPLATE_FULL (GstRTSPSrc, gst_rtspsrc, GstBin, GST_TYPE_BIN, _do_init);

static void
gst_rtspsrc_base_init (gpointer g_class)
{
  GstElementClass *element_class = GST_ELEMENT_CLASS (g_class);

  gst_element_class_add_pad_template (element_class,
      gst_static_pad_template_get (&rtptemplate));

  gst_element_class_set_details (element_class, &gst_rtspsrc_details);
}

static void
gst_rtspsrc_class_init (GstRTSPSrcClass * klass)
{
  GObjectClass *gobject_class;
  GstElementClass *gstelement_class;
  GstBinClass *gstbin_class;

  gobject_class = (GObjectClass *) klass;
  gstelement_class = (GstElementClass *) klass;
  gstbin_class = (GstBinClass *) klass;

  gobject_class->set_property = gst_rtspsrc_set_property;
  gobject_class->get_property = gst_rtspsrc_get_property;

  gobject_class->finalize = gst_rtspsrc_finalize;

  g_object_class_install_property (gobject_class, PROP_LOCATION,
      g_param_spec_string ("location", "RTSP Location",
          "Location of the RTSP url to read",
          DEFAULT_LOCATION, G_PARAM_READWRITE));

  g_object_class_install_property (gobject_class, PROP_PROTOCOLS,
      g_param_spec_flags ("protocols", "Protocols",
          "Allowed lower transport protocols", GST_TYPE_RTSP_LOWER_TRANS,
          DEFAULT_PROTOCOLS, G_PARAM_READWRITE | G_PARAM_CONSTRUCT));

  g_object_class_install_property (gobject_class, PROP_DEBUG,
      g_param_spec_boolean ("debug", "Debug",
          "Dump request and response messages to stdout",
          DEFAULT_DEBUG, G_PARAM_READWRITE | G_PARAM_CONSTRUCT));

  g_object_class_install_property (gobject_class, PROP_RETRY,
      g_param_spec_uint ("retry", "Retry",
          "Max number of retries when allocating RTP ports.",
          0, G_MAXUINT16, DEFAULT_RETRY,
          G_PARAM_READWRITE | G_PARAM_CONSTRUCT));

  g_object_class_install_property (gobject_class, PROP_TIMEOUT,
      g_param_spec_uint64 ("timeout", "Timeout",
          "Retry TCP transport after UDP timeout microseconds (0 = disabled)",
          0, G_MAXUINT64, DEFAULT_TIMEOUT,
          G_PARAM_READWRITE | G_PARAM_CONSTRUCT));

  g_object_class_install_property (gobject_class, PROP_TCP_TIMEOUT,
      g_param_spec_uint64 ("tcp-timeout", "TCP Timeout",
          "Fail after timeout microseconds on TCP connections (0 = disabled)",
          0, G_MAXUINT64, DEFAULT_TCP_TIMEOUT,
          G_PARAM_READWRITE | G_PARAM_CONSTRUCT));

  g_object_class_install_property (gobject_class, PROP_LATENCY,
      g_param_spec_uint ("latency", "Buffer latency in ms",
          "Amount of ms to buffer", 0, G_MAXUINT, DEFAULT_LATENCY_MS,
          G_PARAM_READWRITE | G_PARAM_CONSTRUCT));

  gstelement_class->change_state = gst_rtspsrc_change_state;

  gstbin_class->handle_message = gst_rtspsrc_handle_message;
}

static void
gst_rtspsrc_init (GstRTSPSrc * src, GstRTSPSrcClass * g_class)
{
  src->stream_rec_lock = g_new (GStaticRecMutex, 1);
  g_static_rec_mutex_init (src->stream_rec_lock);

  src->location = g_strdup (DEFAULT_LOCATION);
  src->url = NULL;

#ifdef WITH_EXT_REAL
  src->extension = rtsp_ext_real_get_context ();
#else
  /* install WMS extension by default */
  src->extension = rtsp_ext_wms_get_context ();
#endif
  src->extension->src = (gpointer) src;

  src->state_rec_lock = g_new (GStaticRecMutex, 1);
  g_static_rec_mutex_init (src->state_rec_lock);
  src->state = RTSP_STATE_INVALID;
}

static void
gst_rtspsrc_finalize (GObject * object)
{
  GstRTSPSrc *rtspsrc;

  rtspsrc = GST_RTSPSRC (object);

  g_static_rec_mutex_free (rtspsrc->stream_rec_lock);
  g_free (rtspsrc->stream_rec_lock);
  g_free (rtspsrc->location);
  g_free (rtspsrc->req_location);
  g_free (rtspsrc->content_base);
  rtsp_url_free (rtspsrc->url);
  g_static_rec_mutex_free (rtspsrc->state_rec_lock);
  g_free (rtspsrc->state_rec_lock);

  if (rtspsrc->extension) {
#ifdef WITH_EXT_REAL
    rtsp_ext_real_free_context (rtspsrc->extension);
#else
    rtsp_ext_wms_free_context (rtspsrc->extension);
#endif
  }

  G_OBJECT_CLASS (parent_class)->finalize (object);
}

static void
gst_rtspsrc_set_property (GObject * object, guint prop_id, const GValue * value,
    GParamSpec * pspec)
{
  GstRTSPSrc *rtspsrc;

  rtspsrc = GST_RTSPSRC (object);

  switch (prop_id) {
    case PROP_LOCATION:
      gst_rtspsrc_uri_set_uri (GST_URI_HANDLER (rtspsrc),
          g_value_get_string (value));
      break;
    case PROP_PROTOCOLS:
      rtspsrc->protocols = g_value_get_flags (value);
      break;
    case PROP_DEBUG:
      rtspsrc->debug = g_value_get_boolean (value);
      break;
    case PROP_RETRY:
      rtspsrc->retry = g_value_get_uint (value);
      break;
    case PROP_TIMEOUT:
      rtspsrc->udp_timeout = g_value_get_uint64 (value);
      break;
    case PROP_TCP_TIMEOUT:
    {
      guint64 timeout = g_value_get_uint64 (value);

      rtspsrc->tcp_timeout.tv_sec = timeout / G_USEC_PER_SEC;
      rtspsrc->tcp_timeout.tv_usec = timeout % G_USEC_PER_SEC;
      break;
    }
    case PROP_LATENCY:
      rtspsrc->latency = g_value_get_uint (value);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
  }
}

static void
gst_rtspsrc_get_property (GObject * object, guint prop_id, GValue * value,
    GParamSpec * pspec)
{
  GstRTSPSrc *rtspsrc;

  rtspsrc = GST_RTSPSRC (object);

  switch (prop_id) {
    case PROP_LOCATION:
      g_value_set_string (value, rtspsrc->location);
      break;
    case PROP_PROTOCOLS:
      g_value_set_flags (value, rtspsrc->protocols);
      break;
    case PROP_DEBUG:
      g_value_set_boolean (value, rtspsrc->debug);
      break;
    case PROP_RETRY:
      g_value_set_uint (value, rtspsrc->retry);
      break;
    case PROP_TIMEOUT:
      g_value_set_uint64 (value, rtspsrc->udp_timeout);
      break;
    case PROP_TCP_TIMEOUT:
    {
      guint64 timeout;

      timeout = rtspsrc->tcp_timeout.tv_sec * G_USEC_PER_SEC +
          rtspsrc->tcp_timeout.tv_usec;
      g_value_set_uint64 (value, timeout);
      break;
    }
    case PROP_LATENCY:
      g_value_set_uint (value, rtspsrc->latency);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
  }
}

static gint
find_stream_by_id (GstRTSPStream * stream, gconstpointer a)
{
  gint id = GPOINTER_TO_INT (a);

  if (stream->id == id)
    return 0;

  return -1;
}

static gint
find_stream_by_channel (GstRTSPStream * stream, gconstpointer a)
{
  gint channel = GPOINTER_TO_INT (a);

  if (stream->channel[0] == channel || stream->channel[1] == channel)
    return 0;

  return -1;
}

static gint
find_stream_by_pt (GstRTSPStream * stream, gconstpointer a)
{
  gint pt = GPOINTER_TO_INT (a);

  if (stream->pt == pt)
    return 0;

  return -1;
}

static gint
find_stream_by_udpsrc (GstRTSPStream * stream, gconstpointer a)
{
  GstElement *src = (GstElement *) a;

  if (stream->udpsrc[0] == src)
    return 0;
  if (stream->udpsrc[1] == src)
    return 0;

  return -1;
}

static gint
find_stream_by_setup (GstRTSPStream * stream, gconstpointer a)
{
  /* check qualified setup_url */
  if (!strcmp (stream->setup_url, (gchar *) a))
    return 0;
  /* check original control_url */
  if (!strcmp (stream->control_url, (gchar *) a))
    return 0;

  /* check if qualified setup_url ends with string */
  if (g_str_has_suffix (stream->control_url, (gchar *) a))
    return 0;

  return -1;
}

static GstRTSPStream *
gst_rtspsrc_create_stream (GstRTSPSrc * src, SDPMessage * sdp, gint idx)
{
  GstRTSPStream *stream;
  gchar *control_url;
  gchar *payload;
  SDPMedia *media;

  /* get media, should not return NULL */
  media = sdp_message_get_media (sdp, idx);
  if (media == NULL)
    return NULL;

  stream = g_new0 (GstRTSPStream, 1);
  stream->parent = src;
  /* we mark the pad as not linked, we will mark it as OK when we add the pad to
   * the element. */
  stream->last_ret = GST_FLOW_NOT_LINKED;
  stream->added = FALSE;
  stream->disabled = FALSE;
  stream->id = src->numstreams++;

  /* we must have a payload. No payload means we cannot create caps */
  /* FIXME, handle multiple formats. */
  if ((payload = sdp_media_get_format (media, 0))) {
    stream->pt = atoi (payload);
    /* convert caps */
    stream->caps = gst_rtspsrc_media_to_caps (stream->pt, media);

    if (stream->pt >= 96) {
      /* If we have a dynamic payload type, see if we have a stream with the
       * same payload number. If there is one, they are part of the same
       * container and we only need to add one pad. */
      if (g_list_find_custom (src->streams, GINT_TO_POINTER (stream->pt),
              (GCompareFunc) find_stream_by_pt)) {
        stream->container = TRUE;
      }
    }
  }

  /* get control url to construct the setup url. The setup url is used to
   * configure the transport of the stream and is used to identity the stream in
   * the RTP-Info header field returned from PLAY. */
  control_url = sdp_media_get_attribute_val (media, "control");

  GST_DEBUG_OBJECT (src, "stream %d", stream->id);
  GST_DEBUG_OBJECT (src, " pt: %d", stream->pt);
  GST_DEBUG_OBJECT (src, " container: %d", stream->container);
  GST_DEBUG_OBJECT (src, " caps: %" GST_PTR_FORMAT, stream->caps);
  GST_DEBUG_OBJECT (src, " control: %s", GST_STR_NULL (control_url));

  if (control_url != NULL) {
    stream->control_url = g_strdup (control_url);
    /* Build a fully qualified url using the content_base if any or by prefixing
     * the original request.
     * If the control_url starts with a '/' or a non rtsp: protocol we will most
     * likely build a URL that the server will fail to understand, this is ok,
     * we will fail then. */
    if (g_str_has_prefix (control_url, "rtsp://"))
      stream->setup_url = g_strdup (control_url);
    else if (src->content_base)
      stream->setup_url =
          g_strdup_printf ("%s%s", src->content_base, control_url);
    else
      stream->setup_url =
          g_strdup_printf ("%s/%s", src->req_location, control_url);
  }
  GST_DEBUG_OBJECT (src, " setup: %s", GST_STR_NULL (stream->setup_url));

  /* we keep track of all streams */
  src->streams = g_list_append (src->streams, stream);

  return stream;

  /* ERRORS */
}

static void
gst_rtspsrc_stream_free (GstRTSPSrc * src, GstRTSPStream * stream)
{
  gint i;

  GST_DEBUG_OBJECT (src, "free stream %p", stream);

  if (stream->caps)
    gst_caps_unref (stream->caps);

  g_free (stream->control_url);
  g_free (stream->setup_url);

  for (i = 0; i < 2; i++) {
    GstElement *udpsrc = stream->udpsrc[i];

    if (udpsrc) {
      GstPad *pad;

      /* unlink the pad */
      pad = gst_element_get_pad (udpsrc, "src");
      if (stream->channelpad[i]) {
        gst_pad_unlink (pad, stream->channelpad[i]);
        gst_object_unref (stream->channelpad[i]);
        stream->channelpad[i] = NULL;
      }

      gst_element_set_state (udpsrc, GST_STATE_NULL);
      gst_bin_remove (GST_BIN_CAST (src), udpsrc);
      gst_object_unref (stream->udpsrc[i]);
      stream->udpsrc[i] = NULL;
    }
  }
  if (stream->udpsink) {
    gst_element_set_state (stream->udpsink, GST_STATE_NULL);
    gst_bin_remove (GST_BIN_CAST (src), stream->udpsink);
    gst_object_unref (stream->udpsink);
    stream->udpsink = NULL;
  }
  if (stream->srcpad) {
    gst_pad_set_active (stream->srcpad, FALSE);
    if (stream->added) {
      gst_element_remove_pad (GST_ELEMENT_CAST (src), stream->srcpad);
      stream->added = FALSE;
    }
    stream->srcpad = NULL;
  }
  g_free (stream);
}

static void
gst_rtspsrc_cleanup (GstRTSPSrc * src)
{
  GList *walk;

  GST_DEBUG_OBJECT (src, "cleanup");

  for (walk = src->streams; walk; walk = g_list_next (walk)) {
    GstRTSPStream *stream = (GstRTSPStream *) walk->data;

    gst_rtspsrc_stream_free (src, stream);
  }
  g_list_free (src->streams);
  src->streams = NULL;
  if (src->session) {
    if (src->session_sig_id) {
      g_signal_handler_disconnect (src->session, src->session_sig_id);
      src->session_sig_id = 0;
    }
    gst_element_set_state (src->session, GST_STATE_NULL);
    gst_bin_remove (GST_BIN_CAST (src), src->session);
    src->session = NULL;
  }
  src->numstreams = 0;
  if (src->props)
    gst_structure_free (src->props);
  src->props = NULL;
}

/* FIXME, this should go somewhere else, ideally 
 */
static guint
get_default_rate_for_pt (gint pt)
{
  switch (pt) {
    case 0:
    case 3:
    case 4:
    case 5:
    case 7:
    case 8:
    case 9:
    case 12:
    case 13:
    case 15:
    case 18:
      return 8000;
    case 16:
      return 11025;
    case 17:
      return 22050;
    case 6:
      return 16000;
    case 10:
    case 11:
      return 44100;
    case 14:
    case 25:
    case 26:
    case 28:
    case 31:
    case 32:
    case 33:
    case 34:
      return 90000;
    default:
      return -1;
  }
}

#define PARSE_INT(p, del, res)          \
G_STMT_START {                          \
  gchar *t = p;                         \
  p = strstr (p, del);                  \
  if (p == NULL)                        \
    res = -1;                           \
  else {                                \
    *p = '\0';                          \
    p++;                                \
    res = atoi (t);                     \
  }                                     \
} G_STMT_END

#define PARSE_STRING(p, del, res)       \
G_STMT_START {                          \
  gchar *t = p;                         \
  p = strstr (p, del);                  \
  if (p == NULL)                        \
    res = NULL;                         \
  else {                                \
    *p = '\0';                          \
    p++;                                \
    res = t;                            \
  }                                     \
} G_STMT_END

#define SKIP_SPACES(p)                  \
  while (*p && g_ascii_isspace (*p))    \
    p++;

/* rtpmap contains:
 *
 *  <payload> <encoding_name>/<clock_rate>[/<encoding_params>]
 */
static gboolean
gst_rtspsrc_parse_rtpmap (gchar * rtpmap, gint * payload, gchar ** name,
    gint * rate, gchar ** params)
{
  gchar *p, *t;

  t = p = rtpmap;

  PARSE_INT (p, " ", *payload);
  if (*payload == -1)
    return FALSE;

  SKIP_SPACES (p);
  if (*p == '\0')
    return FALSE;

  PARSE_STRING (p, "/", *name);
  if (*name == NULL) {
    /* no rate, assume -1 then */
    *name = p;
    *rate = -1;
    return TRUE;
  }

  t = p;
  p = strstr (p, "/");
  if (p == NULL) {
    *rate = atoi (t);
    return TRUE;
  }
  *p = '\0';
  p++;
  *rate = atoi (t);

  t = p;
  if (*p == '\0')
    return TRUE;
  *params = t;

  return TRUE;
}

/*
 *  Mapping of caps to and from SDP fields:
 *
 *   m=<media> <UDP port> RTP/AVP <payload> 
 *   a=rtpmap:<payload> <encoding_name>/<clock_rate>[/<encoding_params>]
 *   a=fmtp:<payload> <param>[=<value>];...
 */
static GstCaps *
gst_rtspsrc_media_to_caps (gint pt, SDPMedia * media)
{
  GstCaps *caps;
  gchar *rtpmap;
  gchar *fmtp;
  gchar *name = NULL;
  gint rate = -1;
  gchar *params = NULL;
  gchar *tmp;
  GstStructure *s;
  gint payload = 0;
  gboolean ret;

  /* get and parse rtpmap */
  if ((rtpmap = sdp_media_get_attribute_val (media, "rtpmap"))) {
    ret = gst_rtspsrc_parse_rtpmap (rtpmap, &payload, &name, &rate, &params);
    if (ret) {
      if (payload != pt) {
        /* we ignore the rtpmap if the payload type is different. */
        g_warning ("rtpmap of wrong payload type, ignoring");
        name = NULL;
        rate = -1;
        params = NULL;
      }
    } else {
      /* if we failed to parse the rtpmap for a dynamic payload type, we have an
       * error */
      if (pt >= 96)
        goto no_rtpmap;
      /* else we can ignore */
      g_warning ("error parsing rtpmap, ignoring");
    }
  } else {
    /* dynamic payloads need rtpmap or we fail */
    if (pt >= 96)
      goto no_rtpmap;
  }
  /* check if we have a rate, if not, we need to look up the rate from the
   * default rates based on the payload types. */
  if (rate == -1) {
    rate = get_default_rate_for_pt (pt);
    /* we fail if we cannot find one */
    if (rate == -1)
      goto no_rate;
  }

  tmp = g_ascii_strdown (media->media, -1);
  caps = gst_caps_new_simple ("application/x-unknown",
      "media", G_TYPE_STRING, tmp, "payload", G_TYPE_INT, pt, NULL);
  g_free (tmp);
  s = gst_caps_get_structure (caps, 0);

  if (rate != -1)
    gst_structure_set (s, "clock-rate", G_TYPE_INT, rate, NULL);

  /* encoding name must be upper case */
  if (name != NULL) {
    tmp = g_ascii_strup (name, -1);
    gst_structure_set (s, "encoding-name", G_TYPE_STRING, tmp, NULL);
    g_free (tmp);
  }

  /* params must be lower case */
  if (params != NULL) {
    tmp = g_ascii_strdown (params, -1);
    gst_structure_set (s, "encoding-params", G_TYPE_STRING, tmp, NULL);
    g_free (tmp);
  }

  /* parse optional fmtp: field */
  if ((fmtp = sdp_media_get_attribute_val (media, "fmtp"))) {
    gchar *p;
    gint payload = 0;

    p = fmtp;

    /* p is now of the format <payload> <param>[=<value>];... */
    PARSE_INT (p, " ", payload);
    if (payload != -1 && payload == pt) {
      gchar **pairs;
      gint i;

      /* <param>[=<value>] are separated with ';' */
      pairs = g_strsplit (p, ";", 0);
      for (i = 0; pairs[i]; i++) {
        gchar *valpos;
        gchar *val, *key;

        /* the key may not have a '=', the value can have other '='s */
        valpos = strstr (pairs[i], "=");
        if (valpos) {
          /* we have a '=' and thus a value, remove the '=' with \0 */
          *valpos = '\0';
          /* value is everything between '=' and ';'. FIXME, strip? */
          val = g_strstrip (valpos + 1);
        } else {
          /* simple <param>;.. is translated into <param>=1;... */
          val = "1";
        }
        /* strip the key of spaces, convert key to lowercase but not the value. */
        key = g_strstrip (pairs[i]);
        if (strlen (key) > 1) {
          tmp = g_ascii_strdown (key, -1);
          gst_structure_set (s, tmp, G_TYPE_STRING, val, NULL);
          g_free (tmp);
        }
      }
      g_strfreev (pairs);
    }
  }
  return caps;

  /* ERRORS */
no_rtpmap:
  {
    g_warning ("rtpmap type not given for dynamic payload %d", pt);
    return NULL;
  }
no_rate:
  {
    g_warning ("rate unknown for payload type %d", pt);
    return NULL;
  }
}

static gboolean
gst_rtspsrc_alloc_udp_ports (GstRTSPStream * stream,
    gint * rtpport, gint * rtcpport)
{
  GstRTSPSrc *src;
  GstStateChangeReturn ret;
  GstElement *tmp, *udpsrc0, *udpsrc1;
  gint tmp_rtp, tmp_rtcp;
  guint count;

  src = stream->parent;

  tmp = NULL;
  udpsrc0 = NULL;
  udpsrc1 = NULL;
  count = 0;

  /* try to allocate 2 UDP ports, the RTP port should be an even
   * number and the RTCP port should be the next (uneven) port */
again:
  udpsrc0 = gst_element_make_from_uri (GST_URI_SRC, "udp://0.0.0.0:0", NULL);
  if (udpsrc0 == NULL)
    goto no_udp_protocol;

  ret = gst_element_set_state (udpsrc0, GST_STATE_PAUSED);
  if (ret == GST_STATE_CHANGE_FAILURE)
    goto start_udp_failure;

  g_object_get (G_OBJECT (udpsrc0), "port", &tmp_rtp, NULL);
  GST_DEBUG_OBJECT (src, "got RTP port %d", tmp_rtp);

  /* check if port is even */
  if ((tmp_rtp & 0x01) != 0) {
    /* port not even, close and allocate another */
    count++;
    if (count > src->retry)
      goto no_ports;

    GST_DEBUG_OBJECT (src, "RTP port not even, retry %d", count);
    /* have to keep port allocated so we can get a new one */
    if (tmp != NULL) {
      GST_DEBUG_OBJECT (src, "free temp");
      gst_element_set_state (tmp, GST_STATE_NULL);
      gst_object_unref (tmp);
    }
    tmp = udpsrc0;
    GST_DEBUG_OBJECT (src, "retry %d", count);
    goto again;
  }
  /* free leftover temp element/port */
  if (tmp) {
    gst_element_set_state (tmp, GST_STATE_NULL);
    gst_object_unref (tmp);
    tmp = NULL;
  }

  /* allocate port+1 for RTCP now */
  udpsrc1 = gst_element_make_from_uri (GST_URI_SRC, "udp://0.0.0.0", NULL);
  if (udpsrc1 == NULL)
    goto no_udp_rtcp_protocol;

  /* set port */
  tmp_rtcp = tmp_rtp + 1;
  g_object_set (G_OBJECT (udpsrc1), "port", tmp_rtcp, NULL);

  GST_DEBUG_OBJECT (src, "starting RTCP on port %d", tmp_rtcp);
  ret = gst_element_set_state (udpsrc1, GST_STATE_PAUSED);
  /* FIXME, this could fail if the next port is not free, we
   * should retry with another port then */
  if (ret == GST_STATE_CHANGE_FAILURE)
    goto start_rtcp_failure;

  /* all fine, do port check */
  g_object_get (G_OBJECT (udpsrc0), "port", rtpport, NULL);
  g_object_get (G_OBJECT (udpsrc1), "port", rtcpport, NULL);

  /* this should not happen... */
  if (*rtpport != tmp_rtp || *rtcpport != tmp_rtcp)
    goto port_error;

  /* we keep these elements, we configure all in configure_transport when the
   * server told us to really use the UDP ports. */
  stream->udpsrc[0] = gst_object_ref (udpsrc0);
  stream->udpsrc[1] = gst_object_ref (udpsrc1);

  /* they are ours now */
  gst_object_sink (udpsrc0);
  gst_object_sink (udpsrc1);

  return TRUE;

  /* ERRORS */
no_udp_protocol:
  {
    GST_DEBUG_OBJECT (src, "could not get UDP source");
    goto cleanup;
  }
start_udp_failure:
  {
    GST_DEBUG_OBJECT (src, "could not start UDP source");
    goto cleanup;
  }
no_ports:
  {
    GST_DEBUG_OBJECT (src, "could not allocate UDP port pair after %d retries",
        count);
    goto cleanup;
  }
no_udp_rtcp_protocol:
  {
    GST_DEBUG_OBJECT (src, "could not get UDP source for RTCP");
    goto cleanup;
  }
start_rtcp_failure:
  {
    GST_DEBUG_OBJECT (src, "could not start UDP source for RTCP");
    goto cleanup;
  }
port_error:
  {
    GST_DEBUG_OBJECT (src, "ports don't match rtp: %d<->%d, rtcp: %d<->%d",
        tmp_rtp, *rtpport, tmp_rtcp, *rtcpport);
    goto cleanup;
  }
cleanup:
  {
    if (tmp) {
      gst_element_set_state (tmp, GST_STATE_NULL);
      gst_object_unref (tmp);
    }
    if (udpsrc0) {
      gst_element_set_state (udpsrc0, GST_STATE_NULL);
      gst_object_unref (udpsrc0);
    }
    if (udpsrc1) {
      gst_element_set_state (udpsrc1, GST_STATE_NULL);
      gst_object_unref (udpsrc1);
    }
    return FALSE;
  }
}

static void
gst_rtspsrc_flush (GstRTSPSrc * src, gboolean flush)
{
  GstEvent *event;

  if (flush) {
    event = gst_event_new_flush_start ();
  } else {
    event = gst_event_new_flush_stop ();
  }

  rtsp_connection_flush (src->connection, flush);

  gst_rtspsrc_push_event (src, event);
}

static gboolean
gst_rtspsrc_do_seek (GstRTSPSrc * src, GstSegment * segment)
{
  gboolean res;

  /* PLAY from new position, we are flushing now */
  src->position = ((gdouble) segment->last_stop) / GST_SECOND;

  src->state = RTSP_STATE_SEEKING;

  res = gst_rtspsrc_play (src);

  return res;
}

static gboolean
gst_rtspsrc_perform_seek (GstRTSPSrc * src, GstEvent * event)
{
  gboolean res;
  gdouble rate;
  GstFormat format;
  GstSeekFlags flags;
  GstSeekType cur_type = GST_SEEK_TYPE_NONE, stop_type;
  gint64 cur, stop;
  gboolean flush;
  gboolean update;
  GstSegment seeksegment = { 0, };
  gint64 last_stop;

  if (event) {
    GST_DEBUG_OBJECT (src, "doing seek with event");

    gst_event_parse_seek (event, &rate, &format, &flags,
        &cur_type, &cur, &stop_type, &stop);

    /* no negative rates yet */
    if (rate < 0.0)
      goto negative_rate;

    /* we need TIME format */
    if (format != src->segment.format)
      goto no_format;
  } else {
    GST_DEBUG_OBJECT (src, "doing seek without event");
    flags = 0;
    cur_type = GST_SEEK_TYPE_SET;
    stop_type = GST_SEEK_TYPE_SET;
  }

  /* get flush flag */
  flush = flags & GST_SEEK_FLAG_FLUSH;

  /* now we need to make sure the streaming thread is stopped. We do this by
   * either sending a FLUSH_START event downstream which will cause the
   * streaming thread to stop with a WRONG_STATE.
   * For a non-flushing seek we simply pause the task, which will happen as soon
   * as it completes one iteration (and thus might block when the sink is
   * blocking in preroll). */
  if (flush) {
    GST_DEBUG_OBJECT (src, "starting flush");
    gst_rtspsrc_flush (src, TRUE);
  } else {
    //gst_pad_pause_task (src->sinkpad);
  }

  /* we should now be able to grab the streaming thread because we stopped it
   * with the above flush/pause code */
  //GST_PAD_STREAM_LOCK (src->sinkpad);

  /* save current position */
  last_stop = src->segment.last_stop;

  GST_DEBUG_OBJECT (src, "stopped streaming at %" G_GINT64_FORMAT, last_stop);

  /* copy segment, we need this because we still need the old
   * segment when we close the current segment. */
  memcpy (&seeksegment, &src->segment, sizeof (GstSegment));

  /* configure the seek parameters in the seeksegment. We will then have the
   * right values in the segment to perform the seek */
  if (event) {
    GST_DEBUG_OBJECT (src, "configuring seek");
    gst_segment_set_seek (&seeksegment, rate, format, flags,
        cur_type, cur, stop_type, stop, &update);
  }

  /* figure out the last position we need to play. If it's configured (stop !=
   * -1), use that, else we play until the total duration of the file */
  if ((stop = seeksegment.stop) == -1)
    stop = seeksegment.duration;

  res = gst_rtspsrc_do_seek (src, &seeksegment);

  /* prepare for streaming again */
  if (flush) {
    /* if we started flush, we stop now */
    GST_DEBUG_OBJECT (src, "stopping flush");
    gst_rtspsrc_flush (src, FALSE);
  } else if (src->running) {
    /* we are running the current segment and doing a non-flushing seek,
     * close the segment first based on the previous last_stop. */
    GST_DEBUG_OBJECT (src, "closing running segment %" G_GINT64_FORMAT
        " to %" G_GINT64_FORMAT, src->segment.accum, src->segment.last_stop);

    /* queue the segment for sending in the stream thread */
    if (src->close_segment)
      gst_event_unref (src->close_segment);
    src->close_segment = gst_event_new_new_segment (TRUE,
        src->segment.rate, src->segment.format,
        src->segment.accum, src->segment.last_stop, src->segment.accum);

    /* keep track of our last_stop */
    seeksegment.accum = src->segment.last_stop;
  }

  /* now we did the seek and can activate the new segment values */
  memcpy (&src->segment, &seeksegment, sizeof (GstSegment));

  /* if we're doing a segment seek, post a SEGMENT_START message */
  if (src->segment.flags & GST_SEEK_FLAG_SEGMENT) {
    gst_element_post_message (GST_ELEMENT_CAST (src),
        gst_message_new_segment_start (GST_OBJECT_CAST (src),
            src->segment.format, src->segment.last_stop));
  }

  /* now create the newsegment */
  GST_DEBUG_OBJECT (src, "Creating newsegment from %" G_GINT64_FORMAT
      " to %" G_GINT64_FORMAT, src->segment.last_stop, stop);

  /* store the newsegment event so it can be sent from the streaming thread. */
  if (src->start_segment)
    gst_event_unref (src->start_segment);
  src->start_segment =
      gst_event_new_new_segment (FALSE, src->segment.rate,
      src->segment.format, src->segment.last_stop, stop,
      src->segment.last_stop);

  /* mark discont if we are going to stream from another position. */
  if (last_stop != src->segment.last_stop) {
    GST_DEBUG_OBJECT (src, "mark DISCONT, we did a seek to another position");
    //src->discont = TRUE;
  }

  /* and start the streaming task again */
  src->running = TRUE;
  //gst_pad_start_task (src->sinkpad, (GstTaskFunction) gst_srcparse_loop,
  //    src->sinkpad);

  //GST_PAD_STREAM_UNLOCK (src->sinkpad);

  return TRUE;

  /* ERRORS */
negative_rate:
  {
    GST_DEBUG_OBJECT (src, "negative playback rates are not supported yet.");
    return FALSE;
  }
no_format:
  {
    GST_DEBUG_OBJECT (src, "unsupported format given, seek aborted.");
    return FALSE;
  }
}

static gboolean
gst_rtspsrc_handle_src_event (GstPad * pad, GstEvent * event)
{
  GstRTSPSrc *src;
  gboolean res = FALSE;

  src = GST_RTSPSRC_CAST (gst_pad_get_element_private (pad));

  GST_DEBUG_OBJECT (src, "pad %s:%s received event %s",
      GST_DEBUG_PAD_NAME (pad), GST_EVENT_TYPE_NAME (event));

  switch (GST_EVENT_TYPE (event)) {
    case GST_EVENT_QOS:
      break;
    case GST_EVENT_SEEK:
      res = gst_rtspsrc_perform_seek (src, event);
      break;
    case GST_EVENT_NAVIGATION:
      break;
    case GST_EVENT_LATENCY:
      break;
    default:
      break;
  }
  return res;
}

static gboolean
gst_rtspsrc_handle_src_query (GstPad * pad, GstQuery * query)
{
  GstRTSPSrc *src;
  gboolean res = TRUE;

  src = GST_RTSPSRC_CAST (gst_pad_get_element_private (pad));

  GST_DEBUG_OBJECT (src, "pad %s:%s received query %s",
      GST_DEBUG_PAD_NAME (pad), GST_QUERY_TYPE_NAME (query));

  switch (GST_QUERY_TYPE (query)) {
    case GST_QUERY_POSITION:
    {
      break;
    }
    case GST_QUERY_DURATION:
    {
      GstFormat format;

      gst_query_parse_duration (query, &format, NULL);

      switch (format) {
        case GST_FORMAT_TIME:
          gst_query_set_duration (query, format, src->segment.duration);
          break;
        default:
          res = FALSE;
          break;
      }
      break;
    }
    case GST_QUERY_LATENCY:
    {
      /* we are live with a min latency of 0 and unlimted max latency */
      gst_query_set_latency (query, TRUE, 0, -1);
      break;
    }
    default:
      break;
  }

  return res;
}

/* callback for RTCP messages to be sent to the server when operating in TCP
 * mode. */
static GstFlowReturn
gst_rtspsrc_sink_chain (GstPad * pad, GstBuffer * buffer)
{
  GstRTSPSrc *src;
  GstRTSPStream *stream;
  GstFlowReturn res = GST_FLOW_OK;
  guint8 *data;
  guint size;
  RTSPResult ret;
  RTSPMessage message = { 0 };

  stream = (GstRTSPStream *) gst_pad_get_element_private (pad);
  src = stream->parent;

  data = GST_BUFFER_DATA (buffer);
  size = GST_BUFFER_SIZE (buffer);

  rtsp_message_init_data (&message, stream->channel[1]);

  rtsp_message_take_body (&message, data, size);

  GST_DEBUG_OBJECT (src, "sending %u bytes RTCP", size);
  ret = rtsp_connection_send (src->connection, &message, NULL);
  GST_DEBUG_OBJECT (src, "sent RTCP, %d", ret);

  rtsp_message_steal_body (&message, &data, &size);

  gst_buffer_unref (buffer);

  return res;
}

static void
pad_unblocked (GstPad * pad, gboolean blocked, GstRTSPSrc * src)
{
  GST_DEBUG_OBJECT (src, "pad %s:%s unblocked", GST_DEBUG_PAD_NAME (pad));
}

static void
pad_blocked (GstPad * pad, gboolean blocked, GstRTSPSrc * src)
{
  GST_DEBUG_OBJECT (src, "pad %s:%s blocked, activating streams",
      GST_DEBUG_PAD_NAME (pad));

  /* activate the streams */
  GST_OBJECT_LOCK (src);
  if (!src->need_activate)
    goto was_ok;

  src->need_activate = FALSE;
  GST_OBJECT_UNLOCK (src);

  gst_rtspsrc_activate_streams (src);

  return;

was_ok:
  {
    GST_OBJECT_UNLOCK (src);
    return;
  }
}

/* this callback is called when the session manager generated a new src pad with
 * payloaded RTP packets. We simply ghost the pad here. */
static void
new_session_pad (GstElement * session, GstPad * pad, GstRTSPSrc * src)
{
  gchar *name;
  GstPadTemplate *template;
  gint id, ssrc, pt;
  GList *lstream;
  GstRTSPStream *stream;
  gboolean all_added;

  GST_DEBUG_OBJECT (src, "got new session pad %" GST_PTR_FORMAT, pad);

  GST_RTSP_STATE_LOCK (src);
  /* find stream */
  name = gst_object_get_name (GST_OBJECT_CAST (pad));
  if (sscanf (name, "recv_rtp_src_%d_%d_%d", &id, &ssrc, &pt) != 3)
    goto unknown_stream;

  GST_DEBUG_OBJECT (src, "stream: %u, SSRC %d, PT %d", id, ssrc, pt);

  lstream = g_list_find_custom (src->streams, GINT_TO_POINTER (id),
      (GCompareFunc) find_stream_by_id);
  if (lstream == NULL)
    goto unknown_stream;

  /* get stream */
  stream = (GstRTSPStream *) lstream->data;

  /* create a new pad we will use to stream to */
  template = gst_static_pad_template_get (&rtptemplate);
  stream->srcpad = gst_ghost_pad_new_from_template (name, pad, template);
  gst_object_unref (template);
  g_free (name);

  stream->added = TRUE;
  gst_pad_set_active (stream->srcpad, TRUE);
  gst_element_add_pad (GST_ELEMENT_CAST (src), stream->srcpad);

  /* check if we added all streams */
  all_added = TRUE;
  for (lstream = src->streams; lstream; lstream = g_list_next (lstream)) {
    stream = (GstRTSPStream *) lstream->data;
    /* a container stream only needs one pad added. Also disabled streams don't
     * count */
    if (!stream->container && !stream->disabled && !stream->added) {
      all_added = FALSE;
      break;
    }
  }
  GST_RTSP_STATE_UNLOCK (src);

  if (all_added) {
    GST_DEBUG_OBJECT (src, "We added all streams");
    /* when we get here, all stream are added and we can fire the no-more-pads
     * signal. */
    gst_element_no_more_pads (GST_ELEMENT_CAST (src));
  }

  return;

  /* ERRORS */
unknown_stream:
  {
    GST_DEBUG_OBJECT (src, "ignoring unknown stream");
    GST_RTSP_STATE_UNLOCK (src);
    g_free (name);
    return;
  }
}

static GstCaps *
request_pt_map (GstElement * sess, guint session, guint pt, GstRTSPSrc * src)
{
  GstRTSPStream *stream;
  GList *lstream;
  GstCaps *caps;

  GST_DEBUG_OBJECT (src, "getting pt map for pt %d in session %d", pt, session);

  GST_RTSP_STATE_LOCK (src);
  lstream = g_list_find_custom (src->streams, GINT_TO_POINTER (session),
      (GCompareFunc) find_stream_by_id);
  if (!lstream)
    goto unknown_stream;

  stream = (GstRTSPStream *) lstream->data;
  caps = stream->caps;
  GST_RTSP_STATE_UNLOCK (src);

  return caps;

unknown_stream:
  {
    GST_DEBUG_OBJECT (src, "unknown stream %d", session);
    GST_RTSP_STATE_UNLOCK (src);
    return NULL;
  }
}

/* try to get and configure a manager */
static gboolean
gst_rtspsrc_stream_configure_manager (GstRTSPSrc * src, GstRTSPStream * stream,
    RTSPTransport * transport)
{
  const gchar *manager;
  gchar *name;
  RTSPResult res;
  GstStateChangeReturn ret;

  /* find a manager */
  if ((res = rtsp_transport_get_manager (transport->trans, &manager, 0)) < 0)
    goto no_manager;

  if (manager) {
    GST_DEBUG_OBJECT (src, "using manager %s", manager);

    /* configure the manager */
    if (src->session == NULL) {
      if (!(src->session = gst_element_factory_make (manager, NULL))) {
        /* fallback */
        if ((res =
                rtsp_transport_get_manager (transport->trans, &manager, 1)) < 0)
          goto no_manager;

        if (!manager)
          goto use_no_manager;

        if (!(src->session = gst_element_factory_make (manager, NULL)))
          goto manager_failed;
      }

      /* we manage this element */
      gst_bin_add (GST_BIN_CAST (src), src->session);

      ret = gst_element_set_state (src->session, GST_STATE_PAUSED);
      if (ret == GST_STATE_CHANGE_FAILURE)
        goto start_session_failure;

      g_object_set (src->session, "latency", src->latency, NULL);

      /* connect to signals if we did not already do so */
      GST_DEBUG_OBJECT (src, "connect to signals on session manager");
      src->session_sig_id =
          g_signal_connect (src->session, "pad-added",
          (GCallback) new_session_pad, src);
      src->session_ptmap_id =
          g_signal_connect (src->session, "request-pt-map",
          (GCallback) request_pt_map, src);
    }

    /* we stream directly to the manager, get some pads. Each RTSP stream goes
     * into a separate RTP session. */
    name = g_strdup_printf ("recv_rtp_sink_%d", stream->id);
    stream->channelpad[0] = gst_element_get_request_pad (src->session, name);
    g_free (name);
    name = g_strdup_printf ("recv_rtcp_sink_%d", stream->id);
    stream->channelpad[1] = gst_element_get_request_pad (src->session, name);
    g_free (name);
  }

use_no_manager:
  return TRUE;

  /* ERRORS */
no_manager:
  {
    GST_DEBUG_OBJECT (src, "cannot get a session manager");
    return FALSE;
  }
manager_failed:
  {
    GST_DEBUG_OBJECT (src, "no session manager element %s found", manager);
    return FALSE;
  }
start_session_failure:
  {
    GST_DEBUG_OBJECT (src, "could not start session");
    return FALSE;
  }
}

/* free the UDP sources allocated when negotiating a transport.
 * This function is called when the server negotiated to a transport where the
 * UDP sources are not needed anymore, such as TCP or multicast. */
static void
gst_rtspsrc_stream_free_udp (GstRTSPStream * stream)
{
  gint i;

  for (i = 0; i < 2; i++) {
    if (stream->udpsrc[i]) {
      gst_element_set_state (stream->udpsrc[i], GST_STATE_NULL);
      gst_object_unref (stream->udpsrc[i]);
      stream->udpsrc[i] = NULL;
    }
  }
}

/* for TCP, create pads to send and receive data to and from the manager and to
 * intercept various events and queries
 */
static gboolean
gst_rtspsrc_stream_configure_tcp (GstRTSPSrc * src, GstRTSPStream * stream,
    RTSPTransport * transport, GstPad ** outpad)
{
  gchar *name;
  GstPadTemplate *template;
  GstPad *pad0, *pad1;

  /* configure for interleaved delivery, nothing needs to be done
   * here, the loop function will call the chain functions of the
   * session manager. */
  stream->channel[0] = transport->interleaved.min;
  stream->channel[1] = transport->interleaved.max;
  GST_DEBUG_OBJECT (src, "stream %p on channels %d-%d", stream,
      stream->channel[0], stream->channel[1]);

  /* we can remove the allocated UDP ports now */
  gst_rtspsrc_stream_free_udp (stream);

  /* no session manager, send data to srcpad directly */
  if (!stream->channelpad[0]) {
    GST_DEBUG_OBJECT (src, "no manager, creating pad");

    /* create a new pad we will use to stream to */
    name = g_strdup_printf ("stream%d", stream->id);
    template = gst_static_pad_template_get (&rtptemplate);
    stream->channelpad[0] = gst_pad_new_from_template (template, name);
    gst_object_unref (template);
    g_free (name);

    /* set caps and activate */
    gst_pad_use_fixed_caps (stream->channelpad[0]);
    gst_pad_set_active (stream->channelpad[0], TRUE);

    *outpad = gst_object_ref (stream->channelpad[0]);
  } else {
    GST_DEBUG_OBJECT (src, "using manager source pad");

    template = gst_static_pad_template_get (&anysrctemplate);

    /* allocate pads for sending the channel data into the manager */
    pad0 = gst_pad_new_from_template (template, "internalsrc0");
    gst_pad_set_event_function (pad0, gst_rtspsrc_handle_src_event);
    gst_pad_set_query_function (pad0, gst_rtspsrc_handle_src_query);
    gst_pad_link (pad0, stream->channelpad[0]);
    stream->channelpad[0] = pad0;
    gst_pad_set_active (pad0, TRUE);
    gst_pad_set_element_private (pad0, src);

    if (stream->channelpad[1]) {
      /* if we have a sinkpad for the other channel, create a pad and link to the
       * manager. */
      pad1 = gst_pad_new_from_template (template, "internalsrc1");
      gst_pad_link (pad1, stream->channelpad[1]);
      stream->channelpad[1] = pad1;
      gst_pad_set_active (pad1, TRUE);
    }
    gst_object_unref (template);
  }
  /* setup RTCP transport back to the server */
  if (src->session) {
    GstPad *pad;

    template = gst_static_pad_template_get (&anysinktemplate);

    stream->rtcppad = gst_pad_new_from_template (template, "internalsink0");
    gst_pad_set_chain_function (stream->rtcppad, gst_rtspsrc_sink_chain);
    gst_pad_set_element_private (stream->rtcppad, stream);
    gst_pad_set_active (stream->rtcppad, TRUE);

    /* get session RTCP pad */
    name = g_strdup_printf ("send_rtcp_src_%d", stream->id);
    pad = gst_element_get_request_pad (src->session, name);
    g_free (name);

    /* and link */
    gst_pad_link (pad, stream->rtcppad);
  }
  return TRUE;
}

/* For multicast create UDP sources and join the multicast group. */
static gboolean
gst_rtspsrc_stream_configure_mcast (GstRTSPSrc * src, GstRTSPStream * stream,
    RTSPTransport * transport, GstPad ** outpad)
{
  gchar *uri;

  GST_DEBUG_OBJECT (src, "creating UDP sources for multicast");

  /* we can remove the allocated UDP ports now */
  gst_rtspsrc_stream_free_udp (stream);

  /* creating UDP source */
  if (transport->port.min != -1) {
    uri = g_strdup_printf ("udp://%s:%d", transport->destination,
        transport->port.min);
    stream->udpsrc[0] = gst_element_make_from_uri (GST_URI_SRC, uri, NULL);
    g_free (uri);
    if (stream->udpsrc[0] == NULL)
      goto no_element;

    /* take ownership */
    gst_object_ref (stream->udpsrc[0]);
    gst_object_sink (stream->udpsrc[0]);

    /* change state */
    gst_element_set_state (stream->udpsrc[0], GST_STATE_READY);
  }

  /* creating another UDP source */
  if (transport->port.max != -1) {
    uri = g_strdup_printf ("udp://%s:%d", transport->destination,
        transport->port.max);
    stream->udpsrc[1] = gst_element_make_from_uri (GST_URI_SRC, uri, NULL);
    g_free (uri);
    if (stream->udpsrc[1] == NULL)
      goto no_element;

    /* take ownership */
    gst_object_ref (stream->udpsrc[1]);
    gst_object_sink (stream->udpsrc[1]);

    gst_element_set_state (stream->udpsrc[1], GST_STATE_READY);
  }
  return TRUE;

  /* ERRORS */
no_element:
  {
    GST_DEBUG_OBJECT (src, "no UDP source element found");
    return FALSE;
  }
}

/* configure the remainder of the UDP ports */
static gboolean
gst_rtspsrc_stream_configure_udp (GstRTSPSrc * src, GstRTSPStream * stream,
    RTSPTransport * transport, GstPad ** outpad)
{
  /* we manage the UDP elements now. For unicast, the UDP sources where
   * allocated in the stream when we suggested a transport. */
  if (stream->udpsrc[0]) {
    gst_bin_add (GST_BIN_CAST (src), stream->udpsrc[0]);

    GST_DEBUG_OBJECT (src, "setting up UDP source");

    /* configure a timeout on the UDP port. When the timeout message is
     * posted, we assume UDP transport is not possible. We reconnect using TCP
     * if we can. */
    g_object_set (G_OBJECT (stream->udpsrc[0]), "timeout", src->udp_timeout,
        NULL);

    /* get output pad of the UDP source. */
    *outpad = gst_element_get_pad (stream->udpsrc[0], "src");

    /* save it so we can unblock */
    stream->blockedpad = *outpad;

    /* configure pad block on the pad. As soon as there is dataflow on the
     * UDP source, we know that UDP is not blocked by a firewall and we can
     * configure all the streams to let the application autoplug decoders. */
    gst_pad_set_blocked_async (stream->blockedpad, TRUE,
        (GstPadBlockCallback) pad_blocked, src);

    if (stream->channelpad[0]) {
      GST_DEBUG_OBJECT (src, "connecting UDP source 0 to manager");
      /* configure for UDP delivery, we need to connect the UDP pads to
       * the session plugin. */
      gst_pad_link (*outpad, stream->channelpad[0]);
      gst_object_unref (*outpad);
      *outpad = NULL;
      /* we connected to pad-added signal to get pads from the manager */
    } else {
      GST_DEBUG_OBJECT (src, "using UDP src pad as output");
    }
  }

  /* RTCP port */
  if (stream->udpsrc[1]) {
    gst_bin_add (GST_BIN_CAST (src), stream->udpsrc[1]);

    if (stream->channelpad[1]) {
      GstPad *pad;

      GST_DEBUG_OBJECT (src, "connecting UDP source 1 to manager");

      pad = gst_element_get_pad (stream->udpsrc[1], "src");
      gst_pad_link (pad, stream->channelpad[1]);
      gst_object_unref (pad);
    } else {
      /* leave unlinked */
    }
  }
  return TRUE;
}

/* configure the UDP sink back to the server for status reports */
static gboolean
gst_rtspsrc_stream_configure_udp_sink (GstRTSPSrc * src, GstRTSPStream * stream,
    RTSPTransport * transport)
{
  GstPad *pad;
  gint port;
  gchar *destination, *uri, *name;

  /* no session, we're done */
  if (src->session == NULL)
    return TRUE;

  /* get host and port */
  if (transport->lower_transport == RTSP_LOWER_TRANS_UDP_MCAST)
    port = transport->port.max;
  else
    port = transport->server_port.max;

  /* first take the source, then the endpoint to figure out where to send
   * the RTCP. */
  destination = transport->source;
  if (destination == NULL)
    destination = src->connection->ip;

  GST_DEBUG_OBJECT (src, "configure UDP sink for %s:%d", destination, port);

  uri = g_strdup_printf ("udp://%s:%d", destination, port);
  stream->udpsink = gst_element_make_from_uri (GST_URI_SINK, uri, NULL);
  g_free (uri);
  if (stream->udpsink == NULL)
    goto no_sink_element;

  /* we keep this playing always */
  gst_element_set_locked_state (stream->udpsink, TRUE);
  gst_element_set_state (stream->udpsink, GST_STATE_PLAYING);

  /* no sync needed */
  g_object_set (G_OBJECT (stream->udpsink), "sync", FALSE, NULL);

  gst_object_ref (stream->udpsink);
  gst_bin_add (GST_BIN_CAST (src), stream->udpsink);

  stream->rtcppad = gst_element_get_pad (stream->udpsink, "sink");

  /* get session RTCP pad */
  name = g_strdup_printf ("send_rtcp_src_%d", stream->id);
  pad = gst_element_get_request_pad (src->session, name);
  g_free (name);

  /* and link */
  gst_pad_link (pad, stream->rtcppad);

  return TRUE;

  /* ERRORS */
no_sink_element:
  {
    GST_DEBUG_OBJECT (src, "no UDP sink element found");
    return FALSE;
  }
}

/* sets up all elements needed for streaming over the specified transport.
 * Does not yet expose the element pads, this will be done when there is actuall
 * dataflow detected, which might never happen when UDP is blocked in a
 * firewall, for example.
 */
static gboolean
gst_rtspsrc_stream_configure_transport (GstRTSPStream * stream,
    RTSPTransport * transport)
{
  GstRTSPSrc *src;
  GstPad *outpad = NULL;
  GstPadTemplate *template;
  gchar *name;
  GstStructure *s;
  const gchar *mime;
  RTSPResult res;

  src = stream->parent;

  GST_DEBUG_OBJECT (src, "configuring transport for stream %p", stream);

  s = gst_caps_get_structure (stream->caps, 0);

  /* get the proper mime type for this stream now */
  if ((res = rtsp_transport_get_mime (transport->trans, &mime)) < 0)
    goto unknown_transport;
  if (!mime)
    goto unknown_transport;

  /* configure the final mime type */
  GST_DEBUG_OBJECT (src, "setting mime to %s", mime);
  gst_structure_set_name (s, mime);

  /* try to get and configure a manager, channelpad[0-1] will be configured with
   * the pads for the manager, or NULL when no manager is needed. */
  if (!gst_rtspsrc_stream_configure_manager (src, stream, transport))
    goto no_manager;

  switch (transport->lower_transport) {
    case RTSP_LOWER_TRANS_TCP:
      if (!gst_rtspsrc_stream_configure_tcp (src, stream, transport, &outpad))
        goto transport_failed;
      break;
    case RTSP_LOWER_TRANS_UDP_MCAST:
      if (!gst_rtspsrc_stream_configure_mcast (src, stream, transport, &outpad))
        goto transport_failed;
      /* fallthrough, the rest is the same for UDP and MCAST */
    case RTSP_LOWER_TRANS_UDP:
      if (!gst_rtspsrc_stream_configure_udp (src, stream, transport, &outpad))
        goto transport_failed;
      /* configure udpsink back to the server for RTCP messages. */
      if (!gst_rtspsrc_stream_configure_udp_sink (src, stream, transport))
        goto transport_failed;
      break;
    default:
      goto unknown_transport;
  }

  if (outpad) {
    GST_DEBUG_OBJECT (src, "creating ghostpad");

    gst_pad_use_fixed_caps (outpad);

    /* create ghostpad, don't add just yet, this will be done when we activate
     * the stream. */
    name = g_strdup_printf ("stream%d", stream->id);
    template = gst_static_pad_template_get (&rtptemplate);
    stream->srcpad = gst_ghost_pad_new_from_template (name, outpad, template);
    gst_object_unref (template);
    g_free (name);

    gst_object_unref (outpad);
  }
  /* mark pad as ok */
  stream->last_ret = GST_FLOW_OK;

  return TRUE;

  /* ERRORS */
transport_failed:
  {
    GST_DEBUG_OBJECT (src, "failed to configure transport");
    return FALSE;
  }
unknown_transport:
  {
    GST_DEBUG_OBJECT (src, "unknown transport");
    return FALSE;
  }
no_manager:
  {
    GST_DEBUG_OBJECT (src, "cannot get a session manager");
    return FALSE;
  }
}

/* Adds the source pads of all configured streams to the element.
 * This code is performed when we detected dataflow.
 *
 * We detect dataflow from either the _loop function or with pad probes on the
 * udp sources.
 */
static gboolean
gst_rtspsrc_activate_streams (GstRTSPSrc * src)
{
  GList *walk;

  GST_DEBUG_OBJECT (src, "activating streams");

  for (walk = src->streams; walk; walk = g_list_next (walk)) {
    GstRTSPStream *stream = (GstRTSPStream *) walk->data;

    if (stream->udpsrc[0]) {
      /* remove timeout, we are streaming now and timeouts will be handled by
       * the session manager and jitter buffer */
      g_object_set (G_OBJECT (stream->udpsrc[0]), "timeout", (guint64) 0, NULL);
    }
    if (stream->srcpad) {
      gst_pad_set_active (stream->srcpad, TRUE);
      /* add the pad */
      if (!stream->added) {
        gst_element_add_pad (GST_ELEMENT_CAST (src), stream->srcpad);
        stream->added = TRUE;
      }
    }
  }

  /* unblock all pads */
  for (walk = src->streams; walk; walk = g_list_next (walk)) {
    GstRTSPStream *stream = (GstRTSPStream *) walk->data;

    if (stream->blockedpad) {
      gst_pad_set_blocked_async (stream->blockedpad, FALSE,
          (GstPadBlockCallback) pad_unblocked, src);
      stream->blockedpad = NULL;
    }
  }

  return TRUE;
}

static void
gst_rtspsrc_configure_caps (GstRTSPSrc * src)
{
  GList *walk;
  guint64 start, stop;
  gdouble play_speed, play_scale;

  GST_DEBUG_OBJECT (src, "configuring stream caps");

  start = src->segment.last_stop;
  stop = src->segment.duration;
  play_speed = src->segment.rate;
  play_scale = src->segment.applied_rate;

  for (walk = src->streams; walk; walk = g_list_next (walk)) {
    GstRTSPStream *stream = (GstRTSPStream *) walk->data;
    GstCaps *caps;

    if ((caps = stream->caps)) {
      caps = gst_caps_make_writable (caps);
      /* update caps */
      if (stream->timebase != -1)
        gst_caps_set_simple (caps, "clock-base", G_TYPE_UINT,
            (guint) stream->timebase, NULL);
      if (stream->seqbase != -1)
        gst_caps_set_simple (caps, "seqnum-base", G_TYPE_UINT,
            (guint) stream->seqbase, NULL);
      gst_caps_set_simple (caps, "npt-start", G_TYPE_UINT64, start, NULL);
      if (stop != -1)
        gst_caps_set_simple (caps, "npt-stop", G_TYPE_UINT64, stop, NULL);
      gst_caps_set_simple (caps, "play-speed", G_TYPE_DOUBLE, play_speed, NULL);
      gst_caps_set_simple (caps, "play-scale", G_TYPE_DOUBLE, play_scale, NULL);

      if (stream->caps != caps) {
        gst_caps_unref (stream->caps);
        stream->caps = caps;
      }
    }
  }
  if (src->session)
    g_signal_emit_by_name (src->session, "clear-pt-map", NULL);
}

static GstFlowReturn
gst_rtspsrc_combine_flows (GstRTSPSrc * src, GstRTSPStream * stream,
    GstFlowReturn ret)
{
  GList *streams;

  /* store the value */
  stream->last_ret = ret;

  /* if it's success we can return the value right away */
  if (GST_FLOW_IS_SUCCESS (ret))
    goto done;

  /* any other error that is not-linked can be returned right
   * away */
  if (ret != GST_FLOW_NOT_LINKED)
    goto done;

  /* only return NOT_LINKED if all other pads returned NOT_LINKED */
  for (streams = src->streams; streams; streams = g_list_next (streams)) {
    GstRTSPStream *ostream = (GstRTSPStream *) streams->data;

    ret = ostream->last_ret;
    /* some other return value (must be SUCCESS but we can return
     * other values as well) */
    if (ret != GST_FLOW_NOT_LINKED)
      goto done;
  }
  /* if we get here, all other pads were unlinked and we return
   * NOT_LINKED then */
done:
  return ret;
}

static void
gst_rtspsrc_push_event (GstRTSPSrc * src, GstEvent * event)
{
  GList *streams;

  for (streams = src->streams; streams; streams = g_list_next (streams)) {
    GstRTSPStream *ostream = (GstRTSPStream *) streams->data;

    /* only streams that have a connection to the outside world */
    if (ostream->srcpad == NULL)
      continue;

    if (ostream->channelpad[0]) {
      gst_event_ref (event);
      if (GST_PAD_IS_SRC (ostream->channelpad[0]))
        gst_pad_push_event (ostream->channelpad[0], event);
      else
        gst_pad_send_event (ostream->channelpad[0], event);
    }

    if (ostream->channelpad[1]) {
      gst_event_ref (event);
      if (GST_PAD_IS_SRC (ostream->channelpad[1]))
        gst_pad_push_event (ostream->channelpad[1], event);
      else
        gst_pad_send_event (ostream->channelpad[1], event);
    }
  }
  gst_event_unref (event);
}

/* FIXME, handle server request, reply with OK, for now */
static RTSPResult
gst_rtspsrc_handle_request (GstRTSPSrc * src, RTSPMessage * request)
{
  RTSPMessage response = { 0 };
  RTSPResult res;

  GST_DEBUG_OBJECT (src, "got server request message");

  if (src->debug)
    rtsp_message_dump (request);

  res = rtsp_message_init_response (&response, RTSP_STS_OK, "OK", request);
  if (res < 0)
    goto send_error;

  GST_DEBUG_OBJECT (src, "replying with OK");

  if (src->debug)
    rtsp_message_dump (&response);

  if ((res = rtsp_connection_send (src->connection, &response, NULL)) < 0)
    goto send_error;

  return RTSP_OK;

  /* ERRORS */
send_error:
  {
    return res;
  }
}

/* send server keep-alive */
static RTSPResult
gst_rtspsrc_send_keep_alive (GstRTSPSrc * src)
{
  RTSPMessage request = { 0 };
  RTSPResult res;
  RTSPMethod method;

  GST_DEBUG_OBJECT (src, "creating server keep-alive");

  /* find a method to use for keep-alive */
  if (src->methods & RTSP_GET_PARAMETER)
    method = RTSP_GET_PARAMETER;
  else
    method = RTSP_OPTIONS;

  res = rtsp_message_init_request (&request, method, src->req_location);
  if (res < 0)
    goto send_error;

  if ((res = rtsp_connection_send (src->connection, &request, NULL)) < 0)
    goto send_error;

  rtsp_connection_reset_timeout (src->connection);
  rtsp_message_unset (&request);

  return RTSP_OK;

  /* ERRORS */
send_error:
  {
    gchar *str = rtsp_strresult (res);

    rtsp_message_unset (&request);
    GST_ELEMENT_WARNING (src, RESOURCE, WRITE, (NULL),
        ("Could not send keep-alive. (%s)", str));
    g_free (str);
    return res;
  }
}

static void
gst_rtspsrc_loop_interleaved (GstRTSPSrc * src)
{
  RTSPMessage message = { 0 };
  RTSPResult res;
  gint channel;
  GList *lstream;
  GstRTSPStream *stream;
  GstPad *outpad = NULL;
  guint8 *data;
  guint size;
  GstFlowReturn ret = GST_FLOW_OK;
  GstBuffer *buf;
  gboolean is_rtcp, have_data;

  have_data = FALSE;
  do {
    GTimeVal tv_timeout, *tv;

    /* get the next timeout interval */
    rtsp_connection_next_timeout (src->connection, &tv_timeout);

    /* see if the timeout period expired */
    if ((tv_timeout.tv_usec | tv_timeout.tv_usec) == 0) {
      GST_DEBUG_OBJECT (src, "timout, sending keep-alive");
      /* send keep-alive, ignore the result, a warning will be posted. */
      res = gst_rtspsrc_send_keep_alive (src);
    }

    if ((src->tcp_timeout.tv_sec | src->tcp_timeout.tv_usec))
      tv = &src->tcp_timeout;
    else
      tv = NULL;

    GST_DEBUG_OBJECT (src, "doing receive");

    res = rtsp_connection_receive (src->connection, &message, tv);

    switch (res) {
      case RTSP_OK:
        GST_DEBUG_OBJECT (src, "we received a server message");
        break;
      case RTSP_EINTR:
        /* we got interrupted, see what we have to do */
        GST_DEBUG_OBJECT (src, "we got interrupted, unset flushing");
        /* unset flushing so we can do something else */
        rtsp_connection_flush (src->connection, FALSE);
        goto interrupt;
      default:
        goto receive_error;
    }

    switch (message.type) {
      case RTSP_MESSAGE_REQUEST:
        /* server sends us a request message, handle it */
        if ((res = gst_rtspsrc_handle_request (src, &message)) < 0)
          goto handle_request_failed;
        break;
      case RTSP_MESSAGE_RESPONSE:
        /* we ignore response messages */
        GST_DEBUG_OBJECT (src, "ignoring response message");
        break;
      case RTSP_MESSAGE_DATA:
        GST_DEBUG_OBJECT (src, "got data message");
        have_data = TRUE;
        break;
      default:
        GST_WARNING_OBJECT (src, "ignoring unknown message type %d",
            message.type);
        break;
    }
  }
  while (!have_data);

  channel = message.type_data.data.channel;

  lstream = g_list_find_custom (src->streams, GINT_TO_POINTER (channel),
      (GCompareFunc) find_stream_by_channel);
  if (!lstream)
    goto unknown_stream;

  stream = (GstRTSPStream *) lstream->data;
  if (channel == stream->channel[0]) {
    outpad = stream->channelpad[0];
    is_rtcp = FALSE;
  } else if (channel == stream->channel[1]) {
    outpad = stream->channelpad[1];
    is_rtcp = TRUE;
  } else {
    is_rtcp = FALSE;
  }

  /* take a look at the body to figure out what we have */
  rtsp_message_get_body (&message, &data, &size);
  if (size < 2)
    goto invalid_length;

  /* channels are not correct on some servers, do extra check */
  if (data[1] >= 200 && data[1] <= 204) {
    /* hmm RTCP message switch to the RTCP pad of the same stream. */
    outpad = stream->channelpad[1];
    is_rtcp = TRUE;
  }

  /* we have no clue what this is, just ignore then. */
  if (outpad == NULL)
    goto unknown_stream;

  /* take the message body for further processing */
  rtsp_message_steal_body (&message, &data, &size);

  /* strip the trailing \0 */
  size -= 1;

  buf = gst_buffer_new ();
  GST_BUFFER_DATA (buf) = data;
  GST_BUFFER_MALLOCDATA (buf) = data;
  GST_BUFFER_SIZE (buf) = size;

  /* don't need message anymore */
  rtsp_message_unset (&message);

  GST_DEBUG_OBJECT (src, "pushing data of size %d on channel %d", size,
      channel);

  if (src->need_activate) {
    gst_rtspsrc_activate_streams (src);
    src->need_activate = FALSE;
  }

  /* chain to the peer pad */
  if (GST_PAD_IS_SINK (outpad))
    ret = gst_pad_chain (outpad, buf);
  else
    ret = gst_pad_push (outpad, buf);

  if (!is_rtcp) {
    /* combine all stream flows for the data transport */
    ret = gst_rtspsrc_combine_flows (src, stream, ret);
    if (ret != GST_FLOW_OK)
      goto need_pause;
  }
  return;

  /* ERRORS */
unknown_stream:
  {
    GST_DEBUG_OBJECT (src, "unknown stream on channel %d, ignored", channel);
    rtsp_message_unset (&message);
    return;
  }
interrupt:
  {
    GST_DEBUG_OBJECT (src, "we got interrupted");
    rtsp_message_unset (&message);
    ret = GST_FLOW_WRONG_STATE;
    goto need_pause;
  }
receive_error:
  {
    gchar *str = rtsp_strresult (res);

    GST_ELEMENT_ERROR (src, RESOURCE, READ, (NULL),
        ("Could not receive message. (%s)", str));
    g_free (str);

    rtsp_message_unset (&message);
    ret = GST_FLOW_UNEXPECTED;
    goto need_pause;
  }
handle_request_failed:
  {
    gchar *str = rtsp_strresult (res);

    GST_ELEMENT_ERROR (src, RESOURCE, WRITE, (NULL),
        ("Could not send message. (%s)", str));
    g_free (str);
    rtsp_message_unset (&message);
    ret = GST_FLOW_UNEXPECTED;
    goto need_pause;
  }
invalid_length:
  {
    GST_ELEMENT_WARNING (src, RESOURCE, READ, (NULL),
        ("Short message received."));
    rtsp_message_unset (&message);
    return;
  }
need_pause:
  {
    const gchar *reason = gst_flow_get_name (ret);

    GST_DEBUG_OBJECT (src, "pausing task, reason %s", reason);
    src->running = FALSE;
    gst_task_pause (src->task);
    if (GST_FLOW_IS_FATAL (ret) || ret == GST_FLOW_NOT_LINKED) {
      if (ret == GST_FLOW_UNEXPECTED) {
        /* perform EOS logic */
        if (src->segment.flags & GST_SEEK_FLAG_SEGMENT) {
          gst_element_post_message (GST_ELEMENT_CAST (src),
              gst_message_new_segment_done (GST_OBJECT_CAST (src),
                  src->segment.format, src->segment.last_stop));
        } else {
          gst_rtspsrc_push_event (src, gst_event_new_eos ());
        }
      } else {
        /* for fatal errors we post an error message, post the error
         * first so the app knows about the error first. */
        GST_ELEMENT_ERROR (src, STREAM, FAILED,
            ("Internal data flow error."),
            ("streaming task paused, reason %s (%d)", reason, ret));
        gst_rtspsrc_push_event (src, gst_event_new_eos ());
      }
    }
    return;
  }
}

static void
gst_rtspsrc_loop_udp (GstRTSPSrc * src)
{
  gboolean restart = FALSE;
  RTSPResult res;

  GST_OBJECT_LOCK (src);
  if (src->loop_cmd == CMD_STOP)
    goto stopping;

  while (src->loop_cmd == CMD_WAIT) {
    GST_OBJECT_UNLOCK (src);

    while (TRUE) {
      RTSPMessage message = { 0 };
      GTimeVal tv_timeout;

      /* get the next timeout interval */
      rtsp_connection_next_timeout (src->connection, &tv_timeout);

      GST_DEBUG_OBJECT (src, "doing receive with timeout %d seconds",
          (gint) tv_timeout.tv_sec);

      /* we should continue reading the TCP socket because the server might
       * send us requests. When the session timeout expires, we need to send a
       * keep-alive request to keep the session open. */
      res = rtsp_connection_receive (src->connection, &message, &tv_timeout);

      switch (res) {
        case RTSP_OK:
          GST_DEBUG_OBJECT (src, "we received a server message");
          break;
        case RTSP_EINTR:
          /* we got interrupted, see what we have to do */
          GST_DEBUG_OBJECT (src, "we got interrupted, unset flushing");
          /* unset flushing so we can do something else */
          rtsp_connection_flush (src->connection, FALSE);
          goto interrupt;
        case RTSP_ETIMEOUT:
          /* send keep-alive, ignore the result, a warning will be posted. */
          GST_DEBUG_OBJECT (src, "timout, sending keep-alive");
          res = gst_rtspsrc_send_keep_alive (src);
          continue;
        default:
          goto receive_error;
      }

      switch (message.type) {
        case RTSP_MESSAGE_REQUEST:
          /* server sends us a request message, handle it */
          if ((res = gst_rtspsrc_handle_request (src, &message)) < 0)
            goto handle_request_failed;
          break;
        case RTSP_MESSAGE_RESPONSE:
          /* we ignore response and data messages */
          GST_DEBUG_OBJECT (src, "ignoring response message");
          break;
        case RTSP_MESSAGE_DATA:
          /* we ignore response and data messages */
          GST_DEBUG_OBJECT (src, "ignoring data message");
          break;
        default:
          GST_WARNING_OBJECT (src, "ignoring unknown message type %d",
              message.type);
          break;
      }
    }
  interrupt:
    GST_OBJECT_LOCK (src);
    GST_DEBUG_OBJECT (src, "we have command %d", src->loop_cmd);
    if (src->loop_cmd == CMD_STOP)
      goto stopping;
  }
  if (src->loop_cmd == CMD_RECONNECT) {
    /* when we get here we have to reconnect using tcp */
    src->loop_cmd = CMD_WAIT;

    /* only restart when the pads were not yet activated, else we were
     * streaming over UDP */
    restart = src->need_activate;
  }
  GST_OBJECT_UNLOCK (src);

  /* no need to restart, we're done */
  if (!restart)
    goto done;

  /* We post a warning message now to inform the user
   * that nothing happened. It's most likely a firewall thing. */
  GST_ELEMENT_WARNING (src, RESOURCE, READ, (NULL),
      ("Could not receive any UDP packets for %.4f seconds, maybe your "
          "firewall is blocking it. Retrying using a TCP connection.",
          gst_guint64_to_gdouble (src->udp_timeout / 1000000)));
  /* we can try only TCP now */
  src->cur_protocols = RTSP_LOWER_TRANS_TCP;

  /* pause to prepare for a restart */
  gst_rtspsrc_pause (src);

  if (src->task) {
    /* stop task, we cannot join as this would deadlock */
    gst_task_stop (src->task);
    /* and free the task so that _close will not stop/join it again. */
    gst_object_unref (GST_OBJECT (src->task));
    src->task = NULL;
  }
  /* close and cleanup our state */
  gst_rtspsrc_close (src);

  /* see if we have TCP left to try */
  if (!(src->protocols & RTSP_LOWER_TRANS_TCP))
    goto no_protocols;

  /* open new connection using tcp */
  if (!gst_rtspsrc_open (src))
    goto open_failed;

  /* flush previous state */
  gst_element_post_message (GST_ELEMENT_CAST (src),
      gst_message_new_async_start (GST_OBJECT_CAST (src), TRUE));
  gst_element_post_message (GST_ELEMENT_CAST (src),
      gst_message_new_async_done (GST_OBJECT_CAST (src)));

  /* start playback */
  if (!gst_rtspsrc_play (src))
    goto play_failed;

done:
  return;

  /* ERRORS */
stopping:
  {
    GST_OBJECT_UNLOCK (src);
    src->running = FALSE;
    gst_task_pause (src->task);
    return;
  }
receive_error:
  {
    gchar *str = rtsp_strresult (res);

    GST_ELEMENT_WARNING (src, RESOURCE, READ, (NULL),
        ("Could not receive message. (%s)", str));
    g_free (str);
    /* don't bother continueing if we the connection was closed */
    if (res == RTSP_EEOF) {
      src->running = FALSE;
      gst_task_pause (src->task);
    }
    return;
  }
handle_request_failed:
  {
    gchar *str = rtsp_strresult (res);

    GST_ELEMENT_WARNING (src, RESOURCE, READ, (NULL),
        ("Could not handle server message. (%s)", str));
    g_free (str);
    return;
  }
no_protocols:
  {
    src->cur_protocols = 0;
    /* no transport possible, post an error and stop */
    GST_ELEMENT_ERROR (src, RESOURCE, READ, (NULL),
        ("Could not connect to server, no protocols left"));
    return;
  }
open_failed:
  {
    GST_DEBUG_OBJECT (src, "open failed");
    return;
  }
play_failed:
  {
    GST_DEBUG_OBJECT (src, "play failed");
    return;
  }
}

static void
gst_rtspsrc_loop_send_cmd (GstRTSPSrc * src, gint cmd, gboolean flush)
{
  GST_OBJECT_LOCK (src);
  src->loop_cmd = cmd;
  if (flush) {
    GST_DEBUG_OBJECT (src, "start flush");
    rtsp_connection_flush (src->connection, TRUE);
  }
  GST_OBJECT_UNLOCK (src);
}

static void
gst_rtspsrc_loop (GstRTSPSrc * src)
{
  if (src->interleaved)
    gst_rtspsrc_loop_interleaved (src);
  else
    gst_rtspsrc_loop_udp (src);
}

#ifndef GST_DISABLE_GST_DEBUG
const gchar *
rtsp_auth_method_to_string (RTSPAuthMethod method)
{
  gint index = 0;

  while (method != 0) {
    index++;
    method >>= 1;
  }
  switch (index) {
    case 0:
      return "None";
    case 1:
      return "Basic";
    case 2:
      return "Digest";
  }

  return "Unknown";
}
#endif

/* Parse a WWW-Authenticate Response header and determine the 
 * available authentication methods
 * FIXME: To implement digest or other auth types, we should extract
 * the authentication tokens that the server provided for each method
 * into an array of structures and give those to the connection object.
 *
 * This code should also cope with the fact that each WWW-Authenticate
 * header can contain multiple challenge methods + tokens 
 *
 * At the moment, we just do a minimal check for Basic auth and don't
 * even parse out the realm */
static void
gst_rtspsrc_parse_auth_hdr (gchar * hdr, RTSPAuthMethod * methods)
{
  gchar *start;

  g_return_if_fail (hdr != NULL);
  g_return_if_fail (methods != NULL);

  /* Skip whitespace at the start of the string */
  for (start = hdr; start[0] != '\0' && g_ascii_isspace (start[0]); start++);

  if (g_ascii_strncasecmp (start, "basic", 5) == 0)
    *methods |= RTSP_AUTH_BASIC;
}

/**
 * gst_rtspsrc_setup_auth:
 * @src: the rtsp source
 *
 * Configure a username and password and auth method on the 
 * connection object based on a response we received from the 
 * peer.
 *
 * Currently, this requires that a username and password were supplied
 * in the uri. In the future, they may be requested on demand by sending
 * a message up the bus.
 *
 * Returns: TRUE if authentication information could be set up correctly.
 */
static gboolean
gst_rtspsrc_setup_auth (GstRTSPSrc * src, RTSPMessage * response)
{
  gchar *user = NULL;
  gchar *pass = NULL;
  RTSPAuthMethod avail_methods = RTSP_AUTH_NONE;
  RTSPAuthMethod method;
  RTSPResult auth_result;
  gchar *hdr;

  /* Identify the available auth methods and see if any are supported */
  if (rtsp_message_get_header (response, RTSP_HDR_WWW_AUTHENTICATE, &hdr, 0) ==
      RTSP_OK) {
    gst_rtspsrc_parse_auth_hdr (hdr, &avail_methods);
  }

  if (avail_methods == RTSP_AUTH_NONE)
    goto no_auth_available;

  /* FIXME: For digest auth, if the response indicates that the session
   * data are stale, we just update them in the connection object and
   * return TRUE to retry the request */

  /* Do we have username and password available? */
  if (src->url != NULL && !src->tried_url_auth) {
    user = src->url->user;
    pass = src->url->passwd;
    src->tried_url_auth = TRUE;
    GST_DEBUG_OBJECT (src,
        "Attempting authentication using credentials from the URL");
  }

  /* FIXME: If the url didn't contain username and password or we tried them
   * already, request a username and passwd from the application via some kind
   * of credentials request message */

  /* If we don't have a username and passwd at this point, bail out. */
  if (user == NULL || pass == NULL)
    goto no_user_pass;

  /* Try to configure for each available authentication method, strongest to
   * weakest */
  for (method = RTSP_AUTH_MAX; method != RTSP_AUTH_NONE; method >>= 1) {
    /* Check if this method is available on the server */
    if ((method & avail_methods) == 0)
      continue;

    /* Pass the credentials to the connection to try on the next request */
    auth_result =
        rtsp_connection_set_auth (src->connection, method, user, pass);
    /* INVAL indicates an invalid username/passwd were supplied, so we'll just
     * ignore it and end up retrying later */
    if (auth_result == RTSP_OK || auth_result == RTSP_EINVAL) {
      GST_DEBUG_OBJECT (src, "Attempting %s authentication",
          rtsp_auth_method_to_string (method));
      break;
    }
  }

  if (method == RTSP_AUTH_NONE)
    goto no_auth_available;

  return TRUE;

no_auth_available:
  {
    /* Output an error indicating that we couldn't connect because there were
     * no supported authentication protocols */
    GST_ELEMENT_ERROR (src, RESOURCE, OPEN_READ, (NULL),
        ("No supported authentication protocol was found"));
    return FALSE;
  }
no_user_pass:
  {
    /* We don't fire an error message, we just return FALSE and let the
     * normal NOT_AUTHORIZED error be propagated */
    return FALSE;
  }
}

static RTSPResult
gst_rtspsrc_try_send (GstRTSPSrc * src, RTSPMessage * request,
    RTSPMessage * response, RTSPStatusCode * code)
{
  RTSPResult res;
  RTSPStatusCode thecode;
  gchar *content_base = NULL;
  GTimeVal *tv;

  if (src->extension && src->extension->before_send)
    src->extension->before_send (src->extension, request);

  GST_DEBUG_OBJECT (src, "sending message");

  if (src->debug)
    rtsp_message_dump (request);

  if ((src->tcp_timeout.tv_sec | src->tcp_timeout.tv_usec))
    tv = &src->tcp_timeout;
  else
    tv = NULL;

  if ((res = rtsp_connection_send (src->connection, request, tv)) < 0)
    goto send_error;

  rtsp_connection_reset_timeout (src->connection);

next:
  if ((res = rtsp_connection_receive (src->connection, response, tv)) < 0)
    goto receive_error;

  if (src->debug)
    rtsp_message_dump (response);

  switch (response->type) {
    case RTSP_MESSAGE_REQUEST:
      if ((res = gst_rtspsrc_handle_request (src, response)) < 0)
        goto handle_request_failed;
      goto next;
    case RTSP_MESSAGE_RESPONSE:
      /* ok, a response is good */
      GST_DEBUG_OBJECT (src, "received response message");
      break;
    default:
    case RTSP_MESSAGE_DATA:
      /* get next response */
      GST_DEBUG_OBJECT (src, "ignoring data response message");
      goto next;
  }

  thecode = response->type_data.response.code;

  GST_DEBUG_OBJECT (src, "got response message %d", thecode);

  /* if the caller wanted the result code, we store it. */
  if (code)
    *code = thecode;

  /* If the request didn't succeed, bail out before doing any more */
  if (thecode != RTSP_STS_OK)
    return RTSP_OK;

  /* store new content base if any */
  rtsp_message_get_header (response, RTSP_HDR_CONTENT_BASE, &content_base, 0);
  g_free (src->content_base);
  src->content_base = g_strdup (content_base);

  if (src->extension && src->extension->after_send)
    src->extension->after_send (src->extension, request, response);

  return RTSP_OK;

  /* ERRORS */
send_error:
  {
    gchar *str = rtsp_strresult (res);

    GST_ELEMENT_ERROR (src, RESOURCE, WRITE, (NULL),
        ("Could not send message. (%s)", str));
    g_free (str);
    return res;
  }
receive_error:
  {
    gchar *str = rtsp_strresult (res);

    GST_ELEMENT_ERROR (src, RESOURCE, READ, (NULL),
        ("Could not receive message. (%s)", str));
    g_free (str);
    return res;
  }
handle_request_failed:
  {
    /* ERROR was posted */
    return res;
  }
}

/**
 * gst_rtspsrc_send:
 * @src: the rtsp source
 * @request: must point to a valid request
 * @response: must point to an empty #RTSPMessage
 *
 * send @request and retrieve the response in @response. optionally @code can be
 * non-NULL in which case it will contain the status code of the response.
 *
 * If This function returns TRUE, @response will contain a valid response
 * message that should be cleaned with rtsp_message_unset() after usage. 
 *
 * If @code is NULL, this function will return FALSE (with an invalid @response
 * message) if the response code was not 200 (OK).
 *
 * If the attempt results in an authentication failure, then this will attempt
 * to retrieve authentication credentials via gst_rtspsrc_setup_auth and retry
 * the request.
 *
 * Returns: RTSP_OK if the processing was successful.
 */
RTSPResult
gst_rtspsrc_send (GstRTSPSrc * src, RTSPMessage * request,
    RTSPMessage * response, RTSPStatusCode * code)
{
  RTSPStatusCode int_code = RTSP_STS_OK;
  RTSPResult res;
  gboolean retry;
  RTSPMethod method;

  do {
    retry = FALSE;

    /* save method so we can disable it when the server complains */
    method = request->type_data.request.method;

    if ((res = gst_rtspsrc_try_send (src, request, response, &int_code)) < 0)
      goto error;

    if (int_code == RTSP_STS_UNAUTHORIZED) {
      if (gst_rtspsrc_setup_auth (src, response)) {
        /* Try the request/response again after configuring the auth info
         * and loop again */
        retry = TRUE;
      }
    }
  } while (retry == TRUE);

  /* If the user requested the code, let them handle errors, otherwise
   * post an error below */
  if (code != NULL)
    *code = int_code;
  else if (int_code != RTSP_STS_OK)
    goto error_response;

  return res;

  /* ERRORS */
error:
  {
    GST_DEBUG_OBJECT (src, "got error %d", res);
    return res;
  }
error_response:
  {
    res = RTSP_ERROR;

    switch (response->type_data.response.code) {
      case RTSP_STS_NOT_FOUND:
        GST_ELEMENT_ERROR (src, RESOURCE, NOT_FOUND, (NULL), ("%s",
                response->type_data.response.reason));
        break;
      case RTSP_STS_NOT_ACCEPTABLE:
      case RTSP_STS_NOT_IMPLEMENTED:
        GST_WARNING_OBJECT (src, "got NOT IMPLEMENTED, disable method %s",
            rtsp_method_as_text (method));
        src->methods &= ~method;
        res = RTSP_OK;
        break;
      default:
        GST_ELEMENT_ERROR (src, RESOURCE, READ, (NULL),
            ("Got error response: %d (%s).", response->type_data.response.code,
                response->type_data.response.reason));
        break;
    }
    /* we return FALSE so we should unset the response ourselves */
    rtsp_message_unset (response);
    return res;
  }
}

/* parse the response and collect all the supported methods. We need this
 * information so that we don't try to send an unsupported request to the
 * server.
 */
static gboolean
gst_rtspsrc_parse_methods (GstRTSPSrc * src, RTSPMessage * response)
{
  RTSPHeaderField field;
  gchar *respoptions;
  gchar **options;
  gint indx = 0;
  gint i;

  /* clear supported methods */
  src->methods = 0;

  /* try the Allow header first */
  field = RTSP_HDR_ALLOW;
  while (TRUE) {
    respoptions = NULL;
    rtsp_message_get_header (response, field, &respoptions, indx);
    if (indx == 0 && !respoptions) {
      /* if no Allow header was found then try the Public header... */
      field = RTSP_HDR_PUBLIC;
      rtsp_message_get_header (response, field, &respoptions, indx);
    }
    if (!respoptions)
      break;

    /* If we get here, the server gave a list of supported methods, parse
     * them here. The string is like:
     *
     * OPTIONS, DESCRIBE, ANNOUNCE, PLAY, SETUP, ...
     */
    options = g_strsplit (respoptions, ",", 0);

    for (i = 0; options[i]; i++) {
      gchar *stripped;
      gint method;

      stripped = g_strstrip (options[i]);
      method = rtsp_find_method (stripped);

      /* keep bitfield of supported methods */
      if (method != RTSP_INVALID)
        src->methods |= method;
    }
    g_strfreev (options);

    indx++;
  }

  if (src->methods == 0) {
    /* neither Allow nor Public are required, assume the server supports
     * DESCRIBE, SETUP, PLAY and PAUSE */
    GST_DEBUG_OBJECT (src, "could not get OPTIONS");
    src->methods = RTSP_DESCRIBE | RTSP_SETUP | RTSP_PLAY | RTSP_PAUSE;
  }

  /* we need describe and setup */
  if (!(src->methods & RTSP_DESCRIBE))
    goto no_describe;
  if (!(src->methods & RTSP_SETUP))
    goto no_setup;

  return TRUE;

  /* ERRORS */
no_describe:
  {
    GST_ELEMENT_ERROR (src, RESOURCE, OPEN_READ, (NULL),
        ("Server does not support DESCRIBE."));
    return FALSE;
  }
no_setup:
  {
    GST_ELEMENT_ERROR (src, RESOURCE, OPEN_READ, (NULL),
        ("Server does not support SETUP."));
    return FALSE;
  }
}

static RTSPResult
gst_rtspsrc_create_transports_string (GstRTSPSrc * src,
    RTSPLowerTrans protocols, gchar ** transports)
{
  gchar *result;
  RTSPResult res;

  *transports = NULL;
  if (src->extension && src->extension->get_transports)
    if ((res =
            src->extension->get_transports (src->extension, protocols,
                transports)) < 0)
      goto failed;

  /* extension listed transports, use those */
  if (*transports != NULL)
    return RTSP_OK;

  /* the default RTSP transports */
  result = g_strdup ("");
  if (protocols & RTSP_LOWER_TRANS_UDP) {
    gchar *new;

    GST_DEBUG_OBJECT (src, "adding UDP unicast");

    new =
        g_strconcat (result, "RTP/AVP/UDP;unicast;client_port=%%u1-%%u2", NULL);
    g_free (result);
    result = new;
  }
  if (protocols & RTSP_LOWER_TRANS_UDP_MCAST) {
    gchar *new;

    GST_DEBUG_OBJECT (src, "adding UDP multicast");

    /* we don't have to allocate any UDP ports yet, if the selected transport
     * turns out to be multicast we can create them and join the multicast
     * group indicated in the transport reply */
    new = g_strconcat (result, result[0] ? "," : "",
        "RTP/AVP/UDP;multicast", NULL);
    g_free (result);
    result = new;
  }
  if (protocols & RTSP_LOWER_TRANS_TCP) {
    gchar *new;

    GST_DEBUG_OBJECT (src, "adding TCP");

    new = g_strconcat (result, result[0] ? "," : "",
        "RTP/AVP/TCP;unicast;interleaved=%%i1-%%i2", NULL);
    g_free (result);
    result = new;
  }
  *transports = result;

  return RTSP_OK;

  /* ERRORS */
failed:
  {
    return res;
  }
}

static RTSPResult
gst_rtspsrc_prepare_transports (GstRTSPStream * stream, gchar ** transports)
{
  GstRTSPSrc *src;
  gint nr_udp, nr_int;
  gchar *next, *p;
  gint rtpport = 0, rtcpport = 0;
  GString *str;

  src = stream->parent;

  /* find number of placeholders first */
  if (strstr (*transports, "%%i2"))
    nr_int = 2;
  else if (strstr (*transports, "%%i1"))
    nr_int = 1;
  else
    nr_int = 0;

  if (strstr (*transports, "%%u2"))
    nr_udp = 2;
  else if (strstr (*transports, "%%u1"))
    nr_udp = 1;
  else
    nr_udp = 0;

  if (nr_udp == 0 && nr_int == 0)
    goto done;

  if (nr_udp > 0) {
    if (!gst_rtspsrc_alloc_udp_ports (stream, &rtpport, &rtcpport))
      goto failed;
  }

  str = g_string_new ("");
  p = *transports;
  while ((next = strstr (p, "%%"))) {
    g_string_append_len (str, p, next - p);
    if (next[2] == 'u') {
      if (next[3] == '1')
        g_string_append_printf (str, "%d", rtpport);
      else if (next[3] == '2')
        g_string_append_printf (str, "%d", rtcpport);
    }
    if (next[2] == 'i') {
      if (next[3] == '1')
        g_string_append_printf (str, "%d", src->free_channel);
      else if (next[3] == '2')
        g_string_append_printf (str, "%d", src->free_channel + 1);
    }

    p = next + 4;
  }

  g_free (*transports);
  *transports = g_string_free (str, FALSE);

done:
  return RTSP_OK;

  /* ERRORS */
failed:
  {
    return RTSP_ERROR;
  }
}

/* Perform the SETUP request for all the streams. 
 *
 * We ask the server for a specific transport, which initially includes all the
 * ones we can support (UDP/TCP/MULTICAST). For the UDP transport we allocate
 * two local UDP ports that we send to the server.
 *
 * Once the server replied with a transport, we configure the other streams
 * with the same transport.
 *
 * This function will also configure the stream for the selected transport,
 * which basically means creating the pipeline.
 */
static gboolean
gst_rtspsrc_setup_streams (GstRTSPSrc * src)
{
  GList *walk;
  RTSPResult res;
  RTSPMessage request = { 0 };
  RTSPMessage response = { 0 };
  GstRTSPStream *stream = NULL;
  RTSPLowerTrans protocols;

  /* we initially allow all configured lower transports. based on the URL
   * transports and the replies from the server we narrow them down. */
  protocols = src->url->transports & src->cur_protocols;

  if (protocols == 0)
    goto no_protocols;

  /* reset some state */
  src->free_channel = 0;
  src->interleaved = FALSE;

  for (walk = src->streams; walk; walk = g_list_next (walk)) {
    gchar *transports;
    RTSPStatusCode code;

    stream = (GstRTSPStream *) walk->data;

    /* see if we need to configure this stream */
    if (src->extension && src->extension->configure_stream) {
      if (!src->extension->configure_stream (src->extension, stream)) {
        GST_DEBUG_OBJECT (src, "skipping stream %p, disabled by extension",
            stream);
        stream->disabled = TRUE;
        continue;
      }
    }

    /* merge/overwrite global caps */
    if (stream->caps) {
      guint j, num;
      GstStructure *s;

      s = gst_caps_get_structure (stream->caps, 0);

      num = gst_structure_n_fields (src->props);
      for (j = 0; j < num; j++) {
        const gchar *name;
        const GValue *val;

        name = gst_structure_nth_field_name (src->props, j);
        val = gst_structure_get_value (src->props, name);
        gst_structure_set_value (s, name, val);

        GST_DEBUG_OBJECT (src, "copied %s", name);
      }
    }

    /* skip setup if we have no URL for it */
    if (stream->setup_url == NULL) {
      GST_DEBUG_OBJECT (src, "skipping stream %p, no setup", stream);
      continue;
    }

    GST_DEBUG_OBJECT (src, "doing setup of stream %p with %s", stream,
        stream->setup_url);

    /* create a string with all the transports */
    res = gst_rtspsrc_create_transports_string (src, protocols, &transports);
    if (res < 0)
      goto setup_transport_failed;

    /* replace placeholders with real values, this function will optionally
     * allocate UDP ports and other info needed to execute the setup request */
    res = gst_rtspsrc_prepare_transports (stream, &transports);
    if (res < 0)
      goto setup_transport_failed;

    /* create SETUP request */
    res = rtsp_message_init_request (&request, RTSP_SETUP, stream->setup_url);
    if (res < 0)
      goto create_request_failed;

    /* select transport, copy is made when adding to header so we can free it. */
    rtsp_message_add_header (&request, RTSP_HDR_TRANSPORT, transports);
    g_free (transports);

    /* handle the code ourselves */
    if ((res = gst_rtspsrc_send (src, &request, &response, &code) < 0))
      goto send_error;

    switch (code) {
      case RTSP_STS_OK:
        break;
      case RTSP_STS_UNSUPPORTED_TRANSPORT:
        rtsp_message_unset (&request);
        rtsp_message_unset (&response);
        /* cleanup of leftover transport and move to the next stream */
        gst_rtspsrc_stream_free_udp (stream);
        continue;
      default:
        goto send_error;
    }

    /* parse response transport */
    {
      gchar *resptrans = NULL;
      RTSPTransport transport = { 0 };

      rtsp_message_get_header (&response, RTSP_HDR_TRANSPORT, &resptrans, 0);
      if (!resptrans)
        goto no_transport;

      /* parse transport, go to next stream on parse error */
      if (rtsp_transport_parse (resptrans, &transport) != RTSP_OK)
        continue;

      /* update allowed transports for other streams. once the transport of
       * one stream has been determined, we make sure that all other streams
       * are configured in the same way */
      switch (transport.lower_transport) {
        case RTSP_LOWER_TRANS_TCP:
          GST_DEBUG_OBJECT (src, "stream %p as TCP interleaved", stream);
          protocols = RTSP_LOWER_TRANS_TCP;
          src->interleaved = TRUE;
          /* update free channels */
          src->free_channel =
              MAX (transport.interleaved.min, src->free_channel);
          src->free_channel =
              MAX (transport.interleaved.max, src->free_channel);
          src->free_channel++;
          break;
        case RTSP_LOWER_TRANS_UDP_MCAST:
          /* only allow multicast for other streams */
          GST_DEBUG_OBJECT (src, "stream %p as UDP multicast", stream);
          protocols = RTSP_LOWER_TRANS_UDP_MCAST;
          break;
        case RTSP_LOWER_TRANS_UDP:
          /* only allow unicast for other streams */
          GST_DEBUG_OBJECT (src, "stream %p as UDP unicast", stream);
          protocols = RTSP_LOWER_TRANS_UDP;
          break;
        default:
          GST_DEBUG_OBJECT (src, "stream %p unknown transport %d", stream,
              transport.lower_transport);
          break;
      }

      if (!stream->container || !src->interleaved) {
        /* now configure the stream with the selected transport */
        if (!gst_rtspsrc_stream_configure_transport (stream, &transport)) {
          GST_DEBUG_OBJECT (src,
              "could not configure stream %p transport, skipping stream",
              stream);
        }
      }
      /* clean up our transport struct */
      rtsp_transport_init (&transport);
    }
  }
  if (src->extension && src->extension->stream_select)
    src->extension->stream_select (src->extension);

  /* we need to activate the streams when we detect activity */
  src->need_activate = TRUE;

  return TRUE;

  /* ERRORS */
no_protocols:
  {
    /* no transport possible, post an error and stop */
    GST_ELEMENT_ERROR (src, RESOURCE, READ, (NULL),
        ("Could not connect to server, no protocols left"));
    return FALSE;
  }
create_request_failed:
  {
    gchar *str = rtsp_strresult (res);

    GST_ELEMENT_ERROR (src, LIBRARY, INIT, (NULL),
        ("Could not create request. (%s)", str));
    g_free (str);
    goto cleanup_error;
  }
setup_transport_failed:
  {
    GST_ELEMENT_ERROR (src, RESOURCE, SETTINGS, (NULL),
        ("Could not setup transport."));
    goto cleanup_error;
  }
send_error:
  {
    gchar *str = rtsp_strresult (res);

    GST_ELEMENT_ERROR (src, RESOURCE, WRITE, (NULL),
        ("Could not send message. (%s)", str));
    g_free (str);
    goto cleanup_error;
  }
no_transport:
  {
    GST_ELEMENT_ERROR (src, RESOURCE, SETTINGS, (NULL),
        ("Server did not select transport."));
    goto cleanup_error;
  }
cleanup_error:
  {
    rtsp_message_unset (&request);
    rtsp_message_unset (&response);
    return FALSE;
  }
}

static void
gst_rtspsrc_parse_range (GstRTSPSrc * src, const gchar * range)
{
  RTSPTimeRange *therange;

  if (rtsp_range_parse (range, &therange) == RTSP_OK) {
    gint64 seconds;

    GST_DEBUG_OBJECT (src, "range: '%s', min %f - max %f ",
        GST_STR_NULL (range), therange->min.seconds, therange->max.seconds);

    if (therange->min.type == RTSP_TIME_NOW)
      seconds = 0;
    else if (therange->min.type == RTSP_TIME_END)
      seconds = 0;
    else
      seconds = therange->min.seconds * GST_SECOND;

    gst_segment_set_last_stop (&src->segment, GST_FORMAT_TIME, seconds);

    if (therange->max.type == RTSP_TIME_NOW)
      seconds = -1;
    else if (therange->max.type == RTSP_TIME_END)
      seconds = -1;
    else
      seconds = therange->max.seconds * GST_SECOND;

    gst_segment_set_duration (&src->segment, GST_FORMAT_TIME, seconds);
  }
}

static gboolean
gst_rtspsrc_open (GstRTSPSrc * src)
{
  RTSPResult res;
  RTSPMessage request = { 0 };
  RTSPMessage response = { 0 };
  guint8 *data;
  guint size;
  gint i, n_streams;
  SDPMessage sdp = { 0 };
  GstRTSPStream *stream = NULL;
  gchar *respcont = NULL;

  GST_RTSP_STATE_LOCK (src);

  /* reset our state */
  gst_segment_init (&src->segment, GST_FORMAT_TIME);
  src->position = 0.0;

  /* can't continue without a valid url */
  if (G_UNLIKELY (src->url == NULL))
    goto no_url;
  src->tried_url_auth = FALSE;

  /* create connection */
  GST_DEBUG_OBJECT (src, "creating connection (%s)...", src->req_location);
  if ((res = rtsp_connection_create (src->url, &src->connection)) < 0)
    goto could_not_create;

  /* connect */
  GST_DEBUG_OBJECT (src, "connecting (%s)...", src->req_location);
  if ((res = rtsp_connection_connect (src->connection, &src->tcp_timeout)) < 0)
    goto could_not_connect;

  /* create OPTIONS */
  GST_DEBUG_OBJECT (src, "create options...");
  res = rtsp_message_init_request (&request, RTSP_OPTIONS, src->req_location);
  if (res < 0)
    goto create_request_failed;

  /* send OPTIONS */
  GST_DEBUG_OBJECT (src, "send options...");
  if ((res = gst_rtspsrc_send (src, &request, &response, NULL)) < 0)
    goto send_error;

  /* parse OPTIONS */
  if (!gst_rtspsrc_parse_methods (src, &response))
    goto methods_error;

  /* create DESCRIBE */
  GST_DEBUG_OBJECT (src, "create describe...");
  res = rtsp_message_init_request (&request, RTSP_DESCRIBE, src->req_location);
  if (res < 0)
    goto create_request_failed;

  /* we only accept SDP for now */
  rtsp_message_add_header (&request, RTSP_HDR_ACCEPT, "application/sdp");

  /* prepare global stream caps properties */
  if (src->props)
    gst_structure_remove_all_fields (src->props);
  else
    src->props = gst_structure_empty_new ("RTSP Properties");

  /* send DESCRIBE */
  GST_DEBUG_OBJECT (src, "send describe...");
  if ((res = gst_rtspsrc_send (src, &request, &response, NULL)) < 0)
    goto send_error;

  /* check if reply is SDP */
  rtsp_message_get_header (&response, RTSP_HDR_CONTENT_TYPE, &respcont, 0);
  /* could not be set but since the request returned OK, we assume it
   * was SDP, else check it. */
  if (respcont) {
    if (!g_ascii_strcasecmp (respcont, "application/sdp") == 0)
      goto wrong_content_type;
  }

  /* get message body and parse as SDP */
  rtsp_message_get_body (&response, &data, &size);

  GST_DEBUG_OBJECT (src, "parse SDP...");
  sdp_message_init (&sdp);
  sdp_message_parse_buffer (data, size, &sdp);

  if (src->debug)
    sdp_message_dump (&sdp);

  if (src->extension && src->extension->parse_sdp)
    src->extension->parse_sdp (src->extension, &sdp);

  /* parse range for duration reporting. */
  {
    gchar *range;

    range = sdp_message_get_attribute_val (&sdp, "range");
    if (range)
      gst_rtspsrc_parse_range (src, range);
  }

  /* create streams */
  n_streams = sdp_message_medias_len (&sdp);
  for (i = 0; i < n_streams; i++) {
    stream = gst_rtspsrc_create_stream (src, &sdp, i);
  }

  src->state = RTSP_STATE_INIT;

  /* setup streams */
  if (!gst_rtspsrc_setup_streams (src))
    goto setup_failed;

  src->state = RTSP_STATE_READY;
  GST_RTSP_STATE_UNLOCK (src);

  /* clean up any messages */
  rtsp_message_unset (&request);
  rtsp_message_unset (&response);

  return TRUE;

  /* ERRORS */
no_url:
  {
    GST_ELEMENT_ERROR (src, RESOURCE, NOT_FOUND, (NULL),
        ("No valid RTSP URL was provided"));
    goto cleanup_error;
  }
could_not_create:
  {
    gchar *str = rtsp_strresult (res);

    GST_ELEMENT_ERROR (src, RESOURCE, OPEN_READ_WRITE, (NULL),
        ("Could not create connection. (%s)", str));
    g_free (str);
    goto cleanup_error;
  }
could_not_connect:
  {
    gchar *str = rtsp_strresult (res);

    GST_ELEMENT_ERROR (src, RESOURCE, OPEN_READ_WRITE, (NULL),
        ("Could not connect to server. (%s)", str));
    g_free (str);
    goto cleanup_error;
  }
create_request_failed:
  {
    gchar *str = rtsp_strresult (res);

    GST_ELEMENT_ERROR (src, LIBRARY, INIT, (NULL),
        ("Could not create request. (%s)", str));
    g_free (str);
    goto cleanup_error;
  }
send_error:
  {
    /* Don't post a message - the rtsp_send method will have
     * taken care of it because we passed NULL for the response code */
    goto cleanup_error;
  }
methods_error:
  {
    /* error was posted */
    goto cleanup_error;
  }
wrong_content_type:
  {
    GST_ELEMENT_ERROR (src, RESOURCE, SETTINGS, (NULL),
        ("Server does not support SDP, got %s.", respcont));
    goto cleanup_error;
  }
setup_failed:
  {
    /* error was posted */
    goto cleanup_error;
  }
cleanup_error:
  {
    GST_RTSP_STATE_UNLOCK (src);
    rtsp_message_unset (&request);
    rtsp_message_unset (&response);
    return FALSE;
  }
}

#if 0
static gboolean
gst_rtspsrc_async_open (GstRTSPSrc * src)
{
  GError *error = NULL;
  gboolean res = TRUE;

  src->thread =
      g_thread_create ((GThreadFunc) gst_rtspsrc_open, src, TRUE, &error);
  if (error != NULL) {
    GST_ELEMENT_ERROR (src, RESOURCE, INIT, (NULL),
        ("Could not start async thread (%s).", error->message));
  }
  return res;
}
#endif

static gboolean
gst_rtspsrc_close (GstRTSPSrc * src)
{
  RTSPMessage request = { 0 };
  RTSPMessage response = { 0 };
  RTSPResult res;

  GST_DEBUG_OBJECT (src, "TEARDOWN...");

  GST_RTSP_STATE_LOCK (src);

  gst_rtspsrc_loop_send_cmd (src, CMD_STOP, TRUE);

  /* stop task if any */
  if (src->task) {
    gst_task_stop (src->task);

    /* make sure it is not running */
    GST_RTSP_STREAM_LOCK (src);
    GST_RTSP_STREAM_UNLOCK (src);

    /* no wait for the task to finish */
    gst_task_join (src->task);

    /* and free the task */
    gst_object_unref (GST_OBJECT (src->task));
    src->task = NULL;
  }

  GST_DEBUG_OBJECT (src, "stop flush");
  rtsp_connection_flush (src->connection, FALSE);

  if (src->methods & RTSP_PLAY) {
    /* do TEARDOWN */
    res =
        rtsp_message_init_request (&request, RTSP_TEARDOWN, src->req_location);
    if (res < 0)
      goto create_request_failed;

    if ((res = gst_rtspsrc_send (src, &request, &response, NULL)) < 0)
      goto send_error;

    /* FIXME, parse result? */
    rtsp_message_unset (&request);
    rtsp_message_unset (&response);
  }

  /* close connection */
  GST_DEBUG_OBJECT (src, "closing connection...");
  if ((res = rtsp_connection_close (src->connection)) < 0)
    goto close_failed;

  /* free connection */
  rtsp_connection_free (src->connection);
  src->connection = NULL;

  /* cleanup */
  gst_rtspsrc_cleanup (src);

  src->state = RTSP_STATE_INVALID;
  GST_RTSP_STATE_UNLOCK (src);

  return TRUE;

  /* ERRORS */
create_request_failed:
  {
    GST_RTSP_STATE_UNLOCK (src);
    GST_ELEMENT_ERROR (src, LIBRARY, INIT, (NULL),
        ("Could not create request."));
    return FALSE;
  }
send_error:
  {
    GST_RTSP_STATE_UNLOCK (src);
    rtsp_message_unset (&request);
    GST_ELEMENT_ERROR (src, RESOURCE, WRITE, (NULL),
        ("Could not send message."));
    return FALSE;
  }
close_failed:
  {
    GST_RTSP_STATE_UNLOCK (src);
    GST_ELEMENT_ERROR (src, RESOURCE, CLOSE, (NULL), ("Close failed."));
    return FALSE;
  }
}

/* RTP-Info is of the format:
 *
 * url=<URL>;[seq=<seqbase>;rtptime=<timebase>] [, url=...]
 *
 * rtptime corresponds to the timestamp for the NPT time given in the header 
 * seqbase corresponds to the next sequence number we received. This number
 * indicates the first seqnum after the seek and should be used to discard
 * packets that are from before the seek.
 */
static gboolean
gst_rtspsrc_parse_rtpinfo (GstRTSPSrc * src, gchar * rtpinfo)
{
  gchar **infos;
  gint i, j;

  GST_DEBUG_OBJECT (src, "parsing RTP-Info %s", rtpinfo);

  infos = g_strsplit (rtpinfo, ",", 0);
  for (i = 0; infos[i]; i++) {
    gchar **fields;
    GstRTSPStream *stream;
    gint32 seqbase;
    gint64 timebase;

    GST_DEBUG_OBJECT (src, "parsing info %s", infos[i]);

    /* init values, types of seqbase and timebase are bigger than needed so we can
     * store -1 as uninitialized values */
    stream = NULL;
    seqbase = -1;
    timebase = -1;

    /* parse url, find stream for url.
     * parse seq and rtptime. The seq number should be configured in the rtp
     * depayloader or session manager to detect gaps. Same for the rtptime, it
     * should be used to create an initial time newsegment. */
    fields = g_strsplit (infos[i], ";", 0);
    for (j = 0; fields[j]; j++) {
      GST_DEBUG_OBJECT (src, "parsing field %s", fields[j]);
      /* remove leading whitespace */
      fields[j] = g_strchug (fields[j]);
      if (g_str_has_prefix (fields[j], "url=")) {
        GList *lstream;

        /* get the url and the stream */
        lstream = g_list_find_custom (src->streams, (fields[j] + 4),
            (GCompareFunc) find_stream_by_setup);
        if (lstream)
          stream = (GstRTSPStream *) lstream->data;
      } else if (g_str_has_prefix (fields[j], "seq=")) {
        seqbase = atoi (fields[j] + 4);
      } else if (g_str_has_prefix (fields[j], "rtptime=")) {
        timebase = atol (fields[j] + 8);
      }
    }
    g_strfreev (fields);
    /* now we need to store the values for the caps of the stream */
    if (stream != NULL) {
      GST_DEBUG_OBJECT (src,
          "found stream %p, setting: seqbase %d, timebase %" G_GINT64_FORMAT,
          stream, seqbase, timebase);

      /* we have a stream, configure detected params */
      stream->seqbase = seqbase;
      stream->timebase = timebase;
    }
  }
  g_strfreev (infos);

  return TRUE;
}

static gboolean
gst_rtspsrc_play (GstRTSPSrc * src)
{
  RTSPMessage request = { 0 };
  RTSPMessage response = { 0 };
  RTSPResult res;
  gchar *rtpinfo, *range;

  GST_RTSP_STATE_LOCK (src);

  GST_DEBUG_OBJECT (src, "PLAY...");

  if (!(src->methods & RTSP_PLAY))
    goto not_supported;

  if (src->state == RTSP_STATE_PLAYING)
    goto was_playing;

  /* do play */
  res = rtsp_message_init_request (&request, RTSP_PLAY, src->req_location);
  if (res < 0)
    goto create_request_failed;

  if (src->position == 0.0)
    range = g_strdup_printf ("npt=0-");
  else
    range = g_strdup_printf ("npt=%f-", src->position);

  rtsp_message_add_header (&request, RTSP_HDR_RANGE, range);
  g_free (range);

  if ((res = gst_rtspsrc_send (src, &request, &response, NULL)) < 0)
    goto send_error;

  rtsp_message_unset (&request);

  /* parse RTP npt field. This is the current position in the stream (Normal
   * Play Time) and should be put in the NEWSEGMENT position field. */
  if (rtsp_message_get_header (&response, RTSP_HDR_RANGE, &range, 0) == RTSP_OK)
    gst_rtspsrc_parse_range (src, range);

  /* parse the RTP-Info header field (if ANY) to get the base seqnum and timestamp
   * for the RTP packets. If this is not present, we assume all starts from 0... 
   * This is info for the RTP session manager that we pass to it in caps. */
  if (rtsp_message_get_header (&response, RTSP_HDR_RTP_INFO,
          &rtpinfo, 0) == RTSP_OK)
    gst_rtspsrc_parse_rtpinfo (src, rtpinfo);

  rtsp_message_unset (&response);

  /* configure the caps of the streams after we parsed all headers. */
  gst_rtspsrc_configure_caps (src);

  /* for interleaved transport, we receive the data on the RTSP connection
   * instead of UDP. We start a task to select and read from that connection.
   * For UDP we start the task as well to look for server info and UDP timeouts. */
  if (src->task == NULL) {
    src->task = gst_task_create ((GstTaskFunction) gst_rtspsrc_loop, src);
    gst_task_set_lock (src->task, GST_RTSP_STREAM_GET_LOCK (src));
  }
  src->running = TRUE;
  src->state = RTSP_STATE_PLAYING;
  gst_rtspsrc_loop_send_cmd (src, CMD_WAIT, FALSE);
  gst_task_start (src->task);

done:
  GST_RTSP_STATE_UNLOCK (src);

  return TRUE;

  /* ERRORS */
not_supported:
  {
    GST_DEBUG_OBJECT (src, "PLAY is not supported");
    goto done;
  }
was_playing:
  {
    GST_DEBUG_OBJECT (src, "we were already PLAYING");
    goto done;
  }
create_request_failed:
  {
    GST_RTSP_STATE_UNLOCK (src);
    GST_ELEMENT_ERROR (src, LIBRARY, INIT, (NULL),
        ("Could not create request."));
    return FALSE;
  }
send_error:
  {
    GST_RTSP_STATE_UNLOCK (src);
    rtsp_message_unset (&request);
    GST_ELEMENT_ERROR (src, RESOURCE, WRITE, (NULL),
        ("Could not send message."));
    return FALSE;
  }
}

static gboolean
gst_rtspsrc_pause (GstRTSPSrc * src)
{
  RTSPMessage request = { 0 };
  RTSPMessage response = { 0 };
  RTSPResult res;

  GST_RTSP_STATE_LOCK (src);

  GST_DEBUG_OBJECT (src, "PAUSE...");

  if (!(src->methods & RTSP_PAUSE))
    goto not_supported;

  if (src->state == RTSP_STATE_READY)
    goto was_paused;

  /* wait for streaming to finish */
  GST_RTSP_STREAM_LOCK (src);
  GST_RTSP_STREAM_UNLOCK (src);

  rtsp_connection_flush (src->connection, FALSE);

  /* do pause */
  res = rtsp_message_init_request (&request, RTSP_PAUSE, src->req_location);
  if (res < 0)
    goto create_request_failed;

  if ((res = gst_rtspsrc_send (src, &request, &response, NULL)) < 0)
    goto send_error;

  rtsp_message_unset (&request);
  rtsp_message_unset (&response);

  src->state = RTSP_STATE_READY;

done:
  GST_RTSP_STATE_UNLOCK (src);

  return TRUE;

  /* ERRORS */
not_supported:
  {
    GST_DEBUG_OBJECT (src, "PAUSE is not supported");
    goto done;
  }
was_paused:
  {
    GST_DEBUG_OBJECT (src, "we were already PAUSED");
    goto done;
  }
create_request_failed:
  {
    GST_RTSP_STATE_UNLOCK (src);
    GST_ELEMENT_ERROR (src, LIBRARY, INIT, (NULL),
        ("Could not create request."));
    return FALSE;
  }
send_error:
  {
    GST_RTSP_STATE_UNLOCK (src);
    rtsp_message_unset (&request);
    GST_ELEMENT_ERROR (src, RESOURCE, WRITE, (NULL),
        ("Could not send message."));
    return FALSE;
  }
}

static void
gst_rtspsrc_handle_message (GstBin * bin, GstMessage * message)
{
  GstRTSPSrc *rtspsrc;

  rtspsrc = GST_RTSPSRC (bin);

  switch (GST_MESSAGE_TYPE (message)) {
    case GST_MESSAGE_ELEMENT:
    {
      const GstStructure *s = gst_message_get_structure (message);

      if (gst_structure_has_name (s, "GstUDPSrcTimeout")) {
        gboolean ignore_timeout;

        GST_DEBUG_OBJECT (bin, "timeout on UDP port");

        GST_OBJECT_LOCK (rtspsrc);
        ignore_timeout = rtspsrc->ignore_timeout;
        rtspsrc->ignore_timeout = TRUE;
        GST_OBJECT_UNLOCK (rtspsrc);

        /* we only act on the first udp timeout message, others are irrelevant
         * and can be ignored. */
        if (ignore_timeout)
          gst_message_unref (message);
        else
          gst_rtspsrc_loop_send_cmd (rtspsrc, CMD_RECONNECT, TRUE);
        return;
      }
      GST_BIN_CLASS (parent_class)->handle_message (bin, message);
      break;
    }
    case GST_MESSAGE_ERROR:
    {
      GstObject *udpsrc;
      GList *lstream;
      GstRTSPStream *stream;
      GstFlowReturn ret;

      udpsrc = GST_MESSAGE_SRC (message);

      GST_DEBUG_OBJECT (rtspsrc, "got error from %s",
          GST_ELEMENT_NAME (udpsrc));

      lstream = g_list_find_custom (rtspsrc->streams, udpsrc,
          (GCompareFunc) find_stream_by_udpsrc);
      if (!lstream)
        goto forward;

      stream = (GstRTSPStream *) lstream->data;

      /* we ignore the RTCP udpsrc */
      if (stream->udpsrc[1] == GST_ELEMENT_CAST (udpsrc))
        goto done;

      /* if we get error messages from the udp sources, that's not a problem as
       * long as not all of them error out. We also don't really know what the
       * problem is, the message does not give enough detail... */
      ret = gst_rtspsrc_combine_flows (rtspsrc, stream, GST_FLOW_NOT_LINKED);
      GST_DEBUG_OBJECT (rtspsrc, "combined flows: %s", gst_flow_get_name (ret));
      if (ret != GST_FLOW_OK)
        goto forward;

    done:
      gst_message_unref (message);
      break;

    forward:
      /* fatal but not our message, forward */
      GST_BIN_CLASS (parent_class)->handle_message (bin, message);
      break;
    }
    case GST_MESSAGE_ASYNC_START:
    case GST_MESSAGE_ASYNC_DONE:
      /* ignore messages from our internal sinks */
      gst_message_unref (message);
      break;
    default:
    {
      GST_BIN_CLASS (parent_class)->handle_message (bin, message);
      break;
    }
  }
}

static GstStateChangeReturn
gst_rtspsrc_change_state (GstElement * element, GstStateChange transition)
{
  GstRTSPSrc *rtspsrc;
  GstStateChangeReturn ret;

  rtspsrc = GST_RTSPSRC (element);

  switch (transition) {
    case GST_STATE_CHANGE_NULL_TO_READY:
      break;
    case GST_STATE_CHANGE_READY_TO_PAUSED:
      rtspsrc->cur_protocols = rtspsrc->protocols;
      /* first attempt, don't ignore timeouts */
      rtspsrc->ignore_timeout = FALSE;
      if (!gst_rtspsrc_open (rtspsrc))
        goto open_failed;
      break;
    case GST_STATE_CHANGE_PAUSED_TO_PLAYING:
      GST_DEBUG_OBJECT (rtspsrc, "stop flush");
      rtsp_connection_flush (rtspsrc->connection, FALSE);
      /* FIXME, the server might send UDP packets before we activate the UDP
       * ports */
      gst_rtspsrc_play (rtspsrc);
      break;
    case GST_STATE_CHANGE_PLAYING_TO_PAUSED:
    case GST_STATE_CHANGE_PAUSED_TO_READY:
      GST_DEBUG_OBJECT (rtspsrc, "start flush");
      gst_rtspsrc_loop_send_cmd (rtspsrc, CMD_STOP, TRUE);
      break;
    default:
      break;
  }

  ret = GST_ELEMENT_CLASS (parent_class)->change_state (element, transition);
  if (ret == GST_STATE_CHANGE_FAILURE)
    goto done;

  switch (transition) {
    case GST_STATE_CHANGE_READY_TO_PAUSED:
      ret = GST_STATE_CHANGE_NO_PREROLL;
      break;
    case GST_STATE_CHANGE_PLAYING_TO_PAUSED:
      gst_rtspsrc_pause (rtspsrc);
      ret = GST_STATE_CHANGE_NO_PREROLL;
      break;
    case GST_STATE_CHANGE_PAUSED_TO_READY:
      gst_rtspsrc_close (rtspsrc);
      break;
    case GST_STATE_CHANGE_READY_TO_NULL:
      break;
    default:
      break;
  }

done:
  return ret;

open_failed:
  {
    return GST_STATE_CHANGE_FAILURE;
  }
}

/*** GSTURIHANDLER INTERFACE *************************************************/

static GstURIType
gst_rtspsrc_uri_get_type (void)
{
  return GST_URI_SRC;
}
static gchar **
gst_rtspsrc_uri_get_protocols (void)
{
  static gchar *protocols[] = { "rtsp", "rtspu", "rtspt", NULL };

  return protocols;
}

static const gchar *
gst_rtspsrc_uri_get_uri (GstURIHandler * handler)
{
  GstRTSPSrc *src = GST_RTSPSRC (handler);

  /* should not dup */
  return src->location;
}

static gboolean
gst_rtspsrc_uri_set_uri (GstURIHandler * handler, const gchar * uri)
{
  GstRTSPSrc *src;
  RTSPResult res;
  RTSPUrl *newurl;

  src = GST_RTSPSRC (handler);

  /* same URI, we're fine */
  if (src->location && uri && !strcmp (uri, src->location))
    goto was_ok;

  /* try to parse */
  if ((res = rtsp_url_parse (uri, &newurl)) < 0)
    goto parse_error;

  /* if worked, free previous and store new url object along with the original
   * location. */
  rtsp_url_free (src->url);
  src->url = newurl;
  g_free (src->location);
  g_free (src->req_location);
  src->location = g_strdup (uri);
  src->req_location = rtsp_url_get_request_uri (src->url);

  GST_DEBUG_OBJECT (src, "set uri: %s", GST_STR_NULL (uri));
  GST_DEBUG_OBJECT (src, "request uri is: %s",
      GST_STR_NULL (src->req_location));

  return TRUE;

  /* Special cases */
was_ok:
  {
    GST_DEBUG_OBJECT (src, "URI was ok: '%s'", GST_STR_NULL (uri));
    return TRUE;
  }
parse_error:
  {
    GST_ERROR_OBJECT (src, "Not a valid RTSP url '%s' (%d)",
        GST_STR_NULL (uri), res);
    return FALSE;
  }
}

static void
gst_rtspsrc_uri_handler_init (gpointer g_iface, gpointer iface_data)
{
  GstURIHandlerInterface *iface = (GstURIHandlerInterface *) g_iface;

  iface->get_type = gst_rtspsrc_uri_get_type;
  iface->get_protocols = gst_rtspsrc_uri_get_protocols;
  iface->get_uri = gst_rtspsrc_uri_get_uri;
  iface->set_uri = gst_rtspsrc_uri_set_uri;
}
