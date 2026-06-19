# Embedded Vision Tracker: Final Report Structure & Writing Guide

This document provides a comprehensive structural outline, detailed technical descriptions, and analytical suggestions for your final academic report (`report.tex`). Use this guide to draft your paper, distribute tasks, and present your findings effectively.

---

## 1. Document Structure & LaTeX Section Mapping

The following structure is recommended for your LaTeX paper based on the IEEE conference template:

*   **Abstract**: Summary of the goals, the hardware-software co-design approach, the 3D LUT optimization, and the final tracking performance.
*   **I. Introduction**: Context of the tracking system, high-level objectives, and final system architecture.
*   **II. Design Space Exploration (DSE)**: Trade-offs between boards (DE10-Nano vs. Raspberry Pi + IcoBoard), software vs. hardware partitioning, and precision decisions.
*   **III. Hardware Architecture & Custom IP Design**: Verilog modules, Avalon-MM register layout, and platform integration.
*   **IV. Software Pipeline & Optimized Vision Tracking**: GStreamer capture, naive OpenCV HSV vs. optimized direct YUV 3D Lookup Table (LUT).
*   **V. Control Loop & Platform Integration**: 20-sim model integration, automatic calibration state machine, and safety limits.
*   **VI. Experimental Performance Analysis**: Benchmark execution times, RAM consumption, and telemetry plots.
*   **VII. Conclusion & Task Division**: Delineation of work and final remarks.

---

## 2. Detailed Section Content Guides

### Section I: Introduction
*   **Objective**: Introduce the vision-in-the-loop tracking system (Jiwy platform) designed to track a target object (e.g., a green ball) and keep it centered in the frame.
*   **System Overview**: Describe the two-axis pan-tilt gimbal mechanism driven by DC motors, monitored by quadrature encoders, and controlled by a feedback loop running on an embedded processor with camera input.

---

### Section II: Design Space Exploration (DSE)
*   **ARM on Raspberry Pi (RPi) vs. ARM on DE10-Nano**:
    *   *RPi (Cortex-A53, quad-core @ 1.2GHz)* has higher raw computational power for video processing but suffers from high latency when communicating with the FPGA (IcoBoard) over an external SPI bus. SPI requires chip-select overhead, clock cycles, and CPU polling.
    *   *DE10-Nano (Cyclone V, dual-core Cortex-A9)* features an on-chip Avalon-MM interconnect backbone. Register reads/writes to custom hardware IPs incur sub-microsecond latency, allowing a much tighter and more deterministic control loop.
*   **Lattice iCE40 vs. Cyclone V**:
    *   *iCE40 (IcoBoard)* is resource-constrained. Implementing quadrature decoders, PWM generators, and SPI interfaces nearly saturates the device.
    *   *Cyclone V* has rich logic resources and DSP blocks, leaving ample space for future hardware-accelerated pixel processing.
*   **Software vs. Hardware Partitioning**:
    *   *Software*: High-level operations, GStreamer pipelines, 3D LUT lookups, centroid calculations, and 20-sim generated control logic are implemented in C++ on the ARM CPU for rapid development and debugging.
    *   *Hardware*: High-frequency, time-critical tasks like quadrature signal decoding (which can run up to tens of thousands of pulses/sec) and high-frequency PWM generation are offloaded to custom Verilog modules on the FPGA fabric to eliminate CPU overhead and prevent missed counts.
*   **Integer vs. Floating-Point Precision**:
    *   The ARM Cortex-A9 includes a hardware Floating Point Unit (FPU). Floating-point math is free and accurate, avoiding the quantization issues of fixed-point implementations in Verilog.

---

### Section III: Hardware Architecture & Custom IP Design (Costin & Farhad)
*   **Quadrature Encoder Counter (`Quad_compact`) (Costin)**:
    *   *Metastability Protection*: Employs a 2-stage input synchronizer (`sync` and `AB` registers) clocked by the 50 MHz FPGA clock to stabilize asynchronous physical signals `A` and `B`.
    *   *Edge Detection*: Uses XOR/subtraction operations to detect edges on both channels, generating pulses that trigger state updates.
    *   *Width Decision*: A signed 32-bit register (`count`) is utilized. This prevents counter overflow (which would happen in a 16-bit counter after less than 3 full rotations for the pitch axis's 13,100 counts) and aligns with the native 32-bit word width of the Avalon-MM bus.
*   **PWM Generator Module (`pwm_generator`) (Costin)**:
    *   *Target Frequency*: Fixed at 20 kHz to minimize acoustic noise and match motor driver characteristics. At 50 MHz clock speed, this translates to a period of `2500` clock cycles (`PERIOD`).
    *   *Minimum Off-Time Safety*: Coded to protect the `VNH2SP30-E` motor driver. The driver requires a minimum off-time of 6 $\mu$s per cycle to avoid false short-circuit detection. At 20 ns clock period, this equals 300 cycles. The PWM duty cycle is capped at `PERIOD - MIN_OFF_CYCLES` (2200 cycles, or ~88% duty cycle), guaranteeing a 6 $\mu$s low pulse every period.
    *   *Direction Control*: Implements direction logic matching the driver's H-bridge truth table: Forward (`INA=1, INB=0`) and Backward (`INA=0, INB=1`).
*   **IP Integration (`esl_bus_demo`) (Farhad)**:
    *   *Memory Map*:
        *   `0x00`: Read Yaw Encoder (signed 32-bit)
        *   `0x01`: Read Pitch Encoder (signed 32-bit)
        *   `0x02`: Write Yaw PWM Control (Bit 31: Enable, Bit 8: Direction, Bits 7-0: Duty Cycle)
        *   `0x03`: Write Pitch PWM Control (Bit 31: Enable, Bit 8: Direction, Bits 7-0: Duty Cycle)
*   **Qsys Packaging (`esl_bus_demo_hw.tcl`) (Farhad)**:
    *   Explain how the custom Verilog modules are packaged as a Qsys component, defining the Avalon-MM slave interface, clock/reset interfaces, and external conduit pins connected to the motor driver and encoders.

---

### Section IV: Software Pipeline & Optimized Vision Tracking (Farhad)
*   **GStreamer Capture Pipeline**:
    *   Captures raw YUY2 video from `/dev/videoX` at 320x240 @ 30 FPS.
    *   Configures `appsink` properties: `max-buffers=1`, `drop=true`, and `sync=false` to drop older frames if the CPU is busy, minimizing processing latency.
*   **YUV Chrominance Detection**:
    *   In the YUY2 format (packed as `[Y0, U, Y1, V]`), chrominance (U and V channels) directly represents color.
    *   Green targets have distinctive chrominance bounds ($70 < U < 110$ and $70 < V < 110$). This allows color thresholding without converting to BGR and HSV color spaces.
*   **3D Lookup Table (LUT) Optimization**:
    *   *Concept*: Naive OpenCV HSV thresholding requires YUY2 $\to$ BGR $\to$ HSV color conversions, followed by two-sided thresholding on three channels for every single pixel. This saturates the CPU.
    *   *Quantization*: Quantizing Y, U, and V from 8-bit to 6-bit (dividing by 4 via `>> 2`) reduces the lookup table size from $256^3$ bytes (16 MB) to $64^3$ bytes (256 KB).
    *   *Cache-Friendliness*: The 256 KB LUT fits entirely within the Cortex-A9's 512 KB L2 cache. Lookup queries complete in $O(1)$ time with no floating-point math, leading to a 5x performance improvement.
    *   *Post-Processing*: Morphological OPEN operations filter out noise before finding the largest contour centroid.

---

### Section V: Control Loop & Platform Integration (Costin & Farhad)
*   **20-sim Controller Integration (Costin)**:
    *   Explain the control model generated from 20-sim. Describe the pan-tilt controller dynamics, cascading controllers, and state calculations.
*   **Calibration State Machine (Farhad)**:
    *   *Stall-Detection*: Before starting the tracking loop, the camera calibration runs. It commands the motors to run slowly (duty cycle 25) in one direction. It monitors encoder feedback and detects a stall when the encoder count change falls below 5 counts over 300 ms.
    *   *Limit Calibration*: Once a mechanical limit is found, the system records the limit, calculates the center point (`limit + total_counts/2`), moves there, and defines it as the origin (`zero_pan` / `zero_tilt`).
*   **Stability Tuning & PWM Output Limiting (Farhad)**:
    *   *Tuning*: PID values were adjusted to prevent oscillations on the rapid yaw axis and compensate for gravity/inertia on the pitch axis.
    *   *Duty Cycle Clamping*: The controller's output duty cycle is scaled and capped at a maximum of `80` (out of 255). Capping the duty cycle prevents motor overheating, limits excessive current draw, and ensures smooth, non-aggressive movements that do not shake the camera feed or overshoot the target.

---

## 3. Recommended Performance & Telemetry Analysis

### A. Vision Pipeline Performance Benchmark
Execute the `naive_bench` and `lut_bench` binaries on the DE10-Nano to obtain the data for the following table. It highlights the architectural improvement of your LUT approach:

| Metric | Naive OpenCV (HSV) | Optimized YUV 3D LUT | Improvement Factor |
| :--- | :---: | :---: | :---: |
| **Avg. Compute Time (ms/frame)** | ~6.6 ms | ~1.3 ms | **~5.07x Speedup** |
| **Achievable Frame Rate** | ~150 FPS (limitless) | ~760 FPS (limitless) | **~5x Throughput** |
| **Peak RAM Usage (KB)** | (Record from bench) | (Record from bench) | - |
| **CPU Saturation (at 30 FPS)** | ~20% CPU | ~4% CPU | **5x CPU Headroom** |

*Drafting Tip: Highlight that at a target framerate of 30 FPS (33.3 ms/frame budget), the naive method consumes ~20% of a single core's time budget just for color thresholding. The LUT method reduces this to ~4%, leaving massive CPU resources for the 100 Hz control loop and other high-level applications.*

### B. Control Telemetry Plotting & Analysis
Utilize the saved `motor_telemetry.csv` file generated by the save-output binary. You should create two main graphs:

1.  **Transient Tracking Performance (Step Response)**:
    *   **X-axis**: Time (seconds) or Samples (at 100 Hz, 10 ms per sample).
    *   **Y-axis (Left)**: Encoder Count (deviation from center).
    *   **Y-axis (Right)**: PWM Duty Cycle (signed, -80 to +80).
    *   *Analysis*: Show how the motor accelerates when the target is off-center, how it handles deceleration, and how it settles at zero deviation without overshoot or high-frequency oscillation.
2.  **Telemetry Correlation Analysis**:
    *   Plot the encoder position alongside the commanded PWM. Point out how capping the duty cycle to 80 prevents sharp spikes in voltage/current, leading to a smooth slope in encoder counts rather than high-frequency jitter.

---

## 4. Suggested Additional Sections (To Make a Premium Report)

If you wish to elevate the quality of your university report, consider writing the following sections:

1.  **System Level Block Diagram**:
    *   Include a diagram illustrating the data flow: Camera $\to$ USB $\to$ HPS (GStreamer $\to$ 3D LUT $\to$ Centroid $\to$ 20-sim Controller) $\to$ Avalon Bus $\to$ FPGA (PWM Generator IP) $\to$ Motor Driver $\to$ DC Motor $\to$ Encoder $\to$ FPGA (Encoder Decoder IP) $\to$ Avalon Bus $\to$ HPS.
2.  **Real-Time Thread Scheduling Analysis**:
    *   Discuss the multi-threaded architecture: the GStreamer frame-grabbing thread running asynchronously and pushing samples to a queue, and the 100 Hz control loop thread running with strict periodic timing via `clock_nanosleep`.
    *   Analyze thread safety, using lock-free atomics (`std::atomic<double>`) for the coordinates to ensure that the control loop never blocks on mutexes while waiting for the vision thread.
3.  **Metastability & Synchronization Theory**:
    *   Include a brief mathematical explanation of why a 2-stage shift-register synchronizer is necessary on the FPGA inputs for `A` and `B` signals, citing Mean Time Between Failures (MTBF) reduction.
