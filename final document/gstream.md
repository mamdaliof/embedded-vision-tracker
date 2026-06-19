GStreamer Appsink
Jun 18, 20264 min read

Sources: appsink tutorial github example list of plugins

It allows the application to get access to raw buffer. On the other hand, appsrc allows the application to feed buffers to a pipeline.

It falls under GstElement to be able to use it via API.

What I want is to only return samples when the appsink is in the PLAYING state. All rendered samples will be put in a queue so that the application can pull samples at its own rate.

When the application does not pull samples fast enough, the queued samples could consume a lot of memory

especially when dealing with raw video frames
It’s possible to control the behaviour of the queue with the “leaky-type” and “max-buffers” / “max-bytes” / “max-time” set of properties.
If an EOS event was received before any buffers, this function returns NULL. Use gst_app_sink_is_eos () to check for the EOS condition.

g_signal_emit_by_name (appsink, “pull-sample”, &ret);

leaky-type

When set to any other value than GST_APP_LEAKY_TYPE_NONE then the appsink will drop any buffers that are pushed into it once its internal queue is full. The selected type defines whether to drop the oldest or new buffers.

“leaky-type” GstAppLeakyType *

Tutorial for appsink
Pipelines constructed with GStreamer do not need to be completely closed. Data can be injected into the pipeline and extracted from it at any time.

The element used to inject application data into a GStreamer pipeline is appsrc, and its counterpart, used to extract GStreamer data back to the application is appsink.

appsrc is just a regular source, that provides data magically fallen from the sky (provided by the application, actually).
appsink is a regular sink, where the data flowing through a GStreamer pipeline goes to die (it is recovered by the application, actually).
Buffers

Data travels through a GStreamer pipeline in chunks called buffers. Since this example produces and consumes data, we need to know about GstBuffer. Source Pads produce buffers, that are consumed by Sink Pads; GStreamer takes these buffers and passes them from element to element.

Do not assume all of the buffers have the same size or represent the same amount of time. Elements are free to do with the received buffers as they please. GstBuffers may also contain more than one actual memory buffer. Actual memory buffers are abstracted away using GstMemory objects, and a GstBuffer can contain multiple GstMemory objects.

Every buffer has attached time-stamps and duration, that describe in which moment the content of the buffer should be decoded, rendered or displayed. This is very useful for synchronizing with other sensors in my opinion.

Types of elements:


  GstElement *pipeline, *app_source, *tee, *audio_queue, *audio_convert1, *audio_resample, *audio_sink;
  GstElement *video_queue, *audio_convert2, *visual, *video_convert, *video_sink;
  GstElement *app_queue, *app_sink;
How to create the elements:


  data.app_source = gst_element_factory_make ("appsrc", "audio_source");
  data.tee = gst_element_factory_make ("tee", "tee");
  data.audio_queue = gst_element_factory_make ("queue", "audio_queue");
  data.audio_convert1 = gst_element_factory_make ("audioconvert", "audio_convert1");
  data.audio_resample = gst_element_factory_make ("audioresample", "audio_resample");
  data.audio_sink = gst_element_factory_make ("autoaudiosink", "audio_sink");
  data.video_queue = gst_element_factory_make ("queue", "video_queue");
  data.audio_convert2 = gst_element_factory_make ("audioconvert", "audio_convert2");
  data.visual = gst_element_factory_make ("wavescope", "visual");
  data.video_convert = gst_element_factory_make ("videoconvert", "video_convert");
  data.video_sink = gst_element_factory_make ("autovideosink", "video_sink");
  data.app_queue = gst_element_factory_make ("queue", "app_queue");
  data.app_sink = gst_element_factory_make ("appsink", "app_sink");
Create the empty pipeline and configure appsrc and appsink:


/* Create the empty pipeline */
  data.pipeline = gst_pipeline_new ("test-pipeline");
 
  if (!data.pipeline || !data.app_source || !data.tee || !data.audio_queue || !data.audio_convert1 || !data.audio_resample || !data.audio_sink || !data.video_queue || !data.audio_convert2 || !data.visual || !data.video_convert || !data.video_sink || !data.app_queue || !data.app_sink) {
    g_printerr ("Not all elements could be created.\n");
    return -1;
  }
 
  /* Configure appsrc */
  gst_audio_info_set_format (&info, GST_AUDIO_FORMAT_S16, SAMPLE_RATE, 1, NULL);
  audio_caps = gst_audio_info_to_caps (&info);
  g_object_set (data.app_source, "caps", audio_caps, "format", GST_FORMAT_TIME, NULL);
  g_signal_connect (data.app_source, "need-data", G_CALLBACK (start_feed), &data);
  g_signal_connect (data.app_source, "enough-data", G_CALLBACK (stop_feed), &data);
 
  /* Configure appsink */
  g_object_set (data.app_sink, "emit-signals", TRUE, "caps", audio_caps, NULL);
  g_signal_connect (data.app_sink, "new-sample", G_CALLBACK (new_sample), &data);
  gst_caps_unref (audio_caps);
The first property that needs to be set on the appsrc is caps. It specifies the kind of data that the element is going to produce, so GStreamer can check if linking with downstream elements is possible (this is, if the downstream elements will understand this kind of data). This property must be a GstCaps object, which is easily built from a string with gst_caps_from_string().

We then connect to the need-data and enough-data signals. These are fired by appsrc when its internal queue of data is running low or almost full, respectively. We will use these signals to start and stop (respectively) our signal generation process.

Regarding the appsink configuration, we connect to the new-sample signal, which is emitted every time the sink receives a buffer. Also, the signal emission needs to be enabled through the emit-signals property, because, by default, it is disabled.

Conclusion

Inject data into a pipeline using the appsrcelement.
Retrieve data from a pipeline using the appsink element.
Manipulate this data by accessing the GstBuffer.
