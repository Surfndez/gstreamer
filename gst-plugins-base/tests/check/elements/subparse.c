/* GStreamer unit tests for subparse
 *
 * Copyright (C) 2006 Tim-Philipp Müller <tim centricular net>
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
# include <config.h>
#endif

#include <gst/check/gstcheck.h>

#include <string.h>

static GstStaticPadTemplate sinktemplate = GST_STATIC_PAD_TEMPLATE ("sink",
    GST_PAD_SINK,
    GST_PAD_ALWAYS,
    GST_STATIC_CAPS ("text/plain; text/x-pango-markup")
    );
static GstStaticPadTemplate srctemplate = GST_STATIC_PAD_TEMPLATE ("src",
    GST_PAD_SRC,
    GST_PAD_ALWAYS,
    GST_STATIC_CAPS ("ANY")
    );

static GstElement *subparse;
static GstPad *mysrcpad, *mysinkpad;

static GstBuffer *
buffer_from_static_string (const gchar * s)
{
  GstBuffer *buf;

  buf = gst_buffer_new ();
  GST_BUFFER_DATA (buf) = (guint8 *) s;
  GST_BUFFER_SIZE (buf) = strlen (s);
  GST_BUFFER_FLAG_SET (buf, GST_BUFFER_FLAG_READONLY);

  return buf;
}

typedef struct
{
  const gchar *in;
  GstClockTime from_ts;
  GstClockTime to_ts;
  const gchar *out;
} SubParseInputChunk;

static SubParseInputChunk srt_input[] = {
  {
        "1\n00:00:01,000 --> 00:00:02,000\nOne\n\n",
      1 * GST_SECOND, 2 * GST_SECOND, "One"}, {
        "2\n00:00:02,000 --> 00:00:03,000\nTwo\n\n",
      2 * GST_SECOND, 3 * GST_SECOND, "Two"}, {
        "3\n00:00:03,000 --> 00:00:04,000\nThree\n\n",
      3 * GST_SECOND, 4 * GST_SECOND, "Three"}, {
        "4\n00:00:04,000 --> 00:00:05,000\nFour\n\n",
      4 * GST_SECOND, 5 * GST_SECOND, "Four"}, {
        "5\n00:00:05,000 --> 00:00:06,000\nFive\n\n",
      5 * GST_SECOND, 6 * GST_SECOND, "Five"}, {
        /* markup should be preserved */
        "6\n00:00:06,000 --> 00:00:07,000\n<i>Six</i>\n\n",
      6 * GST_SECOND, 7 * GST_SECOND, "<i>Six</i>"}, {
        /* open markup tags should be closed */
        "7\n00:00:07,000 --> 00:00:08,000\n<i>Seven\n\n",
      7 * GST_SECOND, 8 * GST_SECOND, "<i>Seven</i>"}, {
        /* open markup tags should be closed (II) */
        "8\n00:00:08,000 --> 00:00:09,000\n<b><i>Eight\n\n",
      8 * GST_SECOND, 9 * GST_SECOND, "<b><i>Eight</i></b>"}, {
        /* broken markup should be fixed */
        "9\n00:00:09,000 --> 00:00:10,000\n</b>\n\n",
      9 * GST_SECOND, 10 * GST_SECOND, ""}, {
        "10\n00:00:10,000 --> 00:00:11,000\n</b></i>\n\n",
      10 * GST_SECOND, 11 * GST_SECOND, ""}, {
        "11\n00:00:11,000 --> 00:00:12,000\n<i>xyz</b></i>\n\n",
      11 * GST_SECOND, 12 * GST_SECOND, "<i>xyz</i>"}, {
        "12\n00:00:12,000 --> 00:00:13,000\n<i>xyz</b>\n\n",
      12 * GST_SECOND, 13 * GST_SECOND, "<i>xyz</i>"}, {
        /* skip a few chunk numbers here, the numbers shouldn't matter */
        "24\n00:01:00,000 --> 00:02:00,000\nYep, still here\n\n",
      60 * GST_SECOND, 120 * GST_SECOND, "Yep, still here"}, {
        /* make sure stuff is escaped properly, but allowed markup stays intact */
        "25\n00:03:00,000 --> 00:04:00,000\ngave <i>Rock & Roll</i> to\n\n",
      180 * GST_SECOND, 240 * GST_SECOND, "gave <i>Rock &amp; Roll</i> to"}, {
        "26\n00:04:00,000 --> 00:05:00,000\n<i>Rock & Roll</i>\n\n",
      240 * GST_SECOND, 300 * GST_SECOND, "<i>Rock &amp; Roll</i>"}, {
        "27\n00:06:00,000 --> 00:08:00,000\nRock & Roll\n\n",
      360 * GST_SECOND, 480 * GST_SECOND, "Rock &amp; Roll"}, {
        "28\n00:10:00,000 --> 00:11:00,000\n"
        "<font \"#0000FF\"><joj>This is </xxx>in blue but <5</font>\n\n",
      600 * GST_SECOND, 660 * GST_SECOND, "This is in blue but &lt;5"}
};

static void
setup_subparse (void)
{
  subparse = gst_check_setup_element ("subparse");

  mysrcpad = gst_check_setup_src_pad (subparse, &srctemplate, NULL);
  mysinkpad = gst_check_setup_sink_pad (subparse, &sinktemplate, NULL);

  gst_pad_set_active (mysrcpad, TRUE);
  gst_pad_set_active (mysinkpad, TRUE);

  fail_unless_equals_int (gst_element_set_state (subparse, GST_STATE_PLAYING),
      GST_STATE_CHANGE_SUCCESS);
}

static void
teardown_subparse (void)
{
  GST_DEBUG ("cleaning up");

  g_list_foreach (buffers, (GFunc) gst_mini_object_unref, NULL);
  g_list_free (buffers);
  buffers = NULL;

  gst_pad_set_active (mysrcpad, FALSE);
  gst_pad_set_active (mysinkpad, FALSE);
  gst_check_teardown_sink_pad (subparse);
  gst_check_teardown_src_pad (subparse);
  gst_check_teardown_element (subparse);
  subparse = NULL;
  mysrcpad = NULL;
  mysinkpad = NULL;
}

static void
test_srt_do_test (SubParseInputChunk * input, guint start_idx, guint num)
{
  guint n;

  GST_LOG ("srt test: start_idx = %u, num = %u", start_idx, num);

  setup_subparse ();

  for (n = start_idx; n < start_idx + num; ++n) {
    GstBuffer *buf;

    buf = buffer_from_static_string (input[n].in);
    fail_unless_equals_int (gst_pad_push (mysrcpad, buf), GST_FLOW_OK);
  }

  gst_pad_push_event (mysrcpad, gst_event_new_eos ());

  fail_unless_equals_int (g_list_length (buffers), num);

  for (n = start_idx; n < start_idx + num; ++n) {
    const GstStructure *buffer_caps_struct;
    GstBuffer *buf;
    gchar *out;
    guint out_size;

    buf = g_list_nth_data (buffers, n - start_idx);
    fail_unless (buf != NULL);
    fail_unless (GST_BUFFER_TIMESTAMP_IS_VALID (buf), NULL);
    fail_unless (GST_BUFFER_DURATION_IS_VALID (buf), NULL);
    fail_unless_equals_uint64 (GST_BUFFER_TIMESTAMP (buf), input[n].from_ts);
    fail_unless_equals_uint64 (GST_BUFFER_DURATION (buf),
        input[n].to_ts - input[n].from_ts);
    out = (gchar *) GST_BUFFER_DATA (buf);
    out_size = GST_BUFFER_SIZE (buf);
    /* shouldn't have trailing newline characters */
    fail_if (out_size > 0 && out[out_size - 1] == '\n');
    /* shouldn't include NUL-terminator in data size */
    fail_if (out_size > 0 && out[out_size - 1] == '\0');
    /* but should still have a  NUL-terminator behind the declared data */
    fail_unless_equals_int (out[out_size], '\0');
    /* make sure out string matches expected string */
    fail_unless_equals_string (out, input[n].out);
    /* check caps */
    fail_unless (GST_BUFFER_CAPS (buf) != NULL);
    buffer_caps_struct = gst_caps_get_structure (GST_BUFFER_CAPS (buf), 0);
    fail_unless_equals_string (gst_structure_get_name (buffer_caps_struct),
        "text/x-pango-markup");
  }

  teardown_subparse ();
}

GST_START_TEST (test_srt)
{
  test_srt_do_test (srt_input, 0, G_N_ELEMENTS (srt_input));

  /* make sure everything works fine if we don't start with chunk 1 */
  test_srt_do_test (srt_input, 1, G_N_ELEMENTS (srt_input) - 1);
  test_srt_do_test (srt_input, 2, G_N_ELEMENTS (srt_input) - 2);
  test_srt_do_test (srt_input, 3, G_N_ELEMENTS (srt_input) - 3);
  test_srt_do_test (srt_input, 4, G_N_ELEMENTS (srt_input) - 4);

  /* try with empty input, immediate EOS */
  test_srt_do_test (srt_input, 5, G_N_ELEMENTS (srt_input) - 5);
}

GST_END_TEST;

static void
do_test (SubParseInputChunk * input, guint num, const gchar * media_type)
{
  guint n;

  setup_subparse ();

  for (n = 0; n < num; ++n) {
    GstBuffer *buf;

    buf = buffer_from_static_string (input[n].in);
    fail_unless_equals_int (gst_pad_push (mysrcpad, buf), GST_FLOW_OK);
  }

  gst_pad_push_event (mysrcpad, gst_event_new_eos ());

  fail_unless_equals_int (g_list_length (buffers), num);

  for (n = 0; n < num; ++n) {
    const GstStructure *buffer_caps_struct;
    GstBuffer *buf;
    gchar *out;
    guint out_size;

    buf = g_list_nth_data (buffers, n);
    fail_unless (buf != NULL);
    fail_unless (GST_BUFFER_TIMESTAMP_IS_VALID (buf), NULL);
    fail_unless (GST_BUFFER_DURATION_IS_VALID (buf), NULL);
    fail_unless_equals_uint64 (GST_BUFFER_TIMESTAMP (buf), input[n].from_ts);
    fail_unless_equals_uint64 (GST_BUFFER_DURATION (buf),
        input[n].to_ts - input[n].from_ts);
    out = (gchar *) GST_BUFFER_DATA (buf);
    out_size = GST_BUFFER_SIZE (buf);
    /* shouldn't have trailing newline characters */
    fail_if (out_size > 0 && out[out_size - 1] == '\n');
    /* shouldn't include NUL-terminator in data size */
    fail_if (out_size > 0 && out[out_size - 1] == '\0');
    /* but should still have a  NUL-terminator behind the declared data */
    fail_unless_equals_int (out[out_size], '\0');
    /* make sure out string matches expected string */
    fail_unless_equals_string (out, input[n].out);
    /* check caps */
    fail_unless (GST_BUFFER_CAPS (buf) != NULL);
    buffer_caps_struct = gst_caps_get_structure (GST_BUFFER_CAPS (buf), 0);
    fail_unless_equals_string (gst_structure_get_name (buffer_caps_struct),
        media_type);
  }

  teardown_subparse ();
}

static void
test_tmplayer_do_test (SubParseInputChunk * input, guint num)
{
  do_test (input, num, "text/plain");
}

static void
test_microdvd_do_test (SubParseInputChunk * input, guint num)
{
  do_test (input, num, "text/x-pango-markup");
}

GST_START_TEST (test_tmplayer_multiline)
{
  static SubParseInputChunk tmplayer_multiline_input[] = {
    {
          "00:00:10,1=This is the Earth at a time\n"
          "00:00:10,2=when the dinosaurs roamed...\n" "00:00:13,1=\n",
          10 * GST_SECOND, 13 * GST_SECOND,
        "This is the Earth at a time\nwhen the dinosaurs roamed..."}, {
          "00:00:14,1=a lush and fertile planet.\n" "00:00:16,1=\n",
          14 * GST_SECOND, 16 * GST_SECOND,
        "a lush and fertile planet."}
  };

  test_tmplayer_do_test (tmplayer_multiline_input,
      G_N_ELEMENTS (tmplayer_multiline_input));
}

GST_END_TEST;

GST_START_TEST (test_tmplayer_multiline_with_bogus_lines)
{
  static SubParseInputChunk tmplayer_multiline_b_input[] = {
    {
          "00:00:10,1=This is the Earth at a time\n"
          "Yooboo wabahablablahuguug bogus line hello test 1-2-3-4\n"
          "00:00:10,2=when the dinosaurs roamed...\n" "00:00:13,1=\n",
          10 * GST_SECOND, 13 * GST_SECOND,
        "This is the Earth at a time\nwhen the dinosaurs roamed..."}, {
          "00:00:14,1=a lush and fertile planet.\n" "00:00:16,1=\n",
          14 * GST_SECOND, 16 * GST_SECOND,
        "a lush and fertile planet."}
  };

  test_tmplayer_do_test (tmplayer_multiline_b_input,
      G_N_ELEMENTS (tmplayer_multiline_b_input));
}

GST_END_TEST;

GST_START_TEST (test_tmplayer_style1)
{
  static SubParseInputChunk tmplayer_style1_input[] = {
    {
          "00:00:10:This is the Earth at a time|when the dinosaurs roamed...\n"
          "00:00:13:\n",
          10 * GST_SECOND, 13 * GST_SECOND,
        "This is the Earth at a time\nwhen the dinosaurs roamed..."}, {
          "00:00:14:a lush and fertile planet.\n" "00:00:16:\n",
          14 * GST_SECOND, 16 * GST_SECOND,
        "a lush and fertile planet."}
  };

  test_tmplayer_do_test (tmplayer_style1_input,
      G_N_ELEMENTS (tmplayer_style1_input));
}

GST_END_TEST;

GST_START_TEST (test_tmplayer_style2)
{
  static SubParseInputChunk tmplayer_style2_input[] = {
    {
          "00:00:10=This is the Earth at a time|when the dinosaurs roamed...\n"
          "00:00:13=\n",
          10 * GST_SECOND, 13 * GST_SECOND,
        "This is the Earth at a time\nwhen the dinosaurs roamed..."}, {
          "00:00:14=a lush and fertile planet.\n" "00:00:16=\n",
          14 * GST_SECOND, 16 * GST_SECOND,
        "a lush and fertile planet."}
  };

  test_tmplayer_do_test (tmplayer_style2_input,
      G_N_ELEMENTS (tmplayer_style2_input));
}

GST_END_TEST;

GST_START_TEST (test_tmplayer_style3)
{
  static SubParseInputChunk tmplayer_style3_input[] = {
    {
          "0:00:10:This is the Earth at a time|when the dinosaurs roamed...\n"
          "0:00:13:\n",
          10 * GST_SECOND, 13 * GST_SECOND,
        "This is the Earth at a time\nwhen the dinosaurs roamed..."}, {
          "0:00:14:a lush and fertile planet.\n" "0:00:16:\n",
          14 * GST_SECOND, 16 * GST_SECOND,
        "a lush and fertile planet."}
  };

  test_tmplayer_do_test (tmplayer_style3_input,
      G_N_ELEMENTS (tmplayer_style3_input));
}

GST_END_TEST;

GST_START_TEST (test_tmplayer_style4)
{
  static SubParseInputChunk tmplayer_style4_input[] = {
    {
          "0:00:10=This is the Earth at a time|when the dinosaurs roamed...\n"
          "0:00:13=\n",
          10 * GST_SECOND, 13 * GST_SECOND,
        "This is the Earth at a time\nwhen the dinosaurs roamed..."}, {
          "0:00:14=a lush and fertile planet.\n" "0:00:16=\n",
          14 * GST_SECOND, 16 * GST_SECOND,
        "a lush and fertile planet."}
  };

  test_tmplayer_do_test (tmplayer_style4_input,
      G_N_ELEMENTS (tmplayer_style4_input));
}

GST_END_TEST;

GST_START_TEST (test_tmplayer_style4_with_bogus_lines)
{
  static SubParseInputChunk tmplayer_style4b_input[] = {
    {
          "0:00:10=This is the Earth at a time|when the dinosaurs roamed...\n"
          "# This is a bogus line with a comment and should just be skipped\n"
          "0:00:13=\n",
          10 * GST_SECOND, 13 * GST_SECOND,
        "This is the Earth at a time\nwhen the dinosaurs roamed..."}, {
          "0:00:14=a lush and fertile planet.\n"
          "                                                            \n"
          "0:00:16=\n",
          14 * GST_SECOND, 16 * GST_SECOND,
        "a lush and fertile planet."}
  };

  test_tmplayer_do_test (tmplayer_style4b_input,
      G_N_ELEMENTS (tmplayer_style4b_input));
}

GST_END_TEST;

GST_START_TEST (test_microdvd_with_fps)
{
  static SubParseInputChunk microdvd_input[] = {
    {
          "{1}{1}12.500\n{100}{200}- Hi, Eddie.|- Hiya, Scotty.\n",
          8 * GST_SECOND, 16 * GST_SECOND,
        "<span>- Hi, Eddie.</span>\n<span>- Hiya, Scotty.</span>"}, {
          "{1250}{1350}- Cold enough for you?|- Well, I'm only faintly alive. "
          "It's 25 below\n",
          100 * GST_SECOND, 108 * GST_SECOND,
        "<span>- Cold enough for you?</span>\n"
          "<span>- Well, I&apos;m only faintly alive. It&apos;s 25 below</span>"}
  };

  test_microdvd_do_test (microdvd_input, G_N_ELEMENTS (microdvd_input));

  /* and the same with ',' instead of '.' as floating point divider */
  microdvd_input[0].in =
      "{1}{1}12,500\n{100}{200}- Hi, Eddie.|- Hiya, Scotty.\n";
  test_microdvd_do_test (microdvd_input, G_N_ELEMENTS (microdvd_input));
}

GST_END_TEST;

/* TODO:
 *  - add/modify tests so that lines aren't dogfed to the parsers in complete
 *    lines or sets of complete lines, but rather in random chunks
 */

static Suite *
subparse_suite (void)
{
  Suite *s = suite_create ("subparse");
  TCase *tc_chain = tcase_create ("general");

  suite_add_tcase (s, tc_chain);

  tcase_add_test (tc_chain, test_srt);
  tcase_add_test (tc_chain, test_tmplayer_multiline);
  tcase_add_test (tc_chain, test_tmplayer_multiline_with_bogus_lines);
  tcase_add_test (tc_chain, test_tmplayer_style1);
  tcase_add_test (tc_chain, test_tmplayer_style2);
  tcase_add_test (tc_chain, test_tmplayer_style3);
  tcase_add_test (tc_chain, test_tmplayer_style4);
  tcase_add_test (tc_chain, test_tmplayer_style4_with_bogus_lines);
  tcase_add_test (tc_chain, test_microdvd_with_fps);
  return s;
}

GST_CHECK_MAIN (subparse);
