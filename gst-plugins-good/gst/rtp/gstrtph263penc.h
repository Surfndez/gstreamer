/* Gnome-Streamer
 * Copyright (C) <2005> Wim Taymans <wim@fluendo.com>
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

#ifndef __GST_RTP_H263P_ENC_H__
#define __GST_RTP_H263P_ENC_H__

#include <gst/gst.h>
#include <gst/rtp/gstbasertppayload.h>
#include <gst/base/gstadapter.h>

G_BEGIN_DECLS

#define GST_TYPE_RTP_H263P_ENC \
  (gst_rtph263penc_get_type())
#define GST_RTP_H263P_ENC(obj) \
  (G_TYPE_CHECK_INSTANCE_CAST((obj),GST_TYPE_RTP_H263P_ENC,GstRtpH263PEnc))
#define GST_RTP_H263P_ENC_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_CAST((klass),GST_TYPE_RTP_H263P_ENC,GstRtpH263PEnc))
#define GST_IS_RTP_H263P_ENC(obj) \
  (G_TYPE_CHECK_INSTANCE_TYPE((obj),GST_TYPE_RTP_H263P_ENC))
#define GST_IS_RTP_H263P_ENC_CLASS(obj) \
  (G_TYPE_CHECK_CLASS_TYPE((klass),GST_TYPE_RTP_H263P_ENC))

typedef struct _GstRtpH263PEnc GstRtpH263PEnc;
typedef struct _GstRtpH263PEncClass GstRtpH263PEncClass;

struct _GstRtpH263PEnc
{
  GstBaseRTPPayload payload;

  GstAdapter *adapter;
  GstClockTime first_ts;
};

struct _GstRtpH263PEncClass
{
  GstBaseRTPPayloadClass parent_class;
};

gboolean gst_rtph263penc_plugin_init (GstPlugin * plugin);

G_END_DECLS

#endif /* __GST_RTP_H263P_ENC_H__ */
