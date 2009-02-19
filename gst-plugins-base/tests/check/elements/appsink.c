/* GStreamer
 *
 * Copyright (C) 2009, Axis Communications AB, LUND, SWEDEN
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

#include <gst/check/gstcheck.h>
#include <gst-libs/gst/app/gstappsink.h>

gint global_testdata;

static GstPad *mysrcpad;

static GstStaticPadTemplate srctemplate = GST_STATIC_PAD_TEMPLATE ("src",
    GST_PAD_SRC,
    GST_PAD_ALWAYS,
    GST_STATIC_CAPS ("application/x-gst-check")
    );

static GstElement *
setup_appsink (void)
{
  GstElement *appsink;

  GST_DEBUG ("setup_appsink");
  appsink = gst_check_setup_element ("appsink");
  mysrcpad = gst_check_setup_src_pad (appsink, &srctemplate, NULL);

  return appsink;
}

static void
cleanup_appsink (GstElement * appsink)
{
  GST_DEBUG ("cleanup_appsink");

  gst_check_teardown_src_pad (appsink);
  gst_check_teardown_element (appsink);
}

/* This function does an operation to it's indata argument and returns it.
 * The exact operation performed doesn't matter. Currently it multiplies with
 * two, but it could do anything. The idea is to use the function to verify
 * that the code calling it gets run. */
gint
operate_on_data (gint indata)
{
  return indata * 2;
}

void
notify_test_function (gpointer userdata)
{
  global_testdata = operate_on_data (GPOINTER_TO_INT (userdata));
}

void
callback_function (GstAppSink * appsink, gpointer callback_data)
{
  global_testdata = operate_on_data (*((gint *) callback_data));
}

void
notify_function (gpointer callback_data)
{
  global_testdata = operate_on_data (*((gint *) callback_data));
}

GST_START_TEST (test_non_clients)
{
  GstElement *sink;
  GstBuffer *buffer;
  GstCaps *caps;

  sink = setup_appsink ();

  ASSERT_SET_STATE (sink, GST_STATE_PLAYING, GST_STATE_CHANGE_ASYNC);

  caps = gst_caps_from_string ("application/x-gst-check");
  buffer = gst_buffer_new_and_alloc (4);
  gst_buffer_set_caps (buffer, caps);
  gst_caps_unref (caps);
  fail_unless (gst_pad_push (mysrcpad, buffer) == GST_FLOW_OK);

  GST_DEBUG ("cleaning up appsink");
  ASSERT_SET_STATE (sink, GST_STATE_NULL, GST_STATE_CHANGE_SUCCESS);
  cleanup_appsink (sink);
}

GST_END_TEST;

/* Verifies that the handoff callback gets run one time when passing a buffer */
GST_START_TEST (test_handoff_callback)
{
  GstElement *sink;
  GstBuffer *buffer;
  GstCaps *caps;
  gint testdata;
  GstAppSinkCallbacks callbacks = { NULL };

  sink = setup_appsink ();

  global_testdata = 0;
  testdata = 5;                 /* Arbitrary value */

  callbacks.new_buffer = callback_function;

  gst_app_sink_set_callbacks (GST_APP_SINK (sink), &callbacks, &testdata, NULL);

  ASSERT_SET_STATE (sink, GST_STATE_PLAYING, GST_STATE_CHANGE_ASYNC);

  caps = gst_caps_from_string ("application/x-gst-check");
  buffer = gst_buffer_new_and_alloc (4);
  gst_buffer_set_caps (buffer, caps);
  gst_caps_unref (caps);
  /* Pushing a buffer should run our callback */
  fail_unless (gst_pad_push (mysrcpad, buffer) == GST_FLOW_OK);

  testdata = operate_on_data (testdata);

  /* If both test_data & global_testdata have been operated on, we're happy. */
  fail_unless (testdata == global_testdata);

  GST_DEBUG ("cleaning up appsink");
  ASSERT_SET_STATE (sink, GST_STATE_NULL, GST_STATE_CHANGE_SUCCESS);
  cleanup_appsink (sink);
}

GST_END_TEST;

/* Verifies that the notify function gets executed when the sink is destroyed */
GST_START_TEST (test_notify0)
{
  GstElement *sink;
  gint testdata;
  GstAppSinkCallbacks callbacks = { NULL };

  sink = gst_element_factory_make ("appsink", NULL);

  global_testdata = 0;
  testdata = 17;                /* Arbitrary value */

  gst_app_sink_set_callbacks (GST_APP_SINK (sink), &callbacks,
      &testdata, (*notify_function));

  GST_DEBUG ("cleaning up appsink");
  /* Destroying sink should call our notify_function */
  gst_object_unref (sink);

  testdata = operate_on_data (testdata);

  /* If both test_data & global_testdata have been operated on, we're happy. */
  fail_unless (testdata == global_testdata);
}

GST_END_TEST;


/* Verifies that the notify function gets executed when
 * gst_app_sink_set_callbacks () gets called */
GST_START_TEST (test_notify1)
{
  GstElement *sink;
  gint testdata;
  GstAppSinkCallbacks callbacks = { NULL };

  sink = gst_element_factory_make ("appsink", NULL);

  global_testdata = 0;
  testdata = 42;                /* Arbitrary value */

  gst_app_sink_set_callbacks (GST_APP_SINK (sink), &callbacks,
      &testdata, (*notify_function));
  gst_app_sink_set_callbacks (GST_APP_SINK (sink), &callbacks, &testdata, NULL);

  testdata = operate_on_data (testdata);

  /* If both test_data & global_testdata have been operated on, we're happy. */
  fail_unless (testdata == global_testdata);

  GST_DEBUG ("cleaning up appsink");
  /* Destroying sink should call our notify_function */
  gst_object_unref (sink);
}

GST_END_TEST;

static Suite *
appsink_suite (void)
{
  Suite *s = suite_create ("appsink");
  TCase *tc_chain = tcase_create ("general");

  suite_add_tcase (s, tc_chain);
  tcase_add_test (tc_chain, test_non_clients);
  tcase_add_test (tc_chain, test_handoff_callback);
  tcase_add_test (tc_chain, test_notify0);
  tcase_add_test (tc_chain, test_notify1);

  return s;
}

int
main (int argc, char **argv)
{
  int nf;

  Suite *s = appsink_suite ();
  SRunner *sr = srunner_create (s);

  gst_check_init (&argc, &argv);

  srunner_run_all (sr, CK_NORMAL);
  nf = srunner_ntests_failed (sr);
  srunner_free (sr);

  return nf;
}
