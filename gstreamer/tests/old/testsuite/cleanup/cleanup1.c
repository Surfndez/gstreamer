#include <gst/gst.h>

static GstElement *
create_pipeline (void)
{
  GstElement *fakesrc, *fakesink;
  GstElement *pipeline;

  
  pipeline = gst_pipeline_new ("main_pipeline");

  fakesrc = gst_elementfactory_make ("fakesrc", "fakesrc");
  fakesink = gst_elementfactory_make ("fakesink", "fakesink");

  gst_element_connect (fakesrc, "src", fakesink, "sink");

  gst_bin_add (GST_BIN (pipeline), fakesrc);
  gst_bin_add (GST_BIN (pipeline), fakesink);

  g_object_set (G_OBJECT (fakesrc), "num_buffers", 5, NULL);

  return pipeline;
}

gint
main (gint argc, gchar *argv[])
{
  GstElement *pipeline;
  gint i;

  gst_init (&argc, &argv);

  i = 10000;

  while (i--) {
    g_print ("create...\n");
    pipeline = create_pipeline ();
	  
    gst_element_set_state (pipeline, GST_STATE_PLAYING);

    while (gst_bin_iterate (GST_BIN (pipeline)));

    gst_element_set_state (pipeline, GST_STATE_NULL);

    gst_element_set_state (pipeline, GST_STATE_PLAYING);

    while (gst_bin_iterate (GST_BIN (pipeline)));

    gst_element_set_state (pipeline, GST_STATE_NULL);

    g_print ("cleanup...\n");
    gst_object_unref (GST_OBJECT (pipeline));

    g_mem_chunk_info ();
  }

  return 0;
}
