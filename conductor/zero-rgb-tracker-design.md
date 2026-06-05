# Zero-RGB Vision Tracker Architecture

This specification outlines the design for a highly optimized, "Zero-RGB" vision tracking system for the DE10-Nano, avoiding BGR memory bottlenecks.

## 1. Data Reception (GStreamer)
- **Pipeline:** `v4l2src ! capsfilter ! appsink`
- **Caps:** `video/x-raw, format=YUY2, width=320, height=240, framerate=10/1`
- **Rationale:** Capturing directly in YUYV (16bpp) avoids the memory and CPU overhead of `videoconvert` transforming the stream to BGR (24bpp). 320x240 @ 10fps minimizes bandwidth and CPU load for embedded performance.

## 2. Processing Pipeline: Hybrid Approach

The application will accept a command-line flag (`--mode=yuv` or `--mode=hsv`) to switch logic at runtime. **Neither path allocates a BGR or RGB buffer.**

### Mode A: Direct Plane Masking (YUV)
- **Concept:** Process the YUV data directly.
- **Workflow:**
  1. Extract Y, U, and V channels from the 16-bit packed YUYV buffer into independent 8-bit planes.
  2. Apply `cv::inRange` to the U (Cb) and V (Cr) planes to find the "green" quadrant (low U, low V).
  3. Combine the masks via bitwise AND.
- **Performance:** Extremely high (minimal math, cache-friendly).

### Mode B: 2D Lookup Table (HSV-Equivalent)
- **Concept:** Convert the complex HSV mathematical "wedge" into an O(1) memory lookup.
- **Workflow:**
  1. **Initialization:** At startup, generate a 256x256 byte array (`LUT[U][V]`). Iterate through all possible U and V values, calculate their mathematical Hue and Saturation, and if they fall within the "Green" HSV thresholds, set the LUT value to `255`, else `0`.
  2. **Processing:** For every pixel in the YUYV buffer, read its U and V values, and look up the result: `mask_pixel = LUT[U][V]`.
- **Performance:** Very high. Bypasses trigonometric functions per-pixel. Provides the tuning ease of HSV without the computational cost.

## 3. Post-Processing & Output
- **Denoise:** `cv::morphologyEx(MORPH_OPEN)` to remove false positives.
- **Centroid:** `cv::moments` to find the center of the largest contour.
- **Output:** Normalized coordinates `[-1.0, 1.0]` printed to `stdout` for the control logic.

## 4. Verification
- Validate that the GStreamer pipeline never reports an internal data stream error due to missing `videoconvert`.
- Confirm CPU usage drops significantly compared to the `cvtColor` approach.
- Verify tracking accuracy in both `yuv` and `hsv` modes.