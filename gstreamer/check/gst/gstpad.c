/* GStreamer
 * Copyright (C) <2005> Thomas Vander Stichele <thomas at apestaart dot org>
 *
 * gstpad.c: Unit test for GstPad
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

START_TEST (test_link)
{
  GstPad *src, *sink;
  GstPadTemplate *srct;

  GstPadLinkReturn ret;
  gchar *name;

  src = gst_pad_new ("source", GST_PAD_SRC);
  fail_if (src == NULL);

  name = gst_pad_get_name (src);
  fail_unless (strcmp (name, "source") == 0);

  sink = gst_pad_new ("sink", GST_PAD_SINK);
  fail_if (sink == NULL);

  /* linking without templates or caps should fail */
  ret = gst_pad_link (src, sink);
  fail_unless (ret == GST_PAD_LINK_NOFORMAT);

  ASSERT_CRITICAL (gst_pad_get_pad_template (NULL));

  srct = gst_pad_get_pad_template (src);
  fail_unless (srct == NULL);
}

END_TEST
/* threaded link/unlink */
/* use globals */
    GstPad * src, *sink;

void
thread_link_unlink (gpointer data)
{
  THREAD_START ();

  while (THREAD_TEST_RUNNING ()) {
    gst_pad_link (src, sink);
    gst_pad_unlink (src, sink);
    THREAD_SWITCH ();
  }
}

START_TEST (test_link_unlink_threaded)
{
  GstCaps *caps;
  int i;

  src = gst_pad_new ("source", GST_PAD_SRC);
  fail_if (src == NULL);
  sink = gst_pad_new ("sink", GST_PAD_SINK);
  fail_if (sink == NULL);

  caps = gst_caps_new_any ();
  gst_pad_set_caps (src, caps);
  gst_pad_set_caps (sink, caps);

  MAIN_START_THREADS (5, thread_link_unlink, NULL);
  for (i = 0; i < 1000; ++i) {
    gst_pad_is_linked (src);
    gst_pad_is_linked (sink);
    THREAD_SWITCH ();
  }
  MAIN_STOP_THREADS ();
}

END_TEST
START_TEST (test_refcount)
{
  GstPad *src, *sink;
  GstCaps *caps;
  GstPadLinkReturn plr;

  sink = gst_pad_new ("sink", GST_PAD_SINK);
  fail_if (sink == NULL);

  src = gst_pad_new ("src", GST_PAD_SRC);
  fail_if (src == NULL);

  caps = gst_caps_new_any ();
  /* one for me */
  ASSERT_CAPS_REFCOUNT (caps, "caps", 1);

  gst_pad_set_caps (src, caps);
  gst_pad_set_caps (sink, caps);
  /* one for me and one for each set_caps */
  ASSERT_CAPS_REFCOUNT (caps, "caps", 3);

  plr = gst_pad_link (src, sink);
  fail_unless (GST_PAD_LINK_SUCCESSFUL (plr));
  ASSERT_CAPS_REFCOUNT (caps, "caps", 3);

  gst_pad_unlink (src, sink);
  ASSERT_CAPS_REFCOUNT (caps, "caps", 3);

  /* cleanup */
  gst_object_unref (src);
  gst_object_unref (sink);
  ASSERT_CAPS_REFCOUNT (caps, "caps", 1);

  gst_caps_unref (caps);
}

END_TEST
START_TEST (test_get_allowed_caps)
{
  GstPad *src, *sink;
  GstCaps *caps, *gotcaps;
  GstBuffer *buffer;
  GstPadLinkReturn plr;

  ASSERT_CRITICAL (gst_pad_get_allowed_caps (NULL));

  buffer = gst_buffer_new ();
  ASSERT_CRITICAL (gst_pad_get_allowed_caps ((GstPad *) buffer));
  gst_buffer_unref (buffer);

  sink = gst_pad_new ("sink", GST_PAD_SINK);
  ASSERT_CRITICAL (gst_pad_get_allowed_caps (sink));

  src = gst_pad_new ("src", GST_PAD_SRC);
  fail_if (src == NULL);
  caps = gst_pad_get_allowed_caps (src);
  fail_unless (caps == NULL);

  caps = gst_caps_new_any ();

  gst_pad_set_caps (src, caps);
  gst_pad_set_caps (sink, caps);
  ASSERT_CAPS_REFCOUNT (caps, "caps", 3);

  plr = gst_pad_link (src, sink);
  fail_unless (GST_PAD_LINK_SUCCESSFUL (plr));

  gotcaps = gst_pad_get_allowed_caps (src);
  fail_if (gotcaps == NULL);
  fail_unless (gst_caps_is_equal (gotcaps, caps));

  ASSERT_CAPS_REFCOUNT (gotcaps, "gotcaps", 1);
  gst_caps_unref (gotcaps);

  gst_pad_unlink (src, sink);

  /* cleanup */
  ASSERT_CAPS_REFCOUNT (caps, "caps", 3);
  ASSERT_OBJECT_REFCOUNT (src, "src", 1);
  ASSERT_OBJECT_REFCOUNT (sink, "sink", 1);

  gst_object_unref (src);
  gst_object_unref (sink);

  ASSERT_CAPS_REFCOUNT (caps, "caps", 1);
  gst_caps_unref (caps);
}

END_TEST Suite *
gst_pad_suite (void)
{
  Suite *s = suite_create ("GstPad");
  TCase *tc_chain = tcase_create ("general");

  /* turn off timeout */
  tcase_set_timeout (tc_chain, 60);

  suite_add_tcase (s, tc_chain);
  tcase_add_test (tc_chain, test_link);
  tcase_add_test (tc_chain, test_refcount);
  tcase_add_test (tc_chain, test_get_allowed_caps);
  tcase_add_test (tc_chain, test_link_unlink_threaded);
  return s;
}

int
main (int argc, char **argv)
{
  int nf;

  Suite *s = gst_pad_suite ();
  SRunner *sr = srunner_create (s);

  gst_check_init (&argc, &argv);

  srunner_run_all (sr, CK_NORMAL);
  nf = srunner_ntests_failed (sr);
  srunner_free (sr);

  return nf;
}
