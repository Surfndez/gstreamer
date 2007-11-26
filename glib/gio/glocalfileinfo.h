/* GIO - GLib Input, Output and Streaming Library
 * 
 * Copyright (C) 2006-2007 Red Hat, Inc.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General
 * Public License along with this library; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place, Suite 330,
 * Boston, MA 02111-1307, USA.
 *
 * Author: Alexander Larsson <alexl@redhat.com>
 */

#ifndef __G_LOCAL_FILE_INFO_H__
#define __G_LOCAL_FILE_INFO_H__

#include <gio/gfileinfo.h>
#include <gio/gfile.h>
#include <sys/stat.h>

G_BEGIN_DECLS

typedef struct {
  gboolean writable;
  gboolean is_sticky;
  int owner;
  dev_t device;
} GLocalParentFileInfo;

void       _g_local_file_info_get_parent_info (const char                 *dir,
					       GFileAttributeMatcher      *attribute_matcher,
					       GLocalParentFileInfo       *parent_info);
GFileInfo *_g_local_file_info_get             (const char                 *basename,
					       const char                 *path,
					       GFileAttributeMatcher      *attribute_matcher,
					       GFileQueryInfoFlags         flags,
					       GLocalParentFileInfo       *parent_info,
					       GError                    **error);
GFileInfo *_g_local_file_info_get_from_fd     (int                         fd,
					       char                       *attributes,
					       GError                    **error);
char *     _g_local_file_info_create_etag     (struct stat                *statbuf);
gboolean   _g_local_file_info_set_attribute   (char                       *filename,
					       const char                 *attribute,
					       const GFileAttributeValue  *value,
					       GFileQueryInfoFlags         flags,
					       GCancellable               *cancellable,
					       GError                    **error);
gboolean   _g_local_file_info_set_attributes  (char                       *filename,
					       GFileInfo                  *info,
					       GFileQueryInfoFlags         flags,
					       GCancellable               *cancellable,
					       GError                    **error);

G_END_DECLS

#endif /* __G_FILE_LOCAL_FILE_INFO_H__ */


