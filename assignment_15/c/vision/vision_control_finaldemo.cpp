/**
 * Combined Vision Tracker & Control Loop
 * Usage: ./vision_tracker <video_device> [--show-video] [--mode=lut|yuv]
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
#include <error.h>
#include <fcntl.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/mman.h>
#include <unistd.h>
#include <math.h>
#include <time.h>

#include "soc_system.h"
#include "controllers/PositionControllerPan/pan_submod.h"
#include "controllers/PositionControllerTilt/tilt_submod.h"
#include "lut_gen.hpp"
#include "yuv_process.hpp"

using namespace cv;

#define LOOP_PERIOD_NS 10000000 
#define COUNTS_PITCH 13100
#define COUNTS_YAW 10750

// Shared Atomic Variables for IPC between Vision and Control threads
std::atomic<double> track_x{0.0};
std::atomic<double> track_y{0.0};
std::atomic<bool> is_tracking{false};
std::atomic<bool> program_running{true};

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

class SampleQueue {
    std::queue<UniqueGstSample> queue_;
    std::mutex mtx_;
    std::condition_variable cv_;
public:
    void push(UniqueGstSample sample) {
        std::lock_guard<std::mutex> lock(mtx_);
        if (queue_.size() >= 2) queue_.pop(); 
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
    bool show_video = false;
    std::string mode = "lut";
    int target_u = -1;
    int target_v = -1;
    uint8_t lut_3d[64][64][64];
};

int32_t find_limit(volatile uint32_t* base, int motor_idx, int encoder_idx, int direction, int pwm_duty) {
    base[motor_idx] = (1U << 31) | (direction << 8) | pwm_duty;
    usleep(250000); 

    int32_t last_enc = (int32_t)base[encoder_idx];
    int stall_counter = 0;

    while (stall_counter < 15) { 
        usleep(20000); 
        int32_t current_enc = (int32_t)base[encoder_idx];
        if (abs(current_enc - last_enc) < 5) {
            stall_counter++;
        } else {
            stall_counter = 0; 
        }
        last_enc = current_enc;
    }
    
    base[motor_idx] = (1U << 31) | (0 << 8) | 0;
    usleep(250000); 
    return last_enc;
}

void update_target_lut(AppContext& ctx) {
    g_print("\n[LOCK-ON] Target: U:%d V:%d. Regenerating 3D LUT...\n", ctx.target_u, ctx.target_v);
    int target_u_idx = ctx.target_u >> 2;
    int target_v_idx = ctx.target_v >> 2;

    for (int y = 0; y < 64; y++) {
        for (int u = 0; u < 64; u++) {
            for (int v = 0; v < 64; v++) {
                if (std::abs(u - target_u_idx) <= 4 && std::abs(v - target_v_idx) <= 4) {
                    ctx.lut_3d[y][u][v] = 255;
                } else {
                    ctx.lut_3d[y][u][v] = 0;
                }
            }
        }
    }
}

GstFlowReturn on_new_sample(GstAppSink* appsink, gpointer user_data) {
    AppContext* ctx = static_cast<AppContext*>(user_data);
    GstSample* raw_sample = gst_app_sink_pull_sample(appsink);
    if (raw_sample) {
        ctx->queue.push(UniqueGstSample(raw_sample));
    }
    return GST_FLOW_OK;
}

void hardware_control_loop() {
    int fd = open("/dev/mem", O_RDWR | O_SYNC);
    if (fd < 0) {
        perror("Couldn't open /dev/mem\n");
        return;
    }

    volatile uint32_t* base = (uint32_t*) mmap(
        NULL, HPS_0_ARM_A9_0_ESL_BUS_DEMO_0_SPAN,
        PROT_READ | PROT_WRITE, MAP_SHARED,
        fd, HPS_0_ARM_A9_0_ESL_BUS_DEMO_0_BASE
    );

    if (base == MAP_FAILED) {
        perror("Couldn't map bridge.");
        close(fd);
        return;
    }

    printf("Calibrating Pan (Yaw)...\n");
    int32_t pan_limit = find_limit(base, 2, 0, 0, 25); 
    int32_t pan_center = pan_limit + (COUNTS_YAW / 2);

    printf("Calibrating Tilt (Pitch)...\n");
    int32_t tilt_limit = find_limit(base, 3, 1, 0, 25); 
    int32_t tilt_center = tilt_limit + (COUNTS_PITCH / 2);

    int32_t zero_pan = pan_center; 
    int32_t zero_tilt = tilt_center;

    int32_t target_pan = 0;
    int32_t target_tilt = 0;

    pan_XXDouble pan_u[2], pan_y[2];
    tilt_XXDouble tilt_u[3], tilt_y[1];

    pan_u[0] = 0.0; pan_u[1] = 0.0;
    tilt_u[0] = 0.0; tilt_u[1] = 0.0; tilt_u[2] = 0.0;

    pan_XXInitializeSubmodel(pan_u, pan_y, 0.0);
    tilt_XXInitializeSubmodel(tilt_u, tilt_y, 0.0);

    printf("Controller Initialized. Starting 100Hz Control Loop...\n");

    struct timespec next_step;
    clock_gettime(CLOCK_MONOTONIC, &next_step);

    const double TRACKING_GAIN = 15.0; 

    while (program_running.load()) {
        int32_t current_pan = (int32_t)base[0] - zero_pan;
        int32_t current_tilt = (int32_t)base[1] - zero_tilt;

        if (is_tracking.load()) {
            target_pan -= (int32_t)(track_x.load() * TRACKING_GAIN);
            target_tilt += (int32_t)(track_y.load() * TRACKING_GAIN);
        }

        const int32_t PAN_MAX_LIMIT = COUNTS_YAW / 2;
        const int32_t TILT_MAX_LIMIT = COUNTS_PITCH / 2;

        if (target_pan > PAN_MAX_LIMIT) target_pan = PAN_MAX_LIMIT;
        if (target_pan < -PAN_MAX_LIMIT) target_pan = -PAN_MAX_LIMIT;
        if (target_tilt > TILT_MAX_LIMIT) target_tilt = TILT_MAX_LIMIT;
        if (target_tilt < -TILT_MAX_LIMIT) target_tilt = -TILT_MAX_LIMIT;

        double rad_per_count_pitch = 1.2 * M_PI / COUNTS_PITCH;
        double rad_per_count_yaw = M_PI / COUNTS_YAW;

        pan_u[0] = (double)target_pan * rad_per_count_yaw;
        pan_u[1] = (double)current_pan * rad_per_count_yaw;

        tilt_u[0] = pan_y[0]; 
        tilt_u[1] = (double)target_tilt * rad_per_count_pitch;
        tilt_u[2] = (double)current_tilt * rad_per_count_pitch;

        pan_XXCalculateSubmodel(pan_u, pan_y, pan_xx_time);
        tilt_XXCalculateSubmodel(tilt_u, tilt_y, tilt_xx_time);

        double out_p = 0.8 * pan_y[1]; 
        double out_t = 0.8 * tilt_y[0]; 

        // PWM Reduction applied here (scaling max 1.0 to 80 instead of 255)
        uint8_t duty_p = (uint8_t)(fmin(fabs(out_p), 1.0) * 80.0);
        uint8_t dir_p = (out_p >= 0) ? 1 : 0;

        uint8_t duty_t = (uint8_t)(fmin(fabs(out_t), 1.0) * 80.0);
        uint8_t dir_t = (out_t >= 0) ? 1 : 0;

        base[2] = (1U << 31) | (dir_p << 8) | duty_p;
        base[3] = (1U << 31) | (dir_t << 8) | duty_t;

        next_step.tv_nsec += LOOP_PERIOD_NS;
        if (next_step.tv_nsec >= 1000000000) {
            next_step.tv_nsec -= 1000000000;
            next_step.tv_sec++;
        }
        clock_nanosleep(CLOCK_MONOTONIC, TIMER_ABSTIME, &next_step, NULL);
    }

    base[2] = (1U << 31) | (0 << 8) | 0;
    base[3] = (1U << 31) | (0 << 8) | 0;

    pan_XXTerminateSubmodel(pan_u, pan_y, pan_xx_time);
    tilt_XXTerminateSubmodel(tilt_u, tilt_y, tilt_xx_time);
    munmap((void*) base, HPS_0_ARM_A9_0_ESL_BUS_DEMO_0_SPAN);
    close(fd);
}

int main(int argc, char *argv[]) {
    gst_init(&argc, &argv);
    AppContext ctx;

    if (argc < 2) {
        g_printerr("Usage: %s <video_device> [--show-video] [--mode=lut|yuv]\n", argv[0]);
        return -1;
    }

    std::string device_path = argv[1];

    for (int i = 2; i < argc; ++i) {
        std::string arg = argv[i];
        if (arg == "--show-video") ctx.show_video = true;
        else if (arg.find("--mode=") == 0) ctx.mode = arg.substr(7);
        else if (arg.find("--target-u=") == 0) ctx.target_u = std::stoi(arg.substr(11));
        else if (arg.find("--target-v=") == 0) ctx.target_v = std::stoi(arg.substr(11));
    }

    if (ctx.mode == "lut" || ctx.mode == "hsv") { 
        ctx.mode = "lut"; 
        if (ctx.target_u != -1 && ctx.target_v != -1) {
            update_target_lut(ctx);
        } else {
            g_print("Generating 3D Quantized HSV LUT...\n");
            generate_hsv_lut(ctx.lut_3d); 
        }
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

    gst_element_set_state(pipeline, GST_STATE_PLAYING);

    std::thread control_thread(hardware_control_loop);
    Mat cross_kernel = getStructuringElement(MORPH_CROSS, Size(3, 3));

    while (program_running.load()) {
        UniqueGstSample sample = ctx.queue.pop(program_running);
        if (!sample) continue;

        try {
            GstBuffer* buffer = gst_sample_get_buffer(sample.get());
            GstMapGuard map_guard(buffer);

            int width = 320, height = 240;
            Mat yuy2_img(Size(width, height), CV_8UC2, (char*)map_guard.data(), Mat::AUTO_STEP);
            Mat mask(height, width, CV_8UC1);

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

            std::vector<std::vector<Point>> contours;
            findContours(mask, contours, RETR_EXTERNAL, CHAIN_APPROX_SIMPLE);

            double max_area = 0;
            int largest_idx = -1;
            for (size_t i = 0; i < contours.size(); i++) {
                double area = contourArea(contours[i]);
                if (area > max_area) {
                    max_area = area;
                    largest_idx = static_cast<int>(i);
                }
            }

            int cx = width / 2;
            int cy = height / 2;
            uint8_t* center_ptr = map_guard.data() + (cy * width * 2) + (cx / 2) * 4;
            uint8_t y_val = center_ptr[0], u_val = center_ptr[1], v_val = center_ptr[3];

            if (largest_idx != -1 && max_area > 50) {
                Moments m = moments(contours[largest_idx]);
                double raw_x = m.m10 / m.m00;
                double raw_y = m.m01 / m.m00;
                
                double det_x = ((raw_x / static_cast<double>(width)) * 2.0) - 1.0;
                double det_y = ((raw_y / static_cast<double>(height)) * 2.0) - 1.0;
                
                track_x.store(det_x);
                track_y.store(det_y);
                is_tracking.store(true);

                printf("\r[TRACKING] X=%5.2f, Y=%5.2f (Area:%4.0f Y:%3d U:%3d V:%3d)      ", 
                       det_x, det_y, max_area, y_val, u_val, v_val);
                fflush(stdout);

                if (ctx.show_video) {
                    Mat bgr_img;
                    cvtColor(yuy2_img, bgr_img, COLOR_YUV2BGR_YUY2);
                    drawContours(bgr_img, contours, largest_idx, Scalar(0, 255, 0), 2);
                    circle(bgr_img, Point(raw_x, raw_y), 5, Scalar(0, 0, 255), -1);
                    imshow("Feed", bgr_img);
                    imshow("Mask", mask);
                }
            } else {
                is_tracking.store(false);
                printf("\r[SEARCHING] X= 0.00, Y= 0.00 (Center Y:%3d U:%3d V:%3d)          ", 
                       y_val, u_val, v_val);
                fflush(stdout);

                if (ctx.show_video) {
                    Mat bgr_img;
                    cvtColor(yuy2_img, bgr_img, COLOR_YUV2BGR_YUY2);
                    imshow("Feed", bgr_img);
                    imshow("Mask", mask);
                }
            }

            if (ctx.show_video) {
                char key = (char)waitKey(1);
                if (key == 27) { 
                    program_running.store(false);
                } else if (key == 't' || key == 'T') {
                    ctx.target_u = u_val;
                    ctx.target_v = v_val;
                    update_target_lut(ctx);
                }
            }
        } catch (const std::exception& e) {
            g_printerr("\n[ERROR] %s\n", e.what());
        }
    }

    g_print("\nStopping...\n");
    gst_element_set_state(pipeline, GST_STATE_NULL);
    gst_object_unref(pipeline);
    
    control_thread.join();
    return 0;
}