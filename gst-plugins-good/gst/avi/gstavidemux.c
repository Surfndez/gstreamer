/* GStreamer
 * Copyright (C) <1999> Erik Walthinsen <omega@temple-baptist.com>
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
/* Element-Checklist-Version: 5 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <string.h>

#include "gst/riff/riff-media.h"
#include "gstavidemux.h"
#include "avi-ids.h"

GST_DEBUG_CATEGORY_STATIC (avidemux_debug);
#define GST_CAT_DEFAULT avidemux_debug

static GstStaticPadTemplate sink_templ = GST_STATIC_PAD_TEMPLATE ("sink",
    GST_PAD_SINK,
    GST_PAD_ALWAYS,
    GST_STATIC_CAPS ("video/x-msvideo")
    );

static void gst_avi_demux_base_init (GstAviDemuxClass * klass);
static void gst_avi_demux_class_init (GstAviDemuxClass * klass);
static void gst_avi_demux_init (GstAviDemux * avi);

static void gst_avi_demux_reset (GstAviDemux * avi);
static void gst_avi_demux_loop (GstElement * element);

static gboolean gst_avi_demux_send_event (GstElement * element,
    GstEvent * event);

static const GstEventMask *gst_avi_demux_get_event_mask (GstPad * pad);
static gboolean gst_avi_demux_handle_src_event (GstPad * pad, GstEvent * event);
static const GstFormat *gst_avi_demux_get_src_formats (GstPad * pad);
static const GstQueryType *gst_avi_demux_get_src_query_types (GstPad * pad);
static gboolean gst_avi_demux_handle_src_query (GstPad * pad,
    GstQueryType type, GstFormat * format, gint64 * value);
static gboolean gst_avi_demux_src_convert (GstPad * pad,
    GstFormat src_format,
    gint64 src_value, GstFormat * dest_format, gint64 * dest_value);

static GstElementStateReturn gst_avi_demux_change_state (GstElement * element);

static GstRiffReadClass *parent_class = NULL;

GType
gst_avi_demux_get_type (void)
{
  static GType avi_demux_type = 0;

  if (!avi_demux_type) {
    static const GTypeInfo avi_demux_info = {
      sizeof (GstAviDemuxClass),
      (GBaseInitFunc) gst_avi_demux_base_init,
      NULL,
      (GClassInitFunc) gst_avi_demux_class_init,
      NULL,
      NULL,
      sizeof (GstAviDemux),
      0,
      (GInstanceInitFunc) gst_avi_demux_init,
    };

    avi_demux_type =
        g_type_register_static (GST_TYPE_RIFF_READ,
        "GstAviDemux", &avi_demux_info, 0);
  }

  return avi_demux_type;
}

static void
gst_avi_demux_base_init (GstAviDemuxClass * klass)
{
  static GstElementDetails gst_avi_demux_details =
      GST_ELEMENT_DETAILS ("Avi demuxer",
      "Codec/Demuxer",
      "Demultiplex an avi file into audio and video",
      "Erik Walthinsen <omega@cse.ogi.edu>\n"
      "Wim Taymans <wim.taymans@chello.be>\n"
      "Ronald Bultje <rbultje@ronald.bitfreak.net>");
  GstElementClass *element_class = GST_ELEMENT_CLASS (klass);
  GstPadTemplate *videosrctempl, *audiosrctempl;
  GstCaps *audcaps, *vidcaps;

  audcaps = gst_riff_create_audio_template_caps ();
  audiosrctempl = gst_pad_template_new ("audio_%02d",
      GST_PAD_SRC, GST_PAD_SOMETIMES, audcaps);

  vidcaps = gst_riff_create_video_template_caps ();
  gst_caps_append (vidcaps, gst_riff_create_iavs_template_caps ());
  videosrctempl = gst_pad_template_new ("video_%02d",
      GST_PAD_SRC, GST_PAD_SOMETIMES, vidcaps);

  gst_element_class_add_pad_template (element_class, audiosrctempl);
  gst_element_class_add_pad_template (element_class, videosrctempl);
  gst_element_class_add_pad_template (element_class,
      gst_static_pad_template_get (&sink_templ));
  gst_element_class_set_details (element_class, &gst_avi_demux_details);
}

static void
gst_avi_demux_class_init (GstAviDemuxClass * klass)
{
  GObjectClass *gobject_class;
  GstElementClass *gstelement_class;

  gobject_class = (GObjectClass *) klass;
  gstelement_class = (GstElementClass *) klass;

  GST_DEBUG_CATEGORY_INIT (avidemux_debug, "avidemux",
      0, "Demuxer for AVI streams");

  parent_class = g_type_class_ref (GST_TYPE_RIFF_READ);

  gstelement_class->change_state = gst_avi_demux_change_state;
  gstelement_class->send_event = gst_avi_demux_send_event;
}

static void
gst_avi_demux_init (GstAviDemux * avi)
{
  GST_FLAG_SET (avi, GST_ELEMENT_EVENT_AWARE);

  avi->sinkpad =
      gst_pad_new_from_template (gst_static_pad_template_get (&sink_templ),
      "sink");
  gst_element_add_pad (GST_ELEMENT (avi), avi->sinkpad);
  GST_RIFF_READ (avi)->sinkpad = avi->sinkpad;

  gst_element_set_loop_function (GST_ELEMENT (avi), gst_avi_demux_loop);
  gst_avi_demux_reset (avi);

  avi->index_entries = NULL;
  memset (&avi->stream, 0, sizeof (avi->stream));
}

static void
gst_avi_demux_reset (GstAviDemux * avi)
{
  gint i;

  for (i = 0; i < avi->num_streams; i++) {
    g_free (avi->stream[i].strh);
    gst_element_remove_pad (GST_ELEMENT (avi), avi->stream[i].pad);
    gst_caps_free (avi->stream[i].caps);
  }
  memset (&avi->stream, 0, sizeof (avi->stream));

  avi->num_streams = 0;
  avi->num_v_streams = 0;
  avi->num_a_streams = 0;

  avi->state = GST_AVI_DEMUX_START;
  avi->level_up = 0;

  if (avi->index_entries) {
    g_free (avi->index_entries);
    avi->index_entries = NULL;
  }
  avi->index_size = 0;
  avi->current_entry = 0;

  avi->num_frames = 0;
  avi->us_per_frame = 0;

  avi->seek_offset = (guint64) - 1;
}

static gst_avi_index_entry *
gst_avi_demux_index_next (GstAviDemux * avi,
    gint stream_nr, gint start, guint32 flags)
{
  gint i;
  gst_avi_index_entry *entry = NULL;

  for (i = start; i < avi->index_size; i++) {
    entry = &avi->index_entries[i];

    if (entry->stream_nr == stream_nr && (entry->flags & flags) == flags) {
      break;
    }
  }

  return entry;
}

static gst_avi_index_entry *
gst_avi_demux_index_entry_for_time (GstAviDemux * avi,
    gint stream_nr, guint64 time, guint32 flags)
{
  gst_avi_index_entry *entry = NULL, *last_entry = NULL;
  gint i;

  i = -1;
  do {
    entry = gst_avi_demux_index_next (avi, stream_nr, i + 1, flags);
    if (!entry)
      return NULL;

    i = entry->index_nr;

    if (entry->ts <= time) {
      last_entry = entry;
    }
  } while (entry->ts <= time);

  return last_entry;
}

static gst_avi_index_entry *
gst_avi_demux_index_entry_for_byte (GstAviDemux * avi,
    gint stream_nr, guint64 byte, guint32 flags)
{
  gst_avi_index_entry *entry = NULL, *last_entry = NULL;
  gint i;

  i = -1;
  do {
    entry = gst_avi_demux_index_next (avi, stream_nr, i + 1, flags);
    if (!entry)
      return NULL;

    i = entry->index_nr;

    if (entry->bytes_before <= byte) {
      last_entry = entry;
    }
  } while (entry->bytes_before <= byte);

  return last_entry;
}

static gst_avi_index_entry *
gst_avi_demux_index_entry_for_frame (GstAviDemux * avi,
    gint stream_nr, guint32 frame, guint32 flags)
{
  gst_avi_index_entry *entry = NULL, *last_entry = NULL;
  gint i;

  i = -1;
  do {
    entry = gst_avi_demux_index_next (avi, stream_nr, i + 1, flags);
    if (!entry)
      return NULL;

    i = entry->index_nr;

    if (entry->frames_before <= frame) {
      last_entry = entry;
    }
  } while (entry->frames_before <= frame);

  return last_entry;
}

static const GstFormat *
gst_avi_demux_get_src_formats (GstPad * pad)
{
  avi_stream_context *stream = gst_pad_get_element_private (pad);

  static const GstFormat src_a_formats[] = {
    GST_FORMAT_TIME,
    GST_FORMAT_BYTES,
    GST_FORMAT_DEFAULT,
    0
  };
  static const GstFormat src_v_formats[] = {
    GST_FORMAT_TIME,
    GST_FORMAT_DEFAULT,
    0
  };

  return (stream->strh->type == GST_RIFF_FCC_auds ?
      src_a_formats : src_v_formats);
}

static gboolean
gst_avi_demux_src_convert (GstPad * pad,
    GstFormat src_format,
    gint64 src_value, GstFormat * dest_format, gint64 * dest_value)
{
  gboolean res = TRUE;

  /*GstAviDemux *avi = GST_AVI_DEMUX (gst_pad_get_parent (pad)); */
  avi_stream_context *stream = gst_pad_get_element_private (pad);

  if (stream->strh->type == GST_RIFF_FCC_vids &&
      (src_format == GST_FORMAT_BYTES || *dest_format == GST_FORMAT_BYTES))
    return FALSE;

  switch (src_format) {
    case GST_FORMAT_TIME:
      switch (*dest_format) {
        case GST_FORMAT_BYTES:
          *dest_value = src_value * stream->bitrate / GST_SECOND;
          break;
        case GST_FORMAT_DEFAULT:
          *dest_value = src_value * stream->strh->rate /
              (stream->strh->scale * GST_SECOND);
          break;
        default:
          res = FALSE;
          break;
      }
      break;
    case GST_FORMAT_BYTES:
      switch (*dest_format) {
        case GST_FORMAT_TIME:
          *dest_value = ((gfloat) src_value) * GST_SECOND / stream->bitrate;
          break;
        default:
          res = FALSE;
          break;
      }
      break;
    case GST_FORMAT_DEFAULT:
      switch (*dest_format) {
        case GST_FORMAT_TIME:
          *dest_value = ((((gfloat) src_value) * stream->strh->scale) /
              stream->strh->rate) * GST_SECOND;
          break;
        default:
          res = FALSE;
          break;
      }
      break;
    default:
      res = FALSE;
  }

  return res;
}

static const GstQueryType *
gst_avi_demux_get_src_query_types (GstPad * pad)
{
  static const GstQueryType src_types[] = {
    GST_QUERY_TOTAL,
    GST_QUERY_POSITION,
    0
  };

  return src_types;
}

static gboolean
gst_avi_demux_handle_src_query (GstPad * pad,
    GstQueryType type, GstFormat * format, gint64 * value)
{
  gboolean res = TRUE;
  GstAviDemux *demux = GST_AVI_DEMUX (gst_pad_get_parent (pad));

  /*GstAviDemux *avi = GST_AVI_DEMUX (gst_pad_get_parent (pad)); */
  avi_stream_context *stream = gst_pad_get_element_private (pad);

  switch (type) {
    case GST_QUERY_TOTAL:
      switch (*format) {
        case GST_FORMAT_TIME:
          *value = (((gfloat) stream->strh->scale) * stream->strh->length /
              stream->strh->rate) * GST_SECOND;
          break;
        case GST_FORMAT_BYTES:
          if (stream->strh->type == GST_RIFF_FCC_auds) {
            *value = stream->total_bytes;
          } else
            res = FALSE;
          break;
        case GST_FORMAT_DEFAULT:
          if (stream->strh->type == GST_RIFF_FCC_auds) {
            if (!stream->strh->samplesize)
              *value = stream->total_frames;
            else
              *value = stream->total_bytes / stream->strh->samplesize;
          } else if (stream->strh->type == GST_RIFF_FCC_vids)
            *value = stream->strh->length;
          else
            res = FALSE;
          break;
        default:
          res = FALSE;
          break;
      }
      break;
    case GST_QUERY_POSITION:
      switch (*format) {
        case GST_FORMAT_TIME:
          if (stream->strh->type == GST_RIFF_FCC_auds) {
            if (!stream->strh->samplesize) {
              *value = GST_SECOND * stream->current_frame *
                  stream->strh->scale / stream->strh->rate;
            } else if (stream->bitrate != 0) {
              *value = ((gfloat) stream->current_byte) * GST_SECOND /
                  stream->bitrate;
            } else if (stream->total_frames != 0 && stream->total_bytes != 0) {
              /* calculate timestamps based on video size */
              guint64 len = demux->us_per_frame * demux->num_frames *
                  GST_USECOND;

              if (!stream->strh->samplesize)
                *value = len * stream->current_frame / stream->total_frames;
              else
                *value = len * stream->current_byte / stream->total_bytes;
            } else {
              res = FALSE;
            }
          } else {
            if (stream->strh->rate != 0) {
              *value = ((gfloat) stream->current_frame * stream->strh->scale *
                  GST_SECOND / stream->strh->rate);
            } else {
              *value = stream->current_frame * demux->us_per_frame *
                  GST_USECOND;
            }
          }
          break;
        case GST_FORMAT_BYTES:
          *value = stream->current_byte;
          break;
        case GST_FORMAT_DEFAULT:
          if (stream->strh->samplesize &&
              stream->strh->type == GST_RIFF_FCC_auds)
            *value = stream->current_byte / stream->strh->samplesize;
          else
            *value = stream->current_frame;
          break;
        default:
          res = FALSE;
          break;
      }
      break;
    default:
      res = FALSE;
      break;
  }

  return res;
}

static GstCaps *
gst_avi_demux_src_getcaps (GstPad * pad)
{
  avi_stream_context *stream = gst_pad_get_element_private (pad);

  return gst_caps_copy (stream->caps);
}

static gint32
gst_avi_demux_sync_streams (GstAviDemux * avi, guint64 time)
{
  gint i;
  guint32 min_index = G_MAXUINT;
  avi_stream_context *stream;
  gst_avi_index_entry *entry;

  for (i = 0; i < avi->num_streams; i++) {
    stream = &avi->stream[i];

    GST_DEBUG ("finding %d for time %" G_GINT64_FORMAT, i, time);

    entry = gst_avi_demux_index_entry_for_time (avi, stream->num, time,
        GST_RIFF_IF_KEYFRAME);
    if (entry) {
      min_index = MIN (entry->index_nr, min_index);
    }
  }
  GST_DEBUG ("first index at %d", min_index);

  /* now we know the entry we need to sync on. calculate number of frames to
   * skip fro there on and the stream stats */
  for (i = 0; i < avi->num_streams; i++) {
    gst_avi_index_entry *next_entry;

    stream = &avi->stream[i];

    /* next entry */
    next_entry = gst_avi_demux_index_next (avi, stream->num, min_index, 0);
    /* next entry with keyframe */
    entry = gst_avi_demux_index_next (avi, stream->num, min_index,
        GST_RIFF_IF_KEYFRAME);

    stream->current_byte = next_entry->bytes_before;
    stream->current_frame = next_entry->frames_before;
    stream->skip = entry->frames_before - next_entry->frames_before;

    GST_DEBUG ("%d skip %d", stream->num, stream->skip);
  }

  GST_DEBUG ("final index at %d", min_index);

  return min_index;
}

static gboolean
gst_avi_demux_send_event (GstElement * element, GstEvent * event)
{
  const GList *pads;

  pads = gst_element_get_pad_list (element);

  while (pads) {
    GstPad *pad = GST_PAD (pads->data);

    if (GST_PAD_DIRECTION (pad) == GST_PAD_SRC) {
      /* we ref the event here as we might have to try again if the event
       * failed on this pad */
      gst_event_ref (event);
      if (gst_avi_demux_handle_src_event (pad, event)) {
        gst_event_unref (event);

        return TRUE;
      }
    }

    pads = g_list_next (pads);
  }

  gst_event_unref (event);

  return FALSE;
}

static const GstEventMask *
gst_avi_demux_get_event_mask (GstPad * pad)
{
  static const GstEventMask masks[] = {
    {GST_EVENT_SEEK, GST_SEEK_METHOD_SET | GST_SEEK_FLAG_KEY_UNIT},
    {0,}
  };

  return masks;
}

static gboolean
gst_avi_demux_handle_src_event (GstPad * pad, GstEvent * event)
{
  gboolean res = TRUE;
  GstAviDemux *avi = GST_AVI_DEMUX (gst_pad_get_parent (pad));
  avi_stream_context *stream;

  if (!avi->index_entries)
    return FALSE;

  stream = gst_pad_get_element_private (pad);

  switch (GST_EVENT_TYPE (event)) {
    case GST_EVENT_SEEK:
      GST_DEBUG ("seek format %d, %08x", GST_EVENT_SEEK_FORMAT (event),
          stream->strh->type);

      switch (GST_EVENT_SEEK_FORMAT (event)) {
        case GST_FORMAT_BYTES:
        case GST_FORMAT_DEFAULT:
        case GST_FORMAT_TIME:{
          gst_avi_index_entry *seek_entry, *entry = NULL;
          gint64 desired_offset = GST_EVENT_SEEK_OFFSET (event);
          guint32 flags;
          guint64 min_index;

          /* no seek on audio yet */
          if (stream->strh->type == GST_RIFF_FCC_auds) {
            res = FALSE;
            goto done;
          }
          GST_DEBUG ("seeking to %" G_GINT64_FORMAT, desired_offset);

          flags = GST_RIFF_IF_KEYFRAME;
          switch (GST_EVENT_SEEK_FORMAT (event)) {
            case GST_FORMAT_BYTES:
              entry = gst_avi_demux_index_entry_for_byte (avi, stream->num,
                  desired_offset, flags);
              break;
            case GST_FORMAT_DEFAULT:
              entry = gst_avi_demux_index_entry_for_frame (avi, stream->num,
                  desired_offset, flags);
              break;
            case GST_FORMAT_TIME:
              entry = gst_avi_demux_index_entry_for_time (avi, stream->num,
                  desired_offset, flags);
              break;
          }

          if (entry) {
            min_index = gst_avi_demux_sync_streams (avi, entry->ts);
            seek_entry = &avi->index_entries[min_index];

            avi->seek_offset = seek_entry->offset + avi->index_offset;
            avi->last_seek = entry->ts;
            avi->seek_flush =
                (GST_EVENT_SEEK_FLAGS (event) & GST_SEEK_FLAG_FLUSH);
            avi->seek_entry = entry->index_nr;
          } else {
            GST_DEBUG ("no index entry found for format=%d value=%"
                G_GINT64_FORMAT, GST_EVENT_SEEK_FORMAT (event), desired_offset);
            res = FALSE;
          }
          break;
        }
        default:
          res = FALSE;
          break;
      }
      break;
    default:
      res = FALSE;
      break;
  }

done:
  gst_event_unref (event);

  return res;
}

/*
 * "Open" a RIFF file.
 */

static gboolean
gst_avi_demux_stream_init (GstAviDemux * avi)
{
  GstRiffRead *riff = GST_RIFF_READ (avi);
  guint32 doctype;

  if (!gst_riff_read_header (riff, &doctype))
    return FALSE;
  if (doctype != GST_RIFF_RIFF_AVI) {
    GST_ELEMENT_ERROR (avi, STREAM, WRONG_TYPE, (NULL), (NULL));
    return FALSE;
  }

  return TRUE;
}

/*
 * Read 'avih' header.
 */

static gboolean
gst_avi_demux_stream_avih (GstAviDemux * avi,
    guint32 * flags, guint32 * streams)
{
  GstRiffRead *riff = GST_RIFF_READ (avi);
  guint32 tag;
  GstBuffer *buf;
  gst_riff_avih avih, *_avih;

  if (!gst_riff_read_data (riff, &tag, &buf))
    return FALSE;

  if (tag != GST_RIFF_TAG_avih) {
    g_warning ("Not a avih chunk");
    gst_buffer_unref (buf);
    return FALSE;
  }
  if (GST_BUFFER_SIZE (buf) < sizeof (gst_riff_avih)) {
    g_warning ("Too small avih (%d available, %d needed)",
        GST_BUFFER_SIZE (buf), (int) sizeof (gst_riff_avih));
    gst_buffer_unref (buf);
    return FALSE;
  }

  _avih = (gst_riff_avih *) GST_BUFFER_DATA (buf);
  avih.us_frame = GUINT32_FROM_LE (_avih->us_frame);
  avih.max_bps = GUINT32_FROM_LE (_avih->max_bps);
  avih.pad_gran = GUINT32_FROM_LE (_avih->pad_gran);
  avih.flags = GUINT32_FROM_LE (_avih->flags);
  avih.tot_frames = GUINT32_FROM_LE (_avih->tot_frames);
  avih.init_frames = GUINT32_FROM_LE (_avih->init_frames);
  avih.streams = GUINT32_FROM_LE (_avih->streams);
  avih.bufsize = GUINT32_FROM_LE (_avih->bufsize);
  avih.width = GUINT32_FROM_LE (_avih->width);
  avih.height = GUINT32_FROM_LE (_avih->height);
  avih.scale = GUINT32_FROM_LE (_avih->scale);
  avih.rate = GUINT32_FROM_LE (_avih->rate);
  avih.start = GUINT32_FROM_LE (_avih->start);
  avih.length = GUINT32_FROM_LE (_avih->length);

  /* debug stuff */
  GST_INFO ("avih tag found:");
  GST_INFO (" us_frame    %u", avih.us_frame);
  GST_INFO (" max_bps     %u", avih.max_bps);
  GST_INFO (" pad_gran    %u", avih.pad_gran);
  GST_INFO (" flags       0x%08x", avih.flags);
  GST_INFO (" tot_frames  %u", avih.tot_frames);
  GST_INFO (" init_frames %u", avih.init_frames);
  GST_INFO (" streams     %u", avih.streams);
  GST_INFO (" bufsize     %u", avih.bufsize);
  GST_INFO (" width       %u", avih.width);
  GST_INFO (" height      %u", avih.height);
  GST_INFO (" scale       %u", avih.scale);
  GST_INFO (" rate        %u", avih.rate);
  GST_INFO (" start       %u", avih.start);
  GST_INFO (" length      %u", avih.length);

  avi->num_frames = avih.tot_frames;
  avi->us_per_frame = avih.us_frame;
  *streams = avih.streams;
  *flags = avih.flags;

  gst_buffer_unref (buf);

  return TRUE;
}

/*
 * Add a stream.
 */

static gboolean
gst_avi_demux_add_stream (GstAviDemux * avi)
{
  GstElementClass *klass = GST_ELEMENT_GET_CLASS (avi);
  GstRiffRead *riff = GST_RIFF_READ (avi);
  guint32 tag;
  gst_riff_strh *strh;
  GstBuffer *extradata = NULL, *initdata = NULL;
  gchar *name = NULL, *padname = NULL;
  GstCaps *caps = NULL;
  GstPadTemplate *templ = NULL;
  GstPad *pad;
  avi_stream_context *stream;
  gint blockalign = 0, bitrate = 0;
  union
  {
    gst_riff_strf_vids *vids;
    gst_riff_strf_auds *auds;
    gst_riff_strf_iavs *iavs;
  }
  strf;

  /* the stream starts with a 'strh' header */
  if (!(tag = gst_riff_peek_tag (riff, NULL)))
    return FALSE;
  if (tag != GST_RIFF_TAG_strh) {
    g_warning ("Invalid stream header (no strh at begin)");
    goto skip_stream;
  }
  if (!gst_riff_read_strh (riff, &strh))
    return FALSE;

  /* then comes a 'strf' of that specific type */
  if (!(tag = gst_riff_peek_tag (riff, NULL)))
    return FALSE;
  if (tag != GST_RIFF_TAG_strf) {
    GST_ELEMENT_ERROR (avi, STREAM, DEMUX, (NULL),
        ("Invalid AVI header (no strf as second tag)"));
    goto skip_stream;
  }
  switch (strh->type) {
    case GST_RIFF_FCC_vids:
      if (!gst_riff_read_strf_vids_with_data (riff, &strf.vids, &extradata))
        return FALSE;
      break;
    case GST_RIFF_FCC_auds:
      if (!gst_riff_read_strf_auds_with_data (riff, &strf.auds, &extradata))
        return FALSE;
      break;
    case GST_RIFF_FCC_iavs:
      if (!gst_riff_read_strf_iavs (riff, &strf.iavs))
        return FALSE;
      break;
    default:
      g_warning ("Unknown stream type " GST_FOURCC_FORMAT,
          GST_FOURCC_ARGS (strh->type));
      goto skip_stream;
  }

  /* read other things */
  while (TRUE) {
    if (!(tag = gst_riff_peek_tag (riff, &avi->level_up)))
      return FALSE;
    else if (avi->level_up) {
      avi->level_up--;
      break;
    }

    switch (tag) {
      case GST_RIFF_TAG_strd:
        if (initdata)
          gst_buffer_unref (initdata);
        if (!gst_riff_read_data (riff, &tag, &initdata))
          return FALSE;
        break;

      case GST_RIFF_TAG_strn:
        if (name)
          g_free (name);
        if (!gst_riff_read_ascii (riff, &tag, &name))
          return FALSE;
        break;

      default:
        GST_WARNING ("Unknown tag " GST_FOURCC_FORMAT " in AVI header",
            GST_FOURCC_ARGS (tag));
        /* fall-through */

      case GST_RIFF_TAG_JUNK:
        if (!gst_riff_read_skip (riff))
          return FALSE;
        break;
    }

    if (avi->level_up) {
      avi->level_up--;
      break;
    }
  }

  /* create stream name + pad */
  switch (strh->type) {
    case GST_RIFF_FCC_vids:
    {
      char *codec_name = NULL;
      GstTagList *list = gst_tag_list_new ();

      padname = g_strdup_printf ("video_%02d", avi->num_v_streams);
      templ = gst_element_class_get_pad_template (klass, "video_%02d");
      caps = gst_riff_create_video_caps_with_data (strf.vids->compression,
          strh, strf.vids, extradata, initdata, &codec_name);
      gst_tag_list_add (list, GST_TAG_MERGE_APPEND, GST_TAG_VIDEO_CODEC,
          codec_name, NULL);
      gst_element_found_tags (GST_ELEMENT (avi), list);
      gst_tag_list_free (list);
      if (codec_name)
        g_free (codec_name);
      g_free (strf.vids);
      avi->num_v_streams++;
      break;
    }
    case GST_RIFF_FCC_auds:
    {
      char *codec_name = NULL;
      GstTagList *list = gst_tag_list_new ();

      padname = g_strdup_printf ("audio_%02d", avi->num_a_streams);
      templ = gst_element_class_get_pad_template (klass, "audio_%02d");
      caps =
          gst_riff_create_audio_caps_with_data (strf.auds->format, strh,
          strf.auds, extradata, initdata, &codec_name);
      gst_tag_list_add (list, GST_TAG_MERGE_APPEND, GST_TAG_AUDIO_CODEC,
          codec_name, NULL);
      gst_element_found_tags (GST_ELEMENT (avi), list);
      gst_tag_list_free (list);
      if (codec_name)
        g_free (codec_name);
      blockalign = strf.auds->blockalign;
      bitrate = strf.auds->av_bps;
      g_free (strf.auds);
      avi->num_a_streams++;
      break;
    }
    case GST_RIFF_FCC_iavs:
    {
      char *codec_name = NULL;
      GstTagList *list = gst_tag_list_new ();

      padname = g_strdup_printf ("video_%02d", avi->num_v_streams);
      templ = gst_element_class_get_pad_template (klass, "video_%02d");
      caps = gst_riff_create_iavs_caps (strh->fcc_handler, strh, strf.iavs,
          &codec_name);
      gst_tag_list_add (list, GST_TAG_MERGE_APPEND, GST_TAG_VIDEO_CODEC,
          codec_name, NULL);
      gst_element_found_tags (GST_ELEMENT (avi), list);
      gst_tag_list_free (list);
      if (codec_name)
        g_free (codec_name);
      g_free (strf.iavs);
      avi->num_v_streams++;
      break;
    }
    default:
      g_assert (0);
  }

  /* set proper settings and add it */
  pad = gst_pad_new_from_template (templ, padname);
  g_free (padname);

  gst_pad_set_formats_function (pad, gst_avi_demux_get_src_formats);
  gst_pad_set_event_mask_function (pad, gst_avi_demux_get_event_mask);
  gst_pad_set_event_function (pad, gst_avi_demux_handle_src_event);
  gst_pad_set_query_type_function (pad, gst_avi_demux_get_src_query_types);
  gst_pad_set_query_function (pad, gst_avi_demux_handle_src_query);
  gst_pad_set_convert_function (pad, gst_avi_demux_src_convert);
  gst_pad_set_getcaps_function (pad, gst_avi_demux_src_getcaps);

  stream = &avi->stream[avi->num_streams];
  stream->caps = caps ? caps : gst_caps_new_empty ();
  stream->pad = pad;
  stream->strh = strh;
  stream->num = avi->num_streams;
  stream->delay = 0LL;
  stream->total_bytes = 0LL;
  stream->total_frames = 0;
  stream->current_frame = 0;
  stream->current_byte = 0;
  stream->current_entry = -1;
  stream->skip = 0;
  stream->blockalign = blockalign;
  stream->bitrate = bitrate;
  gst_pad_set_element_private (pad, stream);
  avi->num_streams++;

  /* auto-negotiates */
  gst_element_add_pad (GST_ELEMENT (avi), pad);

  /* clean something up */
  if (initdata)
    gst_buffer_unref (initdata);
  if (extradata)
    gst_buffer_unref (extradata);

  return TRUE;

skip_stream:
  while (TRUE) {
    if (!(tag = gst_riff_peek_tag (riff, &avi->level_up)))
      return FALSE;
    if (avi->level_up) {
      avi->level_up--;
      break;
    }
    if (!gst_riff_read_skip (riff))
      return FALSE;
  }

  /* add a "NULL" stream */
  avi->num_streams++;

  return TRUE;                  /* recoverable */
}

/*
 * Read an openDML-2.0 extension header.
 */

static gboolean
gst_avi_demux_stream_odml (GstAviDemux * avi)
{
  GstRiffRead *riff = GST_RIFF_READ (avi);
  guint32 tag;

  /* read contents */
  while (TRUE) {
    if (!(tag = gst_riff_peek_tag (riff, &avi->level_up)))
      return FALSE;
    else if (avi->level_up) {
      avi->level_up--;
      break;
    }

    switch (tag) {
      case GST_RIFF_TAG_dmlh:{
        gst_riff_dmlh dmlh, *_dmlh;
        GstBuffer *buf;

        if (!gst_riff_read_data (riff, &tag, &buf))
          return FALSE;
        if (GST_BUFFER_SIZE (buf) < sizeof (gst_riff_dmlh)) {
          g_warning ("DMLH entry is too small (%d bytes, %d needed)",
              GST_BUFFER_SIZE (buf), (int) sizeof (gst_riff_dmlh));
          gst_buffer_unref (buf);
          break;
        }
        _dmlh = (gst_riff_dmlh *) GST_BUFFER_DATA (buf);
        dmlh.totalframes = GUINT32_FROM_LE (_dmlh->totalframes);

        GST_INFO ("dmlh tag found:");
        GST_INFO (" totalframes: %u", dmlh.totalframes);

        avi->num_frames = dmlh.totalframes;
        gst_buffer_unref (buf);
        break;
      }

      default:
        GST_WARNING ("Unknown tag " GST_FOURCC_FORMAT " in AVI header",
            GST_FOURCC_ARGS (tag));
        /* fall-through */

      case GST_RIFF_TAG_JUNK:
        if (!gst_riff_read_skip (riff))
          return FALSE;
        break;
    }

    if (avi->level_up) {
      avi->level_up--;
      break;
    }
  }

  return TRUE;
}

/*
 * Seek to index, read it, seek back.
 * Return value indicates if we can continue processing. It
 * does not indicate if index-reading succeeded.
 */

static gboolean
gst_avi_demux_stream_index (GstAviDemux * avi)
{
  GstBuffer *buf = NULL;
  guint i;
  GstEvent *event;
  GstRiffRead *riff = GST_RIFF_READ (avi);
  guint64 pos_before, pos_after, length;
  guint32 tag;

  /* first, we need to know the current position (to seek back
   * when we're done) and the total length of the file. */
  length = gst_bytestream_length (riff->bs);
  pos_before = gst_bytestream_tell (riff->bs);

  /* skip movi
   *
   * FIXME:
   * - we want to add error handling here so we can recover.
   */
  if (!gst_riff_read_skip (riff))
    return FALSE;

  /* assure that we've got data left */
  pos_after = gst_bytestream_tell (riff->bs);
  if (pos_after + 8 > length) {
    g_warning ("File said that it has an index, but there is no index data!");
    goto end;
  }

  /* assure that it's an index */
  if (!(tag = gst_riff_peek_tag (riff, NULL)))
    return FALSE;
  if (tag != GST_RIFF_TAG_idx1) {
    g_warning ("No index after data, but " GST_FOURCC_FORMAT,
        GST_FOURCC_ARGS (tag));
    goto end;
  }

  /* read index */
  if (!gst_riff_read_data (riff, &tag, &buf))
    return FALSE;

  /* parse all entries */
  avi->index_size = GST_BUFFER_SIZE (buf) / sizeof (gst_riff_index_entry);
  avi->index_entries =
      g_malloc (avi->index_size * sizeof (gst_avi_index_entry));
  GST_INFO ("%u index entries", avi->index_size);

  for (i = 0; i < avi->index_size; i++) {
    gst_riff_index_entry entry, *_entry;
    avi_stream_context *stream;
    gint stream_nr;
    gst_avi_index_entry *target;
    GstFormat format;

    _entry = &((gst_riff_index_entry *) GST_BUFFER_DATA (buf))[i];
    entry.id = GUINT32_FROM_LE (_entry->id);
    entry.offset = GUINT32_FROM_LE (_entry->offset);
    entry.flags = GUINT32_FROM_LE (_entry->flags);
    entry.size = GUINT32_FROM_LE (_entry->size);
    target = &avi->index_entries[i];

    if (entry.id == GST_RIFF_rec)
      continue;

    stream_nr = CHUNKID_TO_STREAMNR (entry.id);
    if (stream_nr >= avi->num_streams || stream_nr < 0) {
      g_warning ("Index entry %d has invalid stream nr %d", i, stream_nr);
      target->stream_nr = -1;
      continue;
    }
    target->stream_nr = stream_nr;
    stream = &avi->stream[stream_nr];

    target->index_nr = i;
    target->flags = entry.flags;
    target->size = entry.size;
    target->offset = entry.offset + 8;

    /* figure out if the index is 0 based or relative to the MOVI start */
    if (i == 0) {
      if (target->offset < pos_before)
        avi->index_offset = pos_before + 8;
      else
        avi->index_offset = 0;
    }

    target->bytes_before = stream->total_bytes;
    target->frames_before = stream->total_frames;

    format = GST_FORMAT_TIME;
    if (stream->strh->type == GST_RIFF_FCC_auds) {
      /* all audio frames are keyframes */
      target->flags |= GST_RIFF_IF_KEYFRAME;
    }

    if (stream->strh->samplesize && stream->strh->type == GST_RIFF_FCC_auds) {
      /* constant rate stream */
      gst_pad_convert (stream->pad, GST_FORMAT_BYTES,
          stream->total_bytes, &format, &target->ts);
      gst_pad_convert (stream->pad, GST_FORMAT_BYTES,
          stream->total_bytes + target->size, &format, &target->dur);
    } else {
      /* VBR stream */
      gst_pad_convert (stream->pad, GST_FORMAT_DEFAULT,
          stream->total_frames, &format, &target->ts);
      gst_pad_convert (stream->pad, GST_FORMAT_DEFAULT,
          stream->total_frames + 1, &format, &target->dur);
    }
    target->dur -= target->ts;

    stream->total_bytes += target->size;
    stream->total_frames++;
  }

  /* debug our indexes */
  for (i = 0; i < avi->num_streams; i++) {
    avi_stream_context *stream;

    stream = &avi->stream[i];
    GST_DEBUG ("stream %u: %u frames, %" G_GINT64_FORMAT " bytes",
        i, stream->total_frames, stream->total_bytes);
  }

end:
  if (buf)
    gst_buffer_unref (buf);

  /* seek back to the data */
  if (!(event = gst_riff_read_seek (riff, pos_before)))
    return FALSE;
  gst_event_unref (event);

  return TRUE;
}

/*
 * Sync to next data chunk.
 */

static gboolean
gst_avi_demux_sync (GstAviDemux * avi, guint32 * ret_tag, gboolean prevent_eos)
{
  GstRiffRead *riff = GST_RIFF_READ (avi);
  guint32 tag;
  guint64 length = gst_bytestream_length (riff->bs);

  if (gst_bytestream_tell (riff->bs) + 12 >= length)
    return FALSE;

  /* peek first (for the end of this 'list/movi' section) */
  if (!(tag = gst_riff_peek_tag (riff, &avi->level_up)))
    return FALSE;

  /* if we're at top-level, we didn't read the 'movi'
   * list tag yet. This can also be 'AVIX' in case of
   * openDML-2.0 AVI files. Lastly, it might be idx1,
   * in which case we skip it so we come at EOS. */
  while (g_list_length (riff->level) < 2) {
    if (gst_bytestream_tell (riff->bs) + 12 >= length)
      return FALSE;

    if (!(tag = gst_riff_peek_tag (riff, NULL)))
      return FALSE;

    switch (tag) {
      case GST_RIFF_TAG_LIST:
        if (!(tag = gst_riff_peek_list (riff)))
          return FALSE;

        switch (tag) {
          case GST_RIFF_LIST_AVIX:
          case GST_RIFF_LIST_movi:
            if (!gst_riff_read_list (riff, &tag))
              return FALSE;
            /* we're now going to read buffers! */
            break;

          default:
            GST_WARNING ("Unknown list " GST_FOURCC_FORMAT " before AVI data",
                GST_FOURCC_ARGS (tag));
            /* fall-through */

          case GST_RIFF_TAG_JUNK:
            if (!gst_riff_read_skip (riff))
              return FALSE;
            break;
        }

        break;

      default:
        GST_WARNING ("Unknown tag " GST_FOURCC_FORMAT " before AVI data",
            GST_FOURCC_ARGS (tag));
        /* fall-through */

      case GST_RIFF_TAG_idx1:
      case GST_RIFF_TAG_JUNK:
        if (!gst_riff_read_skip (riff))
          return FALSE;
        break;
    }
  }

  /* And then, we get the data */
  if (gst_bytestream_tell (riff->bs) + 12 >= length)
    return FALSE;

  if (!(tag = gst_riff_peek_tag (riff, NULL)))
    return FALSE;

  /* Support for rec-list files */
  switch (tag) {
    case GST_RIFF_TAG_LIST:
      if (!(tag = gst_riff_peek_list (riff)))
        return FALSE;
      if (tag == GST_RIFF_rec) {
        /* Simply skip the list */
        if (!gst_riff_read_list (riff, &tag))
          return FALSE;
        if (!(tag = gst_riff_peek_tag (riff, NULL)))
          return FALSE;
      }
      break;

    case GST_RIFF_TAG_JUNK:
      gst_riff_read_skip (riff);
      return FALSE;
  }

  if (ret_tag)
    *ret_tag = tag;

  return TRUE;
}

/*
 * Scan the file for all chunks to "create" a new index.
 * Return value indicates if we can continue reading the stream. It
 * does not say anything about whether we created an index.
 */

static gboolean
gst_avi_demux_stream_scan (GstAviDemux * avi)
{
  GstRiffRead *riff = GST_RIFF_READ (avi);
  gst_avi_index_entry *entry;
  avi_stream_context *stream;
  guint64 pos = gst_bytestream_tell (riff->bs);
  guint32 tag;
  GstEvent *event;

  /* FIXME:
   * - implement non-seekable source support.
   */

  GST_LOG_OBJECT (avi, "Creating index");

  while (gst_avi_demux_sync (avi, &tag, TRUE)) {
    gint stream_nr = CHUNKID_TO_STREAMNR (tag);
    guint8 *data;
    GstFormat format = GST_FORMAT_TIME;

    if (stream_nr < 0 || stream_nr >= avi->num_streams)
      goto next;
    stream = &avi->stream[stream_nr];

    /* get chunk size */
    if (gst_bytestream_peek_bytes (riff->bs, &data, 8) != 8)
      goto next;

    /* increase allocated size for index */
    if (avi->index_size % 256 == 0) {
      avi->index_entries = g_renew (gst_avi_index_entry,
          avi->index_entries, avi->index_size + 256);
    }
    entry = &avi->index_entries[avi->index_size];

    /* fill in */
    entry->index_nr = avi->index_size++;
    entry->stream_nr = stream_nr;
    entry->flags = 0;
    entry->offset = gst_bytestream_tell (riff->bs) + 8;
    entry->size = GST_READ_UINT32_LE (&data[4]);

    /* timestamps */
    if (stream->strh->samplesize && stream->strh->type == GST_RIFF_FCC_auds) {
      /* constant rate stream */
      gst_pad_convert (stream->pad, GST_FORMAT_BYTES,
          stream->total_bytes, &format, &entry->ts);
      gst_pad_convert (stream->pad, GST_FORMAT_BYTES,
          stream->total_bytes + entry->size, &format, &entry->dur);
    } else {
      /* VBR stream */
      gst_pad_convert (stream->pad, GST_FORMAT_DEFAULT,
          stream->total_frames, &format, &entry->ts);
      gst_pad_convert (stream->pad, GST_FORMAT_DEFAULT,
          stream->total_frames + 1, &format, &entry->dur);
    }
    entry->dur -= entry->ts;

    /* stream position */
    entry->bytes_before = stream->total_bytes;
    stream->total_bytes += entry->size;
    entry->frames_before = stream->total_frames;
    stream->total_frames++;

  next:
    if (!gst_riff_read_skip (riff))
      break;
  }

  avi->index_offset = 0;

  /* seek back */
  if (!(event = gst_riff_read_seek (riff, pos)))
    return FALSE;
  gst_event_unref (event);

  GST_LOG_OBJECT (avi, "index created, %d items", avi->index_size);

  return TRUE;
}

/*
 * Massage index.
 * We're going to go over each entry in the index and finetune
 * some things we don't like about AVI. For example, a single
 * chunk might be too long. Also, individual streams might be
 * out-of-sync. In the first case, we cut the chunk in several
 * smaller pieces. In the second case, we re-order chunk reading
 * order. The end result should be a smoother playing AVI.
 */

static void
gst_avi_demux_massage_index (GstAviDemux * avi)
{
  gst_avi_index_entry *entry;
  avi_stream_context *stream;
  gint i;

  /* init frames */
  for (i = 0; i < avi->num_streams; i++) {
    stream = &avi->stream[i];
    if (stream->strh->type == GST_RIFF_FCC_vids)
      stream->delay = stream->strh->init_frames * GST_SECOND *
          stream->strh->scale / stream->strh->rate;
    else
      stream->delay = GST_SECOND * stream->strh->init_frames *
          stream->strh->length / (stream->total_frames * stream->bitrate);
  }
  for (i = 0; i < avi->index_size; i++) {
    entry = &avi->index_entries[i];

    if (entry->stream_nr >= avi->num_streams)
      continue;

    stream = &avi->stream[entry->stream_nr];
    entry->ts += stream->delay;
  }

  /* cut chunks in small (seekable) pieces */
  for (i = 0; i < avi->index_size; i++) {
    entry = &avi->index_entries[i];

    if (entry->stream_nr >= avi->num_streams)
      continue;

#define MAX_DURATION (GST_SECOND / 4)

    /* check for max duration of a single buffer. I suppose that
     * the allocation of index entries could be improved. */
    stream = &avi->stream[entry->stream_nr];
    if (entry->dur > MAX_DURATION && stream->strh->type == GST_RIFF_FCC_auds) {
      guint32 ideal_size = stream->bitrate / 10;
      gst_avi_index_entry *entries;
      gint old_size, n, num_added;

      /* copy index */
      old_size = entry->size;
      num_added = (entry->size - 1) / ideal_size;
      avi->index_size += num_added;
      entries = g_malloc (sizeof (gst_avi_index_entry) * avi->index_size);
      memcpy (entries, avi->index_entries,
          sizeof (gst_avi_index_entry) * (entry->index_nr + 1));
      if (entry->index_nr < avi->index_size - num_added - 1) {
        memcpy (&entries[entry->index_nr + 1 + num_added],
            &avi->index_entries[entry->index_nr + 1],
            (avi->index_size - num_added - entry->index_nr - 1) *
            sizeof (gst_avi_index_entry));
        for (n = entry->index_nr + 1 + num_added; n < avi->index_size; n++) {
          entries[n].index_nr += num_added;
          if (entries[n].stream_nr == entry->stream_nr)
            entries[n].frames_before += num_added;
        }
      }

      /* new sized index chunks */
      for (n = entry->index_nr; n < entry->index_nr + num_added + 1; n++) {
        if (old_size >= ideal_size) {
          entries[n].size = ideal_size;
          old_size -= ideal_size;
        } else
          entries[n].size = old_size;

        entries[n].dur = GST_SECOND * entries[n].size / stream->bitrate;
        if (n != entry->index_nr) {
          memcpy (&entries[n], &entries[n - 1], sizeof (gst_avi_index_entry));
          entries[n].index_nr++;
          entries[n].ts += entries[n - 1].dur;
          entries[n].offset += entries[n - 1].size;
          entries[n].bytes_before += entries[n - 1].size;
          entries[n].frames_before++;

          i++;
        }
      }

      /* set new pointer */
      g_free (avi->index_entries);
      avi->index_entries = entries;
    }
  }

  /* re-order for time */
  for (i = 1; i < avi->index_size; i++) {
    entry = &avi->index_entries[i];

    if (entry->stream_nr >= avi->num_streams)
      continue;

    /* check whether to rearrange according to time */
    while (i > 0 && avi->index_entries[i - 1].stream_nr < avi->num_streams &&
        (entry->ts < avi->index_entries[i - 1].ts ||
            (entry->ts == avi->index_entries[i - 1].ts &&
                entry->stream_nr < avi->index_entries[i - 1].stream_nr))) {
      gst_avi_index_entry prev_entry;

      /* move around */
      memcpy (&prev_entry, &avi->index_entries[i - 1],
          sizeof (gst_avi_index_entry));
      entry->index_nr--;
      memcpy (&avi->index_entries[i - 1], entry, sizeof (gst_avi_index_entry));
      memcpy (entry, &prev_entry, sizeof (gst_avi_index_entry));
      entry->index_nr++;

      /* update pointer */
      entry = &avi->index_entries[i - 1];
      i--;
    }
  }
}

/*
 * Read full AVI headers.
 */

gboolean
gst_avi_demux_stream_header (GstAviDemux * avi)
{
  GstRiffRead *riff = GST_RIFF_READ (avi);
  guint32 tag, flags, streams;

  /* the header consists of a 'hdrl' LIST tag */
  if (!(tag = gst_riff_peek_tag (riff, NULL)))
    return FALSE;
  if (tag != GST_RIFF_TAG_LIST) {
    GST_ELEMENT_ERROR (avi, STREAM, DEMUX, (NULL),
        ("Invalid AVI header (no LIST at start): "
            GST_FOURCC_FORMAT, GST_FOURCC_ARGS (tag)));
    return FALSE;
  }
  if (!gst_riff_read_list (riff, &tag))
    return FALSE;
  if (tag != GST_RIFF_LIST_hdrl) {
    GST_ELEMENT_ERROR (avi, STREAM, DEMUX, (NULL),
        ("Invalid AVI header (no hdrl at start): "
            GST_FOURCC_FORMAT, GST_FOURCC_ARGS (tag)));
    return FALSE;
  }

  /* the hdrl starts with a 'avih' header */
  if (!(tag = gst_riff_peek_tag (riff, NULL)))
    return FALSE;
  if (tag != GST_RIFF_TAG_avih) {
    GST_ELEMENT_ERROR (avi, STREAM, DEMUX, (NULL),
        ("Invalid AVI header (no avih at start): "
            GST_FOURCC_FORMAT, GST_FOURCC_ARGS (tag)));
    return FALSE;
  }
  if (!gst_avi_demux_stream_avih (avi, &flags, &streams))
    return FALSE;

  /* now, read the elements from the header until the end */
  while (TRUE) {
    if (!(tag = gst_riff_peek_tag (riff, &avi->level_up)))
      return FALSE;
    else if (avi->level_up) {
      avi->level_up--;
      break;
    }

    switch (tag) {
      case GST_RIFF_TAG_LIST:
        if (!(tag = gst_riff_peek_list (riff)))
          return FALSE;

        switch (tag) {
          case GST_RIFF_LIST_strl:
            if (!gst_riff_read_list (riff, &tag) ||
                !gst_avi_demux_add_stream (avi))
              return FALSE;
            break;

          case GST_RIFF_LIST_odml:
            if (!gst_riff_read_list (riff, &tag) ||
                !gst_avi_demux_stream_odml (avi))
              return FALSE;
            break;

          default:
            GST_WARNING ("Unknown list " GST_FOURCC_FORMAT " in AVI header",
                GST_FOURCC_ARGS (tag));
            /* fall-through */

          case GST_RIFF_TAG_JUNK:
            if (!gst_riff_read_skip (riff))
              return FALSE;
            break;
        }

        break;

      default:
        GST_WARNING ("Unknown tag " GST_FOURCC_FORMAT " in AVI header",
            GST_FOURCC_ARGS (tag));
        /* fall-through */

      case GST_RIFF_TAG_JUNK:
        if (!gst_riff_read_skip (riff))
          return FALSE;
        break;
    }

    if (avi->level_up) {
      avi->level_up--;
      break;
    }
  }

  if (avi->num_streams != streams) {
    g_warning ("Stream header mentioned %d streams, but %d available",
        streams, avi->num_streams);
  }
  /* at this point we know all the streams and we can signal the no more
   * pads signal */
  GST_DEBUG ("signaling no more pads");
  gst_element_no_more_pads (GST_ELEMENT (avi));

  /* Now, find the data (i.e. skip all junk between header and data) */
  while (1) {
    if (!(tag = gst_riff_peek_tag (riff, NULL)))
      return FALSE;
    if (tag != GST_RIFF_TAG_LIST) {
      if (!gst_riff_read_skip (riff))
        return FALSE;
      continue;
    }
    if (!(tag = gst_riff_peek_list (riff)))
      return FALSE;
    if (tag != GST_RIFF_LIST_movi) {
      if (tag == GST_RIFF_LIST_INFO) {
        if (!gst_riff_read_list (riff, &tag) || !gst_riff_read_info (riff))
          return FALSE;
      } else if (!gst_riff_read_skip (riff)) {
        return FALSE;
      }
      continue;
    }
    break;
  }

  /* create or read stream index (for seeking) */
  if (flags & GST_RIFF_AVIH_HASINDEX) {
    if (!gst_avi_demux_stream_index (avi))
      return FALSE;
  }
  if (!avi->index_size) {
    if (!gst_avi_demux_stream_scan (avi))
      return FALSE;
  }
  gst_avi_demux_massage_index (avi);

  return TRUE;
}

/*
 * Handle seek.
 */

static gboolean
gst_avi_demux_handle_seek (GstAviDemux * avi)
{
  guint i;
  GstEvent *event;

  /* FIXME: if we seek in an openDML file, we will have multiple
   * primary levels. Seeking in between those will cause havoc. */

  avi->current_entry = avi->seek_entry;

  for (i = 0; i < avi->num_streams; i++) {
    avi_stream_context *stream = &avi->stream[i];

    if (GST_PAD_IS_USABLE (stream->pad)) {
      if (avi->seek_flush) {
        event = gst_event_new (GST_EVENT_FLUSH);
        gst_pad_push (stream->pad, GST_DATA (event));
      }
      event = gst_event_new_discontinuous (FALSE, GST_FORMAT_TIME,
          avi->last_seek, NULL);
      gst_pad_push (stream->pad, GST_DATA (event));
    }
  }

  return TRUE;
}

static gboolean
gst_avi_demux_process_next_entry (GstAviDemux * avi)
{
  GstRiffRead *riff = GST_RIFF_READ (avi);
  gboolean processed;

  for (processed = FALSE; !processed;) {
    if (avi->current_entry >= avi->index_size) {
      gst_bytestream_seek (riff->bs, 0, GST_SEEK_METHOD_END);

      /* get eos */
      gst_riff_peek_tag (GST_RIFF_READ (avi), &avi->level_up);
      gst_pad_event_default (avi->sinkpad, gst_event_new (GST_EVENT_EOS));
      processed = TRUE;
    } else {
      GstBuffer *buf;
      guint got;
      gst_avi_index_entry *entry = &avi->index_entries[avi->current_entry++];
      avi_stream_context *stream;

      if (entry->stream_nr >= avi->num_streams) {
        continue;
      }

      stream = &avi->stream[entry->stream_nr];

      if (GST_PAD_IS_USABLE (stream->pad) && entry->size > 0) {
        guint64 needed_off = entry->offset + avi->index_offset, pos;
        guint32 remain;

        pos = gst_bytestream_tell (riff->bs);
        gst_bytestream_get_status (riff->bs, &remain, NULL);
        if (pos <= needed_off && needed_off - pos <= remain) {
          gst_bytestream_flush_fast (riff->bs, needed_off - pos);
        } else {
          GstEvent *event;

          event = gst_riff_read_seek (riff, needed_off);
          if (event)
            gst_event_unref (event);
          else {
            GST_ELEMENT_ERROR (avi, RESOURCE, READ, (NULL), (NULL));
            return FALSE;
          }
        }
        if (!(buf = gst_riff_read_element_data (riff, entry->size, &got))) {
          return FALSE;
        }
        if (entry->flags & GST_RIFF_IF_KEYFRAME) {
          GST_BUFFER_FLAG_SET (buf, GST_BUFFER_KEY_UNIT);
        }
        GST_BUFFER_TIMESTAMP (buf) = entry->ts;
        GST_BUFFER_DURATION (buf) = entry->dur;
        GST_DEBUG_OBJECT (avi, "Processing buffer of size %d and time %"
            GST_TIME_FORMAT " on pad %s",
            GST_BUFFER_SIZE (buf), GST_TIME_ARGS (GST_BUFFER_TIMESTAMP (buf)),
            gst_pad_get_name (stream->pad));
        gst_pad_push (stream->pad, GST_DATA (buf));
        processed = TRUE;
      }
      stream->current_frame++;
      stream->current_byte += entry->size;
    }
  }

  return TRUE;
}

/*
 * Read data.
 */

gboolean
gst_avi_demux_stream_data (GstAviDemux * avi)
{
  GstRiffRead *riff = GST_RIFF_READ (avi);
  guint32 tag;
  guint stream_nr;

  if (avi->seek_offset != (guint64) - 1) {
    if (!gst_avi_demux_handle_seek (avi))
      return FALSE;
    avi->seek_offset = (guint64) - 1;
  }

  /* if we have a avi->index_entries[], we don't want to read
   * the stream linearly, but seek to the next ts/index_entry. */
  if (avi->index_entries != NULL) {
    return gst_avi_demux_process_next_entry (avi);
  }

  if (!gst_avi_demux_sync (avi, &tag, FALSE))
    return FALSE;
  stream_nr = CHUNKID_TO_STREAMNR (tag);

  if (stream_nr < 0 || stream_nr >= avi->num_streams) {
    /* recoverable */
    g_warning ("Invalid stream ID %d (" GST_FOURCC_FORMAT ")",
        stream_nr, GST_FOURCC_ARGS (tag));
    if (!gst_riff_read_skip (riff))
      return FALSE;
  } else {
    avi_stream_context *stream;
    GstClockTime next_ts;
    GstFormat format;
    GstBuffer *buf;

    /* get buffer */
    if (!gst_riff_read_data (riff, &tag, &buf))
      return FALSE;

    /* get time of this buffer */
    stream = &avi->stream[stream_nr];
    format = GST_FORMAT_TIME;
    gst_pad_query (stream->pad, GST_QUERY_POSITION, &format, &next_ts);

    /* set delay (if any) */
    if (stream->strh->init_frames == stream->current_frame &&
        stream->delay == 0)
      stream->delay = next_ts;

    stream->current_frame++;
    stream->current_byte += GST_BUFFER_SIZE (buf);

    /* should we skip this data? */
    if (stream->skip) {
      stream->skip--;
      gst_buffer_unref (buf);
    } else {
      if (!stream->pad || !GST_PAD_IS_USABLE (stream->pad)) {
        gst_buffer_unref (buf);
      } else {
        GstClockTime dur_ts;

        GST_BUFFER_TIMESTAMP (buf) = next_ts;
        gst_pad_query (stream->pad, GST_QUERY_POSITION, &format, &dur_ts);
        GST_BUFFER_DURATION (buf) = dur_ts - next_ts;
        GST_DEBUG ("Pushing buffer with time=%" GST_TIME_FORMAT " over pad %s",
            GST_TIME_ARGS (next_ts), gst_pad_get_name (stream->pad));
        gst_pad_push (stream->pad, GST_DATA (buf));
      }
    }
  }

  return TRUE;
}

static void
gst_avi_demux_loop (GstElement * element)
{
  GstAviDemux *avi = GST_AVI_DEMUX (element);

  switch (avi->state) {
    case GST_AVI_DEMUX_START:
      if (!gst_avi_demux_stream_init (avi))
        return;
      avi->state = GST_AVI_DEMUX_HEADER;
      /* fall-through */

    case GST_AVI_DEMUX_HEADER:
      if (!gst_avi_demux_stream_header (avi))
        return;
      avi->state = GST_AVI_DEMUX_MOVI;
      break;

    case GST_AVI_DEMUX_MOVI:
      if (!gst_avi_demux_stream_data (avi))
        return;
      break;

    default:
      g_assert (0);
  }
}

static GstElementStateReturn
gst_avi_demux_change_state (GstElement * element)
{
  GstAviDemux *avi = GST_AVI_DEMUX (element);

  switch (GST_STATE_TRANSITION (element)) {
    case GST_STATE_PAUSED_TO_READY:
      gst_avi_demux_reset (avi);
      break;
    default:
      break;
  }

  if (GST_ELEMENT_CLASS (parent_class)->change_state)
    return GST_ELEMENT_CLASS (parent_class)->change_state (element);

  return GST_STATE_SUCCESS;
}
