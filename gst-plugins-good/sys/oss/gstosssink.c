/* GStreamer
 * Copyright (C) 1999,2000 Erik Walthinsen <omega@cse.ogi.edu>
 *                    2000 Wim Taymans <wim.taymans@chello.be>
 *
 * gstosssink.c: 
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


#include <sys/types.h>
#include <sys/stat.h>
#include <sys/ioctl.h>
#include <fcntl.h>
#include <sys/soundcard.h>
#include <unistd.h>
#include <errno.h>

#include <gstosssink.h>

static GstElementDetails gst_osssink_details = {  
  "Audio Sink (OSS)",
  "Sink/Audio",
  "Output to a sound card via OSS",
  VERSION,
  "Erik Walthinsen <omega@cse.ogi.edu>, "
  "Wim Taymans <wim.taymans@chello.be>",
  "(C) 1999",
};

static void 			gst_osssink_class_init		(GstOssSinkClass *klass);
static void 			gst_osssink_init		(GstOssSink *osssink);
static void 			gst_osssink_finalize		(GObject *object);

static gboolean 		gst_osssink_open_audio		(GstOssSink *sink);
static void 			gst_osssink_close_audio		(GstOssSink *sink);
static gboolean 		gst_osssink_sync_parms 		(GstOssSink *osssink);
static GstElementStateReturn 	gst_osssink_change_state	(GstElement *element);
static void		 	gst_osssink_set_clock		(GstElement *element, GstClock *clock);
static GstClock* 		gst_osssink_get_clock 		(GstElement *element);
static GstClockTime 		gst_osssink_get_time 		(GstClock *clock, gpointer data);

static gboolean 		gst_osssink_convert 		(GstPad *pad, GstFormat src_format, gint64 src_value,
	            						 GstFormat *dest_format, gint64 *dest_value);
static gboolean 		gst_osssink_query 		(GstElement *element, GstPadQueryType type, 
								 GstFormat *format, gint64 *value);
static gboolean 		gst_osssink_sink_query 		(GstPad *pad, GstPadQueryType type,
								 GstFormat *format, gint64 *value);

static GstPadConnectReturn	gst_osssink_sinkconnect		(GstPad *pad, GstCaps *caps);

static void 			gst_osssink_set_property	(GObject *object, guint prop_id, const GValue *value, 
		  						 GParamSpec *pspec);
static void 			gst_osssink_get_property	(GObject *object, guint prop_id, GValue *value, 
								 GParamSpec *pspec);

static void 			gst_osssink_chain		(GstPad *pad,GstBuffer *buf);

/* OssSink signals and args */
enum {
  SIGNAL_HANDOFF,
  LAST_SIGNAL
};

enum {
  ARG_0,
  ARG_DEVICE,
  ARG_MUTE,
  ARG_FORMAT,
  ARG_CHANNELS,
  ARG_FREQUENCY,
  ARG_FRAGMENT,
  ARG_BUFFER_SIZE,
  ARG_SYNC
  /* FILL ME */
};

GST_PAD_TEMPLATE_FACTORY (osssink_sink_factory,
  "sink",
  GST_PAD_SINK,
  GST_PAD_ALWAYS,
  GST_CAPS_NEW (
    "osssink_sink",
    "audio/raw",
      "format",     GST_PROPS_STRING ("int"),   /* hack */
      "law",        GST_PROPS_INT (0),
      "endianness", GST_PROPS_INT (G_BYTE_ORDER),
      "signed",     GST_PROPS_LIST (
         	      GST_PROPS_BOOLEAN (FALSE),
     		      GST_PROPS_BOOLEAN (TRUE)
      		    ),
      "width",      GST_PROPS_LIST (
         	      GST_PROPS_INT (8),
     		      GST_PROPS_INT (16)
      		    ),
      "depth",      GST_PROPS_LIST (
    		      GST_PROPS_INT (8),
     		      GST_PROPS_INT (16)
     		    ),
      "rate",       GST_PROPS_INT_RANGE (1000, 48000),
      "channels",   GST_PROPS_INT_RANGE (1, 2)
  )
);

#define GST_TYPE_OSSSINK_CHANNELS (gst_osssink_channels_get_type())
static GType 
gst_osssink_channels_get_type(void) {
  static GType osssink_channels_type = 0;
  static GEnumValue osssink_channels[] = {
    {0, "0", "Silence"},
    {1, "1", "Mono"},
    {2, "2", "Stereo"},
    {0, NULL, NULL},
  };
  if (!osssink_channels_type) {
    osssink_channels_type = g_enum_register_static("GstAudiosinkChannels", osssink_channels);
  }
  return osssink_channels_type;
}


static GstElementClass *parent_class = NULL;
static guint gst_osssink_signals[LAST_SIGNAL] = { 0 };

GType
gst_osssink_get_type (void) 
{
  static GType osssink_type = 0;

  if (!osssink_type) {
    static const GTypeInfo osssink_info = {
      sizeof(GstOssSinkClass),
      NULL,
      NULL,
      (GClassInitFunc)gst_osssink_class_init,
      NULL,
      NULL,
      sizeof(GstOssSink),
      0,
      (GInstanceInitFunc)gst_osssink_init,
    };
    osssink_type = g_type_register_static (GST_TYPE_ELEMENT, "GstOssSink", &osssink_info, 0);
  }

  return osssink_type;
}

static GstBufferPool*
gst_osssink_get_bufferpool (GstPad *pad)
{
  GstOssSink *oss;
  
  oss = GST_OSSSINK (gst_pad_get_parent(pad));

  return oss->sinkpool;
}

static void
gst_osssink_finalize (GObject *object)
{
  GstOssSink *osssink = (GstOssSink *) object;

  g_free (osssink->device);

  G_OBJECT_CLASS (parent_class)->finalize (object);
}

static void
gst_osssink_class_init (GstOssSinkClass *klass) 
{
  GObjectClass *gobject_class;
  GstElementClass *gstelement_class;

  gobject_class = (GObjectClass*)klass;
  gstelement_class = (GstElementClass*)klass;

  parent_class = g_type_class_ref(GST_TYPE_ELEMENT);

  g_object_class_install_property(G_OBJECT_CLASS(klass), ARG_DEVICE,
    g_param_spec_string("device","device","device",
                        "/dev/dsp",G_PARAM_READWRITE)); /* CHECKME! */
  g_object_class_install_property(G_OBJECT_CLASS(klass), ARG_MUTE,
    g_param_spec_boolean("mute","mute","mute",
                         TRUE,G_PARAM_READWRITE)); 
  g_object_class_install_property (G_OBJECT_CLASS(klass), ARG_SYNC,
    g_param_spec_boolean("sync","Sync","If syncing on timestamps should be enabled",
                         TRUE, G_PARAM_READWRITE)); 

  /* it would be nice to show format in symbolic form, oh well */
  g_object_class_install_property(G_OBJECT_CLASS(klass), ARG_FORMAT,
    g_param_spec_int ("format","format","format",
                      0, G_MAXINT, AFMT_S16_LE, G_PARAM_READWRITE)); 

  g_object_class_install_property(G_OBJECT_CLASS(klass), ARG_CHANNELS,
    g_param_spec_enum("channels","channels","channels",
                      GST_TYPE_OSSSINK_CHANNELS,2,G_PARAM_READWRITE)); 
  g_object_class_install_property(G_OBJECT_CLASS(klass), ARG_FREQUENCY,
    g_param_spec_int("frequency","frequency","frequency",
                     0,G_MAXINT,44100,G_PARAM_READWRITE));
  g_object_class_install_property(G_OBJECT_CLASS(klass), ARG_FRAGMENT,
    g_param_spec_int("fragment","fragment","fragment",
                     0,G_MAXINT,6,G_PARAM_READWRITE));
  g_object_class_install_property(G_OBJECT_CLASS(klass), ARG_BUFFER_SIZE,
    g_param_spec_int("buffer_size","buffer_size","buffer_size",
                     0,G_MAXINT,4096,G_PARAM_READWRITE));

  gst_osssink_signals[SIGNAL_HANDOFF] =
    g_signal_new("handoff",G_TYPE_FROM_CLASS(klass), G_SIGNAL_RUN_LAST,
                   G_STRUCT_OFFSET(GstOssSinkClass,handoff), NULL, NULL,
                   g_cclosure_marshal_VOID__VOID,G_TYPE_NONE,0);
  
  gobject_class->set_property = gst_osssink_set_property;
  gobject_class->get_property = gst_osssink_get_property;
  gobject_class->finalize     = gst_osssink_finalize;
  
  gstelement_class->change_state = GST_DEBUG_FUNCPTR (gst_osssink_change_state);
  gstelement_class->query 	 = GST_DEBUG_FUNCPTR (gst_osssink_query);
}

static void 
gst_osssink_init (GstOssSink *osssink) 
{
  osssink->sinkpad = gst_pad_new_from_template (
		  GST_PAD_TEMPLATE_GET (osssink_sink_factory), "sink");
  gst_element_add_pad (GST_ELEMENT (osssink), osssink->sinkpad);
  gst_pad_set_connect_function (osssink->sinkpad, gst_osssink_sinkconnect);
  gst_pad_set_bufferpool_function (osssink->sinkpad, gst_osssink_get_bufferpool);
  gst_pad_set_convert_function (osssink->sinkpad, gst_osssink_convert);
  gst_pad_set_query_function (osssink->sinkpad, gst_osssink_sink_query);

  gst_pad_set_chain_function (osssink->sinkpad, gst_osssink_chain);

  osssink->device = g_strdup ("/dev/dsp");
  osssink->fd = -1;
  osssink->channels = 1;
  osssink->frequency = 11025;
  osssink->fragment = 6;
/* AFMT_*_BE not available on all OSS includes (e.g. FBSD) */
#ifdef WORDS_BIGENDIAN
  osssink->format = AFMT_S16_BE;
#else
  osssink->format = AFMT_S16_LE;
#endif /* WORDS_BIGENDIAN */  
  osssink->bufsize = 4096;
  osssink->bps = 0;
  osssink->resync = FALSE;
  osssink->sync = TRUE;
  /* 6 buffers per chunk by default */
  osssink->sinkpool = gst_buffer_pool_get_default (osssink->bufsize, 6);
  osssink->provided_clock = GST_CLOCK (gst_oss_clock_new ("ossclock", gst_osssink_get_time, osssink));
  osssink->handled = 0;

  GST_ELEMENT (osssink)->setclockfunc 	 = gst_osssink_set_clock;
  GST_ELEMENT (osssink)->getclockfunc 	 = gst_osssink_get_clock;
  
  GST_FLAG_SET (osssink, GST_ELEMENT_THREAD_SUGGESTED);
  GST_FLAG_SET (osssink, GST_ELEMENT_EVENT_AWARE);
}


static GstPadConnectReturn 
gst_osssink_sinkconnect (GstPad *pad, GstCaps *caps) 
{
  gint law, endianness, depth;
  gboolean sign;
  gint format = -1;
  GstOssSink *osssink = GST_OSSSINK (gst_pad_get_parent (pad));

  if (!GST_CAPS_IS_FIXED (caps))
    return GST_PAD_CONNECT_DELAYED;
  
  gst_caps_get_int (caps, "width", &osssink->width);
  gst_caps_get_int (caps, "depth", &depth);

  if (osssink->width != depth) 
    return GST_PAD_CONNECT_REFUSED;

  /* laws 1 and 2 are 1 bps anyway */
  osssink->bps = 1;

  gst_caps_get_int (caps, "law", &law);
  gst_caps_get_int (caps, "endianness", &endianness);
  gst_caps_get_boolean (caps, "signed", &sign);

  if (law == 0) {
    if (osssink->width == 16) {
      if (sign == TRUE) {
        if (endianness == G_LITTLE_ENDIAN)
	  format = AFMT_S16_LE;
        else if (endianness == G_BIG_ENDIAN)
	  format = AFMT_S16_BE;
      }
      else {
        if (endianness == G_LITTLE_ENDIAN)
	  format = AFMT_U16_LE;
        else if (endianness == G_BIG_ENDIAN)
	  format = AFMT_U16_BE;
      }
      osssink->bps = 2;
    }
    else if (osssink->width == 8) {
      if (sign == TRUE) {
	format = AFMT_S8;
      }
      else {
        format = AFMT_U8;
      }
      osssink->bps = 1;
    }
  } else if (law == 1) {
    format = AFMT_MU_LAW;
  } else if (law == 2) {
    format = AFMT_A_LAW;
  } else {
    g_critical ("unknown law");
    return GST_PAD_CONNECT_REFUSED;
  }

  if (format == -1) 
    return GST_PAD_CONNECT_REFUSED;

  osssink->format = format;
  gst_caps_get_int (caps, "channels", &osssink->channels);
  gst_caps_get_int (caps, "rate", &osssink->frequency);

  osssink->bps *= osssink->channels;
  osssink->bps *= osssink->frequency;

  if (!gst_osssink_sync_parms (osssink)) {
    return GST_PAD_CONNECT_REFUSED;
  }

  return GST_PAD_CONNECT_OK;
}

static gboolean 
gst_osssink_sync_parms (GstOssSink *osssink) 
{
  audio_buf_info ospace;
  int frag;
  gint target_format;
  gint target_channels;
  gint target_frequency;
  GObject *object;

  g_return_val_if_fail (osssink != NULL, FALSE);
  g_return_val_if_fail (GST_IS_OSSSINK (osssink), FALSE);

  if (osssink->fd == -1)
    return FALSE;
  
  if (osssink->fragment >> 16)
    frag = osssink->fragment;
  else
    frag = 0x7FFF0000 | osssink->fragment;
  
  GST_INFO (GST_CAT_PLUGIN_INFO, "osssink: trying to set sound card to %dHz %d bit %s (%08x fragment)",
           osssink->frequency, osssink->format,
           (osssink->channels == 2) ? "stereo" : "mono",frag);

  ioctl (osssink->fd, SNDCTL_DSP_SETFRAGMENT, &frag);

  ioctl (osssink->fd, SNDCTL_DSP_RESET, 0);

  target_format = osssink->format;
  target_channels = osssink->channels;
  target_frequency = osssink->frequency;

  ioctl (osssink->fd, SNDCTL_DSP_SETFMT, &osssink->format);
  ioctl (osssink->fd, SNDCTL_DSP_CHANNELS, &osssink->channels);
  ioctl (osssink->fd, SNDCTL_DSP_SPEED, &osssink->frequency);

  ioctl (osssink->fd, SNDCTL_DSP_GETBLKSIZE, &osssink->fragment);
  ioctl (osssink->fd, SNDCTL_DSP_GETOSPACE, &ospace);

  GST_INFO (GST_CAT_PLUGIN_INFO, "osssink: set sound card to %dHz %d bit %s (%d bytes buffer, %08x fragment)",
           osssink->frequency, osssink->format,
           (osssink->channels == 2) ? "stereo" : "mono", ospace.bytes, osssink->fragment);

  object = G_OBJECT (osssink);
  g_object_freeze_notify (object);
  g_object_notify (object, "channels");
  g_object_notify (object, "frequency");
  g_object_notify (object, "fragment");
  g_object_notify (object, "format");
  g_object_thaw_notify (object);

  osssink->fragment_time = (1000000 * osssink->fragment) / osssink->bps;
  GST_INFO (GST_CAT_PLUGIN_INFO, "fragment time %u %llu\n", osssink->bps, osssink->fragment_time);

  if (target_format != osssink->format ||
      target_channels != osssink->channels ||
      target_frequency != osssink->frequency) 
  {
    g_warning ("could not configure oss with required parameters, enjoy the noise :)");
    /* we could eventually return FALSE here, or just do some additional tests
     * to see that the frequencies don't differ too much etc.. */
  }
  return TRUE;
}

static inline gint64 
gst_osssink_get_delay (GstOssSink *osssink) 
{
  gint delay = 0;

  if (osssink->fd == -1)
    return 0;

  if (ioctl (osssink->fd, SNDCTL_DSP_GETODELAY, &delay) < 0) {
    audio_buf_info info;
    if (ioctl (osssink->fd, SNDCTL_DSP_GETOSPACE, &info) < 0) {
      delay = 0;
    }
    else {
      delay = (info.fragstotal * info.fragsize) - info.bytes;	  
    }
  }
  return delay;
}

static GstClockTime 
gst_osssink_get_time (GstClock *clock, gpointer data) 
{
  GstOssSink *osssink = GST_OSSSINK (data);
  gint delay;
  GstClockTime res;

  if (!osssink->bps)
    return 0;

  delay = gst_osssink_get_delay (osssink);

  /* sometimes delay is bigger than the number of bytes sent to the device, which screws
   * up this calculation, we assume that everything is still in the device then */
  if (((guint64)delay) > osssink->handled) {
    delay = osssink->handled;
  }
  res =  (osssink->handled - delay) * GST_SECOND / osssink->bps;

  /*
  g_print ("from osssink: %lld %d %lld %d\n", res, delay, osssink->handled, osssink->bps);
		  */

  return res;
}

static GstClock*
gst_osssink_get_clock (GstElement *element)
{
  GstOssSink *osssink;
	    
  osssink = GST_OSSSINK (element);

  return GST_CLOCK (osssink->provided_clock);
}

static void
gst_osssink_set_clock (GstElement *element, GstClock *clock)
{
  GstOssSink *osssink;
  
  osssink = GST_OSSSINK (element);

  osssink->clock = clock;  
}

static void 
gst_osssink_chain (GstPad *pad, GstBuffer *buf) 
{
  GstOssSink *osssink;
  GstClockTime buftime;

  /* this has to be an audio buffer */
  osssink = GST_OSSSINK (gst_pad_get_parent (pad));

  if (GST_IS_EVENT (buf)) {
    GstEvent *event = GST_EVENT (buf);

    switch (GST_EVENT_TYPE (event)) {
      case GST_EVENT_EOS:
        ioctl (osssink->fd, SNDCTL_DSP_SYNC);
	gst_oss_clock_set_active (osssink->provided_clock, FALSE);
	gst_pad_event_default (pad, event);
        return;
      case GST_EVENT_NEW_MEDIA:
	g_print ("new media\n");
        break;
      case GST_EVENT_DISCONTINUOUS:
      {
	gint64 value;

        ioctl (osssink->fd, SNDCTL_DSP_RESET);
	if (gst_event_discont_get_value (event, GST_FORMAT_TIME, &value)) {
          if (!gst_clock_handle_discont (osssink->clock, value))
	    gst_oss_clock_set_active (osssink->provided_clock, FALSE);
	  osssink->handled = 0;
	}
	osssink->resync = TRUE;
        break;
      }
      default:
	gst_pad_event_default (pad, event);
        return;
    }
    gst_event_unref (event);
    return;
  }

  if (!osssink->bps) {
    gst_buffer_unref (buf);
    gst_element_error (GST_ELEMENT (osssink), "capsnego was never performed, unknown data type");
    return;
  }

  buftime = GST_BUFFER_TIMESTAMP (buf);

  if (osssink->fd >= 0) {
    if (!osssink->mute) {
      guchar *data = GST_BUFFER_DATA (buf);
      gint size = GST_BUFFER_SIZE (buf);

      if (osssink->clock) {
        gint delay = 0;
	gint64 queued;
	GstClockTimeDiff jitter;
    
	delay = gst_osssink_get_delay (osssink);
	queued = delay * GST_SECOND / osssink->bps;

	if  (osssink->resync && osssink->sync) {
	  gst_element_clock_wait (GST_ELEMENT (osssink), osssink->clock, 
				buftime - queued, &jitter);

	  if (jitter > 0) {
            gst_clock_handle_discont (osssink->clock, buftime - queued + jitter);
	    write (osssink->fd, data, size);
	    gst_oss_clock_set_active (osssink->provided_clock, TRUE);
	    osssink->resync = FALSE;
	    osssink->handled += size;
	  }
	}
	else {
	  write (osssink->fd, data, size);
	  osssink->handled += size;
        }
      }
      /* no clock, try to be as fast as possible */
      else {
        audio_buf_info ospace;

        ioctl (osssink->fd, SNDCTL_DSP_GETOSPACE, &ospace);

        if (ospace.bytes >= size) {
          write (osssink->fd, data, size);
	}
      }
    }
  }
  gst_buffer_unref (buf);
}

static gboolean
gst_osssink_convert (GstPad *pad, GstFormat src_format, gint64 src_value,
	             GstFormat *dest_format, gint64 *dest_value)
{
  gboolean res = TRUE;
	      
  GstOssSink *osssink;

  if (src_format == *dest_format) {
    *dest_value = src_value;
    return TRUE;
  }

  osssink = GST_OSSSINK (gst_pad_get_parent (pad));

  if (osssink->bps == 0 || osssink->channels == 0 || osssink->width == 0)
    return FALSE;

  switch (src_format) {
    case GST_FORMAT_BYTES:
      switch (*dest_format) {
        case GST_FORMAT_DEFAULT:
          *dest_format = GST_FORMAT_TIME;
        case GST_FORMAT_TIME:
	  *dest_value = src_value * GST_SECOND / osssink->bps;
          break;
        case GST_FORMAT_UNITS:
	  *dest_value = src_value / (osssink->channels * osssink->width);
          break;
        default:
          res = FALSE;
      }
      break;
    case GST_FORMAT_TIME:
      switch (*dest_format) {
        case GST_FORMAT_DEFAULT:
          *dest_format = GST_FORMAT_BYTES;
        case GST_FORMAT_BYTES:
	  *dest_value = src_value * osssink->bps / GST_SECOND;
          break;
        case GST_FORMAT_UNITS:
	  *dest_value = src_value * osssink->frequency / GST_SECOND;
          break;
        default:
          res = FALSE;
      }
      break;
    case GST_FORMAT_UNITS:
      switch (*dest_format) {
        case GST_FORMAT_DEFAULT:
          *dest_format = GST_FORMAT_TIME;
        case GST_FORMAT_TIME:
	  *dest_value = src_value * GST_SECOND / osssink->frequency;
          break;
        case GST_FORMAT_BYTES:
	  *dest_value = src_value * osssink->channels * osssink->width;
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
gst_osssink_sink_query (GstPad *pad, GstPadQueryType type, GstFormat *format, gint64 *value) 
{
  gboolean res = TRUE;
  GstOssSink *osssink;

  osssink = GST_OSSSINK (gst_pad_get_parent (pad));
  
  switch (type) {
    case GST_PAD_QUERY_LATENCY:
      if (!gst_osssink_convert (pad, 
			        GST_FORMAT_BYTES, gst_osssink_get_delay (osssink),
		                format, value)) 
      {
        res = FALSE;
      }
      break;
    case GST_PAD_QUERY_POSITION:
      if (!gst_osssink_convert (pad, 
			        GST_FORMAT_TIME, gst_clock_get_time (osssink->provided_clock),
		                format, value)) 
      {
        res = FALSE;
      }
      break;
    default:
      res = gst_pad_query (gst_pad_get_peer (osssink->sinkpad), type, format, value);
      break;
  }

  return res;
}

static gboolean
gst_osssink_query (GstElement *element, GstPadQueryType type, GstFormat *format, gint64 *value) 
{
  GstOssSink *osssink = GST_OSSSINK (element);

  return gst_osssink_sink_query (osssink->sinkpad, type, format, value);
}

static void 
gst_osssink_set_property (GObject *object, guint prop_id, const GValue *value, GParamSpec *pspec) 
{
  GstOssSink *osssink;

  /* it's not null if we got it, but it might not be ours */
  g_return_if_fail (GST_IS_OSSSINK (object));
  
  osssink = GST_OSSSINK (object);

  switch (prop_id) {
    case ARG_DEVICE:
      /* disallow changing the device while it is opened
         get_property("device") should return the right one */
      if (!GST_FLAG_IS_SET (osssink, GST_OSSSINK_OPEN))
      {
	g_free (osssink->device);
        osssink->device = g_strdup (g_value_get_string (value));
	g_object_notify (object, "device");
      }
      break;
    case ARG_MUTE:
      osssink->mute = g_value_get_boolean (value);
      g_object_notify (G_OBJECT (osssink), "mute");
      break;
    case ARG_FORMAT:
      osssink->format = g_value_get_int (value);
      gst_osssink_sync_parms (osssink);
      break;
    case ARG_CHANNELS:
      osssink->channels = g_value_get_enum (value);
      gst_osssink_sync_parms (osssink);
      break;
    case ARG_FREQUENCY:
      osssink->frequency = g_value_get_int (value);
      gst_osssink_sync_parms (osssink);
      break;
    case ARG_FRAGMENT:
      osssink->fragment = g_value_get_int (value);
      gst_osssink_sync_parms (osssink);
      break;
    case ARG_BUFFER_SIZE:
      if (osssink->bufsize == g_value_get_int (value)) break;
      osssink->bufsize = g_value_get_int (value);
      osssink->sinkpool = gst_buffer_pool_get_default (osssink->bufsize, 6);
      g_object_notify (object, "buffer_size");
      break;
    case ARG_SYNC:
      osssink->sync = g_value_get_boolean (value);
      g_object_notify (G_OBJECT (osssink), "sync");
    default:
      break;
  }
}

static void 
gst_osssink_get_property (GObject *object, guint prop_id, GValue *value, GParamSpec *pspec) 
{
  GstOssSink *osssink;

  /* it's not null if we got it, but it might not be ours */
  g_return_if_fail (GST_IS_OSSSINK (object));
  
  osssink = GST_OSSSINK (object);

  switch (prop_id) {
    case ARG_DEVICE:
      g_value_set_string (value, osssink->device);
      break;
    case ARG_MUTE:
      g_value_set_boolean (value, osssink->mute);
      break;
    case ARG_FORMAT:
      g_value_set_int (value, osssink->format);
      break;
    case ARG_CHANNELS:
      g_value_set_enum (value, osssink->channels);
      break;
    case ARG_FREQUENCY:
      g_value_set_int (value, osssink->frequency);
      break;
    case ARG_FRAGMENT:
      g_value_set_int (value, osssink->fragment);
      break;
    case ARG_BUFFER_SIZE:
      g_value_set_int (value, osssink->bufsize);
      break;
    case ARG_SYNC:
      g_value_set_boolean (value, osssink->sync);
      break;
    default:
      break;
  }
}

static gboolean
gst_osssink_open_audio (GstOssSink *sink)
{
  gint caps;
  g_return_val_if_fail (sink->fd == -1, FALSE);

  GST_INFO (GST_CAT_PLUGIN_INFO, "osssink: attempting to open sound device");

  /* first try to open the sound card */
  sink->fd = open (sink->device, O_WRONLY | O_NONBLOCK);
  if (errno == EBUSY) {
    g_warning ("osssink: unable to open the sound device (in use ?)\n");
    return FALSE;
  }

  /* re-open the sound device in blocking mode */
  close (sink->fd);
  sink->fd = open (sink->device, O_WRONLY);

  if (sink->fd < 0) {
    g_warning ("osssink: unable to open the sound device (errno=%d)\n", errno); 
    return FALSE;
  }
	
  /* we have it, set the default parameters and go have fun */
  /* set card state */
  ioctl (sink->fd, SNDCTL_DSP_GETCAPS, &caps);

  GST_INFO (GST_CAT_PLUGIN_INFO, "osssink: Capabilities %08x", caps);

  if (caps & DSP_CAP_DUPLEX)	GST_INFO (GST_CAT_PLUGIN_INFO, "osssink:   Full duplex");
  if (caps & DSP_CAP_REALTIME) 	GST_INFO (GST_CAT_PLUGIN_INFO, "osssink:   Realtime");
  if (caps & DSP_CAP_BATCH)    	GST_INFO (GST_CAT_PLUGIN_INFO, "osssink:   Batch");
  if (caps & DSP_CAP_COPROC)   	GST_INFO (GST_CAT_PLUGIN_INFO, "osssink:   Has coprocessor");
  if (caps & DSP_CAP_TRIGGER)  	GST_INFO (GST_CAT_PLUGIN_INFO, "osssink:   Trigger");
  if (caps & DSP_CAP_MMAP)     	GST_INFO (GST_CAT_PLUGIN_INFO, "osssink:   Direct access");

#ifdef DSP_CAP_MULTI
  if (caps & DSP_CAP_MULTI)    	GST_INFO (GST_CAT_PLUGIN_INFO, "osssink:   Multiple open");
#endif /* DSP_CAP_MULTI */

#ifdef DSP_CAP_BIND
  if (caps & DSP_CAP_BIND)     	GST_INFO (GST_CAT_PLUGIN_INFO, "osssink:   Channel binding");
#endif /* DSP_CAP_BIND */

  ioctl(sink->fd, SNDCTL_DSP_GETFMTS, &caps);

  GST_INFO (GST_CAT_PLUGIN_INFO, "osssink: Formats %08x", caps);
  if (caps & AFMT_MU_LAW)   		GST_INFO (GST_CAT_PLUGIN_INFO, "osssink:   MU_LAW");
  if (caps & AFMT_A_LAW)    		GST_INFO (GST_CAT_PLUGIN_INFO, "osssink:   A_LAW");
  if (caps & AFMT_IMA_ADPCM)    	GST_INFO (GST_CAT_PLUGIN_INFO, "osssink:   IMA_ADPCM");
  if (caps & AFMT_U8)    		GST_INFO (GST_CAT_PLUGIN_INFO, "osssink:   U8");
  if (caps & AFMT_S16_LE)    		GST_INFO (GST_CAT_PLUGIN_INFO, "osssink:   S16_LE");
  if (caps & AFMT_S16_BE)    		GST_INFO (GST_CAT_PLUGIN_INFO, "osssink:   S16_BE");
  if (caps & AFMT_S8)    		GST_INFO (GST_CAT_PLUGIN_INFO, "osssink:   S8");
  if (caps & AFMT_U16_LE)    		GST_INFO (GST_CAT_PLUGIN_INFO, "osssink:   U16_LE");
  if (caps & AFMT_U16_BE)    		GST_INFO (GST_CAT_PLUGIN_INFO, "osssink:   U16_BE");
  if (caps & AFMT_MPEG)    		GST_INFO (GST_CAT_PLUGIN_INFO, "osssink:   MPEG");
#ifdef AFMT_AC3
  if (caps & AFMT_AC3)    		GST_INFO (GST_CAT_PLUGIN_INFO, "osssink:   AC3");
#endif

  GST_INFO (GST_CAT_PLUGIN_INFO, "osssink: opened audio (%s) with fd=%d", sink->device, sink->fd);
  GST_FLAG_SET (sink, GST_OSSSINK_OPEN);

  return TRUE;
}

static void
gst_osssink_close_audio (GstOssSink *sink)
{
  if (sink->fd < 0) return;

  close(sink->fd);
  sink->fd = -1;

  GST_FLAG_UNSET (sink, GST_OSSSINK_OPEN);

  GST_INFO (GST_CAT_PLUGIN_INFO, "osssink: closed sound device");
}

static GstElementStateReturn 
gst_osssink_change_state (GstElement *element) 
{
  GstOssSink *osssink;

  g_return_val_if_fail (GST_IS_OSSSINK (element), FALSE);

  osssink = GST_OSSSINK (element);

  switch (GST_STATE_TRANSITION (element)) {
    case GST_STATE_NULL_TO_READY:
      if (!GST_FLAG_IS_SET (element, GST_OSSSINK_OPEN)) {
        if (!gst_osssink_open_audio (osssink)) {
          return GST_STATE_FAILURE;
        }
      }
      break;
    case GST_STATE_READY_TO_PAUSED:
    case GST_STATE_PAUSED_TO_PLAYING:
      osssink->resync = TRUE;
      break;
    case GST_STATE_PLAYING_TO_PAUSED:
    {
      if (GST_FLAG_IS_SET (element, GST_OSSSINK_OPEN)) 
        ioctl (osssink->fd, SNDCTL_DSP_RESET, 0);
      gst_oss_clock_set_active (osssink->provided_clock, FALSE);
      osssink->resync = TRUE;
      break;
    }
    case GST_STATE_PAUSED_TO_READY:
      if (GST_FLAG_IS_SET (element, GST_OSSSINK_OPEN))
        ioctl (osssink->fd, SNDCTL_DSP_RESET, 0);
      break;
    case GST_STATE_READY_TO_NULL:
      if (GST_FLAG_IS_SET (element, GST_OSSSINK_OPEN))
        gst_osssink_close_audio (osssink);
      break;
  }
      
  if (GST_ELEMENT_CLASS (parent_class)->change_state)
    return GST_ELEMENT_CLASS (parent_class)->change_state (element);

  return GST_STATE_SUCCESS;
}

gboolean 
gst_osssink_factory_init (GstPlugin *plugin) 
{ 
  GstElementFactory *factory;

  factory = gst_element_factory_new ("osssink", GST_TYPE_OSSSINK, &gst_osssink_details);
  g_return_val_if_fail (factory != NULL, FALSE);

  gst_element_factory_add_pad_template (factory, GST_PAD_TEMPLATE_GET (osssink_sink_factory));

  gst_plugin_add_feature (plugin, GST_PLUGIN_FEATURE (factory));

  return TRUE;
}

