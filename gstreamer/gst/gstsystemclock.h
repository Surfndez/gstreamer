/* GStreamer
 * Copyright (C) 1999,2000 Erik Walthinsen <omega@cse.ogi.edu>
 *                    2000 Wim Taymans <wtay@chello.be>
 *
 * gstclock.h: Header for clock subsystem
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


#ifndef __GST_SYSTEM_CLOCK_H__
#define __GST_SYSTEM_CLOCK_H__

#include <gst/gstclock.h>

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

#define GST_TYPE_SYSTEM_CLOCK \
  (gst_system_clock_get_type())
#define GST_SYSTEM_CLOCK(obj) \
  (G_TYPE_CHECK_INSTANCE_CAST((obj),GST_TYPE_SYSTEM_CLOCK,GstSystemClock))
#define GST_SYSTEM_CLOCK_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_CAST((klass),GST_TYPE_SYSTEM_CLOCK,GstSystemClockClass))
#define GST_IS_SYSTEM_CLOCK(obj) \
  (G_TYPE_CHECK_INSTANCE_TYPE((obj),GST_TYPE_SYSTEM_CLOCK))
#define GST_IS_SYSTEM_CLOCK_CLASS(obj) \
  (G_TYPE_CHECK_CLASS_TYPE((klass),GST_TYPE_SYSTEM_CLOCK))


typedef struct _GstSystemClock GstSystemClock;
typedef struct _GstSystemClockClass GstSystemClockClass;

struct _GstSystemClock {
  GstClock clock;
};

struct _GstSystemClockClass {
  GstClockClass parent_class;
};

GType 			gst_system_clock_get_type 	(void);

GstClock*		gst_system_clock_obtain		(void);

#ifdef __cplusplus
}
#endif /* __cplusplus */


#endif /* __GST_SYSTEM_CLOCK_H__ */
