/* GStreamer
 * Copyright (C) <2005> Thomas Vander Stichele <thomas at apestaart dot org>
 *
 * cleanup.c: Unit test for cleanup of pipelines
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


#include "../gstcheck.h"


static GstElement *
setup_pipeline (gchar * pipe_descr)
{
  GstElement *pipeline;

  pipeline = gst_parse_launch (pipe_descr, NULL);
  g_return_val_if_fail (GST_IS_PIPELINE (pipeline), NULL);
  return pipeline;
}

/* events is a mask of expected events. tevent is the expected terminal event.
   the poll call will time out after half a second.
 */
static void
run_pipeline (GstElement * pipe, gchar * descr,
    GstMessageType events, GstMessageType tevent)
{
  GstBus *bus;
  GstMessageType revent;

  bus = gst_element_get_bus (pipe);
  g_assert (bus);
  gst_element_set_state (pipe, GST_STATE_PLAYING);

  while (1) {
    revent = gst_bus_poll (bus, GST_MESSAGE_ANY, GST_SECOND / 2);

    /* always have to pop the message before getting back into poll */
    if (revent != GST_MESSAGE_UNKNOWN)
      gst_message_unref (gst_bus_pop (bus));

    if (revent == tevent) {
      break;
    } else if (revent == GST_MESSAGE_UNKNOWN) {
      g_critical ("Unexpected timeout in gst_bus_poll, looking for %d: %s",
          tevent, descr);
      break;
    } else if (revent & events) {
      continue;
    }
    g_critical ("Unexpected message received of type %d, looking for %d: %s",
        revent, tevent, descr);
  }

  gst_element_set_state (pipe, GST_STATE_NULL);
  gst_object_unref (GST_OBJECT (pipe));
}

START_TEST (test_pipeline_unref)
{
  gchar *s;
  gint count;
  GstElement *pipeline, *src, *sink;

  s = "fakesrc name=src num-buffers=20 ! fakesink name=sink";
  pipeline = setup_pipeline (s);
  /* get_by_name takes a ref */
  src = gst_bin_get_by_name (GST_BIN (pipeline), "src");
  fail_if (src == NULL);
  sink = gst_bin_get_by_name (GST_BIN (pipeline), "sink");
  fail_if (sink == NULL);

  run_pipeline (pipeline, s, GST_MESSAGE_STATE_CHANGED, GST_MESSAGE_EOS);
  count = GST_OBJECT_REFCOUNT_VALUE (src);
  fail_unless (count == 1, "src has a refcount of %d instead of 1", count);
  count = GST_OBJECT_REFCOUNT_VALUE (sink);
  fail_unless (count == 1, "sink has a refcount of %d instead of 1", count);
  gst_object_unref (GST_OBJECT (src));
  gst_object_unref (GST_OBJECT (sink));
}

END_TEST Suite *
cleanup_suite (void)
{
  Suite *s = suite_create ("Pipeline cleanup");
  TCase *tc_chain = tcase_create ("linear");

  /* time out after 20s, not the default 3 */
  tcase_set_timeout (tc_chain, 20);

  suite_add_tcase (s, tc_chain);
  tcase_add_test (tc_chain, test_pipeline_unref);
  return s;
}

int
main (int argc, char **argv)
{
  int nf;

  Suite *s = cleanup_suite ();
  SRunner *sr = srunner_create (s);

  gst_check_init (&argc, &argv);

  srunner_run_all (sr, CK_NORMAL);
  nf = srunner_ntests_failed (sr);
  srunner_free (sr);

  return nf;
}
