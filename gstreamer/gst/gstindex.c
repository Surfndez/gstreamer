/* GStreamer
 * Copyright (C) 2001 RidgeRun (http://www.ridgerun.com/)
 * Written by Erik Walthinsen <omega@ridgerun.com>
 *
 * gstindex.c: Index for mappings and other data
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

#include "gstlog.h"
#include "gst_private.h"
#include "gstregistry.h"

#include "gstindex.h"

/* Index signals and args */
enum {
  ENTRY_ADDED,
  LAST_SIGNAL
};

enum {
  ARG_0,
  /* FILL ME */
};

static void		gst_index_class_init	(GstIndexClass *klass);
static void		gst_index_init		(GstIndex *index);

#define CLASS(index)  GST_INDEX_CLASS (G_OBJECT_GET_CLASS (index))

static GstObject *parent_class = NULL;
static guint gst_index_signals[LAST_SIGNAL] = { 0 };

GType
gst_index_get_type(void) {
  static GType index_type = 0;

  if (!index_type) {
    static const GTypeInfo index_info = {
      sizeof(GstIndexClass),
      NULL,
      NULL,
      (GClassInitFunc)gst_index_class_init,
      NULL,
      NULL,
      sizeof(GstIndex),
      1,
      (GInstanceInitFunc)gst_index_init,
      NULL
    };
    index_type = g_type_register_static(GST_TYPE_OBJECT, "GstIndex", &index_info, 0);
  }
  return index_type;
}

static void
gst_index_class_init (GstIndexClass *klass)
{
  GObjectClass *gobject_class;
  GstElementClass *gstelement_class;

  gobject_class = (GObjectClass*)klass;
  gstelement_class = (GstElementClass*)klass;

  parent_class = g_type_class_ref(GST_TYPE_OBJECT);

  gst_index_signals[ENTRY_ADDED] =
    g_signal_new ("entry_added", G_TYPE_FROM_CLASS (klass), G_SIGNAL_RUN_LAST,
                  G_STRUCT_OFFSET (GstIndexClass, entry_added), NULL, NULL,
                  gst_marshal_VOID__POINTER, G_TYPE_NONE, 1,
                  G_TYPE_POINTER);
}

static GstIndexGroup *
gst_index_group_new(guint groupnum)
{
  GstIndexGroup *indexgroup = g_new(GstIndexGroup,1);

  indexgroup->groupnum = groupnum;
  indexgroup->entries = NULL;
  indexgroup->certainty = GST_INDEX_UNKNOWN;
  indexgroup->peergroup = -1;

  GST_DEBUG(0, "created new index group %d",groupnum);

  return indexgroup;
}

static void
gst_index_init (GstIndex *index)
{
  index->curgroup = gst_index_group_new(0);
  index->maxgroup = 0;
  index->groups = g_list_prepend(NULL, index->curgroup);

  index->writers = g_hash_table_new (NULL, NULL);
  index->last_id = 0;
  
  GST_DEBUG(0, "created new index");
}

/**
 * gst_index_new:
 *
 * Create a new tileindex object
 *
 * Returns: a new index object
 */
GstIndex *
gst_index_new()
{
  GstIndex *index;

  index = g_object_new (gst_index_get_type (), NULL);

  return index;
}

/**
 * gst_index_get_group:
 * @index: the index to get the current group from
 *
 * Get the id of the current group.
 *
 * Returns: the id of the current group.
 */
gint
gst_index_get_group(GstIndex *index)
{
  return index->curgroup->groupnum;
}

/**
 * gst_index_new_group:
 * @index: the index to create the new group in
 *
 * Create a new group for the given index. It will be
 * set as the current group.
 *
 * Returns: the id of the newly created group.
 */
gint
gst_index_new_group(GstIndex *index)
{
  index->curgroup = gst_index_group_new(++index->maxgroup);
  index->groups = g_list_append(index->groups,index->curgroup);
  GST_DEBUG(0, "created new group %d in index",index->maxgroup);
  return index->maxgroup;
}

/**
 * gst_index_set_group:
 * @index: the index to set the new group in
 * @groupnum: the groupnumber to set
 *
 * Set the current groupnumber to the given argument.
 *
 * Returns: TRUE if the operation succeeded, FALSE if the group
 * did not exist.
 */
gboolean
gst_index_set_group(GstIndex *index, gint groupnum)
{
  GList *list;
  GstIndexGroup *indexgroup;

  /* first check for null change */
  if (groupnum == index->curgroup->groupnum)
    return TRUE;

  /* else search for the proper group */
  list = index->groups;
  while (list) {
    indexgroup = (GstIndexGroup *)(list->data);
    list = g_list_next(list);
    if (indexgroup->groupnum == groupnum) {
      index->curgroup = indexgroup;
      GST_DEBUG(0, "switched to index group %d", indexgroup->groupnum);
      return TRUE;
    }
  }

  /* couldn't find the group in question */
  GST_DEBUG(0, "couldn't find index group %d",groupnum);
  return FALSE;
}

/**
 * gst_index_set_certainty:
 * @index: the index to set the certainty on
 * @certainty: the certainty to set
 *
 * Set the certainty of the given index.
 */
void
gst_index_set_certainty(GstIndex *index, GstIndexCertainty certainty)
{
  index->curgroup->certainty = certainty;
}

/**
 * gst_index_get_certainty:
 * @index: the index to get the certainty of
 *
 * Get the certainty of the given index.
 *
 * Returns: the certainty of the index.
 */
GstIndexCertainty
gst_index_get_certainty(GstIndex *index)
{
  return index->curgroup->certainty;
}

/**
 * gst_index_set_filter:
 * @index: the index to register the filter on
 * @filter: the filter to register
 * @user_data: data passed to the filter function
 *
 * Lets the app register a custom filter function so that
 * it can select what entries should be stored in the index.
 */
void
gst_index_set_filter (GstIndex *index, 
		      GstIndexFilter filter, gpointer user_data)
{
  g_return_if_fail (GST_IS_INDEX (index));

  index->filter = filter;
  index->filter_user_data = user_data;
}

/**
 * gst_index_set_resolver:
 * @index: the index to register the resolver on
 * @resolver: the resolver to register
 * @user_data: data passed to the resolver function
 *
 * Lets the app register a custom function to map index
 * ids to writer descriptions.
 */
void
gst_index_set_resolver (GstIndex *index, 
		        GstIndexResolver resolver, gpointer user_data)
{
  g_return_if_fail (GST_IS_INDEX (index));

  index->resolver = resolver;
  index->resolver_user_data = user_data;
}

/**
 * gst_index_entry_free:
 * @entry: the entry to free
 *
 * Free the memory used by the given entry.
 */
void
gst_index_entry_free (GstIndexEntry *entry)
{
  g_free (entry);
}

/**
 * gst_index_add_format:
 * @index: the index to add the entry to
 * @id: the id of the index writer
 * @format: the format to add to the index
 *
 * Adds a format entry into the index. This function is
 * used to map dynamic GstFormat ids to their original
 * format key.
 *
 * Returns: a pointer to the newly added entry in the index.
 */
GstIndexEntry*
gst_index_add_format (GstIndex *index, gint id, GstFormat format)
{
  GstIndexEntry *entry;
  const GstFormatDefinition* def;

  g_return_val_if_fail (GST_IS_INDEX (index), NULL);
  g_return_val_if_fail (format != 0, NULL);
  
  entry = g_new0 (GstIndexEntry, 1);
  entry->type = GST_INDEX_ENTRY_FORMAT;
  entry->id = id;
  entry->data.format.format = format;
  def = gst_format_get_details (format);
  entry->data.format.key = def->nick;
  
  if (CLASS (index)->add_entry)
    CLASS (index)->add_entry (index, entry);

  g_signal_emit (G_OBJECT (index), gst_index_signals[ENTRY_ADDED], 0, entry);

  return entry;
}

/**
 * gst_index_add_id:
 * @index: the index to add the entry to
 * @id: the id of the index writer
 * @description: the description of the index writer
 *
 * Add an id entry into the index.
 *
 * Returns: a pointer to the newly added entry in the index.
 */
GstIndexEntry*
gst_index_add_id (GstIndex *index, gint id, gchar *description)
{
  GstIndexEntry *entry;

  g_return_val_if_fail (GST_IS_INDEX (index), NULL);
  g_return_val_if_fail (description != NULL, NULL);
  
  entry = g_new0 (GstIndexEntry, 1);
  entry->type = GST_INDEX_ENTRY_ID;
  entry->id = id;
  entry->data.id.description = description;

  if (CLASS (index)->add_entry)
    CLASS (index)->add_entry (index, entry);
  
  g_signal_emit (G_OBJECT (index), gst_index_signals[ENTRY_ADDED], 0, entry);

  return entry;
}

/**
 * gst_index_get_writer_id:
 * @index: the index to get a unique write id for
 * @writer: the GstObject to allocate an id for
 * @id: a pointer to a gint to hold the id
 *
 * Before entries can be added to the index, a writer
 * should obtain a unique id. The methods to add new entries
 * to the index require this id as an argument. 
 *
 * The application or a GstIndex subclass can implement
 * custom functions to map the writer object to an id.
 *
 * Returns: TRUE if the writer would be mapped to an id.
 */
gboolean 
gst_index_get_writer_id (GstIndex *index, GstObject *writer, gint *id)
{
  gchar *writer_string = NULL;
  gboolean success = FALSE;
  GstIndexEntry *entry;

  g_return_val_if_fail (GST_IS_INDEX (index), FALSE);
  g_return_val_if_fail (GST_IS_OBJECT (writer), FALSE);
  g_return_val_if_fail (id, FALSE);

  *id = -1;

  entry = g_hash_table_lookup (index->writers, writer);
  if (entry == NULL) { 
    *id = index->last_id;

    writer_string = gst_object_get_path_string (writer);
    
    gst_index_add_id (index, *id, writer_string);
    index->last_id++;
    g_hash_table_insert (index->writers, writer, entry);
  }

  if (CLASS (index)->resolve_writer) {
    success = CLASS (index)->resolve_writer (index, writer, id, &writer_string);
  }

  if (index->resolver) {
    success = index->resolver (index, writer, id, &writer_string, index->resolver_user_data);
  }

  return success;
}

/**
 * gst_index_add_association:
 * @index: the index to add the entry to
 * @id: the id of the index writer
 * @flags: optinal flags for this entry
 * @format: the format of the value
 * @value: the value 
 * @...: other format/value pairs or 0 to end the list
 *
 * Associate given format/value pairs with eachother.
 * Be sure to pass gint64 values to this functions varargs,
 * you might want to use a gint64 cast to be sure.
 *
 * Returns: a pointer to the newly added entry in the index.
 */
GstIndexEntry*
gst_index_add_association (GstIndex *index, gint id, GstAssocFlags flags, 
		           GstFormat format, gint64 value, ...)
{
  va_list args;
  GstIndexAssociation *assoc;
  GstIndexEntry *entry;
  gulong size;
  gint nassocs = 0;
  GstFormat cur_format;
  volatile gint64 dummy;

  g_return_val_if_fail (GST_IS_INDEX (index), NULL);
  g_return_val_if_fail (format != 0, NULL);
  
  va_start (args, value);

  cur_format = format;

  while (cur_format) {
    nassocs++;
    cur_format = va_arg (args, GstFormat);
    if (cur_format)
      dummy = va_arg (args, gint64);
  }
  va_end (args);

  /* make room for two assoc */
  size = sizeof (GstIndexEntry) + (sizeof (GstIndexAssociation) * nassocs);

  entry = g_malloc (size);

  entry->type = GST_INDEX_ENTRY_ASSOCIATION;
  entry->id = id;
  entry->data.assoc.flags = flags;
  assoc = (GstIndexAssociation *) (((guint8 *) entry) + sizeof (GstIndexEntry));
  entry->data.assoc.assocs = assoc;
  entry->data.assoc.nassocs = nassocs;

  va_start (args, value);
  while (format) {
    assoc->format = format;
    assoc->value = value;

    assoc++;

    format = va_arg (args, GstFormat);
    if (format)
      value = va_arg (args, gint64);
  }
  va_end (args);

  if (CLASS (index)->add_entry)
    CLASS (index)->add_entry (index, entry);

  g_signal_emit (G_OBJECT (index), gst_index_signals[ENTRY_ADDED], 0, entry);

  return entry;
}

/**
 * gst_index_add_object:
 * @index: the index to add the object to
 * @id: the id of the index writer
 * @key: a key for the object
 * @type: the GType of the object
 * @object: a pointer to the object to add
 *
 * Add the given object to the index with the given key.
 * 
 * Returns: a pointer to the newly added entry in the index.
 */
GstIndexEntry*
gst_index_add_object (GstIndex *index, gint id, gchar *key,
		      GType type, gpointer object)
{
  return NULL;
}

static gint
gst_index_compare_func (gconstpointer a,
                        gconstpointer b,
                        gpointer user_data)
{
  return a - b;  
}

/**
 * gst_index_get_assoc_entry:
 * @index: the index to search
 * @id: the id of the index writer
 * @method: The lookup method to use
 * @format: the format of the value
 * @value: the value to find
 *
 * Finds the given format/value in the index
 *
 * Returns: the entry associated with the value or NULL if the
 *   value was not found.
 */
GstIndexEntry*
gst_index_get_assoc_entry (GstIndex *index, gint id,
		           GstIndexLookupMethod method,
			   GstFormat format, gint64 value)
{
  g_return_val_if_fail (GST_IS_INDEX (index), NULL);

  return gst_index_get_assoc_entry_full (index, id, method, format, value, 
		                  gst_index_compare_func, NULL);
}

/**
 * gst_index_get_assoc_entry_full:
 * @index: the index to search
 * @id: the id of the index writer
 * @method: The lookup method to use
 * @format: the format of the value
 * @value: the value to find
 * @func: the function used to compare entries
 * @user_data: user data passed to the compare function
 *
 * Finds the given format/value in the index with the given
 * compare function and user_data.
 *
 * Returns: the entry associated with the value or NULL if the
 *   value was not found.
 */
GstIndexEntry*
gst_index_get_assoc_entry_full (GstIndex *index, gint id,
		                GstIndexLookupMethod method,
			        GstFormat format, gint64 value,
			        GCompareDataFunc func,
			        gpointer user_data)
{
  g_return_val_if_fail (GST_IS_INDEX (index), NULL);

  if (CLASS(index)->get_assoc_entry)
    return CLASS (index)->get_assoc_entry (index, id, method, format, value, func, user_data);
  
  return NULL;
}

/**
 * gst_index_entry_assoc_map:
 * @entry: the index to search
 * @format: the format of the value the find
 * @value: a pointer to store the value
 *
 * Gets alternative formats associated with the indexentry.
 *
 * Returns: TRUE if there was a value associated with the given 
 * format.
 */
gboolean
gst_index_entry_assoc_map (GstIndexEntry *entry,
		           GstFormat format, gint64 *value)
{
  gint i;

  g_return_val_if_fail (entry != NULL, FALSE);
  g_return_val_if_fail (value != NULL, FALSE);

  for (i = 0; i < GST_INDEX_NASSOCS (entry); i++) {
     if (GST_INDEX_ASSOC_FORMAT (entry, i) == format) {
       *value = GST_INDEX_ASSOC_VALUE (entry, i);
       return TRUE;
     }
  }
  return FALSE;
}


static void 		gst_index_factory_class_init 		(GstIndexFactoryClass *klass);
static void 		gst_index_factory_init 		(GstIndexFactory *factory);

static GstPluginFeatureClass *factory_parent_class = NULL;
/* static guint gst_index_factory_signals[LAST_SIGNAL] = { 0 }; */

GType 
gst_index_factory_get_type (void) 
{
  static GType indexfactory_type = 0;

  if (!indexfactory_type) {
    static const GTypeInfo indexfactory_info = {
      sizeof (GstIndexFactoryClass),
      NULL,
      NULL,
      (GClassInitFunc) gst_index_factory_class_init,
      NULL,
      NULL,
      sizeof(GstIndexFactory),
      0,
      (GInstanceInitFunc) gst_index_factory_init,
      NULL
    };
    indexfactory_type = g_type_register_static (GST_TYPE_PLUGIN_FEATURE, 
	    				  "GstIndexFactory", &indexfactory_info, 0);
  }
  return indexfactory_type;
}

static void
gst_index_factory_class_init (GstIndexFactoryClass *klass)
{
  GObjectClass *gobject_class;
  GstObjectClass *gstobject_class;
  GstPluginFeatureClass *gstpluginfeature_class;

  gobject_class = (GObjectClass*)klass;
  gstobject_class = (GstObjectClass*)klass;
  gstpluginfeature_class = (GstPluginFeatureClass*) klass;

  factory_parent_class = g_type_class_ref (GST_TYPE_PLUGIN_FEATURE);
}

static void
gst_index_factory_init (GstIndexFactory *factory)
{
}

/**
 * gst_index_factory_new:
 * @name: name of indexfactory to create
 * @longdesc: long description of indexfactory to create
 * @type: the GType of the GstIndex element of this factory
 *
 * Create a new indexfactory with the given parameters
 *
 * Returns: a new #GstIndexFactory.
 */
GstIndexFactory*
gst_index_factory_new (const gchar *name, const gchar *longdesc, GType type)
{
  GstIndexFactory *factory;

  g_return_val_if_fail(name != NULL, NULL);
  factory = gst_index_factory_find (name);
  if (!factory) {
    factory = GST_INDEX_FACTORY (g_object_new (GST_TYPE_INDEX_FACTORY, NULL));
  }

  GST_PLUGIN_FEATURE_NAME (factory) = g_strdup (name);
  if (factory->longdesc)
    g_free (factory->longdesc);
  factory->longdesc = g_strdup (longdesc);
  factory->type = type;

  return factory;
}

/**
 * gst_index_factory_destroy:
 * @factory: factory to destroy
 *
 * Removes the index from the global list.
 */
void
gst_index_factory_destroy (GstIndexFactory *factory)
{
  g_return_if_fail (factory != NULL);

  /* we don't free the struct bacause someone might  have a handle to it.. */
}

/**
 * gst_index_factory_find:
 * @name: name of indexfactory to find
 *
 * Search for an indexfactory of the given name.
 *
 * Returns: #GstIndexFactory if found, NULL otherwise
 */
GstIndexFactory*
gst_index_factory_find (const gchar *name)
{
  GstPluginFeature *feature;

  g_return_val_if_fail (name != NULL, NULL);

  GST_DEBUG (0,"gstindex: find \"%s\"", name);

  feature = gst_registry_pool_find_feature (name, GST_TYPE_INDEX_FACTORY);
  if (feature)
    return GST_INDEX_FACTORY (feature);

  return NULL;
}

/**
 * gst_index_factory_create:
 * @factory: the factory used to create the instance
 *
 * Create a new #GstIndex instance from the 
 * given indexfactory.
 *
 * Returns: A new #GstIndex instance.
 */
GstIndex*
gst_index_factory_create (GstIndexFactory *factory)
{
  GstIndex *new = NULL;

  g_return_val_if_fail (factory != NULL, NULL);

  if (gst_plugin_feature_ensure_loaded (GST_PLUGIN_FEATURE (factory))) {
    g_return_val_if_fail (factory->type != 0, NULL);

    new = GST_INDEX (g_object_new(factory->type,NULL));
  }

  return new;
}

/**
 * gst_index_factory_make:
 * @name: the name of the factory used to create the instance
 *
 * Create a new #GstIndex instance from the 
 * indexfactory with the given name.
 *
 * Returns: A new #GstIndex instance.
 */
GstIndex*
gst_index_factory_make (const gchar *name)
{
  GstIndexFactory *factory;

  g_return_val_if_fail (name != NULL, NULL);

  factory = gst_index_factory_find (name);

  if (factory == NULL)
    return NULL;

  return gst_index_factory_create (factory);
}

