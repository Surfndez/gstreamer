/*
 * Siren Payloader Gst Element
 *
 *   @author: Youness Alaoui <kakaroto@kakaroto.homelinux.net>
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

#ifndef __GST_RTP_SIREN_PAY_H__
#define __GST_RTP_SIREN_PAY_H__

#include <gst/gst.h>
#include <gst/rtp/gstbasertpaudiopayload.h>

G_BEGIN_DECLS

#define GST_TYPE_RTP_SIREN_PAY \
  (gst_rtpsirenpay_get_type())
#define GST_RTP_SIREN_PAY(obj) \
  (G_TYPE_CHECK_INSTANCE_CAST((obj),GST_TYPE_RTP_SIREN_PAY,GstRTPSirenPay))
#define GST_RTP_SIREN_PAY_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_CAST((klass),GST_TYPE_RTP_SIREN_PAY,GstRTPSirenPayClass))
#define GST_IS_RTP_SIREN_PAY(obj) \
  (G_TYPE_CHECK_INSTANCE_TYPE((obj),GST_TYPE_RTP_SIREN_PAY))
#define GST_IS_RTP_SIREN_PAY_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_TYPE((klass),GST_TYPE_RTP_SIREN_PAY))

typedef struct _GstRTPSirenPay GstRTPSirenPay;
typedef struct _GstRTPSirenPayClass GstRTPSirenPayClass;

struct _GstRTPSirenPay
{
  GstBaseRTPAudioPayload audiopayload;
};

struct _GstRTPSirenPayClass
{
  GstBaseRTPAudioPayloadClass parent_class;
};

gboolean gst_rtp_siren_pay_plugin_init (GstPlugin * plugin);

G_END_DECLS

#endif /* __GST_RTP_SIREN_PAY_H__ */
