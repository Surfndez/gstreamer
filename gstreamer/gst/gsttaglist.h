/* GStreamer
 * Copyright (C) 2003 Benjamin Otte <in7y118@public.uni-hamburg.de>
 *
 * gsttag.h: Header for tag support
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


#ifndef __GST_TAG_H__
#define __GST_TAG_H__

#include <gst/gststructure.h>
#include <gst/gstevent.h>

G_BEGIN_DECLS

typedef enum {
  GST_TAG_MERGE_UNDEFINED,
  GST_TAG_MERGE_REPLACE_ALL,
  GST_TAG_MERGE_REPLACE,
  GST_TAG_MERGE_APPEND,
  GST_TAG_MERGE_PREPEND,
  GST_TAG_MERGE_KEEP,
  GST_TAG_MERGE_KEEP_ALL,
  /* add more */
  GST_TAG_MERGE_COUNT
} GstTagMergeMode;
#define GST_TAG_MODE_IS_VALID(mode)     (((mode) > GST_TAG_MERGE_UNDEFINED) && ((mode) < GST_TAG_MERGE_COUNT))

typedef GstStructure GstTagList;
#define GST_TAG_LIST(x)		((GstTagList *) (x))
#define GST_IS_TAG_LIST(x)	(gst_is_tag_list (GST_TAG_LIST (x)))

typedef void		(* GstTagForeachFunc)	(const GstTagList *list, const gchar *tag, gpointer user_data);
typedef void		(* GstTagMergeFunc)	(GValue *dest, const GValue *src);

/* initialize tagging system */
void		_gst_tag_initialize		(void);

void		gst_tag_register		(gchar *		name,
						 GType			type,
						 gchar *		nick,
						 gchar *		blurb,
						 GstTagMergeFunc	func);
/* some default merging functions */
void		gst_tag_merge_use_first		(GValue *		dest,
						 const GValue *		values);
void		gst_tag_merge_strings_with_comma (GValue *		dest,
						 const GValue *		values);

/* basic tag support */
gboolean	gst_tag_exists			(const gchar *		tag);
GType		gst_tag_get_type		(const gchar *		tag);
G_CONST_RETURN gchar *
		gst_tag_get_nick		(const gchar *		tag);
G_CONST_RETURN gchar *
		gst_tag_get_description		(const gchar *		tag);
gboolean	gst_tag_is_fixed		(const gchar *		tag);

/* tag lists */
GstTagList *	gst_tag_list_new		(void);
gboolean	gst_is_tag_list			(gconstpointer		p);
GstTagList *	gst_tag_list_copy		(const GstTagList *	list);
void		gst_tag_list_insert		(GstTagList *		into,
						 const GstTagList *	from,
						 GstTagMergeMode	mode);
GstTagList *	gst_tag_list_merge		(const GstTagList *	list1,
						 const GstTagList *	list2,
						 GstTagMergeMode	mode);
void		gst_tag_list_free		(GstTagList *		list);
guint		gst_tag_list_get_tag_size	(const GstTagList *	list,
						 const gchar *		tag);
void		gst_tag_list_add		(GstTagList *		list,
						 GstTagMergeMode	mode,
						 const gchar *		tag,
						 ...);
void		gst_tag_list_add_values		(GstTagList *		list,
						 GstTagMergeMode	mode,
						 const gchar *		tag,
						 ...);
void		gst_tag_list_add_valist		(GstTagList *		list,
						 GstTagMergeMode	mode,
						 const gchar *		tag,
						 va_list		var_args);
void		gst_tag_list_add_valist_values	(GstTagList *		list,
						 GstTagMergeMode	mode,
						 const gchar *		tag,
						 va_list		var_args);
void		gst_tag_list_remove_tag		(GstTagList *		list,
						 const gchar *		tag);
void		gst_tag_list_foreach		(GstTagList *		list,
						 GstTagForeachFunc	func,
						 gpointer		user_data);

G_CONST_RETURN GValue *
		gst_tag_list_get_value_index	(const GstTagList *	list,
						 const gchar *		tag,
						 guint			index);
gboolean      	gst_tag_list_copy_value		(GValue *		dest,
						 const GstTagList *	list,
						 const gchar *		tag);

/* simplifications (FIXME: do we want them?) */
gboolean	gst_tag_list_get_char		(const GstTagList *	list,
						 const gchar *		tag,
						 gchar *		value);
gboolean	gst_tag_list_get_char_index	(const GstTagList *	list,
						 const gchar *		tag,
						 guint			index,
						 gchar *		value);
gboolean	gst_tag_list_get_uchar		(const GstTagList *	list,
						 const gchar *		tag,
						 guchar *		value);
gboolean	gst_tag_list_get_uchar_index	(const GstTagList *	list,
						 const gchar *		tag,
						 guint			index,
						 guchar *		value);
gboolean	gst_tag_list_get_boolean	(const GstTagList *	list,
						 const gchar *		tag,
						 gboolean *		value);
gboolean	gst_tag_list_get_boolean_index	(const GstTagList *	list,
						 const gchar *		tag,
						 guint			index,
						 gboolean *		value);
gboolean	gst_tag_list_get_int		(const GstTagList *	list,
						 const gchar *		tag,
						 gint *			value);
gboolean	gst_tag_list_get_int_index	(const GstTagList *	list,
						 const gchar *		tag,
						 guint			index,
						 gint *			value);
gboolean	gst_tag_list_get_uint		(const GstTagList *	list,
						 const gchar *		tag,
						 guint *		value);
gboolean	gst_tag_list_get_uint_index	(const GstTagList *	list,
						 const gchar *		tag,
						 guint			index,
						 guint *		value);
gboolean	gst_tag_list_get_long		(const GstTagList *	list,
						 const gchar *		tag,
						 glong *		value);
gboolean	gst_tag_list_get_long_index	(const GstTagList *	list,
						 const gchar *		tag,
						 guint			index,
						 glong *		value);
gboolean	gst_tag_list_get_ulong		(const GstTagList *	list,
						 const gchar *		tag,
						 gulong *		value);
gboolean	gst_tag_list_get_ulong_index	(const GstTagList *	list,
						 const gchar *		tag,
						 guint			index,
						 gulong *		value);
gboolean	gst_tag_list_get_int64		(const GstTagList *	list,
						 const gchar *		tag,
						 gint64 *		value);
gboolean	gst_tag_list_get_int64_index	(const GstTagList *	list,
						 const gchar *		tag,
						 guint			index,
						 gint64 *		value);
gboolean	gst_tag_list_get_uint64		(const GstTagList *	list,
						 const gchar *		tag,
						 guint64 *		value);
gboolean	gst_tag_list_get_uint64_index	(const GstTagList *	list,
						 const gchar *		tag,
						 guint			index,
						 guint64 *		value);
gboolean	gst_tag_list_get_float		(const GstTagList *	list,
						 const gchar *		tag,
						 gfloat *		value);
gboolean	gst_tag_list_get_float_index	(const GstTagList *	list,
						 const gchar *		tag,
						 guint			index,
						 gfloat *		value);
gboolean	gst_tag_list_get_double		(const GstTagList *	list,
						 const gchar *		tag,
						 gdouble *		value);
gboolean	gst_tag_list_get_double_index	(const GstTagList *	list,
						 const gchar *		tag,
						 guint			index,
						 gdouble *		value);
gboolean	gst_tag_list_get_string		(const GstTagList *	list,
						 const gchar *		tag,
						 gchar **		value);
gboolean	gst_tag_list_get_string_index	(const GstTagList *	list,
						 const gchar *		tag,
						 guint			index,
						 gchar **		value);
gboolean	gst_tag_list_get_pointer      	(const GstTagList *	list,
						 const gchar *		tag,
						 gpointer *		value);
gboolean	gst_tag_list_get_pointer_index	(const GstTagList *	list,
						 const gchar *		tag,
						 guint			index,
						 gpointer *		value);

/* tag events */
GstEvent *	gst_event_new_tag		(GstTagList *		list);
GstTagList *	gst_event_tag_get_list		(GstEvent *		tag_event);


/* GStreamer core tags (need to be discussed) */
#define GST_TAG_TITLE			"title"
#define GST_TAG_ARTIST			"artist"
#define GST_TAG_ALBUM			"album"
#define GST_TAG_DATE			"date"
#define GST_TAG_GENRE			"genre"
#define GST_TAG_COMMENT			"comment"
#define GST_TAG_TRACK_NUMBER		"track-number"
#define GST_TAG_TRACK_COUNT		"track-count"
#define GST_TAG_LOCATION		"location"
#define GST_TAG_DESCRIPTION		"description"
#define GST_TAG_VERSION			"version"
#define GST_TAG_ISRC			"isrc"
#define GST_TAG_ORGANIZATION		"organization"
#define GST_TAG_COPYRIGHT		"copyright"
#define GST_TAG_CONTACT			"contact"
#define GST_TAG_LICENSE			"license"
#define GST_TAG_PERFORMER		"performer"
#define GST_TAG_DURATION		"duration"
#define GST_TAG_CODEC			"codec"
#define GST_TAG_BITRATE			"bitrate"
#define GST_TAG_MINIMUM_BITRATE		"minimum-bitrate"
#define GST_TAG_MAXIMUM_BITRATE		"maximum-bitrate"
#define GST_TAG_TRACK_GAIN		"replaygain_track_gain"
#define GST_TAG_TRACK_PEAK		"replaygain_track_peak"
#define GST_TAG_ALBUM_GAIN  		"replaygain_album_gain"
#define GST_TAG_ALBUM_PEAK		"replaygain_album_peak"


G_END_DECLS

#endif /* __GST_EVENT_H__ */
