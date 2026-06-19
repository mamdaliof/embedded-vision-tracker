# Embedded Vision Tracker: Final Report Structure & Writing Guide

This document provides a comprehensive structural outline, detailed technical descriptions, and analytical suggestions for your final academic report (`report.tex`). Each section indicates the responsible owner (Farhad or Costin) and highlights new sections/concepts with a **[NEW]** tag (without assigning owners to them).

---

## 1. Document Structure & LaTeX Section Mapping

*   **Abstract** — **Farhad & Costin**
*   **I. Introduction** — **Farhad & Costin**
*   **II. Design Space Exploration (DSE)** — **Costin**
*   **III. Hardware Architecture & Custom IP Design**:
    *   Quadrature Encoder Counter (`Quad_compact`) — **Costin**
    *   PWM Generator (`pwm_generator`) — **Costin**
    *   IP Connection / Integration (`esl_bus_demo`) — **Costin (bus logic) & Farhad (parameters)**
    *   Qsys Packaging (`esl_bus_demo_hw.tcl`) — **Farhad**
*   **IV. Software Pipeline & Optimized Vision Tracking**:
    *   GStreamer Capture Pipeline — **Farhad**
    *   YUV Chrominance Detection — **Farhad**
    *   3D Lookup Table (LUT) Optimization — **Farhad**
*   **V. Control Loop & Platform Integration**:
    *   20-sim Controller Dynamics — **Costin**
    *   **[NEW]** Automatic Calibration State Machine
    *   Controller Integration & PWM Output Limiting — **Farhad**
*   **VI. Experimental Performance Analysis**:
    *   Vision Pipeline Performance Benchmark (LUT vs Naive HSV) — **Farhad**
    *   Control Telemetry Plotting & Analysis (Encoders & PWM) — **Costin**
*   **[NEW] VII. Suggested Premium Sections**:
    *   **[NEW]** System-Level Hardware-Software Block Diagram
    *   **[NEW]** Real-Time Thread Scheduling & Latency Analysis
    *   **[NEW]** Metastability & Synchronization Theory
*   **VIII. Conclusion & Task Division Summary** — **Farhad & Costin**

---

## 2. Detailed Section Content Guides & Assignment of Responsibilities

### Section I: Introduction
*   **Owner**: **Farhad & Costin**
*   **Objective**: Introduce the vision-in-the-loop tracking system (Jiwy platform) designed to track a target object (e.g., a green ball) and keep it centered in the frame.
*   **System Overview**: Describe the two-axis pan-tilt gimbal mechanism driven by DC motors, monitored by quadrature encoders, and controlled by a feedback loop running on an embedded processor with camera input.

---

### Section II: Design Space Exploration (DSE)
*   **Owner**: **Costin** (DSE Lead)
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

### Section III: Hardware Architecture & Custom IP Design
*   **Quadrature Encoder Counter (`Quad_compact`)**:
    *   **Owner**: **Costin**
    *   *Metastability Protection*: Employs a 2-stage input synchronizer (`sync` and `AB` registers) clocked by the 50 MHz FPGA clock to stabilize asynchronous physical signals `A` and `B`.
    *   *Edge Detection*: Uses XOR/subtraction operations to detect edges on both channels, generating pulses that trigger state updates.
    *   *Width Decision*: A signed 32-bit register (`count`) is utilized. This prevents counter overflow (which would happen in a 16-bit counter after less than 3 full revolutions for the pitch axis's 13,100 counts) and aligns with the native 32-bit word width of the Avalon-MM bus.
*   **PWM Generator Module (`pwm_generator`)**:
    *   **Owner**: **Costin**
    *   *Target Frequency*: Fixed at 20 kHz to minimize acoustic noise and match motor driver characteristics. At 50 MHz clock speed, this translates to a period of `2500` clock cycles (`PERIOD`).
    *   *Minimum Off-Time Safety*: Coded to protect the `VNH2SP30-E` motor driver. The driver requires a minimum off-time of 6 $\mu$s per cycle to avoid false short-circuit detection. At 20 ns clock period, this equals 300 cycles. The PWM duty cycle is capped at `PERIOD - MIN_OFF_CYCLES` (2200 cycles, or ~88% duty cycle), guaranteeing a 6 $\mu$s low pulse every period.
    *   *Direction Control*: Implements direction logic matching the driver's H-bridge truth table: Forward (`INA=1, INB=0`) and Backward (`INA=0, INB=1`).
*   **IP Connection / Integration (`esl_bus_demo`)**:
    *   **Owner**: **Costin (bus logic) & Farhad (parameters)**
    *   *Memory Map*:
        *   `0x00`: Read Yaw Encoder (signed 32-bit)
        *   `0x01`: Read Pitch Encoder (signed 32-bit)
        *   `0x02`: Write Yaw PWM Control (Bit 31: Enable, Bit 8: Direction, Bits 7-0: Duty Cycle)
        *   `0x03`: Write Pitch PWM Control (Bit 31: Enable, Bit 8: Direction, Bits 7-0: Duty Cycle)
*   **Qsys Packaging (`esl_bus_demo_hw.tcl`)**:
    *   **Owner**: **Farhad**
    *   Explain how the custom Verilog modules are packaged as a Qsys component, defining the Avalon-MM slave interface, clock/reset interfaces, and external conduit pins connected to the motor driver and encoders.

---

### Section IV: Software Pipeline & Optimized Vision Tracking
*   **Owner**: **Farhad** (Vision Lead)
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

### Section V: Control Loop & Platform Integration
*   **20-sim Controller Dynamics**:
    *   **Owner**: **Costin**
    *   Explain the control model generated from 20-sim. Describe the pan-tilt controller dynamics, cascading controllers, and state calculations.
*   **[NEW] Automatic Calibration State Machine**:
    *   *Stall-Detection*: Before starting the tracking loop, the camera calibration runs. It commands the motors to run slowly (duty cycle 25) in one direction. It monitors encoder feedback and detects a stall when the encoder count change falls below 5 counts over 300 ms.
    *   *Limit Calibration*: Once a mechanical limit is found, the system records the limit, calculates the center point (`limit + total_counts/2`), moves there, and defines it as the origin (`zero_pan` / `zero_tilt`).
*   **Controller Integration & PWM Output Limiting**:
    *   **Owner**: **Farhad**
    *   *Tuning*: PID values were adjusted to prevent oscillations on the rapid yaw axis and compensate for gravity/inertia on the pitch axis.
    *   *Duty Cycle Clamping*: The controller's output duty cycle is scaled and capped at a maximum of `80` (out of 255). Capping the duty cycle prevents motor overheating, limits excessive current draw, and ensures smooth, non-aggressive movements that do not shake the camera feed or overshoot the target.

---

## 3. Recommended Performance & Telemetry Analysis

### A. Vision Pipeline Performance Benchmark
*   **Owner**: **Farhad**
*   Execute the `naive_bench` and `lut_bench` binaries on the DE10-Nano to obtain the data for the following table. It highlights the architectural improvement of your LUT approach:

| Metric | Naive OpenCV (HSV) | Optimized YUV 3D LUT | Improvement Factor |
| :--- | :---: | :---: | :---: |
| **Avg. Compute Time (ms/frame)** | ~6.6 ms | ~1.3 ms | **~5.07x Speedup** |
| **Achievable Frame Rate** | ~150 FPS (limitless) | ~760 FPS (limitless) | **~5x Throughput** |
| **Peak RAM Usage (KB)** | (Record from bench) | (Record from bench) | - |
| **CPU Saturation (at 30 FPS)** | ~20% CPU | ~4% CPU | **5x CPU Headroom** |

### B. Control Telemetry Plotting & Analysis
*   **Owner**: **Costin**
*   Utilize the saved `motor_telemetry.csv` file generated by the save-output binary. You should create two main graphs:
    1.  **Transient Tracking Performance (Step Response)**: Plot encoder counts and PWM duty cycles over time to analyze transient settling behavior.
    2.  **Telemetry Correlation Analysis**: Plot encoder position vs. PWM command to illustrate how duty cycle capping eliminates jitter.

---

## 4. Suggested Additional Premium Sections [NEW]

*   **[NEW] System-Level Hardware-Software Block Diagram**:
    *   Illustrate data flow: Camera $\to$ HPS (GStreamer $\to$ LUT $\to$ Centroid $\to$ Controller) $\to$ Avalon Bus $\to$ FPGA (PWM Gen) $\to$ Motor Driver $\to$ DC Motor $\to$ Encoder $\to$ FPGA (Encoder Decoder) $\to$ Avalon Bus $\to$ HPS.
*   **[NEW] Real-Time Thread Scheduling & Latency Analysis**:
    *   Describe the GStreamer frame-grabbing thread running asynchronously, and the 100 Hz control loop thread running with strict periodic timing via `clock_nanosleep`. Detail the use of lock-free atomics (`std::atomic`) to avoid blocking.
*   **[NEW] Metastability & Synchronization Theory**:
    *   Provide a theoretical explanation of why a 2-stage shift-register synchronizer is necessary on the FPGA inputs for the quadrature encoder signals `A` and `B` to reduce the Mean Time Between Failures (MTBF).
