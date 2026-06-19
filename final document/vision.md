Vision-in-the-loop Architecture
Jun 18, 20265 min read

We need to perform Design-Space Exploration for a vision-in-the-loop setup which detects an object and keeps it in the middle of the frame. However, we need to take several topics into consideration:

ARM on Raspberry Pi vs ARM on DE10-Nano
Lattice ICE40 vs Cyclone 5
Software vs Hardware
Integer vs Floating point precision in the control loop
Development time vs performance
Source quality (RAW / JPEG / …) vs required processing power
The current questions we should have an answer to are:

The communication at the software side can be handled in different ways (polling, interrupts, etc) and how does this influence the performance?
Measure interesting details of the implementation (speed, overhead, resource usage, etc), which can be used later on.
ARM on Raspberry Pi vs ARM on DE10-Nano
source: RocketBoards

RPi uses a Cortex-A53 (64-bit, quad-core @ 1.2GHz)
DE10-Nano integrates a dual-core Cortex A9 (32-bit) tied to the FPGA fabric via a high-bandwidth Avalon interconnect backbone.
Just by looking at the surface, it is clear that A53 is faster, and the RPi also provides more RAM which translates into generally more horse-power for the GStreamer image processing. However, the RPi communicates with the IcoBoard over SPI, which burns time every encoder read and pwm write. (chip-select assertion, clock cycles, deselect)

The A9 and the Cyclone V FPGA share the same die. Reads/writes to the encoder and PWM IP go over an on-chip Avalon bus — no additional overhead. This would give us sub-microsecond register access latency.

Regarding Real-Time constraints, both are capable of securing such performances. However, we learned in ASDfR that RPi requires a special library (evl) to access the only core assigned for real-time processes via the Xenomai framework, which might include additional effort on the software side for RPi.

Bottom line: The DE10-Nano wins on control-loop tightness, while the RPi wins in case we need more computational power for the vision pipeline.

Lattice ICE40 vs Cyclone V
sources: ICE40, Cyclone V, short guideline

The Lattice iCE40 sits in the low-complexity category for simple control logic, while the Cyclone V is in the embedded-processing category targeting designs that need ARM cores and richer FPGA fabric.

We believe both FPGA’s could get the job done considering it’s software-only vision, but the Cyclone V wins both in resources and communication aspects. The iCE40 on the icoBoard might reach near capacity when handling encoder reading+PWM writing+SPI logic, leaving no room for any additional image processing. These facts were very well demonstrated in ASDfR.

Software vs Hardware (control/vision loops)
i.e. what do we implement in Verilog on the FPGA vs in C on the ARM?

Software (C on the ARM): The 20-sim generated controller, the GStreamer pipeline, and the blob centroid calculation are all natural fits here. Easier to develop and debug, floating-point is free.
Hardware (Verilog on FPGA): The encoder counter and PWM generator are already in hardware. A hardware color thresholder (operating on raw pixel values directly from a camera sensor) would add signifincant development time which might not be required in the end.
Bottom line: If the software vision pipeline runs at ~30 FPS, that translates into ~33ms latency which is enough, in our opinion, to demonstrate the concept. A hardware implementation could reduce this to a few clock cycles.

Integer vs Floating-Point in the Control Loop
sources: FPGA best practices guide

The hardware necessary to implement a fixed-point operation is typically smaller than the floating-point operation, so we could fit more fixed-point operations into an FPGA than the floating-point equivalent.

ARM (C code): The A9 and A53 both have hardware FPUs (a specialized coprocessor or execution block integrated into a CPU that executes decimal and fractional math). Using float or double is essentially free for both.
ICE40: Does not provide FPU. Floating-point could consume too many resources. The prototype must be done with fixed-point realistically speaking.
Cyclone V: Has DSP blocks with 18×18-bit multipliers. Fixed-point is still preferred for efficiency, but it’s much less constraining than the ICE40.
You can also implement multiplication directly in logic (LUTs and flip-flops), but it takes significant resources. Using dedicated DSP blocks for multiplication makes sense from a performance and logic-use perspective. Hence, even small FPGAs dedicate space to DSP blocks.

source

Bottom line: Since the controller runs on the ARM in C (from 20-sim), we can safely use floating-point — it’s free on this CPU. If we were to implement the controller in Verilog, we’d need fixed-point arithmetic and would have to quantize the PID coefficients manually.

Development Time vs Performance
Vision
Software (Gstreamer + C): Low dev time, expected ~30 fps ⇒ ~33ms latency
Hardware (FPGA pixel pipeline): Very High dev time, expected sub-ms latency
Controller
ARM (20-sim generated C code): Low dev time (Assignment 13) and should be sufficient for JIWY bandwidth
FPGA (Verilog PID): High dev time and with very low jitter. Also deterministic
Communication
Polling: Low dev time with non-deterministic latency
Interrupts: Low-Medium dev time with strict, deterministic latency.
The highlighted options are what we suggest.

Source Quality — RAW / JPEG / YUV vs Required Processing Power
Source: edge ai vision

MJPEG: Lower USB bandwidth usage, higher frame rates achievable over USB. But every frame must be JPEG-decoded before we can access pixel values which translates into CPU cost per frame.
YUV (RAW): Doesn’t require decoding, pixel data is immediately accessible. Less data per processing step means less time and energy per image, which is why YUV is preferred in embedded imaging applications. However, raw YUV at high resolutions requires more USB bandwidth, which can limit our achievable frame rate.
take into consideration that it’s also USB 2.0, not 3.0. Needs to be tested.
we can control the image size we get
Bottom line: since we need the hue information for color tracking, the U and V channels of YUV provide that naturally. It offers direct color access with no decode overhead. We could just go with a lower resolution to keep bandwidth and processing time low.
