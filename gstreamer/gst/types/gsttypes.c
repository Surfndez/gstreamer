/* Gnome-Streamer
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


#include <gst/gst.h>
#include <string.h>

GstTypeFactory _factories[] = {
  { "audio/raw", ".raw", NULL },
  { "video/raw image/raw", ".raw", NULL },
  { NULL, NULL, NULL },
};


static gboolean
plugin_init (GModule *module, GstPlugin *plugin)
{
  gint i = 0;

  while (_factories[i].mime) {
    gst_type_register (&_factories[i]);
    gst_plugin_add_type (plugin, &_factories[i]);
    GST_DEBUG(0, "added factory #%d '%s'\n",i,_factories[i].mime);
    i++;
  }

  //gst_info ("gsttypes: loaded %d standard types\n",i);

  return TRUE;
}

GstPluginDesc plugin_desc = {
  GST_VERSION_MAJOR,
  GST_VERSION_MINOR,
  "gsttypes",
  plugin_init
};
