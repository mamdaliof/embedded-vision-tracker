#include <gst/gst.h>
#include <gst/app/gstappsink.h>
#include <glib.h>
#include <opencv2/opencv.hpp>
#include <iostream>

using namespace std;
using namespace cv;

int main(int argc, char *argv[]) {
    GstElement *pipeline, *source, *videoconvert, *capsfilter, *sink;
    GstCaps *caps;
    GstBus *bus;

    /* Initialization */
    gst_init(&argc, &argv);

    if (argc < 2 || argc > 3) {
        g_printerr("Usage: %s <video device path, e.g. /dev/video0> [--show-video]\n", argv[0]);
        return -1;
    }

    bool show_video = false;
    if (argc == 3 && string(argv[2]) == "--show-video") {
        show_video = true;
    }

    /* Create gstreamer elements */
    pipeline = gst_pipeline_new("vision-tracker");
    source = gst_element_factory_make("v4l2src", "webcam-source");
    videoconvert = gst_element_factory_make("videoconvert", "converter");
    capsfilter = gst_element_factory_make("capsfilter", "caps");
    sink = gst_element_factory_make("appsink", "app-sink");

    if (!pipeline || !source || !videoconvert || !capsfilter || !sink) {
        g_printerr("One element could not be created. Exiting.\n");
        return -1;
    }

    /* Set up the pipeline caps: We force BGR format so OpenCV cv::Mat can use it directly */
    caps = gst_caps_new_simple("video/x-raw",
        "format", G_TYPE_STRING, "BGR",
        "width", G_TYPE_INT, 640,
        "height", G_TYPE_INT, 480,
        "framerate", GST_TYPE_FRACTION, 30, 1,
        NULL);
    g_object_set(G_OBJECT(capsfilter), "caps", caps, NULL);
    gst_caps_unref(caps);

    /* Set webcam device */
    g_object_set(G_OBJECT(source), "device", argv[1], NULL);

    /* Configure appsink to NOT emit signals, we will pull samples manually */
    g_object_set(G_OBJECT(sink), "emit-signals", FALSE, "sync", FALSE, NULL);

    /* Add elements into the pipeline and link them */
    gst_bin_add_many(GST_BIN(pipeline), source, videoconvert, capsfilter, sink, NULL);
    gst_element_link_many(source, videoconvert, capsfilter, sink, NULL);

    /* Set the pipeline to "playing" state */
    g_print("Initializing Tracking Pipeline...\n");
    if (show_video) {
        g_print("Video streams enabled. Press ESC in the video window to stop, or Ctrl+C in terminal.\n");
    }
    gst_element_set_state(pipeline, GST_STATE_PLAYING);
    bus = gst_pipeline_get_bus(GST_PIPELINE(pipeline));

    g_print("Running...\n");

    while (true) {
        // Check for bus messages (errors or end of stream)
        GstMessage *msg = gst_bus_pop(bus);
        if (msg != NULL) {
            GstMessageType msg_type = GST_MESSAGE_TYPE(msg);
            if (msg_type == GST_MESSAGE_EOS) {
                g_print("\nEnd of stream\n");
                gst_message_unref(msg);
                break;
            } else if (msg_type == GST_MESSAGE_ERROR) {
                gchar *debug;
                GError *error;
                gst_message_parse_error(msg, &error, &debug);
                g_printerr("\nError: %s\n", error->message);
                g_free(debug);
                g_error_free(error);
                gst_message_unref(msg);
                break;
            }
            gst_message_unref(msg);
        }

        // Pull a sample from the appsink. This blocks until a sample is available.
        GstSample *sample = gst_app_sink_pull_sample(GST_APP_SINK(sink));
        if (sample) {
            GstCaps *sample_caps = gst_sample_get_caps(sample);
            GstStructure *structure = gst_caps_get_structure(sample_caps, 0);
            gint width, height;
            gst_structure_get_int(structure, "width", &width);
            gst_structure_get_int(structure, "height", &height);

            GstBuffer *buffer = gst_sample_get_buffer(sample);
            GstMapInfo map;

            if (gst_buffer_map(buffer, &map, GST_MAP_READ)) {
                // Map the buffer to an OpenCV Mat (YUY2 is 2 bytes per pixel)
                Mat yuy2_img(Size(width, height), CV_8UC2, (char*)map.data, Mat::AUTO_STEP);
                Mat mask;
                Mat display_img; // For visualization if needed

                if (use_ycrcb) {
                    // --- YCrCb Thresholding Logic ---
                    // TODO: Implement YCrCb green detection directly from YUV data
                    g_print("\r[MODE] YCrCb tracking not yet implemented...     ");
                } else {
                    // --- BGR -> HSV Tracking Logic ---
                    Mat bgr_img, hsv_img;
                    cvtColor(yuy2_img, bgr_img, COLOR_YUV2BGR_YUY2);
                    display_img = bgr_img; // Store BGR for display
                    
                    cvtColor(bgr_img, hsv_img, COLOR_BGR2HSV);

                    // Set green for HSV thresholding
                    Scalar lower_green(36, 50, 70); 
                    Scalar upper_green(89, 255, 255);
                    inRange(hsv_img, lower_green, upper_green, mask);
                }

                if (!mask.empty()) {
                    // Denoise the binary mask using morphological opening
                    Mat cross_kernel = getStructuringElement(MORPH_CROSS, Size(3, 3));
                    morphologyEx(mask, mask, MORPH_OPEN, cross_kernel, Point(-1, -1), 5);
                    
                    Moments m = moments(mask, true);
                    double detection_x = 0.0;
                    double detection_y = 0.0;

                    if (m.m00 > 10) {
                        double raw_x = m.m10 / m.m00;
                        double raw_y = m.m01 / m.m00;

                        // Normalization [-1,1] where 0 is center
                        detection_x = ((raw_x / static_cast<double>(width)) * 2.0) - 1.0;
                        detection_y = ((raw_y / static_cast<double>(height)) * 2.0) - 1.0;
                        
                        // Print the tracking target position
                        printf("\r[TRACKING] Green Ball at: X=%.2f, Y=%.2f          ", detection_x, detection_y);
                        fflush(stdout);
                        
                        if (show_video && !display_img.empty()) {
                            circle(display_img, Point(raw_x, raw_y), 10, Scalar(0, 0, 255), -1);
                        }
                    } else {
                        printf("\r[SEARCHING] Green Ball not detected...            ");
                        fflush(stdout);
                    }
                }

                if (show_video) {
                    if (!display_img.empty()) imshow("Webcam View", display_img);
                    if (!mask.empty()) imshow("Masked Binary", mask);
                    // waitKey(1) is REQUIRED to process GUI events on the main thread
                    char key = (char)waitKey(1);
                    if (key == 27) { // 27 is ESC key
                        gst_buffer_unmap(buffer, &map);
                        gst_sample_unref(sample);
                        break;
                    }
                }

                gst_buffer_unmap(buffer, &map);
            }
            gst_sample_unref(sample);
        }
    }

    /* Cleanup */
    g_print("\nStopping...\n");
    gst_element_set_state(pipeline, GST_STATE_NULL);
    g_print("Deleting pipeline\n");
    gst_object_unref(GST_OBJECT(bus));
    gst_object_unref(GST_OBJECT(pipeline));

    return 0;
}
