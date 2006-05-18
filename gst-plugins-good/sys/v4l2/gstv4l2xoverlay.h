/* G-Streamer generic V4L2 element - X overlay interface implementation
 * Copyright (C) 2003 Ronald Bultje <rbultje@ronald.bitfreak.net>
 * Copyright (C) 2006 Edgard Lima <edgard.lima@indt.org.br>
 *
 * gstv4l2xoverlay.h: tv mixer interface implementation for V4L2
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

#ifndef __GST_V4L2_X_OVERLAY_H__
#define __GST_V4L2_X_OVERLAY_H__

#include <X11/X.h>

#include <gst/gst.h>
#include <gst/interfaces/xoverlay.h>

#include "gstv4l2object.h"

G_BEGIN_DECLS

void gst_v4l2_xoverlay_start (GstV4l2Object  *v4l2object);
void gst_v4l2_xoverlay_stop  (GstV4l2Object  *v4l2object);

extern void
gst_v4l2_xoverlay_interface_init (GstXOverlayClass * klass);

#define GST_IMPLEMENT_V4L2_XOVERLAY_METHODS(Type, interface_as_function)              \
                                                                                      \
static void                                                                           \
interface_as_function ## _xoverlay_set_xwindow_id (GstXOverlay * xoverlay,            \
                                                   XID xwindow_id)                    \
{                                                                                     \
  Type *this = (Type*) xoverlay;                                                      \
  gst_v4l2_xoverlay_set_xwindow_id (this->v4l2object, xwindow_id);                    \
}                                                                                     \
                                                                                      \
static void                                                                           \
interface_as_function ## _xoverlay_interface_init (GstXOverlayClass * klass)          \
{                                                                                     \
  /* default virtual functions */                                                     \
  klass->set_xwindow_id = interface_as_function ## _xoverlay_set_xwindow_id;          \
                                                                                      \
  gst_v4l2_xoverlay_interface_init(GstXOverlayClass * klass);                         \
}                                                                                     \
                                                                                      \


#endif /* __GST_V4L2_X_OVERLAY_H__ */
