/* GStreamer
 * Copyright (C) <2007> Wim Taymans <wim.taymans@gmail.com>
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

#ifndef __GST_RTP_BIN_H__
#define __GST_RTP_BIN_H__

#include <gst/gst.h>

#include "rtpsession.h"

#define GST_TYPE_RTP_BIN \
  (gst_rtp_bin_get_type())
#define GST_RTP_BIN(obj) \
  (G_TYPE_CHECK_INSTANCE_CAST((obj),GST_TYPE_RTP_BIN,GstRtpBin))
#define GST_RTP_BIN_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_CAST((klass),GST_TYPE_RTP_BIN,GstRtpBinClass))
#define GST_IS_RTP_BIN(obj) \
  (G_TYPE_CHECK_INSTANCE_TYPE((obj),GST_TYPE_RTP_BIN))
#define GST_IS_RTP_BIN_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_TYPE((klass),GST_TYPE_RTP_BIN))

typedef struct _GstRtpBin GstRtpBin;
typedef struct _GstRtpBinClass GstRtpBinClass;
typedef struct _GstRtpBinPrivate GstRtpBinPrivate;

struct _GstRtpBin {
  GstBin         bin;

  /*< private >*/
  /* default latency for sessions */
  guint           latency;
  gboolean        do_lost;
  /* a list of session */
  GSList         *sessions;
  /* clock we provide */
  GstClock       *provided_clock;

  /* a list of clients, these are streams with the same CNAME */
  GSList         *clients;

  /* the default SDES items for sessions */
  gchar          *sdes[9];

  /*< private >*/
  GstRtpBinPrivate *priv;
};

struct _GstRtpBinClass {
  GstBinClass  parent_class;

  /* get the caps for pt */
  GstCaps*    (*request_pt_map)       (GstRtpBin *rtpbin, guint session, guint pt);

  /* action signals */
  void        (*clear_pt_map)         (GstRtpBin *rtpbin);
  void        (*reset_sync)           (GstRtpBin *rtpbin);
  RTPSession* (*get_internal_session) (GstRtpBin *rtpbin, guint session_id);

  /* session manager signals */
  void     (*on_new_ssrc)       (GstRtpBin *rtpbin, guint session, guint32 ssrc);
  void     (*on_ssrc_collision) (GstRtpBin *rtpbin, guint session, guint32 ssrc);
  void     (*on_ssrc_validated) (GstRtpBin *rtpbin, guint session, guint32 ssrc);
  void     (*on_ssrc_active)    (GstRtpBin *rtpbin, guint session, guint32 ssrc);
  void     (*on_ssrc_sdes)      (GstRtpBin *rtpbin, guint session, guint32 ssrc);
  void     (*on_bye_ssrc)       (GstRtpBin *rtpbin, guint session, guint32 ssrc);
  void     (*on_bye_timeout)    (GstRtpBin *rtpbin, guint session, guint32 ssrc);
  void     (*on_timeout)        (GstRtpBin *rtpbin, guint session, guint32 ssrc);
  void     (*on_sender_timeout) (GstRtpBin *rtpbin, guint session, guint32 ssrc);
  void     (*on_npt_stop)       (GstRtpBin *rtpbin, guint session, guint32 ssrc);
};

GType gst_rtp_bin_get_type (void);

#endif /* __GST_RTP_BIN_H__ */
