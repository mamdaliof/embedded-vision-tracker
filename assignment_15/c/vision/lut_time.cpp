/**
 * Vision Tracker for DE10-Nano / Embedded Systems - BENCHMARK EDITION
 * Usage: ./bench_yuv <video_device> [--show-video] [--mode=lut|yuv]
 */

#include <gst/gst.h>
#include <gst/app/gstappsink.h>
#include <opencv2/opencv.hpp>
#include <iostream>
#include <string>
#include <vector>
#include <queue>
#include <mutex>
#include <condition_variable>
#include <atomic>
#include <thread>
#include <chrono>

// OS-level resource tracking
#include <sys/resource.h>

// Assuming these exist in your include path
#include "lut_gen.hpp"
#include "yuv_process.hpp"

using namespace cv;

// -----------------------------------------------------------------------
// RAII Wrappers for Exception-Safe Memory
// -----------------------------------------------------------------------
struct GstSampleDeleter {
    void operator()(GstSample* sample) const { if (sample) gst_sample_unref(sample); }
};
using UniqueGstSample = std::unique_ptr<GstSample, GstSampleDeleter>;

class GstMapGuard {
    GstBuffer* buffer_;
    GstMapInfo map_;
    bool mapped_;
public:
    GstMapGuard(GstBuffer* buffer) : buffer_(buffer), mapped_(false) {
        if (gst_buffer_map(buffer_, &map_, GST_MAP_READ)) {
            mapped_ = true;
        } else {
            throw std::runtime_error("Failed to map GStreamer buffer");
        }
    }
    ~GstMapGuard() { if (mapped_) gst_buffer_unmap(buffer_, &map_); }
    uint8_t* data() { return map_.data; }
    size_t size() const { return map_.size; }
};

// -----------------------------------------------------------------------
// Thread-Safe SPSC Queue
// -----------------------------------------------------------------------
class SampleQueue {
    std::queue<UniqueGstSample> queue_;
    std::mutex mtx_;
    std::condition_variable cv_;
public:
    void push(UniqueGstSample sample) {
        std::lock_guard<std::mutex> lock(mtx_);
        if (queue_.size() >= 2) queue_.pop(); // Drop oldest to prevent latency
        queue_.push(std::move(sample));
        cv_.notify_one();
    }
    UniqueGstSample pop(std::atomic<bool>& running) {
        std::unique_lock<std::mutex> lock(mtx_);
        cv_.wait(lock, [this, &running] { return !queue_.empty() || !running; });
        if (!running && queue_.empty()) return nullptr;
        UniqueGstSample sample = std::move(queue_.front());
        queue_.pop();
        return sample;
    }
};

struct AppContext {
    SampleQueue queue;
    std::atomic<bool> running{true};
    bool show_video = false;
    std::string mode = "lut";
    uint8_t lut_3d[64][64][64];
};

// -----------------------------------------------------------------------
// GStreamer Callback (Producer)
// -----------------------------------------------------------------------
GstFlowReturn on_new_sample(GstAppSink* appsink, gpointer user_data) {
    AppContext* ctx = static_cast<AppContext*>(user_data);
    GstSample* raw_sample = gst_app_sink_pull_sample(appsink);
    if (raw_sample) {
        ctx->queue.push(UniqueGstSample(raw_sample));
    }
    return GST_FLOW_OK;
}

// -----------------------------------------------------------------------
// Main Vision Application
// -----------------------------------------------------------------------
int main(int argc, char *argv[]) {
    gst_init(&argc, &argv);
    AppContext ctx;

    if (argc < 2) {
        g_printerr("Usage: %s <video_device> [--show-video] [--mode=lut|yuv]\n", argv[0]);
        return -1;
    }

    std::string device_path;

    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        if (arg == "--show-video") ctx.show_video = true;
        else if (arg.find("--mode=") == 0) ctx.mode = arg.substr(7);
        else if (arg.find("--") != 0 && device_path.empty()) device_path = arg;
    }

    if (device_path.empty()) {
        g_printerr("[FATAL] No video device specified.\n");
        return -1;
    }

    if (ctx.mode == "lut" || ctx.mode == "hsv") { 
        ctx.mode = "lut"; 
        g_print("Generating 3D Quantized HSV LUT (Green Ball Target)...\n");
        generate_hsv_lut(ctx.lut_3d); 
    }

    std::string pipeline_str = 
        "v4l2src device=" + device_path + " ! "
        "video/x-raw,format=YUY2,width=320,height=240,framerate=30/1 ! "
        "appsink name=sink emit-signals=true max-buffers=1 drop=true sync=false";

    GstElement* pipeline = gst_parse_launch(pipeline_str.c_str(), nullptr);
    if (!pipeline) {
        g_printerr("[FATAL] Pipeline failed to parse.\n");
        return -1;
    }

    GstElement* appsink = gst_bin_get_by_name(GST_BIN(pipeline), "sink");
    g_signal_connect(appsink, "new-sample", G_CALLBACK(on_new_sample), &ctx);
    gst_object_unref(appsink);

    g_print("Initializing Tracking Pipeline...\n");
    if (gst_element_set_state(pipeline, GST_STATE_PLAYING) == GST_STATE_CHANGE_FAILURE) {
        g_printerr("[FATAL] Failed to set pipeline to PLAYING state. Check if %s is busy or available.\n", device_path.c_str());
        gst_object_unref(pipeline);
        return -1;
    }
    g_print("[INFO] Initiating benchmark. This will auto-terminate after 1000 frames...\n");

    Mat cross_kernel = getStructuringElement(MORPH_CROSS, Size(3, 3));

    // Benchmarking variables
    int frame_count = 0;
    double total_compute_ms = 0.0;

    while (ctx.running) {
        UniqueGstSample sample = ctx.queue.pop(ctx.running);
        if (!sample) continue;

        try {
            GstBuffer* buffer = gst_sample_get_buffer(sample.get());
            GstMapGuard map_guard(buffer);

            int width = 320, height = 240;
            Mat yuy2_img(Size(width, height), CV_8UC2, (char*)map_guard.data(), Mat::AUTO_STEP);
            Mat mask(height, width, CV_8UC1);

            double max_area = 0;
            int largest_idx = -1;
            double raw_x = 0.0, raw_y = 0.0;
            std::vector<std::vector<Point>> contours;

            // ==========================================================
            // MICROBENCHMARK START
            // ==========================================================
            auto start_time = std::chrono::high_resolution_clock::now();

            if (ctx.mode == "lut") {
                uint8_t* raw = map_guard.data();
                uint8_t* mptr = mask.data;
                int num_pixels_pairs = (width * height) / 2;
                
                for (int i = 0; i < num_pixels_pairs; ++i) {
                    uint8_t y0 = raw[0];
                    uint8_t u  = raw[1];
                    uint8_t y1 = raw[2];
                    uint8_t v  = raw[3];

                    uint8_t u_idx = u >> 2;
                    uint8_t v_idx = v >> 2;
                    
                    mptr[0] = ctx.lut_3d[y0 >> 2][u_idx][v_idx]; 
                    mptr[1] = ctx.lut_3d[y1 >> 2][u_idx][v_idx]; 

                    mptr += 2;
                    raw += 4;
                }
            } else {
                mask = process_yuv_mask(yuy2_img);
            }

            morphologyEx(mask, mask, MORPH_OPEN, cross_kernel, Point(-1, -1), 2);

            findContours(mask, contours, RETR_EXTERNAL, CHAIN_APPROX_SIMPLE);

            for (size_t i = 0; i < contours.size(); i++) {
                double area = contourArea(contours[i]);
                if (area > max_area) {
                    max_area = area;
                    largest_idx = static_cast<int>(i);
                }
            }

            if (largest_idx != -1 && max_area > 50) {
                Moments m = moments(contours[largest_idx]);
                raw_x = m.m10 / m.m00;
                raw_y = m.m01 / m.m00;
            }

            auto end_time = std::chrono::high_resolution_clock::now();
            // ==========================================================
            // MICROBENCHMARK END
            // ==========================================================

            std::chrono::duration<double, std::milli> compute_duration = end_time - start_time;
            total_compute_ms += compute_duration.count();
            frame_count++;

            if (frame_count % 100 == 0) {
                std::cout << "[BENCHMARK] Processed " << frame_count << "/1000 frames...\n";
            }

            if (frame_count >= 1000) {
                struct rusage usage;
                getrusage(RUSAGE_SELF, &usage);

                std::cout << "\n======================================================\n";
                std::cout << "                LUT ALGORITHM PROFILE                 \n";
                std::cout << "======================================================\n";
                std::cout << "Frames Processed : 1000\n";
                std::cout << "Avg Compute Time : " << (total_compute_ms / 1000.0) << " ms / frame\n";
                std::cout << "Peak RAM Usage   : " << usage.ru_maxrss << " KB\n";
                std::cout << "======================================================\n\n";

                ctx.running = false;
                break;
            }

            // Diagnostic & Rendering (Excluded from timer)
            if (ctx.show_video) {
                Mat bgr_img;
                cvtColor(yuy2_img, bgr_img, COLOR_YUV2BGR_YUY2);
                if (largest_idx != -1 && max_area > 50) {
                    drawContours(bgr_img, contours, largest_idx, Scalar(0, 255, 0), 2);
                    circle(bgr_img, Point(raw_x, raw_y), 5, Scalar(0, 0, 255), -1);
                }
                imshow("Feed", bgr_img);
                imshow("Mask", mask);
                
                char key = (char)waitKey(1);
                if (key == 27) ctx.running = false;
            }

        } catch (const std::exception& e) {
            g_printerr("\n[ERROR] %s\n", e.what());
        }
    }

    gst_element_set_state(pipeline, GST_STATE_NULL);
    gst_object_unref(pipeline);
    
    return 0;
}