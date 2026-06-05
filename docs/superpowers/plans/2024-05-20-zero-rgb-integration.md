# Zero-RGB Main Application Integration Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Refactor `vision_tracker.cpp` to use LUT and YUV plane masking for "Zero-RGB" tracking, optimizing for embedded performance.

**Architecture:** Initialize a 256x256 HSV LUT at startup. Capture 320x240 @ 10fps YUYV video. Implement two tracking modes: `hsv` (manual LUT lookup per pixel) and `yuv` (OpenCV plane split thresholding). Remove RGB/BGR logic from the tracking path.

**Tech Stack:** C++, GStreamer, OpenCV (display/moments only), V4L2.

---

### Task 1: Refactor `vision_tracker.cpp`

**Files:**
- Modify: `assignment_15/vision_tracker.cpp`

- [ ] **Step 1: Update headers and add CLI flags**

```cpp
#include "lut_gen.hpp"
#include "yuv_process.hpp"

// ... existing includes ...

// In main():
// Parse --mode=[yuv|hsv]
// Initialize uint8_t hsv_lut[256][256] using generate_hsv_lut(hsv_lut)
```

- [ ] **Step 2: Update GStreamer caps to 320x240 @ 10fps**

```cpp
caps = gst_caps_new_simple("video/x-raw",
    "format", G_TYPE_STRING, "YUY2",
    "width", G_TYPE_INT, 320,
    "height", G_TYPE_INT, 240,
    "framerate", GST_TYPE_FRACTION, 10, 1,
    NULL);
```

- [ ] **Step 3: Implement Zero-RGB Processing Loop**

- Replace `cvtColor` logic with:
    - If `hsv` mode: Manually iterate over YUYV buffer. Every 4 bytes is `[Y0, U0, Y1, V0]`.
    - For `Y0`: use `hsv_lut[U0][V0]`.
    - For `Y1`: use `hsv_lut[U0][V0]`.
    - Fill a `Mat mask(240, 320, CV_8UC1)`.
    - If `yuv` mode: `mask = process_yuv_mask(yuy2_img)`.

- [ ] **Step 4: Cleanup Display Logic**

- Only use `cvtColor(yuy2_img, bgr_img, COLOR_YUV2BGR_YUY2)` inside `if (show_video)`.

### Task 2: Update `Makefile` and Compile

**Files:**
- Modify: `assignment_15/Makefile`

- [ ] **Step 1: Add `vision_tracker` target to Makefile**

```makefile
vision_tracker: vision_tracker.cpp lut_gen.hpp yuv_process.hpp
	g++ -O3 vision_tracker.cpp -o vision_tracker `pkg-config --cflags --libs gstreamer-1.0 gstreamer-app-1.0 opencv4`
```

- [ ] **Step 2: Compile and Verify**

Run: `make vision_tracker`
Expected: Success.

### Task 3: Verification

- [ ] **Step 1: Check binary help output**

Run: `./vision_tracker --help` (or just check the usage message if implemented)

- [ ] **Step 2: Commit changes**

```bash
git add assignment_15/vision_tracker.cpp assignment_15/Makefile
git commit -m "feat(vision): implement Zero-RGB tracking with HSV LUT and YUV masking"
```
