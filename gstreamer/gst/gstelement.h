/* GStreamer
 * Copyright (C) 1999,2000 Erik Walthinsen <omega@cse.ogi.edu>
 *                    2000 Wim Taymans <wtay@chello.be>
 *
 * gstelement.h: Header for GstElement
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


#ifndef __GST_ELEMENT_H__
#define __GST_ELEMENT_H__

#include <parser.h> // NOTE: this is xml-config's fault

// Include compatability defines: if libxml hasn't already defined these,
// we have an old version 1.x
#ifndef xmlChildrenNode
#define xmlChildrenNode childs
#define xmlRootNode root
#endif

#include <gst/gstobject.h>
#include <gst/gstpad.h>
#include <gst/cothreads.h>


#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */


typedef enum {
  GST_STATE_NONE_PENDING	= 0,
  GST_STATE_NULL		= (1 << 0),
  GST_STATE_READY		= (1 << 1),
  GST_STATE_PLAYING		= (1 << 2),
  GST_STATE_PAUSED		= (1 << 3),
} GstElementState;

typedef enum {
  GST_STATE_FAILURE		= 0,
  GST_STATE_SUCCESS		= 1,
  GST_STATE_ASYNC		= 2,
} GstElementStateReturn;

static inline char *_gst_print_statename(int state) {
  switch (state) {
    case GST_STATE_NONE_PENDING: return "none pending";break;
    case GST_STATE_NULL: return "null";break;
    case GST_STATE_READY: return "ready";break;
    case GST_STATE_PLAYING: return "playing";break;
    case GST_STATE_PAUSED: return "paused";break;
    default: return "";
  }
  return "";
}

#define GST_STATE(obj)			(GST_ELEMENT(obj)->current_state)
#define GST_STATE_PENDING(obj)		(GST_ELEMENT(obj)->pending_state)

// Note: using 8 bit shift mostly "just because", it leaves us enough room to grow <g>
#define GST_STATE_TRANSITION(obj)	((GST_STATE(obj)<<8) | GST_STATE_PENDING(obj))
#define GST_STATE_NULL_TO_READY 	((GST_STATE_NULL<<8) | GST_STATE_READY)
#define GST_STATE_READY_TO_PLAYING 	((GST_STATE_READY<<8) | GST_STATE_PLAYING)
#define GST_STATE_PLAYING_TO_PAUSED	((GST_STATE_PLAYING<<8) | GST_STATE_PAUSED)
#define GST_STATE_PAUSED_TO_PLAYING	((GST_STATE_PAUSED<<8) | GST_STATE_PLAYING)
#define GST_STATE_PLAYING_TO_READY 	((GST_STATE_PLAYING<<8) | GST_STATE_READY)
#define GST_STATE_READY_TO_NULL		((GST_STATE_READY<<8) | GST_STATE_NULL)

#define GST_TYPE_ELEMENT \
  (gst_element_get_type())
#define GST_ELEMENT(obj) \
  (GTK_CHECK_CAST((obj),GST_TYPE_ELEMENT,GstElement))
#define GST_ELEMENT_CLASS(klass) \
  (GTK_CHECK_CLASS_CAST((klass),GST_TYPE_ELEMENT,GstElementClass))
#define GST_IS_ELEMENT(obj) \
  (GTK_CHECK_TYPE((obj),GST_TYPE_ELEMENT))
#define GST_IS_ELEMENT_CLASS(klass) \
  (GTK_CHECK_CLASS_TYPE((klass),GST_TYPE_ELEMENT))

typedef enum {
  /* element is complex (for some def.) and generally require a cothread */
  GST_ELEMENT_COMPLEX		= GST_OBJECT_FLAG_LAST,
  /* input and output pads aren't directly coupled to each other
     examples: queues, multi-output async readers, etc. */
  GST_ELEMENT_DECOUPLED,
  /* this element should be placed in a thread if at all possible */
  GST_ELEMENT_THREAD_SUGGESTED,
  /* this element is incable of seeking (FIXME: does this apply to filters?) */
  GST_ELEMENT_NO_SEEK,

  /* there is a new loopfunction ready for placement */
  GST_ELEMENT_NEW_LOOPFUNC,
  /* the cothread holding this element needs to be stopped */
  GST_ELEMENT_COTHREAD_STOPPING,
  /* the element has to be scheduled as a cothread for any sanity */
  GST_ELEMENT_USE_COTHREAD,

  /* use some padding for future expansion */
  GST_ELEMENT_FLAG_LAST		= GST_OBJECT_FLAG_LAST + 8,
} GstElementFlags;

#define GST_ELEMENT_IS_THREAD_SUGGESTED(obj)	(GST_FLAG_IS_SET(obj,GST_ELEMENT_THREAD_SUGGESTED))
#define GST_ELEMENT_IS_COTHREAD_STOPPING(obj)	(GST_FLAG_IS_SET(obj,GST_ELEMENT_COTHREAD_STOPPING))


typedef struct _GstElement GstElement;
typedef struct _GstElementClass GstElementClass;
typedef struct _GstElementDetails GstElementDetails;
typedef struct _GstElementFactory GstElementFactory;

typedef void (*GstElementLoopFunction) (GstElement *element);

struct _GstElement {
  GstObject object;

  gchar *name;

  guint8 current_state;
  guint8 pending_state;

  GstElementLoopFunction loopfunc;
  cothread_state *threadstate;

  guint16 numpads;
  guint16 numsrcpads;
  guint16 numsinkpads;
  GList *pads;

  GstElement *manager;
};

struct _GstElementClass {
  GstObjectClass parent_class;

  /* the elementfactory that created us */
  GstElementFactory *elementfactory;

  /* signal callbacks */
  void (*state_change) 	(GstElement *element,GstElementState state);
  void (*new_pad) 	(GstElement *element,GstPad *pad);
  void (*new_ghost_pad) (GstElement *element,GstPad *pad);
  void (*error) 	(GstElement *element,gchar *error);
  void (*eos)		(GstElement *element);

  /* change the element state */
  GstElementStateReturn (*change_state) (GstElement *element);

  /* create or read XML representation of self */
  xmlNodePtr	(*save_thyself) 	(GstElement *element, xmlNodePtr parent);
  void 		(*restore_thyself) 	(GstElement *element, xmlNodePtr self, GHashTable *elements);
};

struct _GstElementDetails {
  gchar *longname;              /* long, english name */
  gchar *klass;                 /* type of element, as hierarchy */
  gchar *description;           /* insights of one form or another */
  gchar *version;               /* version of the element */
  gchar *author;                /* who wrote this thing? */
  gchar *copyright;             /* copyright details (year, etc.) */
};

struct _GstElementFactory {
  gchar *name;			/* name of element */
  GtkType type;			/* unique GtkType of element */

  GstElementDetails *details;	/* pointer to details struct */

  GList *padtemplates;
  guint16 numpadtemplates;
};

GtkType 		gst_element_get_type		(void);
GstElement*		gst_element_new			(void);
#define 		gst_element_destroy(element) 	gst_object_destroy (GST_OBJECT (element))

void 			gst_element_set_loop_function	(GstElement *element,
                                   			 GstElementLoopFunction loop);

void 			gst_element_set_name		(GstElement *element, const gchar *name);
const gchar*		gst_element_get_name		(GstElement *element);

void 			gst_element_set_manager		(GstElement *element, GstElement *manager);
GstElement*		gst_element_get_manager		(GstElement *element);

void 			gst_element_add_pad		(GstElement *element, GstPad *pad);
GstPad*			gst_element_get_pad		(GstElement *element, const gchar *name);
GList*			gst_element_get_pad_list	(GstElement *element);
GList*			gst_element_get_padtemplate_list	(GstElement *element);
void 			gst_element_add_ghost_pad	(GstElement *element, GstPad *pad);
void			gst_element_remove_ghost_pad	(GstElement *element, GstPad *pad);

void 			gst_element_connect		(GstElement *src, const gchar *srcpadname,
                         				 GstElement *dest, const gchar *destpadname);
void 			gst_element_disconnect		(GstElement *src, const gchar *srcpadname,
                         				 GstElement *dest, const gchar *destpadname);

void			gst_element_announce_eos	(GstElement *element, gboolean success);
void			gst_element_signal_eos		(GstElement *element);


/* called by the app to set the state of the element */
gint 			gst_element_set_state		(GstElement *element, GstElementState state);

void 			gst_element_error		(GstElement *element, const gchar *error);

GstElementFactory*	gst_element_get_factory		(GstElement *element);

/* XML write and read */
xmlNodePtr 		gst_element_save_thyself	(GstElement *element, xmlNodePtr parent);
GstElement*		gst_element_load_thyself	(xmlNodePtr parent, GHashTable *elements);


/* 
 *
 * factories stuff
 *
 **/
GstElementFactory*	gst_elementfactory_new			(const gchar *name,GtkType type,
                                          			 GstElementDetails *details);
void 			gst_elementfactory_destroy		(GstElementFactory *elementfactory);

void 			gst_elementfactory_add_padtemplate	(GstElementFactory *elementfactory, 
							 	 GstPadTemplate *temp);

GstElementFactory*	gst_elementfactory_find			(const gchar *name);
GList*			gst_elementfactory_get_list		(void);

gboolean		gst_elementfactory_can_src_caps 	(GstElementFactory *factory,
								 GstCaps *caps);
gboolean		gst_elementfactory_can_sink_caps 	(GstElementFactory *factory,
							 	 GstCaps *caps);
gboolean		gst_elementfactory_can_src_caps_list 	(GstElementFactory *factory,
								 GList *caps);
gboolean		gst_elementfactory_can_sink_caps_list 	(GstElementFactory *factory,
							 	 GList *caps);

GstElement*		gst_elementfactory_create		(GstElementFactory *factory,
                                      				 const gchar *name);
/* FIXME this name is wrong, probably so is the one above it */
GstElement*		gst_elementfactory_make			(const gchar *factoryname, const gchar *name);

xmlNodePtr 		gst_elementfactory_save_thyself		(GstElementFactory *factory, xmlNodePtr parent); 
GstElementFactory*	gst_elementfactory_load_thyself		(xmlNodePtr parent);

#ifdef __cplusplus
}
#endif /* __cplusplus */


#endif /* __GST_ELEMENT_H__ */     

