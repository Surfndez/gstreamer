/* 
 * The contents of this file are subject to the Mozilla Public License
 * Version 1.1 (the "License"); you may not use this file except in
 * compliance with the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/.
 *
 * Software distributed under the License is distributed on an "AS IS"
 * basis, WITHOUT WARRANTY OF ANY KIND, either express or implied. See the
 * License for the specific language governing rights and limitations
 * under the License.
 *
 * The Original Code is Fluendo MPEG Demuxer plugin.
 *
 * The Initial Developer of the Original Code is Fluendo, S.L.
 * Portions created by Fluendo, S.L. are Copyright (C) 2005
 * Fluendo, S.L. All Rights Reserved.
 *
 * Contributor(s): Jan Schmidt <jan@fluendo.com>
 */

#ifndef __MPEGTS_PMT_INFO_H__
#define __MPEGTS_PMT_INFO_H__

#include <glib.h>
#include "flutspmtstreaminfo.h"

G_BEGIN_DECLS

typedef struct MpegTSPmtInfoClass {
  GObjectClass parent_class;
} MpegTSPmtInfoClass;

typedef struct MpegTSPmtInfo {
  GObject parent;

  guint16 program_no;
  guint16 pcr_pid;

  guint8 version_no;

  GValueArray *descriptors;
  GValueArray *streams;
} MpegTSPmtInfo;

MpegTSPmtInfo *mpegts_pmt_info_new (guint16 program_no, guint16 pcr_pid, guint8 version);
void mpegts_pmt_info_add_stream (MpegTSPmtInfo *pmt_info, MpegTSPmtStreamInfo *stream);
void mpegts_pmt_info_add_descriptor (MpegTSPmtInfo *pmt_info,	
  const gchar *descriptor, guint length);

GType mpegts_pmt_info_get_type (void);

#define MPEGTS_TYPE_PMT_INFO (mpegts_pmt_info_get_type ())
#define MPEGTS_IS_PMT_INFO(obj) (G_TYPE_CHECK_INSTANCE_TYPE((obj), MPEGTS_TYPE_PMT_INFO))
#define MPEGTS_PMT_INFO(obj) (G_TYPE_CHECK_INSTANCE_CAST((obj),MPEGTS_TYPE_PMT_INFO, MpegTSPmtInfo))

G_END_DECLS

#endif
