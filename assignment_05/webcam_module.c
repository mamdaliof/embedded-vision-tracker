// Source: https://gstreamer.freedesktop.org/documentation/application-development/basics/helloworld.html?gi-language=c#section-helloworld

#include <gst/gst.h>
#include <glib.h>


static gboolean
bus_call (GstBus     *bus,
          GstMessage *msg,
          gpointer    data)
{
  GMainLoop *loop = (GMainLoop *) data;

  switch (GST_MESSAGE_TYPE (msg)) {

    case GST_MESSAGE_EOS:
      g_print ("End of stream\n");
      g_main_loop_quit (loop);
      break;

    case GST_MESSAGE_ERROR: {
      gchar  *debug;
      GError *error;

      gst_message_parse_error (msg, &error, &debug);
      g_free (debug);

      g_printerr ("Error: %s\n", error->message);
      g_error_free (error);

      g_main_loop_quit (loop);
      break;
    }
    default:
      break;
  }

  return TRUE;
}

int
main (int   argc,
      char *argv[])
{
  GMainLoop *loop;

  GstElement *pipeline, *source, *capsfilter, *sink;
  GstCaps *caps;
  GstBus *bus;
  guint bus_watch_id;

  /* Initialisation */
  gst_init (&argc, &argv);

  loop = g_main_loop_new (NULL, FALSE);


  /* Check input arguments */
  if (argc != 2) {
    g_printerr ("Usage: %s <video device path, e.g. /dev/video0>\n", argv[0]);
    return -1;
  }

  /* Create gstreamer elements 
  Logitech C250 webcam supports both MJPEG and YUYV natively. 
  Since I want YUV output and the camera can output YUYV directlym no encoder/decoder is needed.
  `capsfilter` can enforce limitations on the data format. will enforce YUYV format and 640x480 resolution.
  */
  // Source: https://gstreamer.freedesktop.org/documentation/tutorials/basic/short-cutting-the-pipeline.html?gi-language=c
  pipeline = gst_pipeline_new ("webcam-player");
  source   = gst_element_factory_make ("v4l2src",    "webcam-source");
  capsfilter = gst_element_factory_make("capsfilter", "caps");
  sink     = gst_element_factory_make ("filesink",   "file-sink");

  if (!pipeline || !source || !capsfilter || !sink) {
    g_printerr ("One element could not be created. Exiting.\n");
    return -1;
  }

  /* Set up the pipeline */
  /* Force YUYV 640x480 @ 30fps from the camera 
  source: https://gstreamer.freedesktop.org/documentation/gstreamer/gstcaps.html?gi-language=c
  */
    caps = gst_caps_new_simple("video/x-raw",
        "format",    G_TYPE_STRING,       "YUY2",
        "width",     G_TYPE_INT,          640,
        "height",    G_TYPE_INT,          480,
        "framerate", GST_TYPE_FRACTION,   30, 1,
        NULL);
    g_object_set(G_OBJECT(capsfilter), "caps", caps, NULL);
    gst_caps_unref(caps);

  /* Set webcam device and output file */
  g_object_set(G_OBJECT(source), "device", argv[1], NULL); // change based on webcam port.
  g_object_set(G_OBJECT(sink),   "location", "file.yuv",  NULL);

  /* we add a message handler */
  bus = gst_pipeline_get_bus (GST_PIPELINE (pipeline));
  bus_watch_id = gst_bus_add_watch (bus, bus_call, loop);
  gst_object_unref (bus);

  /* we add all elements into the pipeline and link them*/
  /* webcam-source | capsfilter | file-output */
  gst_bin_add_many (GST_BIN (pipeline),
                    source, capsfilter, sink, NULL);
  gst_element_link_many (source, capsfilter, sink, NULL);

  /* Set the pipeline to "playing" state*/
  g_print ("Recording to file.yuv\n");
  gst_element_set_state (pipeline, GST_STATE_PLAYING);
  /* Iterate */
  g_print ("Running ...\n");
  g_main_loop_run (loop);


  /* Cleanup */
  g_print ("Stopping ...\n");
  gst_element_set_state (pipeline, GST_STATE_NULL);

  g_print ("Deleting pipeline\n");
  gst_object_unref (GST_OBJECT (pipeline));
  g_source_remove (bus_watch_id);
  g_main_loop_unref (loop);

  return 0;
}