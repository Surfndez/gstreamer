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

//#define DEBUG_ENABLED

#include "gstdebug.h"
#include "gstprops.h"
#include "gstpropsprivate.h"

static gboolean 	gst_props_entry_check_compatibility 	(GstPropsEntry *entry1, GstPropsEntry *entry2);
	

void 
_gst_props_initialize (void) 
{
}

static GstPropsEntry *
gst_props_create_entry (GstPropsFactory factory, gint *skipped)
{
  GstPropsFactoryEntry tag;
  GstPropsEntry *entry;
  guint i=0;

  entry = g_new0 (GstPropsEntry, 1);

  tag = factory[i++];
  switch (GPOINTER_TO_INT (tag)) {
    case GST_PROPS_INT_ID:
      entry->propstype = GST_PROPS_INT_ID_NUM;
      entry->data.int_data = GPOINTER_TO_INT (factory[i++]);
      break;
    case GST_PROPS_INT_RANGE_ID:
      entry->propstype = GST_PROPS_INT_RANGE_ID_NUM;
      entry->data.int_range_data.min = GPOINTER_TO_INT (factory[i++]);
      entry->data.int_range_data.max = GPOINTER_TO_INT (factory[i++]);
      break;
    case GST_PROPS_FOURCC_ID:
      entry->propstype = GST_PROPS_FOURCC_ID_NUM;
      entry->data.fourcc_data = GPOINTER_TO_INT (factory[i++]);
      break;
    case GST_PROPS_LIST_ID:
      g_print("gstprops: list not allowed in list\n");
      break;
    case GST_PROPS_BOOL_ID:
      entry->propstype = GST_PROPS_BOOL_ID_NUM;
      entry->data.bool_data = GPOINTER_TO_INT (factory[i++]);
      break;
    default:
      g_print("gstprops: unknown props id found\n");
      g_free (entry);
      entry = NULL;
      break;
  }

  *skipped = i;

  return entry;
}


static gint 
props_compare_func (gconstpointer a,
		   gconstpointer b) 
{
  GstPropsEntry *entry1 = (GstPropsEntry *)a;
  GstPropsEntry *entry2 = (GstPropsEntry *)b;

  return (entry1->propid - entry2->propid);
}

/**
 * gst_props_register:
 * @factory: the factory to register
 *
 * Register the factory. 
 *
 * Returns: The registered capability
 */
GstProps *
gst_props_register (GstPropsFactory factory)
{
  GstPropsFactoryEntry tag;
  gint i = 0;
  GstProps *props;
  gint skipped;
  
  g_return_val_if_fail (factory != NULL, NULL);

  tag = factory[i++];

  if (!tag) return NULL;

  props = g_new0 (GstProps, 1);
  g_return_val_if_fail (props != NULL, NULL);

  props->properties = NULL;
  
  while (tag) {
    GQuark quark;
    GstPropsEntry *entry;
    
    quark = g_quark_from_string ((gchar *)tag);

    tag = factory[i];
    switch (GPOINTER_TO_INT (tag)) {
      case GST_PROPS_LIST_ID: 
      {
        GstPropsEntry *list_entry;

        entry = g_new0 (GstPropsEntry, 1);
	entry->propid = quark;
	entry->propstype = GST_PROPS_LIST_ID_NUM;
	entry->data.list_data.entries = NULL;

	i++; // skip list tag
        tag = factory[i];
	while (tag) {
	  list_entry = gst_props_create_entry (&factory[i], &skipped);
	  list_entry->propid = quark;
	  i += skipped;
          tag = factory[i];
	  entry->data.list_data.entries = g_list_prepend (entry->data.list_data.entries, list_entry);
	}
	entry->data.list_data.entries = g_list_reverse (entry->data.list_data.entries);
	i++; //skip NULL (list end)
	break;
      }
      default:
      {
	entry = gst_props_create_entry (&factory[i], &skipped);
	entry->propid = quark;
	i += skipped;
	break;
      }
    }
    props->properties = g_slist_insert_sorted (props->properties, entry, props_compare_func);
     
    tag = factory[i++];
  }

  return props;
}

/* entry2 is always a list, entry1 never is */
static gboolean
gst_props_entry_check_list_compatibility (GstPropsEntry *entry1, GstPropsEntry *entry2)
{
  GList *entrylist = entry2->data.list_data.entries;
  gboolean found = FALSE;

  while (entrylist && !found) {
    GstPropsEntry *entry = (GstPropsEntry *) entrylist->data;

    found |= gst_props_entry_check_compatibility (entry1, entry);

    entrylist = g_list_next (entrylist);
  }

  return found;
}

static gboolean
gst_props_entry_check_compatibility (GstPropsEntry *entry1, GstPropsEntry *entry2)
{
  DEBUG ("compare: %s %s\n", g_quark_to_string (entry1->propid),
	                     g_quark_to_string (entry2->propid));
  switch (entry1->propstype) {
    case GST_PROPS_LIST_ID_NUM:
    {
      GList *entrylist = entry1->data.list_data.entries;
      gboolean valid = TRUE;    // innocent until proven guilty

      while (entrylist && valid) {
	GstPropsEntry *entry = (GstPropsEntry *) entrylist->data;

	valid &= gst_props_entry_check_compatibility (entry, entry2);
	
	entrylist = g_list_next (entrylist);
      }
      
      return valid;
    }
    case GST_PROPS_INT_RANGE_ID_NUM:
      switch (entry2->propstype) {
	// a - b   <--->   a - c
        case GST_PROPS_INT_RANGE_ID_NUM:
	  return (entry2->data.int_range_data.min <= entry1->data.int_range_data.min &&
	          entry2->data.int_range_data.max >= entry1->data.int_range_data.max);
        case GST_PROPS_LIST_ID_NUM:
	  return gst_props_entry_check_list_compatibility (entry1, entry2);
        default:
          return FALSE;
      }
      break;
    case GST_PROPS_FOURCC_ID_NUM:
      switch (entry2->propstype) {
	// b   <--->   a
        case GST_PROPS_FOURCC_ID_NUM:
	  return (entry2->data.fourcc_data == entry1->data.fourcc_data);
	// b   <--->   a,b,c
        case GST_PROPS_LIST_ID_NUM:
	  return gst_props_entry_check_list_compatibility (entry1, entry2);
        default:
          return FALSE;
      }
      break;
    case GST_PROPS_INT_ID_NUM:
      switch (entry2->propstype) {
	// b   <--->   a - d
        case GST_PROPS_INT_RANGE_ID_NUM:
	  return (entry2->data.int_range_data.min <= entry1->data.int_data &&
	          entry2->data.int_range_data.max >= entry1->data.int_data);
	// b   <--->   a
        case GST_PROPS_INT_ID_NUM:
	  return (entry2->data.int_data == entry1->data.int_data);
	// b   <--->   a,b,c
        case GST_PROPS_LIST_ID_NUM:
	  return gst_props_entry_check_list_compatibility (entry1, entry2);
        default:
          return FALSE;
      }
      break;
    case GST_PROPS_BOOL_ID_NUM:
      switch (entry2->propstype) {
	// t   <--->   t
        case GST_PROPS_BOOL_ID_NUM:
          return (entry2->data.bool_data == entry1->data.bool_data);
        case GST_PROPS_LIST_ID_NUM:
	  return gst_props_entry_check_list_compatibility (entry1, entry2);
        default:
          return FALSE;
      }
    default:
      break;
  }

  return FALSE;
}

/**
 * gst_props_check_compatibility:
 * @fromprops: a capabilty
 * @toprops: a capabilty
 *
 * Checks whether two capabilities are compatible
 *
 * Returns: true if compatible, false otherwise
 */
gboolean
gst_props_check_compatibility (GstProps *fromprops, GstProps *toprops)
{
  GSList *sourcelist;
  GSList *sinklist;
  gint missing = 0;
  gint more = 0;
  gboolean compatible = TRUE;

  g_return_val_if_fail (fromprops != NULL, FALSE);
  g_return_val_if_fail (toprops != NULL, FALSE);
	
  sourcelist = fromprops->properties;
  sinklist   = toprops->properties;

  while (sourcelist && sinklist && compatible) {
    GstPropsEntry *entry1;
    GstPropsEntry *entry2;

    entry1 = (GstPropsEntry *)sourcelist->data;
    entry2 = (GstPropsEntry *)sinklist->data;

    while (entry1->propid < entry2->propid) {
      DEBUG ("source is more specific in \"%s\"\n", g_quark_to_string (entry1->propid));
      more++;
      sourcelist = g_slist_next (sourcelist);
      if (sourcelist) entry1 = (GstPropsEntry *)sourcelist->data;
      else goto end;
    }
    while (entry1->propid > entry2->propid) {
      DEBUG ("source has missing property \"%s\"\n", g_quark_to_string (entry2->propid));
      missing++;
      sinklist = g_slist_next (sinklist);
      if (sinklist) entry2 = (GstPropsEntry *)sinklist->data;
      else goto end;
    }

    compatible &= gst_props_entry_check_compatibility (entry1, entry2);

    sourcelist = g_slist_next (sourcelist);
    sinklist = g_slist_next (sinklist);
  }
end:

  if (missing)
    return FALSE;

  return compatible;
}

static xmlNodePtr
gst_props_save_thyself_func (GstPropsEntry *entry, xmlNodePtr parent)
{
  xmlNodePtr subtree;

  switch (entry->propstype) {
    case GST_PROPS_INT_ID_NUM: 
      subtree = xmlNewChild (parent, NULL, "int", NULL);
      xmlNewProp (subtree, "name", g_quark_to_string (entry->propid));
      xmlNewProp (subtree, "value", g_strdup_printf ("%d", entry->data.int_data));
      break;
    case GST_PROPS_INT_RANGE_ID_NUM: 
      subtree = xmlNewChild (parent, NULL, "range", NULL);
      xmlNewProp (subtree, "name", g_quark_to_string (entry->propid));
      xmlNewProp (subtree, "min", g_strdup_printf ("%d", entry->data.int_range_data.min));
      xmlNewProp (subtree, "max", g_strdup_printf ("%d", entry->data.int_range_data.max));
      break;
    case GST_PROPS_FOURCC_ID_NUM: 
      xmlAddChild (parent, xmlNewComment (g_strdup_printf ("%4.4s", (gchar *)&entry->data.fourcc_data)));
      subtree = xmlNewChild (parent, NULL, "fourcc", NULL);
      xmlNewProp (subtree, "name", g_quark_to_string (entry->propid));
      xmlNewProp (subtree, "hexvalue", g_strdup_printf ("%08x", entry->data.fourcc_data));
      break;
    case GST_PROPS_BOOL_ID_NUM: 
      subtree = xmlNewChild (parent, NULL, "boolean", NULL);
      xmlNewProp (subtree, "name", g_quark_to_string (entry->propid));
      xmlNewProp (subtree, "value", (entry->data.bool_data ?  "true" : "false"));
      break;
    default:
      break;
  }

  return parent;
}

xmlNodePtr
gst_props_save_thyself (GstProps *props, xmlNodePtr parent)
{
  GSList *proplist;
  xmlNodePtr subtree;

  g_return_val_if_fail (props != NULL, NULL);

  proplist = props->properties;

  while (proplist) {
    GstPropsEntry *entry = (GstPropsEntry *) proplist->data;

    switch (entry->propstype) {
      case GST_PROPS_LIST_ID_NUM: 
        subtree = xmlNewChild (parent, NULL, "list", NULL);
        xmlNewProp (subtree, "name", g_quark_to_string (entry->propid));
        g_list_foreach (entry->data.list_data.entries, (GFunc) gst_props_save_thyself_func, subtree);
      default:
    	gst_props_save_thyself_func (entry, parent);
    }

    proplist = g_slist_next (proplist);
  }
  
  return parent;
}

static GstPropsEntry*
gst_props_load_thyself_func (xmlNodePtr field)
{
  GstPropsEntry *entry;

  entry = g_new0 (GstPropsEntry, 1);

  if (!strcmp(field->name, "int")) {
    entry->propstype = GST_PROPS_INT_ID_NUM;
    entry->propid = g_quark_from_string (xmlGetProp(field, "name"));
    sscanf (xmlGetProp(field, "value"), "%d", &entry->data.int_data);
  }
  else if (!strcmp(field->name, "range")) {
    entry->propstype = GST_PROPS_INT_RANGE_ID_NUM;
    entry->propid = g_quark_from_string (xmlGetProp(field, "name"));
    sscanf (xmlGetProp(field, "min"), "%d", &entry->data.int_range_data.min);
    sscanf (xmlGetProp(field, "max"), "%d", &entry->data.int_range_data.max);
  }
  else if (!strcmp(field->name, "boolean")) {
    entry->propstype = GST_PROPS_BOOL_ID_NUM;
    entry->propid = g_quark_from_string (xmlGetProp(field, "name"));
    if (!strcmp (xmlGetProp(field, "value"), "false")) entry->data.bool_data = 0;
    else entry->data.bool_data = 1;
  }
  else if (!strcmp(field->name, "fourcc")) {
    entry->propstype = GST_PROPS_FOURCC_ID_NUM;
    entry->propid = g_quark_from_string (xmlGetProp(field, "name"));
    sscanf (xmlGetProp(field, "hexvalue"), "%08x", &entry->data.fourcc_data);
  }

  return entry;
}

GstProps*
gst_props_load_thyself (xmlNodePtr parent)
{
  GstProps *props = g_new0 (GstProps, 1);
  xmlNodePtr field = parent->childs;

  while (field) {
    if (!strcmp (field->name, "list")) {
      GstPropsEntry *entry;
      xmlNodePtr subfield = field->childs;

      entry = g_new0 (GstPropsEntry, 1);
      entry->propstype = GST_PROPS_LIST_ID_NUM;
      entry->propid = g_quark_from_string (xmlGetProp(field, "name"));

      while (subfield) {
        GstPropsEntry *subentry = gst_props_load_thyself_func (subfield);

	entry->data.list_data.entries = g_list_prepend (entry->data.list_data.entries, subentry);

        subfield = subfield->next;
      }
      entry->data.list_data.entries = g_list_reverse (entry->data.list_data.entries);
      props->properties = g_slist_insert_sorted (props->properties, entry, props_compare_func);
    }
    else {
      GstPropsEntry *entry;

      entry = gst_props_load_thyself_func (field);

      props->properties = g_slist_insert_sorted (props->properties, entry, props_compare_func);
    }
    field = field->next;
  }

  return props;
}

