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
#include "gstelement.h"
#include "gstplugin.h"


/* global list of registered elementfactories */
GList* _gst_elementfactories;

void 
_gst_elementfactory_initialize (void) 
{
  _gst_elementfactories = NULL;
}

/**
 * gst_elementfactory_unregister:
 * @elementfactory: factory to register
 *
 * Removes the elementfactory from the global list.
 */
void 
gst_elementfactory_destroy (GstElementFactory *factory) 
{
  g_return_if_fail (factory != NULL);

  _gst_elementfactories = g_list_remove (_gst_elementfactories, factory);

  g_free (factory);
}

/**
 * gst_elementfactory_find:
 * @name: name of factory to find
 *
 * Search for an elementfactory of the given name.
 *
 * Returns: #GstElementFactory if found, NULL otherwise
 */
GstElementFactory*
gst_elementfactory_find (gchar *name) 
{
  GList *walk;
  GstElementFactory *factory;

  DEBUG("gstelementfactory: find \"%s\"\n", name);

  walk = _gst_elementfactories;
  while (walk) {
    factory = (GstElementFactory *)(walk->data);
    if (!strcmp(name,factory->name))
      return factory;
    walk = g_list_next(walk);
  }

  return NULL;
}

/**
 * gst_elementfactory_get_list:
 *
 * Get the global list of elementfactories.
 *
 * Returns: GList of type #GstElementFactory
 */
GList*
gst_elementfactory_get_list (void) 
{
  return _gst_elementfactories;
}


/**
 * gst_elementfactory_new:
 * @name: name of new elementfactory
 * @type: GtkType of new element
 * @details: #GstElementDetails structure with element details
 *
 * Create a new elementfactory capable of insantiating objects of the
 * given type.
 *
 * Returns: new elementfactory
 */
GstElementFactory*
gst_elementfactory_new (gchar *name, GtkType type,
                        GstElementDetails *details) 
{
  GstElementFactory *factory = g_new0(GstElementFactory, 1);

  factory->name = g_strdup(name);
  factory->type = type;
  factory->details = details;
  factory->padtemplates = NULL;

  _gst_elementfactories = g_list_prepend (_gst_elementfactories, factory);

  return factory;
}

/**
 * gst_elementfactory_create:
 * @factory: factory to instantiate
 * @name: name of new element
 *
 * Create a new element of the type defined by the given elementfactory.
 * It wll be given the name supplied, since all elements require a name as
 * their first argument.
 *
 * Returns: new #GstElement
 */
GstElement *
gst_elementfactory_create (GstElementFactory *factory,
                           gchar *name) 
{
  GstElement *element;
  GstElementClass *oclass;

  g_return_val_if_fail(factory != NULL, NULL);

  DEBUG("gstelementfactory: create \"%s\" \"%s\"\n", factory->name, name);

  // it's not loaded, try to load the plugin
  if (factory->type == 0) {
    factory = gst_plugin_load_elementfactory(factory->name);
  }
  g_return_val_if_fail(factory != NULL, NULL);
  g_return_val_if_fail(factory->type != 0, NULL);

  // create an instance of the element
  element = GST_ELEMENT(gtk_type_new(factory->type));
  g_assert(element != NULL);
  gst_object_ref(GST_OBJECT(element));

  // attempt to set the elemenfactory class pointer if necessary
  oclass = GST_ELEMENT_CLASS(GTK_OBJECT(element)->klass);
  if (oclass->elementfactory == NULL) {
    g_print ("gstelementfactory: class %s\n", factory->name);
    oclass->elementfactory = factory;
  }

  gst_element_set_name(GST_ELEMENT(element),name);

  return element;
}

/**
 * gst_elementfactory_make:
 * @factoryname: a named factory to instantiate
 * @name: name of new element
 *
 * Create a new element of the type defined by the given elementfactory.
 * It wll be given the name supplied, since all elements require a name as
 * their first argument.
 *
 * Returns: new #GstElement
 */
GstElement*
gst_elementfactory_make (gchar *factoryname, gchar *name) 
{
  GstElementFactory *factory;
  GstElement *element;

  DEBUG("gstelementfactory: make \"%s\" \"%s\"\n", factoryname, name);

  //gst_plugin_load_elementfactory(factoryname);
  factory = gst_elementfactory_find(factoryname);
  if (factory == NULL) return NULL;
  element = gst_elementfactory_create(factory,name);
  return element;
}

/**
 * gst_elementfactory_add_padtemplate :
 * @elementfactory: factory to add the src id to
 * @template: the padtemplate to add
 *
 * Add the given padtemplate to this elementfactory. 
 */
void 
gst_elementfactory_add_padtemplate (GstElementFactory *factory, 
			            GstPadTemplate *template) 
{
  g_return_if_fail(factory != NULL);
  g_return_if_fail(template != NULL);

  factory->padtemplates = g_list_append (factory->padtemplates, template); 
}

gboolean
gst_elementfactory_can_src_caps (GstElementFactory *factory,
		                 GstCaps *caps)
{
  GList *templates;

  g_return_val_if_fail(factory != NULL, FALSE);
  g_return_val_if_fail(caps != NULL, FALSE);

  templates = factory->padtemplates;

  while (templates) {
    GstPadTemplate *template = (GstPadTemplate *)templates->data;

    if (template->direction == GST_PAD_SRC) {
      if (gst_caps_check_compatibility (template->caps, caps))
	return TRUE;
    }
    templates = g_list_next (templates);
  }

  return FALSE;
}

gboolean
gst_elementfactory_can_sink_caps (GstElementFactory *factory,
		                  GstCaps *caps)
{
  GList *templates;

  g_return_val_if_fail(factory != NULL, FALSE);
  g_return_val_if_fail(caps != NULL, FALSE);

  templates = factory->padtemplates;

  while (templates) {
    GstPadTemplate *template = (GstPadTemplate *)templates->data;

    if (template->direction == GST_PAD_SINK) {
      if (gst_caps_check_compatibility (caps, template->caps))
	return TRUE;
    }
    templates = g_list_next (templates);
  }

  return FALSE;
}

/**
 * gst_elementfactory_save_thyself:
 * @factory: factory to save
 * @parent: the parent xmlNodePtr 
 *
 * Saves the factory into an XML tree
 * 
 * Returns: the new xmlNodePtr
 */
xmlNodePtr 
gst_elementfactory_save_thyself (GstElementFactory *factory, 
		                 xmlNodePtr parent) 
{
  GList *pads;

  xmlNewChild(parent,NULL,"name",factory->name);
  xmlNewChild(parent,NULL,"longname", factory->details->longname);
  xmlNewChild(parent,NULL,"class", factory->details->klass);
  xmlNewChild(parent,NULL,"description", factory->details->description);
  xmlNewChild(parent,NULL,"version", factory->details->version);
  xmlNewChild(parent,NULL,"author", factory->details->author);
  xmlNewChild(parent,NULL,"copyright", factory->details->copyright);

  pads = factory->padtemplates;
  if (pads) {
    while (pads) {
      xmlNodePtr subtree;
      GstPadTemplate *padtemplate = (GstPadTemplate *)pads->data;

      subtree = xmlNewChild(parent, NULL, "padtemplate", NULL);
      gst_padtemplate_save_thyself(padtemplate, subtree);

      pads = g_list_next (pads);
    }
  }
  return parent;
}

/**
 * gst_elementfactory_load_thyself:
 * @parent: the parent xmlNodePtr 
 *
 * Creates a new factory from an xmlNodePtr
 * 
 * Returns: the new factory
 */
GstElementFactory *
gst_elementfactory_load_thyself (xmlNodePtr parent) 
{
  GstElementFactory *factory = g_new0(GstElementFactory, 1);
  xmlNodePtr children = parent->childs;
  factory->details = g_new0(GstElementDetails, 1);
  factory->padtemplates = NULL;

  while (children) {
    if (!strcmp(children->name, "name")) {
      factory->name = g_strdup(xmlNodeGetContent(children));
    }
    if (!strcmp(children->name, "longname")) {
      factory->details->longname = g_strdup(xmlNodeGetContent(children));
    }
    if (!strcmp(children->name, "class")) {
      factory->details->klass = g_strdup(xmlNodeGetContent(children));
    }
    if (!strcmp(children->name, "description")) {
      factory->details->description = g_strdup(xmlNodeGetContent(children));
    }
    if (!strcmp(children->name, "version")) {
      factory->details->version = g_strdup(xmlNodeGetContent(children));
    }
    if (!strcmp(children->name, "author")) {
      factory->details->author = g_strdup(xmlNodeGetContent(children));
    }
    if (!strcmp(children->name, "copyright")) {
      factory->details->copyright = g_strdup(xmlNodeGetContent(children));
    }
    if (!strcmp(children->name, "padtemplate")) {
       GstPadTemplate *template;
       
       template = gst_padtemplate_load_thyself (children);

       gst_elementfactory_add_padtemplate (factory, template);
    }

    children = children->next;
  }

  _gst_elementfactories = g_list_prepend (_gst_elementfactories, factory);

  return factory;
}

