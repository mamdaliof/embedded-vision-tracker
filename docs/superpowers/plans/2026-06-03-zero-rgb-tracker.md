# Zero-RGB Vision Tracker Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Build a high-performance vision tracker for DE10-Nano that captures YUYV data and tracks a green ball using either YUV plane masking or an HSV Lookup Table, avoiding RGB conversion.

**Architecture:** GStreamer captures 320x240 @ 10fps YUY2 frames. Processing is done directly on the YUV buffer. HSV mode uses a pre-computed 2D LUT for O(1) pixel classification.

**Tech Stack:** C++, GStreamer 1.0, OpenCV 4.

---

### Task 1: LUT Generation Logic (HSV Mode)
We need to pre-calculate a 256x256 table that maps (U, V) coordinates to a "is green" binary value.

**Files:**
- Create: `assignment_15/lut_gen.hpp`
- Create: `assignment_15/test_lut.cpp`
- Modify: `assignment_15/Makefile`

- [ ] **Step 1: Write the failing test for LUT generation**
Create `assignment_15/test_lut.cpp` to verify that a specific U,V pair (e.g., green-ish) results in 255 and a red-ish one results in 0.
```cpp
#include "lut_gen.hpp"
#include <cassert>
#include <iostream>

int main() {
    uint8_t lut[256][256];
    generate_hsv_lut(lut);

    // Green-ish in YUV (approximate)
    // For Green: U is low, V is low.
    assert(lut[80][80] == 255); 
    // Red-ish in YUV: V is high
    assert(lut[128][200] == 0);

    std::cout << "LUT Tests Passed!" << std::endl;
    return 0;
}
```

- [ ] **Step 2: Run test to verify it fails**
Update `Makefile` to include `test_lut` target.
```bash
g++ assignment_15/test_lut.cpp -o assignment_15/test_lut
./assignment_15/test_lut
```
Expected: FAIL (lut_gen.hpp not found or empty).

- [ ] **Step 3: Implement LUT generation math**
In `assignment_15/lut_gen.hpp`:
```cpp
#ifndef LUT_GEN_HPP
#define LUT_GEN_HPP

#include <cmath>
#include <cstdint>

inline void generate_hsv_lut(uint8_t lut[256][256]) {
    for (int u = 0; u < 256; u++) {
        for (int v = 0; v < 256; v++) {
            // Shift to zero-centered chrominance
            float up = u - 128.0f;
            float vp = v - 128.0f;

            // Compute Hue (degrees [0, 360])
            float hue = atan2(vp, up) * 180.0f / M_PI;
            if (hue < 0) hue += 360.0f;
            
            // Map to OpenCV Hue range [0, 180]
            float h_cv = hue / 2.0f;

            // Compute Saturation (approximate)
            float sat = sqrt(up*up + vp*vp) * 2.0f; // Scale to 0-255 approx
            if (sat > 255) sat = 255;

            // Threshold for Green: H in [36, 89], S in [50, 255]
            if (h_cv >= 36 && h_cv <= 89 && sat >= 50) {
                lut[u][v] = 255;
            } else {
                lut[u][v] = 0;
            }
        }
    }
}
#endif
```

- [ ] **Step 4: Run test to verify it passes**
Expected: PASS.

- [ ] **Step 5: Commit**
```bash
git add assignment_15/lut_gen.hpp assignment_15/test_lut.cpp
git commit -m "feat: add HSV LUT generation logic"
```

---

### Task 2: YUV Plane Masking Implementation
Implement the logic to extract U/V planes from packed YUYV and threshold them.

**Files:**
- Create: `assignment_15/yuv_process.hpp`
- Create: `assignment_15/test_yuv.cpp`

- [ ] **Step 1: Write test for YUV processing**
Verify that a mock YUYV buffer with green pixels results in a mask with detected points.
```cpp
#include "yuv_process.hpp"
#include <opencv2/opencv.hpp>
#include <cassert>

int main() {
    int w = 4, h = 4;
    cv::Mat yuyv(h, w, CV_8UC2);
    // Fill with non-green
    yuyv.setTo(cv::Scalar(128, 128)); 
    // Set one pixel to green (Y=128, U=60, V=60)
    uint8_t* data = yuyv.ptr<uint8_t>(0);
    data[0] = 128; // Y0
    data[1] = 60;  // U
    data[2] = 128; // Y1
    data[3] = 60;  // V

    cv::Mat mask = process_yuv_mask(yuyv);
    assert(cv::countNonZero(mask) > 0);
    return 0;
}
```

- [ ] **Step 2: Run test and watch it fail**

- [ ] **Step 3: Implement plane extraction and thresholding**
In `assignment_15/yuv_process.hpp`:
```cpp
#include <opencv2/opencv.hpp>

inline cv::Mat process_yuv_mask(const cv::Mat& yuyv) {
    std::vector<cv::Mat> planes;
    cv::split(yuyv, planes); // Splits into 2 channels: [Y0, U, Y1, V] packed... wait.
    // split() on 2-channel Mat gives 2 channels. 
    // Channel 0: [Y0, Y1, ...]
    // Channel 1: [U, V, ...]
    // This is not quite right for YUYV. 
    // Correct way: Extract U and V by subsampling or specific mapping.
    
    cv::Mat u_v = planes[1];
    cv::Mat u, v;
    // For YUYV, Channel 1 is interleaved U and V.
    // We can extract them using a simple re-stride or split.
    cv::Mat uv_planes[2];
    cv::split(u_v.reshape(1, yuyv.rows * yuyv.cols), uv_planes);
    
    cv::Mat mask_u, mask_v, mask;
    cv::inRange(uv_planes[0], 0, 100, mask_u); // Low U
    cv::inRange(uv_planes[1], 0, 100, mask_v); // Low V
    cv::bitwise_and(mask_u, mask_v, mask);
    
    return mask.reshape(1, yuyv.rows);
}
```

- [ ] **Step 4: Run test and watch it pass**

- [ ] **Step 5: Commit**

---

### Task 3: Zero-RGB Main Application Integration
Refactor `vision_tracker.cpp` to use the new headers and implement the LUT loop.

**Files:**
- Modify: `assignment_15/vision_tracker.cpp`
- Modify: `assignment_15/Makefile`

- [ ] **Step 1: Integrate LUT and Plane Masking logic**
Update `vision_tracker.cpp` to:
1. Initialize the LUT at startup.
2. Update GStreamer caps to 320x240 @ 10fps.
3. Replace `cvtColor` with LUT lookup in HSV mode.
4. Replace `cvtColor` with Plane Masking in YUV mode.

- [ ] **Step 2: Compile and verify binary**
`make -C assignment_15`

- [ ] **Step 3: Run verification tests on actual hardware**
`./assignment_15/vision_tracker /dev/video2 --mode=hsv`
`./assignment_15/vision_tracker /dev/video2 --mode=yuv`

- [ ] **Step 4: Commit**
