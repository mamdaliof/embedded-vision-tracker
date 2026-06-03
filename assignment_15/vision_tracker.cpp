/**
 * Vision Tracker for DE10-Nano / Embedded Systems
 * 
 * Usage: ./vision_tracker <video_device> [--show-video] [--mode=hsv|yuv]
 * 
 * Arguments:
 *   <video_device> : Path to the V4L2 device (e.g., /dev/video0).
 *   --show-video   : Optional. Opens OpenCV windows to display the raw feed and processing masks.
 *   --mode         : Optional. Tracking mode: 'hsv' (LUT lookup) or 'yuv' (Plane masking). Default: hsv.
 * 
 * Logic:
 *   Captures natively in YUYV (16bpp) at 320x240 @ 10fps to save memory bandwidth.
 *   Zero-RGB path: Processes YUV data directly without BGR conversion for tracking.
 */

#include <gst/gst.h>
#include <gst/app/gstappsink.h>
#include <glib.h>
#include <opencv2/opencv.hpp>
#include <iostream>
#include <string>
#include <vector>
#include "lut_gen.hpp"
#include "yuv_process.hpp"

using namespace std;
using namespace cv;

int main(int argc, char *argv[]) {
    GstElement *pipeline, *source, *videoconvert, *capsfilter, *sink;
    GstCaps *caps;
    GstBus *bus;

    /* Initialization */
    gst_init(&argc, &argv);

    if (argc < 2) {
        g_printerr("Usage: %s <video device path> [--show-video] [--mode=hsv|yuv]\n", argv[0]);
        return -1;
    }

    bool show_video = false;
    string mode = "hsv";

    for (int i = 2; i < argc; ++i) {
        string arg = argv[i];
        if (arg == "--show-video") {
            show_video = true;
        } else if (arg.find("--mode=") == 0) {
            mode = arg.substr(7);
        }
    }

    if (mode != "hsv" && mode != "yuv") {
        g_printerr("Invalid mode: %s. Use 'hsv' or 'yuv'.\n", mode.c_str());
        return -1;
    }

    /* Initialize HSV LUT */
    uint8_t hsv_lut[256][256];
    if (mode == "hsv") {
        g_print("Generating HSV LUT...\n");
        generate_hsv_lut(hsv_lut);
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

    /* Set up the pipeline caps: 320x240 @ 10fps YUY2 */
    caps = gst_caps_new_simple("video/x-raw",
        "format", G_TYPE_STRING, "YUY2",
        "width", G_TYPE_INT, 320,
        "height", G_TYPE_INT, 240,
        "framerate", GST_TYPE_FRACTION, 10, 1,
        NULL);
    g_object_set(G_OBJECT(capsfilter), "caps", caps, NULL);
    gst_caps_unref(caps);

    /* Set webcam device */
    g_object_set(G_OBJECT(source), "device", argv[1], NULL);

    /* Configure appsink to pull samples manually */
    g_object_set(G_OBJECT(sink), "emit-signals", FALSE, "sync", FALSE, NULL);

    /* Link: source -> caps -> converter -> sink */
    gst_bin_add_many(GST_BIN(pipeline), source, capsfilter, videoconvert, sink, NULL);
    if (!gst_element_link_many(source, capsfilter, videoconvert, sink, NULL)) {
        g_printerr("Elements could not be linked. Exiting.\n");
        return -1;
    }

    /* Set the pipeline to "playing" state */
    g_print("Initializing Tracking Pipeline (Mode: %s)...\n", mode.c_str());
    gst_element_set_state(pipeline, GST_STATE_PLAYING);
    bus = gst_pipeline_get_bus(GST_PIPELINE(pipeline));

    while (true) {
        GstMessage *msg = gst_bus_pop(bus);
        if (msg != NULL) {
            if (GST_MESSAGE_TYPE(msg) == GST_MESSAGE_EOS || GST_MESSAGE_TYPE(msg) == GST_MESSAGE_ERROR) {
                gst_message_unref(msg);
                break;
            }
            gst_message_unref(msg);
        }

        GstSample *sample = gst_app_sink_pull_sample(GST_APP_SINK(sink));
        if (sample) {
            GstStructure *s = gst_caps_get_structure(gst_sample_get_caps(sample), 0);
            gint width, height;
            gst_structure_get_int(s, "width", &width);
            gst_structure_get_int(s, "height", &height);

            GstBuffer *buffer = gst_sample_get_buffer(sample);
            GstMapInfo map;

            if (gst_buffer_map(buffer, &map, GST_MAP_READ)) {
                Mat yuy2_img(Size(width, height), CV_8UC2, (char*)map.data, Mat::AUTO_STEP);
                Mat mask(height, width, CV_8UC1);

                if (mode == "hsv") {
                    // Manual LUT lookup: YUYV is [Y0, U0, Y1, V0]
                    uint8_t* raw = (uint8_t*)map.data;
                    uint8_t* mptr = mask.data;
                    for (int i = 0; i < width * height / 2; ++i) {
                        uint8_t u = raw[1];
                        uint8_t v = raw[3];
                        uint8_t res = hsv_lut[u][v];
                        *mptr++ = res; // Y0
                        *mptr++ = res; // Y1
                        raw += 4;
                    }
                } else {
                    // YUV plane masking
                    mask = process_yuv_mask(yuy2_img);
                }

                // Denoise
                Mat cross_kernel = getStructuringElement(MORPH_CROSS, Size(3, 3));
                morphologyEx(mask, mask, MORPH_OPEN, cross_kernel, Point(-1, -1), 2);
                
                Moments m = moments(mask, true);
                if (m.m00 > 10) {
                    double raw_x = m.m10 / m.m00;
                    double raw_y = m.m01 / m.m00;
                    double det_x = ((raw_x / static_cast<double>(width)) * 2.0) - 1.0;
                    double det_y = ((raw_y / static_cast<double>(height)) * 2.0) - 1.0;
                    
                    printf("\r[TRACKING] X=%.2f, Y=%.2f          ", det_x, det_y);
                    fflush(stdout);
                } else {
                    printf("\r[SEARCHING]...                      ");
                    fflush(stdout);
                }

                if (show_video) {
                    Mat bgr_img;
                    cvtColor(yuy2_img, bgr_img, COLOR_YUV2BGR_YUY2);
                    
                    if (m.m00 > 10) {
                        circle(bgr_img, Point(m.m10/m.m00, m.m01/m.m00), 10, Scalar(0, 0, 255), -1);
                    }

                    imshow("Feed (Zero-RGB Path)", bgr_img);
                    imshow("Mask", mask);
                    if ((char)waitKey(1) == 27) {
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
    gst_object_unref(GST_OBJECT(bus));
    gst_object_unref(GST_OBJECT(pipeline));
    return 0;
}
