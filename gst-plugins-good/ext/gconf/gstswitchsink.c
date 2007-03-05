/* GStreamer
 * (c) 2005 Ronald S. Bultje <rbultje@ronald.bitfreak.net>
 * (c) 2006 Jürg Billeter <j@bitron.ch>
 * (c) 2007 Jan Schmidt <thaytan@noraisin.net>
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

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <string.h>

#include "gstswitchsink.h"

GST_DEBUG_CATEGORY_STATIC (switch_debug);
#define GST_CAT_DEFAULT switch_debug

static void gst_switch_sink_dispose (GObject * object);
static GstStateChangeReturn
gst_switch_sink_change_state (GstElement * element, GstStateChange transition);

enum
{
  PROP_0
};

GST_BOILERPLATE (GstSwitchSink, gst_switch_sink, GstBin, GST_TYPE_BIN);

static void gst_switch_sink_set_property (GObject * object, guint prop_id,
    const GValue * value, GParamSpec * pspec);
static void gst_switch_sink_get_property (GObject * object, guint prop_id,
    GValue * value, GParamSpec * pspec);

static void
gst_switch_sink_base_init (gpointer klass)
{
  GST_DEBUG_CATEGORY_INIT (switch_debug, "switchsink", 0, "switchsink element");
}

static void
gst_switch_sink_class_init (GstSwitchSinkClass * klass)
{
  GObjectClass *oklass = G_OBJECT_CLASS (klass);
  GstElementClass *eklass = GST_ELEMENT_CLASS (klass);
  static GstStaticPadTemplate sink_template = GST_STATIC_PAD_TEMPLATE ("sink",
      GST_PAD_SINK,
      GST_PAD_ALWAYS,
      GST_STATIC_CAPS_ANY);
  GstPadTemplate *child_pad_templ;

  oklass->set_property = gst_switch_sink_set_property;
  oklass->get_property = gst_switch_sink_get_property;
  oklass->dispose = gst_switch_sink_dispose;
  eklass->change_state = gst_switch_sink_change_state;

  /* Provide a default pad template if the child didn't */
  child_pad_templ = gst_element_class_get_pad_template (eklass, "sink");
  if (child_pad_templ == NULL) {
    gst_element_class_add_pad_template (eklass,
        gst_static_pad_template_get (&sink_template));
  }
}

static void
gst_switch_sink_reset (GstSwitchSink * sink)
{
  /* this will install fakesink if no other child has been set,
   * otherwise we rely on the subclass to know when to unset its
   * custom kid */
  if (sink->kid == NULL) {
    gst_switch_sink_set_child (sink, NULL);
  }
}

static void
gst_switch_sink_init (GstSwitchSink * sink, GstSwitchSinkClass * g_class)
{
  sink->pad = gst_ghost_pad_new_no_target ("sink", GST_PAD_SINK);
  gst_element_add_pad (GST_ELEMENT (sink), sink->pad);

  gst_switch_sink_reset (sink);
}

static void
gst_switch_sink_dispose (GObject * object)
{
  GstSwitchSink *sink = GST_SWITCH_SINK (object);
  GstElement **p_kid;

  GST_OBJECT_LOCK (sink);
  p_kid = &sink->new_kid;
  gst_object_replace ((GstObject **) p_kid, NULL);

  if (sink->new_kid) {
    p_kid = &sink->kid;
    gst_element_set_state (sink->kid, GST_STATE_NULL);
    gst_object_replace ((GstObject **) p_kid, NULL);
  }
  GST_OBJECT_UNLOCK (sink);

  GST_CALL_PARENT (G_OBJECT_CLASS, dispose, (object));
}

gboolean
gst_switch_commit_new_kid (GstSwitchSink * sink)
{
  GstPad *targetpad;
  GstState cur_state;
  GstElement *new_kid, *old_kid;
  gboolean is_fakesink = FALSE;

  /* need locking around member accesses */
  GST_OBJECT_LOCK (sink);
  cur_state = GST_STATE (sink);
  new_kid = sink->new_kid;
  sink->new_kid = NULL;
  GST_OBJECT_UNLOCK (sink);

  /* Fakesink by default if NULL is passed as the new child */
  if (new_kid == NULL) {
    GST_DEBUG_OBJECT (sink, "Replacing kid with fakesink");
    new_kid = gst_element_factory_make ("fakesink", "testsink");
    /* Add a reference, as it would if the element came from sink->new_kid */
    gst_object_ref (new_kid);
    g_object_set (new_kid, "sync", TRUE, NULL);
    is_fakesink = TRUE;
  } else {
    GST_DEBUG_OBJECT (sink, "Setting new kid");
  }

  if (gst_element_set_state (new_kid, cur_state) == GST_STATE_CHANGE_FAILURE) {
    GST_ELEMENT_ERROR (sink, CORE, STATE_CHANGE, (NULL),
        ("Failed to set state on new child."));
    gst_object_unref (new_kid);
    return FALSE;
  }
  gst_bin_add (GST_BIN (sink), new_kid);

  /* Now, replace the existing child */
  GST_OBJECT_LOCK (sink);
  old_kid = sink->kid;
  sink->kid = new_kid;
  /* Mark whether a custom kid or fakesink has been installed */
  sink->have_kid = !is_fakesink;
  GST_OBJECT_UNLOCK (sink);

  /* kill old element */
  if (old_kid) {
    GST_DEBUG_OBJECT (sink, "Removing old kid %" GST_PTR_FORMAT, old_kid);
    gst_element_set_state (old_kid, GST_STATE_NULL);
    gst_bin_remove (GST_BIN (sink), old_kid);
    gst_object_unref (old_kid);
  }

  /* re-attach ghostpad */
  GST_DEBUG_OBJECT (sink, "Creating new ghostpad");
  targetpad = gst_element_get_pad (sink->kid, "sink");
  gst_ghost_pad_set_target (GST_GHOST_PAD (sink->pad), targetpad);
  gst_object_unref (targetpad);
  GST_DEBUG_OBJECT (sink, "done changing child of switchsink");

  /* FIXME: Push new-segment info and pre-roll buffer(s) into the kid */

  /* Unblock the target pad if necessary */
  if (sink->awaiting_block) {
    gst_pad_set_blocked (sink->pad, FALSE);
    sink->awaiting_block = FALSE;
  }

  return TRUE;
}

gboolean
gst_switch_sink_set_child (GstSwitchSink * sink, GstElement * new_kid)
{
  GstState cur, next;
  GstElement **p_kid;

  /* Nothing to do if clearing the child and we've already installed fakesink */
  if (new_kid == NULL && sink->kid != NULL && sink->have_kid == FALSE)
    return TRUE;

  /* Store the new kid to be committed later */
  GST_OBJECT_LOCK (sink);
  cur = GST_STATE (sink);
  next = GST_STATE_NEXT (sink);
  p_kid = &sink->new_kid;
  gst_object_replace ((GstObject **) p_kid, (GstObject *) new_kid);
  GST_OBJECT_UNLOCK (sink);

  /* Sometime, it would be lovely to allow sink changes even when
   * already running, but this involves sending an appropriate new-segment
   * and possibly prerolling etc */
  /* FIXME: Block the pad and replace the kid when it completes */
  if (cur > GST_STATE_READY || next == GST_STATE_PAUSED) {
    GST_DEBUG_OBJECT (sink,
        "Switch-sink is already running. Ignoring change of child.");
    gst_object_unref (new_kid);
    return TRUE;
  }

  return gst_switch_commit_new_kid (sink);
}

static void
gst_switch_sink_set_property (GObject * object, guint prop_id,
    const GValue * value, GParamSpec * pspec)
{
  GstSwitchSink *sink;

  g_return_if_fail (GST_IS_SWITCH_SINK (object));

  sink = GST_SWITCH_SINK (object);

  switch (prop_id) {
      break;
    default:
      break;
  }
}

#if 0
static gboolean
gst_switch_sink_handle_event (GstPad * pad, GstEvent * event)
{
}
#endif

static void
gst_switch_sink_get_property (GObject * object, guint prop_id,
    GValue * value, GParamSpec * pspec)
{
  GstSwitchSink *sink;

  g_return_if_fail (GST_IS_SWITCH_SINK (object));

  sink = GST_SWITCH_SINK (object);

  switch (prop_id) {
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
  }
}

static GstStateChangeReturn
gst_switch_sink_change_state (GstElement * element, GstStateChange transition)
{
  GstStateChangeReturn ret = GST_STATE_CHANGE_SUCCESS;
  GstSwitchSink *sink = GST_SWITCH_SINK (element);

  ret = GST_CALL_PARENT_WITH_DEFAULT (GST_ELEMENT_CLASS, change_state,
      (element, transition), GST_STATE_CHANGE_SUCCESS);

  switch (transition) {
    case GST_STATE_CHANGE_READY_TO_NULL:
      gst_switch_sink_reset (sink);
      break;
    default:
      break;
  }

  return ret;
}
