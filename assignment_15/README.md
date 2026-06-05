# Assignment 15: Vision-in-the-loop & Performance Optimization

This assignment focuses on optimized vision tracking for the DE10-Nano, specifically comparing standard OpenCV processing against a 3D Lookup Table (LUT) approach.

## 🚀 Build Instructions

A `Makefile` is provided in the `c/vision` directory to build all vision-related tools.

```bash
cd assignment_15/c/vision
make clean && make
```

### Generated Binaries:
- `vision`: Standard tracker using OpenCV HSV thresholding.
- `yuv_tracker`: Optimized tracker using 3D LUT or direct YUV processing.
- `lut_bench`: Benchmark tool for the LUT-based approach.
- `naive_bench`: Benchmark tool for the standard OpenCV approach.

## 🛠️ Usage

### Running the Trackers
Both trackers support interactive camera selection if no index is provided.

```bash
# Run standard tracker with video windows
./vision --show

# Run optimized YUV tracker on a specific device
./yuv_tracker /dev/video0

# Run YUV tracker in naive mode (no LUT)
./yuv_tracker /dev/video0 --mode=yuv
```

### Running Benchmarks
Benchmarks automatically terminate after processing 1000 frames and output a performance profile.

```bash
./lut_bench /dev/video0
./naive_bench /dev/video0
```

## 📝 CLI Options
- `[camera_index]` or `/dev/videoX`: Specify the camera device.
- `--show`: Open OpenCV windows for camera feed and mask (not recommended for headless benchmarking).
- `--list`: List all available V4L2 capture devices.
- `--mode=lut|yuv`: (yuv_tracker only) Switch between 3D LUT and direct YUV processing.

## ⚠️ Notes
- **Targeting**: The system is pre-configured to track a **green ball**.
- **Resolution**: Hardcoded to `320x240` at `30fps` for optimal embedded performance.
- **Dependencies**: Requires `gstreamer-1.0`, `gstreamer-app-1.0`, and `opencv4`.
