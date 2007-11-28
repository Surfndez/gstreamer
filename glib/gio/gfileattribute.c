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

#include <config.h>

#include <string.h>

#include "gfileattribute.h"
#include <glib-object.h>
#include "glibintl.h"

#include "gioalias.h"

/**
 * SECTION:gfileattribute
 * @short_description: Key-Value Paired File Attributes
 * @see_also: #GFile, #GFileInfo
 * 
 * File attributes in GIO consist of a list of key-value pairs. 
 * 
 * Keys are strings that contain a key namespace and a key name, separated
 * by a colon, e.g. "namespace:keyname". Namespaces are included to sort
 * key-value pairs by namespaces for relevance. Keys can be searched 
 * for using wildcards, e.g. "std:*" will return all of the keys in the 
 * "std" namespace.
 * 
 * Values are stored within the list in #GFileAttributeValue structures.
 * Values can store different types, listed in the enum #GFileAttributeType.
 * Upon creation of a #GFileAttributeValue, the type will be set to 
 * %G_FILE_ATTRIBUTE_TYPE_INVALID. 
 * 
 * The Key-value list is stored within the #GFile structure as a 
 * #GFileAttributeInfoList. This list is queryable by key names 
 * as indicated earlier.
 * 
 * Classes that implement #GFileIface will create a #GFileAttributeInfoList and 
 * install default keys and values for their given file system, architecture, 
 * and other possible implementation details (e.g., on a UNIX system, a file 
 * attribute key will be registered for the user id for a given file). Other 
 * attributes can be appended to a #GFileAttributeList later by 
 * g_file_attribute_info_list_add(). 
 * 
 * <para>
 * <table>
 * <title>GFileAttributes Namespaces</title>
 * <tgroup cols='2' align='left'><thead>
 * <row><entry>Namspace</entry><entry>Description</entry></row>
 * </thead>
 * <tbody>
 * <row><entry>"std"</entry><entry>The "Standard" namespace. General file
 * information that any application may need should be put in this namespace. Examples
 * include the file's name, type, and size.</entry></row> 
 * <row><entry>"etag"</entry><entry>The "Entity Tag" namespace. Remotely shared
 * files, like those on HTTP/1.1 file systems, use "entity tags" to quickly determine 
 * if a file has been modified. Entity tags are globally unique, and should
 * always be sent with the current revision of a file. An example of a key
 * in this namespace is "value", which contains the value of the current
 * entity tag.</entry></row>
 * <row><entry>"id"</entry><entry>The "Identification" namespace. This 
 * namespace is used by file managers and applications that list directories
 * to check for loops and to uniquely identify files.</entry></row>
 * <row><entry>"access"</entry><entry>The "Access" namespace. Used to check
 * if a user has the proper privilidges to access files and perform
 * file operations. Keys in this namespace are made to be generic 
 * and easily understood, e.g. the "can_read" key is %TRUE if 
 * the current user has permission to read the file. UNIX permissions and
 * NTFS ACLs in Windows should be mapped to these values.</entry></row>
 * <row><entry>"mountable"</entry><entry>The "Mountable" namespace. Includes 
 * simple boolean keys for checking if a file or path supports mount operations, e.g.
 * mount, unmount, eject.</entry></row>
 * <row><entry>"time"</entry><entry>The "Time" namespace. Includes file 
 * access, changed, created times. </entry></row>
 * <row><entry>"unix"</entry><entry>The "Unix" namespace. Includes UNIX-specific
 * information and may not be available for all files. Examples include 
 * the UNIX "UID", "GID", etc.</entry></row>
 * <row><entry>"dos"</entry><entry>The "DOS" namespace. Includes DOS-specific 
 * information and may not be available for all files. Examples include
 * "is_system" for checking if a file is marked as a system file, and "is_archive"
 * for checking if a file is marked as an archive file.</entry></row>
 * <row><entry>"owner"</entry><entry>The "Owner" namespace. Includes information
 * about who owns a file. May not be available for all file systems. Examples include
 * "user" for getting the user name of the file owner.</entry></row>
 * <row><entry>"thumbnail"</entry><entry>The "Thumbnail" namespace. Includes 
 * information about file thumbnails and their location within the file system. Exaples of 
 * keys in this namespace include "path" to get the location of a thumbnail, and "failed"
 * to check if thumbnailing failed.</entry></row>
 * <row><entry>"fs"</entry><entry>The "Filesystem" namespace. Gets information
 * about the file system where a file is located, such as its type, how much
 * space is left available, and the overall size of the file system.</entry></row>
 * <row><entry>"gvfs"</entry><entry>The "GVFS" namespace. Keys in this namespace
 * contain information about the current GVFS backend in use. </entry></row>
 * </tbody>
 * </tgroup>
 * </table>
 * </para>
 * 
 * More namespaces can be added from GIO modules or by individual applications. 
 * For more information about writing GIO modules, see #GIOModule.
 * 
 * <para><table>
 * <title>GFileAttributes Built-in Keys and Value Types</title>
 * <tgroup cols='3' align='left'><thead>
 * <row><entry>Enum Value</entry><entry>Namespace:Key</entry><entry>Value Type</entry></row>
 * </thead><tbody>
 * <row><entry>%G_FILE_ATTRIBUTE_STD_TYPE</entry><entry>std:type</entry><entry>uint32 (#GFileType)</entry></row>
 * <row><entry>%G_FILE_ATTRIBUTE_STD_IS_HIDDEN</entry><entry>std:is_hidden</entry><entry>boolean</entry></row>
 * <row><entry>%G_FILE_ATTRIBUTE_STD_IS_BACKUP</entry><entry>std:is_backup</entry><entry>boolean</entry></row>
 * <row><entry>%G_FILE_ATTRIBUTE_STD_IS_SYMLINK</entry><entry>std:is_symlink</entry><entry>boolean</entry></row>
 * <row><entry>%G_FILE_ATTRIBUTE_STD_IS_VIRTUAL</entry><entry>std:is_virtual</entry><entry>boolean</entry></row>
 * <row><entry>%G_FILE_ATTRIBUTE_STD_NAME</entry><entry>std:name</entry><entry>byte string</entry></row>
 * <row><entry>%G_FILE_ATTRIBUTE_STD_DISPLAY_NAME</entry><entry>std:display_name</entry><entry>string</entry></row>
 * <row><entry>%G_FILE_ATTRIBUTE_STD_EDIT_NAME</entry><entry>std:edit_name</entry><entry>string</entry></row>
 * <row><entry>%G_FILE_ATTRIBUTE_STD_ICON</entry><entry>std:icon</entry><entry>object (#GIcon)</entry></row>
 * <row><entry>%G_FILE_ATTRIBUTE_STD_CONTENT_TYPE</entry><entry>std:content_type</entry><entry>string</entry></row>
 * <row><entry>%G_FILE_ATTRIBUTE_STD_FAST_CONTENT_TYPE</entry><entry>std:fast_content_type</entry><entry>string</entry></row>
 * <row><entry>%G_FILE_ATTRIBUTE_STD_SIZE</entry><entry>std:size</entry><entry>uint64</entry></row>
 * <row><entry>%G_FILE_ATTRIBUTE_STD_SYMLINK_TARGET</entry><entry>std:symlink_target</entry><entry>byte string</entry></row>
 * <row><entry>%G_FILE_ATTRIBUTE_STD_TARGET_URI</entry><entry>std:target_uri</entry><entry>string</entry></row>
 * <row><entry>%G_FILE_ATTRIBUTE_STD_SORT_ORDER</entry><entry>std:sort_order</entry><entry>int32</entry></row>
 * <row><entry>%G_FILE_ATTRIBUTE_ETAG_VALUE</entry><entry>etag:value</entry><entry>string</entry></row>
 * <row><entry>%G_FILE_ATTRIBUTE_ID_FILE</entry><entry>id:file</entry><entry>string</entry></row>
 * <row><entry>%G_FILE_ATTRIBUTE_ID_FS</entry><entry>id:fs</entry><entry>string</entry></row>
 * <row><entry>%G_FILE_ATTRIBUTE_ACCESS_CAN_READ</entry><entry>access:can_read</entry><entry>boolean</entry></row>
 * <row><entry>%G_FILE_ATTRIBUTE_ACCESS_CAN_WRITE</entry><entry>access:can_write</entry><entry>boolean</entry></row>
 * <row><entry>%G_FILE_ATTRIBUTE_ACCESS_CAN_EXECUTE</entry><entry>access:can_execute</entry><entry>boolean</entry></row>
 * <row><entry>%G_FILE_ATTRIBUTE_ACCESS_CAN_DELETE</entry><entry>access:can_delete</entry><entry>boolean</entry></row>
 * <row><entry>%G_FILE_ATTRIBUTE_ACCESS_CAN_TRASH</entry><entry>access:can_trash</entry><entry>boolean</entry></row>
 * <row><entry>%G_FILE_ATTRIBUTE_ACCESS_CAN_RENAME</entry><entry>access:can_rename</entry><entry>boolean</entry></row>
 * <row><entry>%G_FILE_ATTRIBUTE_MOUNTABLE_CAN_MOUNT</entry><entry>mountable:can_mount</entry><entry>boolean</entry></row>
 * <row><entry>%G_FILE_ATTRIBUTE_MOUNTABLE_CAN_UNMOUNT</entry><entry>mountable:can_unmount</entry><entry>boolean</entry></row>
 * <row><entry>%G_FILE_ATTRIBUTE_MOUNTABLE_CAN_EJECT</entry><entry>mountable:can_eject</entry><entry>boolean</entry></row>
 * <row><entry>%G_FILE_ATTRIBUTE_MOUNTABLE_UNIX_DEVICE</entry><entry>mountable:unix_device</entry><entry>uint32</entry></row>
 * <row><entry>%G_FILE_ATTRIBUTE_MOUNTABLE_HAL_UDI</entry><entry>mountable:hal_udi</entry><entry>string</entry></row>
 * <row><entry>%G_FILE_ATTRIBUTE_TIME_MODIFIED</entry><entry>time:modified</entry><entry>uint64</entry></row>
 * <row><entry>%G_FILE_ATTRIBUTE_TIME_MODIFIED_USEC</entry><entry>time:modified_usec</entry><entry>uint32</entry></row>
 * <row><entry>%G_FILE_ATTRIBUTE_TIME_ACCESS</entry><entry>time:access</entry><entry>uint64</entry></row>
 * <row><entry>%G_FILE_ATTRIBUTE_TIME_ACCESS_USEC</entry><entry>time:access_usec</entry><entry>uint32</entry></row>
 * <row><entry>%G_FILE_ATTRIBUTE_TIME_CHANGED</entry><entry>time:changed</entry><entry>uint64</entry></row>
 * <row><entry>%G_FILE_ATTRIBUTE_TIME_CHANGED_USEC</entry><entry>time:changed_usec</entry><entry>uint32</entry></row>
 * <row><entry>%G_FILE_ATTRIBUTE_TIME_CREATED</entry><entry>time:created</entry><entry>uint64</entry></row>
 * <row><entry>%G_FILE_ATTRIBUTE_TIME_CREATED_USEC</entry><entry>time:created_usec</entry><entry>uint32</entry></row>
 * <row><entry>%G_FILE_ATTRIBUTE_UNIX_DEVICE</entry><entry>unix:device</entry><entry>uint32</entry></row>
 * <row><entry>%G_FILE_ATTRIBUTE_UNIX_INODE</entry><entry>unix:inode</entry><entry>uint64</entry></row>
 * <row><entry>%G_FILE_ATTRIBUTE_UNIX_MODE</entry><entry>unix:mode</entry><entry>uint32</entry></row>
 * <row><entry>%G_FILE_ATTRIBUTE_UNIX_NLINK</entry><entry>unix:nlink</entry><entry>uint32</entry></row>
 * <row><entry>%G_FILE_ATTRIBUTE_UNIX_UID</entry><entry>unix:uid</entry><entry>uint32</entry></row>
 * <row><entry>%G_FILE_ATTRIBUTE_UNIX_GID</entry><entry>unix:gid</entry><entry>uint32</entry></row>
 * <row><entry>%G_FILE_ATTRIBUTE_UNIX_RDEV</entry><entry>unix:rdev</entry><entry>uint32</entry></row>
 * <row><entry>%G_FILE_ATTRIBUTE_UNIX_BLOCK_SIZE</entry><entry>unix:block_size</entry><entry>uint32</entry></row>
 * <row><entry>%G_FILE_ATTRIBUTE_UNIX_BLOCKS</entry><entry>unix:blocks</entry><entry>uint64</entry></row>
 * <row><entry>%G_FILE_ATTRIBUTE_UNIX_IS_MOUNTPOINT</entry><entry>unix:is_mountpoint</entry><entry>boolean</entry></row>
 * <row><entry>%G_FILE_ATTRIBUTE_DOS_IS_ARCHIVE</entry><entry>dos:is_archive</entry><entry>boolean</entry></row>
 * <row><entry>%G_FILE_ATTRIBUTE_DOS_IS_SYSTEM</entry><entry>dos:is_system</entry><entry>boolean</entry></row>
 * <row><entry>%G_FILE_ATTRIBUTE_OWNER_USER</entry><entry>owner:user</entry><entry>string</entry></row>
 * <row><entry>%G_FILE_ATTRIBUTE_OWNER_USER_REAL</entry><entry>owner:user_real</entry><entry>string</entry></row>
 * <row><entry>%G_FILE_ATTRIBUTE_OWNER_GROUP</entry><entry>owner:group</entry><entry>string</entry></row>
 * <row><entry>%G_FILE_ATTRIBUTE_THUMBNAIL_PATH</entry><entry>thumbnail:path</entry><entry>bytestring</entry></row>
 * <row><entry>%G_FILE_ATTRIBUTE_THUMBNAILING_FAILED</entry><entry>thumbnail:failed</entry><entry>boolean</entry></row>
 * <row><entry>%G_FILE_ATTRIBUTE_FS_SIZE</entry><entry>fs:size</entry><entry>uint64</entry></row>
 * <row><entry>%G_FILE_ATTRIBUTE_FS_FREE</entry><entry>fs:free</entry><entry>uint64</entry></row>
 * <row><entry>%G_FILE_ATTRIBUTE_FS_TYPE</entry><entry>fs:type</entry><entry>string</entry></row>
 * <row><entry>%G_FILE_ATTRIBUTE_FS_READONLY</entry><entry>fs:readonly</entry><entry>boolean</entry></row>
 * <row><entry>%G_FILE_ATTRIBUTE_GVFS_BACKEND</entry><entry>gvfs:backend</entry><entry>string</entry></row>
 * </tbody></tgroup></table></para>
 *  
 **/ 

/**
 * g_file_attribute_value_free:
 * @attr: a #GFileAttributeValue. 
 * 
 * Frees the memory used by @attr.
 *
 **/
void
g_file_attribute_value_free (GFileAttributeValue *attr)
{
  g_return_if_fail (attr != NULL);

  g_file_attribute_value_clear (attr);
  g_free (attr);
}

/**
 * g_file_attribute_value_clear:
 * @attr: a #GFileAttributeValue.
 *
 * Clears the value of @attr and sets its type to 
 * %G_FILE_ATTRIBUTE_TYPE_INVALID.
 * 
 **/
void
g_file_attribute_value_clear (GFileAttributeValue *attr)
{
  g_return_if_fail (attr != NULL);

  if (attr->type == G_FILE_ATTRIBUTE_TYPE_STRING ||
      attr->type == G_FILE_ATTRIBUTE_TYPE_BYTE_STRING)
    g_free (attr->u.string);
  
  if (attr->type == G_FILE_ATTRIBUTE_TYPE_OBJECT &&
      attr->u.obj != NULL)
    g_object_unref (attr->u.obj);
  
  attr->type = G_FILE_ATTRIBUTE_TYPE_INVALID;
}

/**
 * g_file_attribute_value_set:
 * @attr: a #GFileAttributeValue to set the value in.
 * @new_value: a #GFileAttributeValue to get the value from.
 * 
 * Sets an attribute's value from another attribute.
 **/
void
g_file_attribute_value_set (GFileAttributeValue *attr,
			    const GFileAttributeValue *new_value)
{
  g_return_if_fail (attr != NULL);
  g_return_if_fail (new_value != NULL);

  g_file_attribute_value_clear (attr);
  *attr = *new_value;

  if (attr->type == G_FILE_ATTRIBUTE_TYPE_STRING ||
      attr->type == G_FILE_ATTRIBUTE_TYPE_BYTE_STRING)
    attr->u.string = g_strdup (attr->u.string);
  
  if (attr->type == G_FILE_ATTRIBUTE_TYPE_OBJECT &&
      attr->u.obj != NULL)
    g_object_ref (attr->u.obj);
}

/**
 * g_file_attribute_value_new:
 * 
 * Creates a new file attribute.
 * 
 * Returns: a #GFileAttributeValue.
 **/
GFileAttributeValue *
g_file_attribute_value_new (void)
{
  GFileAttributeValue *attr;

  attr = g_new (GFileAttributeValue, 1);
  attr->type = G_FILE_ATTRIBUTE_TYPE_INVALID;
  return attr;
}


/**
 * g_file_attribute_value_dup:
 * @other: a #GFileAttributeValue to duplicate.
 * 
 * Duplicates a file attribute.
 * 
 * Returns: a duplicate of the @other.
 **/
GFileAttributeValue *
g_file_attribute_value_dup (const GFileAttributeValue *other)
{
  GFileAttributeValue *attr;

  g_return_val_if_fail (other != NULL, NULL);

  attr = g_new (GFileAttributeValue, 1);
  attr->type = G_FILE_ATTRIBUTE_TYPE_INVALID;
  g_file_attribute_value_set (attr, other);
  return attr;
}

static gboolean
valid_char (char c)
{
  return c >= 32 && c <= 126 && c != '\\';
}

static char *
escape_byte_string (const char *str)
{
  size_t len;
  int num_invalid, i;
  char *escaped_val, *p;
  unsigned char c;
  char *hex_digits = "0123456789abcdef";
  
  len = strlen (str);
  
  num_invalid = 0;
  for (i = 0; i < len; i++)
    {
      if (!valid_char (str[i]))
	num_invalid++;
    }
	
  if (num_invalid == 0)
    return g_strdup (str);
  else
    {
      escaped_val = g_malloc (len + num_invalid*3 + 1);

      p = escaped_val;
      for (i = 0; i < len; i++)
	{
	  c = str[i];
	  if (valid_char (c))
	    *p++ = c;
	  else
	    {
	      *p++ = '\\';
	      *p++ = 'x';
	      *p++ = hex_digits[(c >> 8) & 0xf];
	      *p++ = hex_digits[c & 0xf];
	    }
	}
      *p++ = 0;
      return escaped_val;
    }
}

/**
 * g_file_attribute_value_as_string:
 * @attr: a #GFileAttributeValue.
 *
 * Converts a #GFileAttributeValue to a string for display.
 * The returned string should be freed when no longer needed.
 * 
 * Returns: a string from the @attr, %NULL on error, or "&lt;invalid&gt;" 
 * if @attr is of type %G_FILE_ATTRIBUTE_TYPE_INVALID.
 **/
char *
g_file_attribute_value_as_string (const GFileAttributeValue *attr)
{
  char *str;

  g_return_val_if_fail (attr != NULL, NULL);

  switch (attr->type)
    {
    case G_FILE_ATTRIBUTE_TYPE_STRING:
      str = g_strdup (attr->u.string);
      break;
    case G_FILE_ATTRIBUTE_TYPE_BYTE_STRING:
      str = escape_byte_string (attr->u.string);
      break;
    case G_FILE_ATTRIBUTE_TYPE_BOOLEAN:
      str = g_strdup_printf ("%s", attr->u.boolean?"TRUE":"FALSE");
      break;
    case G_FILE_ATTRIBUTE_TYPE_UINT32:
      str = g_strdup_printf ("%u", (unsigned int)attr->u.uint32);
      break;
    case G_FILE_ATTRIBUTE_TYPE_INT32:
      str = g_strdup_printf ("%i", (int)attr->u.int32);
      break;
    case G_FILE_ATTRIBUTE_TYPE_UINT64:
      str = g_strdup_printf ("%"G_GUINT64_FORMAT, attr->u.uint64);
      break;
    case G_FILE_ATTRIBUTE_TYPE_INT64:
      str = g_strdup_printf ("%"G_GINT64_FORMAT, attr->u.int64);
      break;
    case G_FILE_ATTRIBUTE_TYPE_OBJECT:
      str = g_strdup_printf ("%s:%p", g_type_name_from_instance
                                          ((GTypeInstance *) attr->u.obj),
                                      attr->u.obj);
      break;
    default:
      g_warning ("Invalid type in GFileInfo attribute");
      str = g_strdup ("<invalid>");
      break;
    }
  
  return str;
}

/**
 * g_file_attribute_value_get_string:
 * @attr: a #GFileAttributeValue.
 * 
 * Gets the string from a file attribute value. If the value is not the
 * right type then %NULL will be returned.
 * 
 * Returns: the string value contained within the attribute, or %NULL.
 **/
const char *
g_file_attribute_value_get_string (const GFileAttributeValue *attr)
{
  if (attr == NULL)
    return NULL;

  g_return_val_if_fail (attr->type == G_FILE_ATTRIBUTE_TYPE_STRING, NULL);

  return attr->u.string;
}

/**
 * g_file_attribute_value_get_byte_string:
 * @attr: a #GFileAttributeValue.
 * 
 * Gets the byte string from a file attribute value. If the value is not the
 * right type then %NULL will be returned.
 * 
 * Returns: the byte string contained within the attribute or %NULL.
 **/
const char *
g_file_attribute_value_get_byte_string (const GFileAttributeValue *attr)
{
  if (attr == NULL)
    return NULL;

  g_return_val_if_fail (attr->type == G_FILE_ATTRIBUTE_TYPE_BYTE_STRING, NULL);

  return attr->u.string;
}
  
/**
 * g_file_attribute_value_get_boolean:
 * @attr: a #GFileAttributeValue.
 * 
 * Gets the boolean value from a file attribute value. If the value is not the
 * right type then %FALSE will be returned.
 * 
 * Returns: the boolean value contained within the attribute, or %FALSE.
 **/
gboolean
g_file_attribute_value_get_boolean (const GFileAttributeValue *attr)
{
  if (attr == NULL)
    return FALSE;

  g_return_val_if_fail (attr->type == G_FILE_ATTRIBUTE_TYPE_BOOLEAN, FALSE);

  return attr->u.boolean;
}
  
/**
 * g_file_attribute_value_get_uint32:
 * @attr: a #GFileAttributeValue.
 * 
 * Gets the unsigned 32-bit integer from a file attribute value. If the value 
 * is not the right type then %0 will be returned.
 * 
 * Returns: the unsigned 32-bit integer from the attribute, or %0.
 **/
guint32
g_file_attribute_value_get_uint32 (const GFileAttributeValue *attr)
{
  if (attr == NULL)
    return 0;

  g_return_val_if_fail (attr->type == G_FILE_ATTRIBUTE_TYPE_UINT32, 0);

  return attr->u.uint32;
}

/**
 * g_file_attribute_value_get_int32:
 * @attr: a #GFileAttributeValue.
 * 
 * Gets the signed 32-bit integer from a file attribute value. If the value 
 * is not the right type then %0 will be returned.
 * 
 * Returns: the signed 32-bit integer from the attribute, or %0.
 **/
gint32
g_file_attribute_value_get_int32 (const GFileAttributeValue *attr)
{
  if (attr == NULL)
    return 0;

  g_return_val_if_fail (attr->type == G_FILE_ATTRIBUTE_TYPE_INT32, 0);

  return attr->u.int32;
}

/**
 * g_file_attribute_value_get_uint64:
 * @attr: a #GFileAttributeValue.
 * 
 * Gets the unsigned 64-bit integer from a file attribute value. If the value 
 * is not the right type then %0 will be returned.
 * 
 * Returns: the unsigned 64-bit integer from the attribute, or %0.
 **/  
guint64
g_file_attribute_value_get_uint64 (const GFileAttributeValue *attr)
{
  if (attr == NULL)
    return 0;

  g_return_val_if_fail (attr->type == G_FILE_ATTRIBUTE_TYPE_UINT64, 0);

  return attr->u.uint64;
}

/**
 * g_file_attribute_value_get_int64:
 * @attr: a #GFileAttributeValue.
 * 
 * Gets the signed 64-bit integer from a file attribute value. If the value 
 * is not the right type then %0 will be returned.
 * 
 * Returns: the signed 64-bit integer from the attribute, or %0. 
 **/
gint64
g_file_attribute_value_get_int64 (const GFileAttributeValue *attr)
{
  if (attr == NULL)
    return 0;

  g_return_val_if_fail (attr->type == G_FILE_ATTRIBUTE_TYPE_INT64, 0);

  return attr->u.int64;
}

/**
 * g_file_attribute_value_get_object:
 * @attr: a #GFileAttributeValue.
 * 
 * Gets the GObject from a file attribute value. If the value 
 * is not the right type then %NULL will be returned.
 * 
 * Returns: the GObject from the attribute, or %0.
 **/
GObject *
g_file_attribute_value_get_object (const GFileAttributeValue *attr)
{
  if (attr == NULL)
    return NULL;

  g_return_val_if_fail (attr->type == G_FILE_ATTRIBUTE_TYPE_OBJECT, NULL);

  return attr->u.obj;
}
  
/**
 * g_file_attribute_value_set_string:
 * @attr: a #GFileAttributeValue.
 * @string: a string to set within the type.
 * 
 * Sets the attribute value to a given string.
 * 
 **/
void
g_file_attribute_value_set_string (GFileAttributeValue *attr,
				   const char          *string)
{
  g_return_if_fail (attr != NULL);
  g_return_if_fail (string != NULL);

  g_file_attribute_value_clear (attr);
  attr->type = G_FILE_ATTRIBUTE_TYPE_STRING;
  attr->u.string = g_strdup (string);
}

/**
 * g_file_attribute_value_set_byte_string:
 * @attr: a #GFileAttributeValue.
 * @string: a byte string to set within the type.
 * 
 * Sets the attribute value to a given byte string.
 * 
 **/
void
g_file_attribute_value_set_byte_string (GFileAttributeValue *attr,
					const char          *string)
{
  g_return_if_fail (attr != NULL);
  g_return_if_fail (string != NULL);

  g_file_attribute_value_clear (attr);
  attr->type = G_FILE_ATTRIBUTE_TYPE_BYTE_STRING;
  attr->u.string = g_strdup (string);
}

/**
 * g_file_attribute_value_set_boolean:
 * @attr: a #GFileAttributeValue.
 * @value: a #gboolean to set within the type.
 * 
 * Sets the attribute value to the given boolean value. 
 * 
 **/
void
g_file_attribute_value_set_boolean (GFileAttributeValue *attr,
				    gboolean             value)
{
  g_return_if_fail (attr != NULL);

  g_file_attribute_value_clear (attr);
  attr->type = G_FILE_ATTRIBUTE_TYPE_BOOLEAN;
  attr->u.boolean = !!value;
}

/**
 * g_file_attribute_value_set_uint32:
 * @attr: a #GFileAttributeValue.
 * @value: a #guint32 to set within the type.
 * 
 * Sets the attribute value to the given unsigned 32-bit integer.
 * 
 **/ 
void
g_file_attribute_value_set_uint32 (GFileAttributeValue *attr,
				   guint32              value)
{
  g_return_if_fail (attr != NULL);

  g_file_attribute_value_clear (attr);
  attr->type = G_FILE_ATTRIBUTE_TYPE_UINT32;
  attr->u.uint32 = value;
}

/**
 * g_file_attribute_value_set_int32:
 * @attr: a #GFileAttributeValue.
 * @value: a #gint32 to set within the type.
 * 
 * Sets the attribute value to the given signed 32-bit integer.
 *  
 **/
void
g_file_attribute_value_set_int32 (GFileAttributeValue *attr,
				  gint32               value)
{
  g_return_if_fail (attr != NULL);

  g_file_attribute_value_clear (attr);
  attr->type = G_FILE_ATTRIBUTE_TYPE_INT32;
  attr->u.int32 = value;
}

/**
 * g_file_attribute_value_set_uint64:
 * @attr: a #GFileAttributeValue.
 * @value: a #guint64 to set within the type.
 * 
 * Sets the attribute value to a given unsigned 64-bit integer.
 * 
 **/
void
g_file_attribute_value_set_uint64 (GFileAttributeValue *attr,
				   guint64              value)
{
  g_return_if_fail (attr != NULL);

  g_file_attribute_value_clear (attr);
  attr->type = G_FILE_ATTRIBUTE_TYPE_UINT64;
  attr->u.uint64 = value;
}

/**
 * g_file_attribute_value_set_int64:
 * @attr: a #GFileAttributeValue.
 * @value: a #gint64 to set within the type.
 * 
 * Sets the attribute value to a given signed 64-bit integer. 
 * 
 **/
void
g_file_attribute_value_set_int64 (GFileAttributeValue *attr,
				  gint64               value)
{
  g_return_if_fail (attr != NULL);

  g_file_attribute_value_clear (attr);
  attr->type = G_FILE_ATTRIBUTE_TYPE_INT64;
  attr->u.int64 = value;
}

/**
 * g_file_attribute_value_set_object:
 * @attr: a #GFileAttributeValue.
 * @obj: a #GObject.
 *
 * Sets the attribute to contain the value @obj.
 * The @attr references the GObject internally.
 * 
 **/
void
g_file_attribute_value_set_object (GFileAttributeValue *attr,
				   GObject             *obj)
{
  g_return_if_fail (attr != NULL);
  g_return_if_fail (obj != NULL);

  g_file_attribute_value_clear (attr);
  attr->type = G_FILE_ATTRIBUTE_TYPE_OBJECT;
  attr->u.obj = g_object_ref (obj);
}

typedef struct {
  GFileAttributeInfoList public;
  GArray *array;
  int ref_count;
} GFileAttributeInfoListPriv;

static void
list_update_public (GFileAttributeInfoListPriv *priv)
{
  priv->public.infos = (GFileAttributeInfo *)priv->array->data;
  priv->public.n_infos = priv->array->len;
}

/**
 * g_file_attribute_info_list_new:
 * 
 * Creates a new file attribute info list.
 * 
 * Returns: a #GFileAttributeInfoList.
 **/
GFileAttributeInfoList *
g_file_attribute_info_list_new (void)
{
  GFileAttributeInfoListPriv *priv;

  priv = g_new0 (GFileAttributeInfoListPriv, 1);
  
  priv->ref_count = 1;
  priv->array = g_array_new (TRUE, FALSE, sizeof (GFileAttributeInfo));
  
  list_update_public (priv);
  
  return (GFileAttributeInfoList *)priv;
}

/**
 * g_file_attribute_info_list_dup:
 * @list: a #GFileAttributeInfoList to duplicate.
 * 
 * Makes a duplicate of a file attribute info list.
 * 
 * Returns: a copy of the given @list. 
 **/
GFileAttributeInfoList *
g_file_attribute_info_list_dup (GFileAttributeInfoList *list)
{
  GFileAttributeInfoListPriv *new;
  int i;
  
  g_return_val_if_fail (list != NULL, NULL);

  new = g_new0 (GFileAttributeInfoListPriv, 1);
  new->ref_count = 1;
  new->array = g_array_new (TRUE, FALSE, sizeof (GFileAttributeInfo));

  g_array_set_size (new->array, list->n_infos);
  list_update_public (new);
  for (i = 0; i < list->n_infos; i++)
    {
      new->public.infos[i].name = g_strdup (list->infos[i].name);
      new->public.infos[i].type = list->infos[i].type;
      new->public.infos[i].flags = list->infos[i].flags;
    }
  
  return (GFileAttributeInfoList *)new;
}

/**
 * g_file_attribute_info_list_ref:
 * @list: a #GFileAttributeInfoList to reference.
 * 
 * References a file attribute info list.
 * 
 * Returns: #GFileAttributeInfoList or %NULL on error.
 **/
GFileAttributeInfoList *
g_file_attribute_info_list_ref (GFileAttributeInfoList *list)
{
  GFileAttributeInfoListPriv *priv = (GFileAttributeInfoListPriv *)list;
  
  g_return_val_if_fail (list != NULL, NULL);
  g_return_val_if_fail (priv->ref_count > 0, NULL);
  
  g_atomic_int_inc (&priv->ref_count);
  
  return list;
}

/**
 * g_file_attribute_info_list_unref:
 * @list: The #GFileAttributeInfoList to unreference.
 * 
 * Removes a reference from the given @list. If the reference count
 * falls to zero, the @list is deleted.
 **/
void
g_file_attribute_info_list_unref (GFileAttributeInfoList *list)
{
  GFileAttributeInfoListPriv *priv = (GFileAttributeInfoListPriv *)list;
  int i;
  
  g_return_if_fail (list != NULL);
  g_return_if_fail (priv->ref_count > 0);
  
  if (g_atomic_int_dec_and_test (&priv->ref_count))
    {
      for (i = 0; i < list->n_infos; i++)
        g_free (list->infos[i].name);
      g_array_free (priv->array, TRUE);
    }
}

static int
g_file_attribute_info_list_bsearch (GFileAttributeInfoList *list,
				    const char             *name)
{
  int start, end, mid;
  
  start = 0;
  end = list->n_infos;

  while (start != end)
    {
      mid = start + (end - start) / 2;

      if (strcmp (name, list->infos[mid].name) < 0)
	end = mid;
      else if (strcmp (name, list->infos[mid].name) > 0)
	start = mid + 1;
      else
	return mid;
    }
  return start;
}

/**
 * g_file_attribute_info_list_lookup:
 * @list: a #GFileAttributeInfoList.
 * @name: the name of the attribute to lookup.
 * 
 * Gets the file attribute with the name @name from @list.
 *
 * Returns: a #GFileAttributeInfo for the @name, or %NULL if an 
 * attribute isn't found.
 **/
const GFileAttributeInfo *
g_file_attribute_info_list_lookup (GFileAttributeInfoList *list,
				   const char             *name)
{
  int i;
  
  g_return_val_if_fail (list != NULL, NULL);
  g_return_val_if_fail (name != NULL, NULL);
  
  i = g_file_attribute_info_list_bsearch (list, name);

  if (i < list->n_infos && strcmp (list->infos[i].name, name) == 0)
    return &list->infos[i];
  
  return NULL;
}

/**
 * g_file_attribute_info_list_add:
 * @list: a #GFileAttributeInfoList.
 * @name: the name of the attribute to add.
 * @type: the #GFileAttributeType for the attribute.
 * @flags: #GFileAttributeFlags for the attribute.
 * 
 * Adds a new attribute with @name to the @list, setting
 * its @type and @flags. 
 **/
void
g_file_attribute_info_list_add    (GFileAttributeInfoList *list,
				   const char             *name,
				   GFileAttributeType      type,
				   GFileAttributeFlags     flags)
{
  GFileAttributeInfoListPriv *priv = (GFileAttributeInfoListPriv *)list;
  GFileAttributeInfo info;
  int i;
  
  g_return_if_fail (list != NULL);
  g_return_if_fail (name != NULL);

  i = g_file_attribute_info_list_bsearch (list, name);

  if (i < list->n_infos && strcmp (list->infos[i].name, name) == 0)
    {
      list->infos[i].type = type;
      return;
    }

  info.name = g_strdup (name);
  info.type = type;
  info.flags = flags;
  g_array_insert_vals (priv->array, i, &info, 1);

  list_update_public (priv);
}

#define __G_FILE_ATTRIBUTE_C__
#include "gioaliasdef.c"
