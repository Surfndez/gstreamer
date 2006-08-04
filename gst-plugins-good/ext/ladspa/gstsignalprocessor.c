/* GStreamer
 * Copyright (C) 1999,2000 Erik Walthinsen <omega@cse.ogi.edu>
 *                    2000 Wim Taymans <wtay@chello.be>
 *                    2005 Wim Taymans <wim@fluendo.com>
 *
 * gstsignalprocessor.c: 
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


#include <stdlib.h>

#ifdef HAVE_CONFIG_H
#  include "config.h"
#endif

#include <gst/audio/audio.h>
#include "gstsignalprocessor.h"


GST_DEBUG_CATEGORY_STATIC (gst_signal_processor_debug);
#define GST_CAT_DEFAULT gst_signal_processor_debug


static GstStaticCaps template_caps =
GST_STATIC_CAPS (GST_AUDIO_FLOAT_STANDARD_PAD_TEMPLATE_CAPS);

#define GST_TYPE_SIGNAL_PROCESSOR_PAD_TEMPLATE \
    (gst_signal_processor_pad_template_get_type ())
#define GST_SIGNAL_PROCESSOR_PAD_TEMPLATE(obj) \
    (G_TYPE_CHECK_INSTANCE_CAST((obj),GST_TYPE_SIGNAL_PROCESSOR_PAD_TEMPLATE,\
    GstSignalProcessorPadTemplate))
typedef struct _GstSignalProcessorPadTemplate GstSignalProcessorPadTemplate;
typedef GstPadTemplateClass GstSignalProcessorPadTemplateClass;

struct _GstSignalProcessorPadTemplate
{
  GstPadTemplate parent;

  guint index;
};

static GType
gst_signal_processor_pad_template_get_type (void)
{
  static GType type = 0;

  if (!type) {
    static const GTypeInfo info = {
      sizeof (GstSignalProcessorPadTemplateClass), NULL, NULL, NULL, NULL,
      NULL, sizeof (GstSignalProcessorPadTemplate), 0, NULL
    };

    type = g_type_register_static (GST_TYPE_PAD_TEMPLATE,
        "GstSignalProcessorPadTemplate", &info, 0);
  }
  return type;
}

void
gst_signal_processor_class_add_pad_template (GstSignalProcessorClass * klass,
    const gchar * name, GstPadDirection direction, guint index)
{
  GstPadTemplate *new;

  g_return_if_fail (GST_IS_SIGNAL_PROCESSOR_CLASS (klass));
  g_return_if_fail (name != NULL);
  g_return_if_fail (direction == GST_PAD_SRC || direction == GST_PAD_SINK);

  new = g_object_new (gst_signal_processor_pad_template_get_type (),
      "name", name, NULL);

  GST_PAD_TEMPLATE_NAME_TEMPLATE (new) = g_strdup (name);
  GST_PAD_TEMPLATE_DIRECTION (new) = direction;
  GST_PAD_TEMPLATE_PRESENCE (new) = GST_PAD_ALWAYS;
  GST_PAD_TEMPLATE_CAPS (new) = gst_caps_copy (gst_static_caps_get
      (&template_caps));
  GST_SIGNAL_PROCESSOR_PAD_TEMPLATE (new)->index = index;

  gst_element_class_add_pad_template (GST_ELEMENT_CLASS (klass), new);
  gst_object_unref (new);
}


#define GST_TYPE_SIGNAL_PROCESSOR_PAD (gst_signal_processor_pad_get_type ())
#define GST_SIGNAL_PROCESSOR_PAD(obj) \
    (G_TYPE_CHECK_INSTANCE_CAST((obj),GST_TYPE_SIGNAL_PROCESSOR_PAD,\
    GstSignalProcessorPad))
typedef struct _GstSignalProcessorPad GstSignalProcessorPad;
typedef GstPadClass GstSignalProcessorPadClass;

struct _GstSignalProcessorPad
{
  GstPad parent;

  GstBuffer *pen;

  guint index;

  /* these are only used for sink pads */
  guint samples_avail;
  gfloat *data;
};

static GType
gst_signal_processor_pad_get_type (void)
{
  static GType type = 0;

  if (!type) {
    static const GTypeInfo info = {
      sizeof (GstSignalProcessorPadClass), NULL, NULL, NULL, NULL,
      NULL, sizeof (GstSignalProcessorPad), 0, NULL
    };

    type = g_type_register_static (GST_TYPE_PAD,
        "GstSignalProcessorPad", &info, 0);
  }
  return type;
}


GST_BOILERPLATE (GstSignalProcessor, gst_signal_processor, GstElement,
    GST_TYPE_ELEMENT);


static void gst_signal_processor_finalize (GObject * object);
static void gst_signal_processor_set_property (GObject * object, guint prop_id,
    const GValue * value, GParamSpec * pspec);
static void gst_signal_processor_get_property (GObject * object, guint prop_id,
    GValue * value, GParamSpec * pspec);
static gboolean gst_signal_processor_src_activate_pull (GstPad * pad,
    gboolean active);
static gboolean gst_signal_processor_sink_activate_push (GstPad * pad,
    gboolean active);
static GstStateChangeReturn gst_signal_processor_change_state (GstElement *
    element, GstStateChange transition);

static gboolean gst_signal_processor_event (GstPad * pad, GstEvent * event);
static GstFlowReturn gst_signal_processor_getrange (GstPad * pad,
    guint64 offset, guint length, GstBuffer ** buffer);
static GstFlowReturn gst_signal_processor_chain (GstPad * pad,
    GstBuffer * buffer);
static gboolean gst_signal_processor_setcaps (GstPad * pad, GstCaps * caps);


static void
gst_signal_processor_base_init (gpointer g_class)
{
  GST_DEBUG_CATEGORY_INIT (gst_signal_processor_debug, "gst-dsp", 0,
      "signalprocessor element");
}

static void
gst_signal_processor_class_init (GstSignalProcessorClass * klass)
{
  GObjectClass *gobject_class;
  GstElementClass *gstelement_class;

  gobject_class = G_OBJECT_CLASS (klass);
  gstelement_class = GST_ELEMENT_CLASS (klass);

  gobject_class->finalize = GST_DEBUG_FUNCPTR (gst_signal_processor_finalize);
  gobject_class->set_property =
      GST_DEBUG_FUNCPTR (gst_signal_processor_set_property);
  gobject_class->get_property =
      GST_DEBUG_FUNCPTR (gst_signal_processor_get_property);

  gstelement_class->change_state =
      GST_DEBUG_FUNCPTR (gst_signal_processor_change_state);
}

static void
gst_signal_processor_add_pad_from_template (GstSignalProcessor * self,
    GstPadTemplate * templ)
{
  GstPad *new;

  new = g_object_new (GST_TYPE_SIGNAL_PROCESSOR_PAD,
      "name", GST_OBJECT_NAME (templ), "direction", templ->direction,
      "template", templ, NULL);
  GST_SIGNAL_PROCESSOR_PAD (new)->index =
      GST_SIGNAL_PROCESSOR_PAD_TEMPLATE (templ)->index;

  gst_pad_set_setcaps_function (new,
      GST_DEBUG_FUNCPTR (gst_signal_processor_setcaps));

  if (templ->direction == GST_PAD_SINK) {
    GST_DEBUG ("added new sink pad");

    gst_pad_set_event_function (new,
        GST_DEBUG_FUNCPTR (gst_signal_processor_event));
    gst_pad_set_chain_function (new,
        GST_DEBUG_FUNCPTR (gst_signal_processor_chain));
    gst_pad_set_activatepush_function (new,
        GST_DEBUG_FUNCPTR (gst_signal_processor_sink_activate_push));
  } else {
    GST_DEBUG ("added new src pad");

    gst_pad_set_getrange_function (new,
        GST_DEBUG_FUNCPTR (gst_signal_processor_getrange));
    gst_pad_set_activatepull_function (new,
        GST_DEBUG_FUNCPTR (gst_signal_processor_src_activate_pull));
  }

  gst_element_add_pad (GST_ELEMENT (self), new);
}

static void
gst_signal_processor_init (GstSignalProcessor * self,
    GstSignalProcessorClass * klass)
{
  GList *templates;

  templates =
      gst_element_class_get_pad_template_list (GST_ELEMENT_CLASS (klass));

  while (templates) {
    GstPadTemplate *templ = GST_PAD_TEMPLATE (templates->data);

    gst_signal_processor_add_pad_from_template (self, templ);
    templates = templates->next;
  }

  self->audio_in = g_new0 (gfloat *, klass->num_audio_in);
  self->control_in = g_new0 (gfloat, klass->num_control_in);
  self->audio_out = g_new0 (gfloat *, klass->num_audio_out);
  self->control_out = g_new0 (gfloat, klass->num_control_out);

  /* init */
  self->pending_in = klass->num_audio_in;
  self->pending_out = 0;

  self->sample_rate = 0;
}

static void
gst_signal_processor_finalize (GObject * object)
{
  GstSignalProcessor *self = GST_SIGNAL_PROCESSOR (object);

  g_free (self->audio_in);
  self->audio_in = NULL;
  g_free (self->control_in);
  self->control_in = NULL;
  g_free (self->audio_out);
  self->audio_out = NULL;
  g_free (self->control_out);
  self->control_out = NULL;

  G_OBJECT_CLASS (parent_class)->finalize (object);
}

static gboolean
gst_signal_processor_setcaps (GstPad * pad, GstCaps * caps)
{
  GstSignalProcessor *self;
  GstSignalProcessorClass *klass;

  self = GST_SIGNAL_PROCESSOR (gst_pad_get_parent (pad));
  klass = GST_SIGNAL_PROCESSOR_GET_CLASS (self);

  /* the whole processor has one caps; if the sample rate changes, let subclass
     implementations know */
  if (caps != self->caps) {
    GstStructure *s;
    gint sample_rate;

    s = gst_caps_get_structure (caps, 0);
    if (!gst_structure_get_int (s, "rate", &sample_rate)) {
      GST_WARNING ("got no sample-rate");
      goto impossible;
    } else {
      GST_DEBUG_OBJECT (self, "Got rate=%d", sample_rate);
    }

    if (!klass->setup (self, sample_rate)) {
      goto setup_failed;
    } else {
      self->sample_rate = sample_rate;
      gst_caps_replace (&self->caps, caps);
    }
  } else {
    GST_DEBUG_OBJECT (self, "skipping, have caps already");
  }

  /* FIXME: handle was_active, etc */

  gst_object_unref (self);

  return TRUE;

setup_failed:
  {
    GST_INFO_OBJECT (self, "setup() failed");
    gst_object_unref (self);
    return FALSE;
  }
impossible:
  {
    g_critical ("something impossible happened");
    gst_object_unref (self);
    return FALSE;
  }
}

static gboolean
gst_signal_processor_event (GstPad * pad, GstEvent * event)
{
  GstSignalProcessor *self;
  GstSignalProcessorClass *bclass;
  gboolean ret;

  self = GST_SIGNAL_PROCESSOR (gst_pad_get_parent (pad));
  bclass = GST_SIGNAL_PROCESSOR_GET_CLASS (self);

  /* this probably isn't the correct interface: what about return values, what
     about overriding event_default */
  if (bclass->event)
    bclass->event (self, event);

  ret = gst_pad_event_default (pad, event);

  gst_object_unref (self);

  return ret;
}

static guint
gst_signal_processor_prepare (GstSignalProcessor * self)
{
  GstElement *elem = (GstElement *) self;
  GList *sinks, *srcs;
  guint samples_avail = G_MAXUINT;

  /* first, assign audio_in pointers, and determine the number of samples that
   * we can process */
  for (sinks = elem->sinkpads; sinks; sinks = sinks->next) {
    GstSignalProcessorPad *sinkpad;

    sinkpad = (GstSignalProcessorPad *) sinks->data;
    g_assert (sinkpad->samples_avail > 0);
    samples_avail = MIN (samples_avail, sinkpad->samples_avail);
    self->audio_in[sinkpad->index] = sinkpad->data;
  }

  if (samples_avail == G_MAXUINT) {
    /* we don't have any sink pads, just choose a size -- should fix this
     * function to have a suggested number of samples in the case of
     * gst_pad_pull_range */
    samples_avail = 256;
  }

  /* now assign output buffers. we can avoid allocation by reusing input
     buffers, but only if process() can work in place, and if the input buffer
     is the exact size of the number of samples we are processing. */
  sinks = elem->sinkpads;
  srcs = elem->srcpads;
  while (sinks && srcs) {
    GstSignalProcessorPad *sinkpad, *srcpad;

    sinkpad = (GstSignalProcessorPad *) sinks->data;
    srcpad = (GstSignalProcessorPad *) srcs->data;

    if (GST_BUFFER_SIZE (sinkpad->pen) == samples_avail * sizeof (gfloat)) {
      /* reusable, yay */
      g_assert (sinkpad->samples_avail == samples_avail);
      srcpad->pen = sinkpad->pen;
      sinkpad->pen = NULL;
      self->audio_out[srcpad->index] = sinkpad->data;
      self->pending_out++;

      srcs = srcs->next;
    }

    sinks = sinks->next;
  }

  /* now allocate for any remaining outputs */
  while (srcs) {
    GstSignalProcessorPad *srcpad;
    GstFlowReturn ret;

    srcpad = (GstSignalProcessorPad *) srcs->data;

    ret =
        gst_pad_alloc_buffer_and_set_caps (GST_PAD (srcpad), -1,
        samples_avail, GST_PAD_CAPS (srcpad), &srcpad->pen);

    if (ret != GST_FLOW_OK) {
      self->state = ret;
      return 0;
    } else {
      self->audio_out[srcpad->index] = (gfloat *) GST_BUFFER_DATA (srcpad->pen);
      self->pending_out++;
    }

    srcs = srcs->next;
  }

  return samples_avail;
}

static void
gst_signal_processor_update_inputs (GstSignalProcessor * self, guint nprocessed)
{
  GstElement *elem = (GstElement *) self;
  GList *sinks;

  for (sinks = elem->sinkpads; sinks; sinks = sinks->next) {
    GstSignalProcessorPad *sinkpad;

    sinkpad = (GstSignalProcessorPad *) sinks->data;
    g_assert (sinkpad->samples_avail >= nprocessed);

    if (sinkpad->pen && sinkpad->samples_avail == nprocessed) {
      /* used up this buffer, unpen */
      gst_buffer_unref (sinkpad->pen);
      sinkpad->pen = NULL;
    }

    if (!sinkpad->pen) {
      /* this buffer was used up */
      self->pending_in++;
      sinkpad->data = NULL;
      sinkpad->samples_avail = 0;
    } else {
      /* advance ->data pointers and decrement ->samples_avail, unreffing buffer
         if no samples are left */
      sinkpad->samples_avail -= nprocessed;
      sinkpad->data += nprocessed;      /* gfloat* arithmetic */
    }
  }
}

static void
gst_signal_processor_process (GstSignalProcessor * self)
{
  GstElement *elem;
  GstSignalProcessorClass *klass;
  guint nframes;

  g_return_if_fail (self->pending_in == 0);
  g_return_if_fail (self->pending_out == 0);

  elem = GST_ELEMENT (self);

  nframes = gst_signal_processor_prepare (self);
  if (G_UNLIKELY (nframes == 0))
    goto flow_error;

  klass = GST_SIGNAL_PROCESSOR_GET_CLASS (self);

  GST_LOG_OBJECT (self, "process(%u)", nframes);

  klass->process (self, nframes);

  gst_signal_processor_update_inputs (self, nframes);

  return;

flow_error:
  {
    GST_WARNING ("gst_pad_alloc_buffer_and_set_caps() returned %d",
        self->state);
    return;
  }
}

static void
gst_signal_processor_pen_buffer (GstSignalProcessor * self, GstPad * pad,
    GstBuffer * buffer)
{
  GstSignalProcessorPad *spad = (GstSignalProcessorPad *) pad;

  if (spad->pen) {
    GST_WARNING ("Pad %s:%s already has penned buffer",
        GST_DEBUG_PAD_NAME (pad));
    gst_buffer_unref (buffer);
    return;
  }

  /* keep the reference */
  spad->pen = buffer;
  spad->data = (gfloat *) GST_BUFFER_DATA (buffer);
  spad->samples_avail = GST_BUFFER_SIZE (buffer) / sizeof (float);

  g_assert (self->pending_in != 0);

  self->pending_in--;
}

static void
gst_signal_processor_flush (GstSignalProcessor * self)
{
  GList *pads;

  pads = GST_ELEMENT (self)->pads;

  for (pads = GST_ELEMENT (self)->pads; pads; pads = pads->next) {
    GstSignalProcessorPad *spad = (GstSignalProcessorPad *) pads->data;

    if (spad->pen) {
      gst_buffer_unref (spad->pen);
      spad->pen = NULL;
      spad->data = NULL;
      spad->samples_avail = 0;
    }
  }
}

static void
gst_signal_processor_do_pulls (GstSignalProcessor * self, guint nframes)
{
  GList *sinkpads;

  /* FIXME: not threadsafe atm */

  sinkpads = GST_ELEMENT (self)->sinkpads;

  for (; sinkpads; sinkpads = sinkpads->next) {
    GstSignalProcessorPad *spad = (GstSignalProcessorPad *) sinkpads->data;
    GstFlowReturn ret = GST_FLOW_OK;
    GstBuffer *buf;

    if (spad->pen) {
      g_warning ("Unexpectedly full buffer pen for pad %s:%s",
          GST_DEBUG_PAD_NAME (spad));
      continue;
    }

    ret = gst_pad_pull_range (GST_PAD (spad), -1, nframes, &buf);

    if (ret != GST_FLOW_OK) {
      self->state = ret;
      gst_signal_processor_flush (self);
      return;
    } else if (!buf) {
      g_critical ("Pull failed to make a buffer!");
      self->state = GST_FLOW_ERROR;
      return;
    } else {
      gst_signal_processor_pen_buffer (self, GST_PAD (spad), buf);
    }
  }

  if (self->pending_in != 0) {
    g_critical ("Something wierd happened...");
    self->state = GST_FLOW_ERROR;
  } else {
    gst_signal_processor_process (self);
  }
}

static GstFlowReturn
gst_signal_processor_getrange (GstPad * pad, guint64 offset,
    guint length, GstBuffer ** buffer)
{
  GstSignalProcessor *self;
  GstSignalProcessorPad *spad = (GstSignalProcessorPad *) pad;
  GstFlowReturn ret = GST_FLOW_ERROR;

  self = GST_SIGNAL_PROCESSOR (gst_pad_get_parent (pad));

  if (spad->pen) {
    *buffer = spad->pen;
    spad->pen = NULL;
    g_assert (self->pending_out != 0);
    self->pending_out--;
    ret = GST_FLOW_OK;
  } else {
    gst_signal_processor_do_pulls (self, length);
    if (!spad->pen) {
      /* this is an error condition */
      *buffer = NULL;
      ret = self->state;
    } else {
      *buffer = spad->pen;
      spad->pen = NULL;
      self->pending_out--;
      ret = GST_FLOW_OK;
    }
  }

  GST_DEBUG_OBJECT (self, "returns %s", gst_flow_get_name (ret));

  gst_object_unref (self);

  return ret;
}

static void
gst_signal_processor_do_pushes (GstSignalProcessor * self)
{
  GList *srcpads;

  /* not threadsafe atm */

  srcpads = GST_ELEMENT (self)->srcpads;

  for (; srcpads; srcpads = srcpads->next) {
    GstSignalProcessorPad *spad = (GstSignalProcessorPad *) srcpads->data;
    GstFlowReturn ret = GST_FLOW_OK;

    if (!spad->pen) {
      g_warning ("Unexpectedly empty buffer pen for pad %s:%s",
          GST_DEBUG_PAD_NAME (spad));
      continue;
    }

    ret = gst_pad_push (GST_PAD (spad), spad->pen);

    if (ret != GST_FLOW_OK) {
      self->state = ret;
      gst_signal_processor_flush (self);
      return;
    } else {
      spad->pen = NULL;
      g_assert (self->pending_out > 0);
      self->pending_out--;
    }
  }

  if (self->pending_out != 0) {
    g_critical ("Something wierd happened...");
    self->state = GST_FLOW_ERROR;
  }
}

static GstFlowReturn
gst_signal_processor_chain (GstPad * pad, GstBuffer * buffer)
{
  GstSignalProcessor *self;

  self = GST_SIGNAL_PROCESSOR (gst_pad_get_parent (pad));

  gst_signal_processor_pen_buffer (self, pad, buffer);

  if (self->pending_in == 0) {
    gst_signal_processor_process (self);

    gst_signal_processor_do_pushes (self);
  }

  gst_object_unref (self);

  return self->state;
}

static void
gst_signal_processor_set_property (GObject * object, guint prop_id,
    const GValue * value, GParamSpec * pspec)
{
  /* GstSignalProcessor *self = GST_SIGNAL_PROCESSOR (object); */

  switch (prop_id) {
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
  }
}

static void
gst_signal_processor_get_property (GObject * object, guint prop_id,
    GValue * value, GParamSpec * pspec)
{
  /* GstSignalProcessor *self = GST_SIGNAL_PROCESSOR (object); */

  switch (prop_id) {
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
  }
}

static gboolean
gst_signal_processor_sink_activate_push (GstPad * pad, gboolean active)
{
  gboolean result = TRUE;
  GstSignalProcessor *self;
  GstSignalProcessorClass *bclass;

  self = GST_SIGNAL_PROCESSOR (gst_pad_get_parent (pad));
  bclass = GST_SIGNAL_PROCESSOR_GET_CLASS (self);

  if (active) {
    if (self->mode == GST_ACTIVATE_NONE) {
      self->mode = GST_ACTIVATE_PUSH;
      result = TRUE;
    } else if (self->mode == GST_ACTIVATE_PUSH) {
      result = TRUE;
    } else {
      g_warning ("foo");
      result = FALSE;
    }
  } else {
    if (self->mode == GST_ACTIVATE_NONE) {
      result = TRUE;
    } else if (self->mode == GST_ACTIVATE_PUSH) {
      self->mode = GST_ACTIVATE_NONE;
      result = TRUE;
    } else {
      g_warning ("foo");
      result = FALSE;
    }
  }

  GST_DEBUG_OBJECT (self, "result : %d", result);

  gst_object_unref (self);

  return result;
}

static gboolean
gst_signal_processor_src_activate_pull (GstPad * pad, gboolean active)
{
  gboolean result = TRUE;
  GstSignalProcessor *self;
  GstSignalProcessorClass *bclass;

  self = GST_SIGNAL_PROCESSOR (gst_pad_get_parent (pad));
  bclass = GST_SIGNAL_PROCESSOR_GET_CLASS (self);

  if (active) {
    if (self->mode == GST_ACTIVATE_NONE) {
      GList *l;

      for (l = GST_ELEMENT (self)->sinkpads; l; l = l->next)
        result &= gst_pad_activate_pull (pad, active);
      if (result)
        self->mode = GST_ACTIVATE_PULL;
    } else if (self->mode == GST_ACTIVATE_PULL) {
      result = TRUE;
    } else {
      g_warning ("foo");
      result = FALSE;
    }
  } else {
    if (self->mode == GST_ACTIVATE_NONE) {
      result = TRUE;
    } else if (self->mode == GST_ACTIVATE_PULL) {
      GList *l;

      for (l = GST_ELEMENT (self)->sinkpads; l; l = l->next)
        result &= gst_pad_activate_pull (pad, active);
      if (result)
        self->mode = GST_ACTIVATE_NONE;
      result = TRUE;
    } else {
      g_warning ("foo");
      result = FALSE;
    }
  }

  GST_DEBUG_OBJECT (self, "result : %d", result);

  gst_object_unref (self);

  return result;
}

static GstStateChangeReturn
gst_signal_processor_change_state (GstElement * element,
    GstStateChange transition)
{
  /* GstSignalProcessor *self;
     GstSignalProcessorClass *klass; */
  GstStateChangeReturn result;

  /* self = GST_SIGNAL_PROCESSOR (element);
     klass = GST_SIGNAL_PROCESSOR_GET_CLASS (self); */


  switch (transition) {
    case GST_STATE_CHANGE_NULL_TO_READY:
      break;
    case GST_STATE_CHANGE_READY_TO_PAUSED:
      break;
    case GST_STATE_CHANGE_PAUSED_TO_PLAYING:
      break;
    default:
      break;
  }

  if ((result =
          GST_ELEMENT_CLASS (parent_class)->change_state (element,
              transition)) == GST_STATE_CHANGE_FAILURE)
    goto failure;

  switch (transition) {
    case GST_STATE_CHANGE_PLAYING_TO_PAUSED:
      break;
    case GST_STATE_CHANGE_PAUSED_TO_READY:
      break;
    case GST_STATE_CHANGE_READY_TO_NULL:
      /* gst_signal_processor_cleanup (self); */
      break;
    default:
      break;
  }

  return result;

  /* ERRORS */
failure:
  {
    GST_DEBUG_OBJECT (element, "parent failed state change");
    return result;
  }
}
