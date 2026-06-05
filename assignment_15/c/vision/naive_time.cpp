#include <gst/gst.h>
#include <gst/app/gstappsink.h>
#include <opencv2/opencv.hpp>

#include <iostream>
#include <string>
#include <vector>
#include <mutex>
#include <atomic>
#include <thread>
#include <chrono>

// v4l2 headers for device enumeration
#include <fcntl.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <linux/videodev2.h>
#include <dirent.h>
#include <cstring>
#include <algorithm>

// OS-level resource tracking for Memory
#include <sys/resource.h>

// -----------------------------------------------------------------------
// Structs
// -----------------------------------------------------------------------

struct Point3D
{
    double x = 0.0, y = 0.0, z = 0.0;
    bool detected = false;
};

struct SharedFrames
{
    cv::Mat display_frame;
    cv::Mat mask_frame;
    std::mutex mtx;
    std::atomic<bool> new_frame{false};
};

SharedFrames g_shared;

// -----------------------------------------------------------------------
// CLI arguments
// -----------------------------------------------------------------------

struct Args
{
    int camera_index = -1; 
    bool show_windows = false;
};

void print_usage(const char *prog)
{
    std::cout << "\nUsage:\n"
              << "  " << prog << " [camera_index] [--show] [--list]\n\n"
              << "Options:\n"
              << "  camera_index   Index of the camera to use (e.g. 0, 1, 2)\n"
              << "                 If omitted, an interactive menu is shown\n"
              << "  --show         Open OpenCV windows for the camera feed and mask\n"
              << "  --list         List all available cameras and exit\n"
              << "  --help, -h     Show this help message\n\n";
}

Args parse_args(int argc, char *argv[])
{
    Args args;
    for (int i = 1; i < argc; ++i)
    {
        std::string a = argv[i];
        if (a == "--help" || a == "-h")
        {
            print_usage(argv[0]);
            exit(0);
        }
        else if (a == "--show")
        {
            args.show_windows = true;
        }
        else if (a == "--list")
        {
            args.camera_index = -2; 
        }
        else if (std::isdigit(static_cast<unsigned char>(a[0])))
        {
            args.camera_index = std::stoi(a);
        }
        else
        {
            std::cerr << "[WARN] Unknown argument: " << a << "\n";
        }
    }
    return args;
}

// -----------------------------------------------------------------------
// Camera enumeration via V4L2 ioctl
// -----------------------------------------------------------------------

struct CameraInfo
{
    std::string path; 
    std::string name; 
};

std::vector<CameraInfo> list_cameras()
{
    std::vector<CameraInfo> cameras;
    DIR *dir = opendir("/dev");
    if (!dir) return cameras;

    std::vector<std::string> candidates;
    struct dirent *entry;
    while ((entry = readdir(dir)) != nullptr)
    {
        if (std::strncmp(entry->d_name, "video", 5) == 0)
            candidates.push_back(std::string("/dev/") + entry->d_name);
    }
    closedir(dir);

    std::sort(candidates.begin(), candidates.end());

    for (const auto &path : candidates)
    {
        int fd = open(path.c_str(), O_RDONLY | O_NONBLOCK);
        if (fd < 0) continue;
        struct v4l2_capability cap{};
        if (ioctl(fd, VIDIOC_QUERYCAP, &cap) == 0)
            if (cap.device_caps & V4L2_CAP_VIDEO_CAPTURE)
                cameras.push_back({path, reinterpret_cast<char *>(cap.card)});
        close(fd);
    }
    return cameras;
}

void print_cameras(const std::vector<CameraInfo> &cameras)
{
    std::cout << "\n=== Available Cameras ===\n";
    if (cameras.empty())
    {
        std::cout << "  (none found)\n";
        return;
    }
    for (size_t i = 0; i < cameras.size(); ++i)
        std::cout << "  [" << i << "] " << cameras[i].path
                  << "  →  " << cameras[i].name << "\n";
    std::cout << "\n";
}

// -----------------------------------------------------------------------
// Object detection
// -----------------------------------------------------------------------

Point3D object_detection(const cv::Mat &bgr, cv::Mat &mask_out)
{
    cv::Mat hsv_img;
    cv::cvtColor(bgr, hsv_img, cv::COLOR_BGR2HSV);

    cv::Scalar lower_green(36, 50, 70);
    cv::Scalar upper_green(89, 255, 255);
    cv::inRange(hsv_img, lower_green, upper_green, mask_out);

    cv::Mat cross_kernel = cv::getStructuringElement(cv::MORPH_CROSS, cv::Size(3, 3));
    cv::morphologyEx(mask_out, mask_out, cv::MORPH_OPEN, cross_kernel, cv::Point(-1, -1), 5);

    cv::Moments m = cv::moments(mask_out, true);
    Point3D pt;
    if (m.m00 > 10)
    {
        double raw_x = m.m10 / m.m00;
        double raw_y = m.m01 / m.m00;
        pt.x = ((raw_x / static_cast<double>(bgr.cols)) * 2.0) - 1.0;
        pt.y = ((raw_y / static_cast<double>(bgr.rows)) * 2.0) - 1.0;
        pt.z = 1.0;
        pt.detected = true;
    }
    return pt;
}

// -----------------------------------------------------------------------
// GStreamer appsink callback (GStreamer thread)
// -----------------------------------------------------------------------

GstFlowReturn on_new_sample(GstAppSink *appsink, gpointer user_data)
{
    bool show = *reinterpret_cast<bool *>(user_data);

    // Benchmarking state
    static int frame_count = 0;
    static double total_compute_ms = 0.0;

    GstSample *sample = gst_app_sink_pull_sample(appsink);
    if (!sample) return GST_FLOW_ERROR;

    GstBuffer *buffer = gst_sample_get_buffer(sample);
    GstCaps *caps = gst_sample_get_caps(sample);
    GstStructure *s = gst_caps_get_structure(caps, 0);

    int width = 0, height = 0;
    gst_structure_get_int(s, "width", &width);
    gst_structure_get_int(s, "height", &height);

    GstMapInfo map;
    gst_buffer_map(buffer, &map, GST_MAP_READ);
    
    cv::Mat yuy2(height, width, CV_8UC2, map.data);
    cv::Mat bgr, mask;
    Point3D loc;

    // ==========================================================
    // MICROBENCHMARK START
    // Strictly profiling memory conversions and mathematical ops
    // ==========================================================
    auto start_time = std::chrono::high_resolution_clock::now();

    cv::cvtColor(yuy2, bgr, cv::COLOR_YUV2BGR_YUY2);
    loc = object_detection(bgr, mask);

    auto end_time = std::chrono::high_resolution_clock::now();
    // ==========================================================
    // MICROBENCHMARK END
    // ==========================================================

    std::chrono::duration<double, std::milli> compute_duration = end_time - start_time;
    total_compute_ms += compute_duration.count();
    frame_count++;

    // Print progress without polluting the timer
    if (frame_count % 100 == 0) {
        std::cout << "[BENCHMARK] Processed " << frame_count << "/1000 frames...\n";
    }

    if (frame_count >= 1000)
    {
        struct rusage usage;
        getrusage(RUSAGE_SELF, &usage);

        std::cout << "\n======================================================\n";
        std::cout << "               NAIVE OPENCV ALGORITHM PROFILE               \n";
        std::cout << "======================================================\n";
        std::cout << "Frames Processed : 1000\n";
        std::cout << "Avg Compute Time : " << (total_compute_ms / 1000.0) << " ms / frame\n";
        // ru_maxrss is typically in kilobytes on Linux
        std::cout << "Peak RAM Usage   : " << usage.ru_maxrss << " KB\n";
        std::cout << "======================================================\n\n";

        exit(0); // Force termination after completing the benchmark
    }

    if (show)
    {
        cv::Mat annotated = bgr.clone();
        if (loc.detected)
        {
            int px = static_cast<int>((loc.x + 1.0) / 2.0 * width);
            int py = static_cast<int>((loc.y + 1.0) / 2.0 * height);

            cv::circle(annotated, {px, py}, 10, {255, 255, 255}, 2);
            cv::circle(annotated, {px, py}, 3, {0, 255, 0}, -1);
        }

        cv::Mat mask_display;
        cv::cvtColor(mask, mask_display, cv::COLOR_GRAY2BGR);

        {
            std::lock_guard<std::mutex> lock(g_shared.mtx);
            g_shared.display_frame = annotated;
            g_shared.mask_frame = mask_display;
        }
        g_shared.new_frame.store(true);
    }

    gst_buffer_unmap(buffer, &map);
    gst_sample_unref(sample);
    return GST_FLOW_OK;
}

// -----------------------------------------------------------------------
// main
// -----------------------------------------------------------------------

int main(int argc, char *argv[])
{
    gst_init(&argc, &argv);
    Args args = parse_args(argc, argv);
    std::vector<CameraInfo> cameras = list_cameras();

    if (args.camera_index == -2)
    {
        print_cameras(cameras);
        return 0;
    }

    if (cameras.empty())
    {
        std::cerr << "[ERROR] No V4L2 capture devices found.\n";
        return -1;
    }

    int chosen = args.camera_index;
    std::string device_path;
    std::string device_name;

    if (chosen == -1)
    {
        print_cameras(cameras);
        int list_idx = 0;
        if (cameras.size() == 1)
        {
            std::cout << "Only one camera found, selecting it automatically.\n";
            list_idx = 0;
        }
        else
        {
            std::cout << "Select camera [0-" << (cameras.size() - 1) << "]: ";
            std::cin >> list_idx;
            if (list_idx < 0 || static_cast<size_t>(list_idx) >= cameras.size())
            {
                std::cerr << "[ERROR] Invalid selection.\n";
                return -1;
            }
        }
        device_path = cameras[list_idx].path;
        device_name = cameras[list_idx].name;
    }
    else
    {
        std::string target = "/dev/video" + std::to_string(chosen);
        auto it = std::find_if(cameras.begin(), cameras.end(),
                               [&](const CameraInfo &c) { return c.path == target; });
        if (it == cameras.end())
        {
            std::cerr << "[ERROR] /dev/video" << chosen << " not found.\n";
            return -1;
        }
        device_path = it->path;
        device_name = it->name;
    }

    std::cout << "[INFO] Using: " << device_path << "  (" << device_name << ")\n";
    std::cout << "[INFO] Initiating benchmark. This will auto-terminate after 1000 frames...\n";

    GstElement *pipeline = gst_pipeline_new("object-detection-pipeline");
    GstElement *source = gst_element_factory_make("v4l2src", "source");
    GstElement *capsfilter = gst_element_factory_make("capsfilter", "capsfilter");
    GstElement *appsink = gst_element_factory_make("appsink", "appsink");

    g_object_set(source, "device", device_path.c_str(), NULL);
    GstCaps *caps = gst_caps_new_simple("video/x-raw",
                                        "format", G_TYPE_STRING, "YUY2",
                                        "width", G_TYPE_INT, 320,
                                        "height", G_TYPE_INT, 240,
                                        NULL);
    g_object_set(capsfilter, "caps", caps, NULL);
    gst_caps_unref(caps);

    GstAppSink *app = GST_APP_SINK(appsink);
    gst_app_sink_set_emit_signals(app, TRUE);
    gst_app_sink_set_drop(app, TRUE);
    gst_app_sink_set_max_buffers(app, 1);

    GstAppSinkCallbacks callbacks = {};
    callbacks.new_sample = on_new_sample;
    gst_app_sink_set_callbacks(app, &callbacks, reinterpret_cast<gpointer>(&args.show_windows), nullptr);

    gst_bin_add_many(GST_BIN(pipeline), source, capsfilter, appsink, NULL);
    gst_element_link_many(source, capsfilter, appsink, NULL);
    if (gst_element_set_state(pipeline, GST_STATE_PLAYING) == GST_STATE_CHANGE_FAILURE)
    {
        std::cerr << "[ERROR] Failed to start pipeline. Check if " << device_path << " is busy or available.\n";
        gst_object_unref(pipeline);
        return -1;
    }

    const std::string win_feed = "Camera Feed  [" + device_path + "]";
    const std::string win_mask = "Ball Mask";

    if (args.show_windows)
    {
        cv::namedWindow(win_feed, cv::WINDOW_AUTOSIZE);
        cv::namedWindow(win_mask, cv::WINDOW_AUTOSIZE);
    }

    while (true)
    {
        if (args.show_windows)
        {
            if (g_shared.new_frame.load())
            {
                cv::Mat frame_copy, mask_copy;
                {
                    std::lock_guard<std::mutex> lock(g_shared.mtx);
                    frame_copy = g_shared.display_frame.clone();
                    mask_copy = g_shared.mask_frame.clone();
                }
                g_shared.new_frame.store(false);
                cv::imshow(win_feed, frame_copy);
                cv::imshow(win_mask, mask_copy);
            }
            if (cv::waitKey(10) == 27) break;
        }
        else
        {
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
        }
    }

    if (args.show_windows) cv::destroyAllWindows();
    gst_element_set_state(pipeline, GST_STATE_NULL);
    gst_object_unref(pipeline);
    return 0;
}