/* GStreamer
 *
 * unit test for spectrum
 *
 * Copyright (C) <2007> Stefan Kost <ensonic@users.sf.net>
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

#include <unistd.h>

#include <gst/check/gstcheck.h>

GList *buffers = NULL;
gboolean have_eos = FALSE;

/* For ease of programming we use globals to keep refs for our floating
 * src and sink pads we create; otherwise we always have to do get_pad,
 * get_peer, and then remove references in every test function */
GstPad *mysrcpad, *mysinkpad;

#define SPECT_CAPS_TEMPLATE_STRING \
    "audio/x-raw-int, "                                               \
    " width = (int) 16, "                                             \
    " depth = (int) 16, "                                             \
    " signed = (boolean) true, "                                      \
    " endianness = (int) BYTE_ORDER, "                                \
    " rate = (int) [ 1, MAX ], "                                      \
    " channels = (int) [ 1, MAX ]; "                                  \
    "audio/x-raw-int, "                                               \
    " width = (int) 32, "                                             \
    " depth = (int) 32, "                                             \
    " signed = (boolean) true, "                                      \
    " endianness = (int) BYTE_ORDER, "                                \
    " rate = (int) [ 1, MAX ], "                                      \
    " channels = (int) [ 1, MAX ]; "                                  \
    "audio/x-raw-float, "                                             \
    " width = (int) { 32, 64 }, "                                     \
    " endianness = (int) BYTE_ORDER, "                                \
    " rate = (int) [ 1, MAX ], "                                      \
    " channels = (int) [ 1, MAX ]"

#define SPECT_CAPS_STRING_S16 \
  "audio/x-raw-int, " \
    "rate = (int) 44100, " \
    "channels = (int) 1, " \
    "endianness = (int) BYTE_ORDER, " \
    "width = (int) 16, " \
    "depth = (int) 16, " \
    "signed = (boolean) true"

#define SPECT_CAPS_STRING_S32 \
  "audio/x-raw-int, " \
    "rate = (int) 44100, " \
    "channels = (int) 1, " \
    "endianness = (int) BYTE_ORDER, " \
    "width = (int) 32, " \
    "depth = (int) 32, " \
    "signed = (boolean) true"

#define SPECT_CAPS_STRING_F32 \
    "audio/x-raw-float, "                                             \
    " width = (int) 32, "                                             \
    " endianness = (int) BYTE_ORDER, "                                \
    " rate = (int) 44100, "                                           \
    " channels = (int) 1"

#define SPECT_CAPS_STRING_F64 \
    "audio/x-raw-float, "                                             \
    " width = (int) 64, "                                             \
    " endianness = (int) BYTE_ORDER, "                                \
    " rate = (int) 44100, "                                           \
    " channels = (int) 1"

#define SPECT_BANDS 256

static GstStaticPadTemplate sinktemplate = GST_STATIC_PAD_TEMPLATE ("sink",
    GST_PAD_SINK,
    GST_PAD_ALWAYS,
    GST_STATIC_CAPS (SPECT_CAPS_TEMPLATE_STRING)
    );
static GstStaticPadTemplate srctemplate = GST_STATIC_PAD_TEMPLATE ("src",
    GST_PAD_SRC,
    GST_PAD_ALWAYS,
    GST_STATIC_CAPS (SPECT_CAPS_TEMPLATE_STRING)
    );

/* takes over reference for outcaps */
static GstElement *
setup_spectrum (void)
{
  GstElement *spectrum;

  GST_DEBUG ("setup_spectrum");
  spectrum = gst_check_setup_element ("spectrum");
  mysrcpad = gst_check_setup_src_pad (spectrum, &srctemplate, NULL);
  mysinkpad = gst_check_setup_sink_pad (spectrum, &sinktemplate, NULL);
  gst_pad_set_active (mysrcpad, TRUE);
  gst_pad_set_active (mysinkpad, TRUE);

  return spectrum;
}

static void
cleanup_spectrum (GstElement * spectrum)
{
  GST_DEBUG ("cleanup_spectrum");

  gst_pad_set_active (mysrcpad, FALSE);
  gst_pad_set_active (mysinkpad, FALSE);
  gst_check_teardown_src_pad (spectrum);
  gst_check_teardown_sink_pad (spectrum);
  gst_check_teardown_element (spectrum);

  g_list_foreach (buffers, (GFunc) gst_mini_object_unref, NULL);
  g_list_free (buffers);
  buffers = NULL;
}


GST_START_TEST (test_int16)
{
  GstElement *spectrum;
  GstBuffer *inbuffer, *outbuffer;
  GstBus *bus;
  GstCaps *caps;
  GstMessage *message;
  const GstStructure *structure;
  int i, j;
  gint16 *data;
  const GValue *list, *value;
  GstClockTime endtime;
  gfloat level;

  spectrum = setup_spectrum ();
  g_object_set (spectrum, "message", TRUE, "interval", GST_SECOND / 100,
      "bands", SPECT_BANDS, "threshold", -80, NULL);

  fail_unless (gst_element_set_state (spectrum,
          GST_STATE_PLAYING) == GST_STATE_CHANGE_SUCCESS,
      "could not set to playing");

  /* create a 1 sec buffer with an 11025 Hz sine wave */
  inbuffer = gst_buffer_new_and_alloc (44100 * sizeof (gint16));
  data = (gint16 *) GST_BUFFER_DATA (inbuffer);

  for (j = 0; j < 44100; j += 4) {
    *data = 0;
    ++data;
    *data = 32767;
    ++data;
    *data = 0;
    ++data;
    *data = -32767;
    ++data;
  }

  caps = gst_caps_from_string (SPECT_CAPS_STRING_S16);
  gst_buffer_set_caps (inbuffer, caps);
  gst_caps_unref (caps);
  ASSERT_BUFFER_REFCOUNT (inbuffer, "inbuffer", 1);

  /* create a bus to get the spectrum message on */
  bus = gst_bus_new ();
  ASSERT_OBJECT_REFCOUNT (bus, "bus", 1);
  gst_element_set_bus (spectrum, bus);
  ASSERT_OBJECT_REFCOUNT (bus, "bus", 2);

  /* pushing gives away my reference ... */
  fail_unless (gst_pad_push (mysrcpad, inbuffer) == GST_FLOW_OK);
  /* ... but it ends up being collected on the global buffer list */
  ASSERT_BUFFER_REFCOUNT (inbuffer, "inbuffer", 1);
  fail_unless_equals_int (g_list_length (buffers), 1);
  fail_if ((outbuffer = (GstBuffer *) buffers->data) == NULL);
  fail_unless (inbuffer == outbuffer);

  message = gst_bus_poll (bus, GST_MESSAGE_ELEMENT, -1);
  ASSERT_OBJECT_REFCOUNT (message, "message", 1);

  fail_unless (message != NULL);
  fail_unless (GST_MESSAGE_SRC (message) == GST_OBJECT (spectrum));
  fail_unless (GST_MESSAGE_TYPE (message) == GST_MESSAGE_ELEMENT);
  structure = gst_message_get_structure (message);
  fail_if (structure == NULL);
  fail_unless_equals_string ((char *) gst_structure_get_name (structure),
      "spectrum");
  fail_unless (gst_structure_get_clock_time (structure, "endtime", &endtime));

  list = gst_structure_get_value (structure, "magnitude");
  for (i = 0; i < SPECT_BANDS; ++i) {
    value = gst_value_list_get_value (list, i);
    level = g_value_get_float (value);
    GST_DEBUG ("band[%3d] is %.2f", i, level);
    /* Only the bands in the middle should have a level above 60 */
    fail_if ((i == SPECT_BANDS / 2 || i == SPECT_BANDS / 2 - 1)
        && level < -20.0);
    fail_if ((i != SPECT_BANDS / 2 && i != SPECT_BANDS / 2 - 1)
        && level > -20.0);
  }
  fail_unless_equals_int (g_list_length (buffers), 1);
  fail_if ((outbuffer = (GstBuffer *) buffers->data) == NULL);
  fail_unless (inbuffer == outbuffer);

  /* clean up */
  /* flush current messages,and future state change messages */
  gst_bus_set_flushing (bus, TRUE);

  /* message has a ref to the element */
  ASSERT_OBJECT_REFCOUNT (spectrum, "spectrum", 2);
  gst_message_unref (message);
  ASSERT_OBJECT_REFCOUNT (spectrum, "spectrum", 1);

  gst_element_set_bus (spectrum, NULL);
  ASSERT_OBJECT_REFCOUNT (bus, "bus", 1);
  gst_object_unref (bus);
  fail_unless (gst_element_set_state (spectrum,
          GST_STATE_NULL) == GST_STATE_CHANGE_SUCCESS, "could not set to null");
  ASSERT_OBJECT_REFCOUNT (spectrum, "spectrum", 1);
  cleanup_spectrum (spectrum);
}

GST_END_TEST;

GST_START_TEST (test_int32)
{
  GstElement *spectrum;
  GstBuffer *inbuffer, *outbuffer;
  GstBus *bus;
  GstCaps *caps;
  GstMessage *message;
  const GstStructure *structure;
  int i, j;
  gint32 *data;
  const GValue *list, *value;
  GstClockTime endtime;
  gfloat level;

  spectrum = setup_spectrum ();
  g_object_set (spectrum, "message", TRUE, "interval", GST_SECOND / 100,
      "bands", SPECT_BANDS, "threshold", -80, NULL);

  fail_unless (gst_element_set_state (spectrum,
          GST_STATE_PLAYING) == GST_STATE_CHANGE_SUCCESS,
      "could not set to playing");

  /* create a 1 sec buffer with an 11025 Hz sine wave */
  inbuffer = gst_buffer_new_and_alloc (44100 * sizeof (gint32));
  data = (gint32 *) GST_BUFFER_DATA (inbuffer);
  for (j = 0; j < 44100; j += 4) {
    *data = 0;
    ++data;
    *data = 2147483647;
    ++data;
    *data = 0;
    ++data;
    *data = -2147483647;
    ++data;
  }
  caps = gst_caps_from_string (SPECT_CAPS_STRING_S32);
  gst_buffer_set_caps (inbuffer, caps);
  gst_caps_unref (caps);
  ASSERT_BUFFER_REFCOUNT (inbuffer, "inbuffer", 1);

  /* create a bus to get the spectrum message on */
  bus = gst_bus_new ();
  ASSERT_OBJECT_REFCOUNT (bus, "bus", 1);
  gst_element_set_bus (spectrum, bus);
  ASSERT_OBJECT_REFCOUNT (bus, "bus", 2);

  /* pushing gives away my reference ... */
  fail_unless (gst_pad_push (mysrcpad, inbuffer) == GST_FLOW_OK);
  /* ... but it ends up being collected on the global buffer list */
  ASSERT_BUFFER_REFCOUNT (inbuffer, "inbuffer", 1);
  fail_unless_equals_int (g_list_length (buffers), 1);
  fail_if ((outbuffer = (GstBuffer *) buffers->data) == NULL);
  fail_unless (inbuffer == outbuffer);

  message = gst_bus_poll (bus, GST_MESSAGE_ELEMENT, -1);
  ASSERT_OBJECT_REFCOUNT (message, "message", 1);

  fail_unless (message != NULL);
  fail_unless (GST_MESSAGE_SRC (message) == GST_OBJECT (spectrum));
  fail_unless (GST_MESSAGE_TYPE (message) == GST_MESSAGE_ELEMENT);
  structure = gst_message_get_structure (message);
  fail_if (structure == NULL);
  fail_unless_equals_string ((char *) gst_structure_get_name (structure),
      "spectrum");
  fail_unless (gst_structure_get_clock_time (structure, "endtime", &endtime));

  list = gst_structure_get_value (structure, "magnitude");
  for (i = 0; i < SPECT_BANDS; ++i) {
    value = gst_value_list_get_value (list, i);
    level = g_value_get_float (value);
    GST_DEBUG ("band[%3d] is %.2f", i, level);
    /* Only the bands in the middle should have a level above 60 */
    fail_if ((i == SPECT_BANDS / 2 || i == SPECT_BANDS / 2 - 1)
        && level < -20.0);
    fail_if ((i != SPECT_BANDS / 2 && i != SPECT_BANDS / 2 - 1)
        && level > -20.0);
  }
  fail_unless_equals_int (g_list_length (buffers), 1);
  fail_if ((outbuffer = (GstBuffer *) buffers->data) == NULL);
  fail_unless (inbuffer == outbuffer);

  /* clean up */
  /* flush current messages,and future state change messages */
  gst_bus_set_flushing (bus, TRUE);

  /* message has a ref to the element */
  ASSERT_OBJECT_REFCOUNT (spectrum, "spectrum", 2);
  gst_message_unref (message);
  ASSERT_OBJECT_REFCOUNT (spectrum, "spectrum", 1);

  gst_element_set_bus (spectrum, NULL);
  ASSERT_OBJECT_REFCOUNT (bus, "bus", 1);
  gst_object_unref (bus);
  fail_unless (gst_element_set_state (spectrum,
          GST_STATE_NULL) == GST_STATE_CHANGE_SUCCESS, "could not set to null");
  ASSERT_OBJECT_REFCOUNT (spectrum, "spectrum", 1);
  cleanup_spectrum (spectrum);
}

GST_END_TEST;

GST_START_TEST (test_float32)
{
  GstElement *spectrum;
  GstBuffer *inbuffer, *outbuffer;
  GstBus *bus;
  GstCaps *caps;
  GstMessage *message;
  const GstStructure *structure;
  int i, j;
  gfloat *data;
  const GValue *list, *value;
  GstClockTime endtime;
  gfloat level;

  spectrum = setup_spectrum ();
  g_object_set (spectrum, "message", TRUE, "interval", GST_SECOND / 100,
      "bands", SPECT_BANDS, "threshold", -80, NULL);

  fail_unless (gst_element_set_state (spectrum,
          GST_STATE_PLAYING) == GST_STATE_CHANGE_SUCCESS,
      "could not set to playing");

  /* create a 1 sec buffer with an 11025 Hz sine wave */
  inbuffer = gst_buffer_new_and_alloc (44100 * sizeof (gfloat));
  data = (gfloat *) GST_BUFFER_DATA (inbuffer);
  for (j = 0; j < 44100; j += 4) {
    *data = 0.0;
    ++data;
    *data = 1.0;
    ++data;
    *data = 0.0;
    ++data;
    *data = -1.0;
    ++data;
  }
  caps = gst_caps_from_string (SPECT_CAPS_STRING_F32);
  gst_buffer_set_caps (inbuffer, caps);
  gst_caps_unref (caps);
  ASSERT_BUFFER_REFCOUNT (inbuffer, "inbuffer", 1);

  /* create a bus to get the spectrum message on */
  bus = gst_bus_new ();
  ASSERT_OBJECT_REFCOUNT (bus, "bus", 1);
  gst_element_set_bus (spectrum, bus);
  ASSERT_OBJECT_REFCOUNT (bus, "bus", 2);

  /* pushing gives away my reference ... */
  fail_unless (gst_pad_push (mysrcpad, inbuffer) == GST_FLOW_OK);
  /* ... but it ends up being collected on the global buffer list */
  ASSERT_BUFFER_REFCOUNT (inbuffer, "inbuffer", 1);
  fail_unless_equals_int (g_list_length (buffers), 1);
  fail_if ((outbuffer = (GstBuffer *) buffers->data) == NULL);
  fail_unless (inbuffer == outbuffer);

  message = gst_bus_poll (bus, GST_MESSAGE_ELEMENT, -1);
  ASSERT_OBJECT_REFCOUNT (message, "message", 1);

  fail_unless (message != NULL);
  fail_unless (GST_MESSAGE_SRC (message) == GST_OBJECT (spectrum));
  fail_unless (GST_MESSAGE_TYPE (message) == GST_MESSAGE_ELEMENT);
  structure = gst_message_get_structure (message);
  fail_if (structure == NULL);
  fail_unless_equals_string ((char *) gst_structure_get_name (structure),
      "spectrum");
  fail_unless (gst_structure_get_clock_time (structure, "endtime", &endtime));

  list = gst_structure_get_value (structure, "magnitude");
  for (i = 0; i < SPECT_BANDS; ++i) {
    value = gst_value_list_get_value (list, i);
    level = g_value_get_float (value);
    GST_DEBUG ("band[%3d] is %.2f", i, level);
    /* Only the bands in the middle should have a level above 60 */
    fail_if ((i == SPECT_BANDS / 2 || i == SPECT_BANDS / 2 - 1)
        && level < -20.0);
    fail_if ((i != SPECT_BANDS / 2 && i != SPECT_BANDS / 2 - 1)
        && level > -20.0);
  }
  fail_unless_equals_int (g_list_length (buffers), 1);
  fail_if ((outbuffer = (GstBuffer *) buffers->data) == NULL);
  fail_unless (inbuffer == outbuffer);

  /* clean up */
  /* flush current messages,and future state change messages */
  gst_bus_set_flushing (bus, TRUE);

  /* message has a ref to the element */
  ASSERT_OBJECT_REFCOUNT (spectrum, "spectrum", 2);
  gst_message_unref (message);
  ASSERT_OBJECT_REFCOUNT (spectrum, "spectrum", 1);

  gst_element_set_bus (spectrum, NULL);
  ASSERT_OBJECT_REFCOUNT (bus, "bus", 1);
  gst_object_unref (bus);
  fail_unless (gst_element_set_state (spectrum,
          GST_STATE_NULL) == GST_STATE_CHANGE_SUCCESS, "could not set to null");
  ASSERT_OBJECT_REFCOUNT (spectrum, "spectrum", 1);
  cleanup_spectrum (spectrum);
}

GST_END_TEST;

GST_START_TEST (test_float64)
{
  GstElement *spectrum;
  GstBuffer *inbuffer, *outbuffer;
  GstBus *bus;
  GstCaps *caps;
  GstMessage *message;
  const GstStructure *structure;
  int i, j;
  gdouble *data;
  const GValue *list, *value;
  GstClockTime endtime;
  gfloat level;

  spectrum = setup_spectrum ();
  g_object_set (spectrum, "message", TRUE, "interval", GST_SECOND / 100,
      "bands", SPECT_BANDS, "threshold", -80, NULL);

  fail_unless (gst_element_set_state (spectrum,
          GST_STATE_PLAYING) == GST_STATE_CHANGE_SUCCESS,
      "could not set to playing");

  /* create a 1 sec buffer with an 11025 Hz sine wave */
  inbuffer = gst_buffer_new_and_alloc (44100 * sizeof (gdouble));
  data = (gdouble *) GST_BUFFER_DATA (inbuffer);
  for (j = 0; j < 44100; j += 4) {
    *data = 0.0;
    ++data;
    *data = 1.0;
    ++data;
    *data = 0.0;
    ++data;
    *data = -1.0;
    ++data;
  }
  caps = gst_caps_from_string (SPECT_CAPS_STRING_F64);
  gst_buffer_set_caps (inbuffer, caps);
  gst_caps_unref (caps);
  ASSERT_BUFFER_REFCOUNT (inbuffer, "inbuffer", 1);

  /* create a bus to get the spectrum message on */
  bus = gst_bus_new ();
  ASSERT_OBJECT_REFCOUNT (bus, "bus", 1);
  gst_element_set_bus (spectrum, bus);
  ASSERT_OBJECT_REFCOUNT (bus, "bus", 2);

  /* pushing gives away my reference ... */
  fail_unless (gst_pad_push (mysrcpad, inbuffer) == GST_FLOW_OK);
  /* ... but it ends up being collected on the global buffer list */
  ASSERT_BUFFER_REFCOUNT (inbuffer, "inbuffer", 1);
  fail_unless_equals_int (g_list_length (buffers), 1);
  fail_if ((outbuffer = (GstBuffer *) buffers->data) == NULL);
  fail_unless (inbuffer == outbuffer);

  message = gst_bus_poll (bus, GST_MESSAGE_ELEMENT, -1);
  ASSERT_OBJECT_REFCOUNT (message, "message", 1);

  fail_unless (message != NULL);
  fail_unless (GST_MESSAGE_SRC (message) == GST_OBJECT (spectrum));
  fail_unless (GST_MESSAGE_TYPE (message) == GST_MESSAGE_ELEMENT);
  structure = gst_message_get_structure (message);
  fail_if (structure == NULL);
  fail_unless_equals_string ((char *) gst_structure_get_name (structure),
      "spectrum");
  fail_unless (gst_structure_get_clock_time (structure, "endtime", &endtime));

  list = gst_structure_get_value (structure, "magnitude");
  for (i = 0; i < SPECT_BANDS; ++i) {
    value = gst_value_list_get_value (list, i);
    level = g_value_get_float (value);
    GST_DEBUG ("band[%3d] is %.2f", i, level);
    /* Only the bands in the middle should have a level above 60 */
    fail_if ((i == SPECT_BANDS / 2 || i == SPECT_BANDS / 2 - 1)
        && level < -20.0);
    fail_if ((i != SPECT_BANDS / 2 && i != SPECT_BANDS / 2 - 1)
        && level > -20.0);
  }
  fail_unless_equals_int (g_list_length (buffers), 1);
  fail_if ((outbuffer = (GstBuffer *) buffers->data) == NULL);
  fail_unless (inbuffer == outbuffer);

  /* clean up */
  /* flush current messages,and future state change messages */
  gst_bus_set_flushing (bus, TRUE);

  /* message has a ref to the element */
  ASSERT_OBJECT_REFCOUNT (spectrum, "spectrum", 2);
  gst_message_unref (message);
  ASSERT_OBJECT_REFCOUNT (spectrum, "spectrum", 1);

  gst_element_set_bus (spectrum, NULL);
  ASSERT_OBJECT_REFCOUNT (bus, "bus", 1);
  gst_object_unref (bus);
  fail_unless (gst_element_set_state (spectrum,
          GST_STATE_NULL) == GST_STATE_CHANGE_SUCCESS, "could not set to null");
  ASSERT_OBJECT_REFCOUNT (spectrum, "spectrum", 1);
  cleanup_spectrum (spectrum);
}

GST_END_TEST;


static Suite *
spectrum_suite (void)
{
  Suite *s = suite_create ("spectrum");
  TCase *tc_chain = tcase_create ("general");

  suite_add_tcase (s, tc_chain);
  tcase_add_test (tc_chain, test_int16);
  tcase_add_test (tc_chain, test_int32);
  tcase_add_test (tc_chain, test_float32);
  tcase_add_test (tc_chain, test_float64);

  return s;
}

int
main (int argc, char **argv)
{
  int nf;

  Suite *s = spectrum_suite ();
  SRunner *sr = srunner_create (s);

  gst_check_init (&argc, &argv);

  srunner_run_all (sr, CK_NORMAL);
  nf = srunner_ntests_failed (sr);
  srunner_free (sr);

  return nf;
}
