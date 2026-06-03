# Assignment 14: Controller Performance Comparison & Integration

This assignment focuses on integrating the **20-sim generated controllers** for the Pan (Yaw) and Tilt (Pitch) axes into the C application and comparing performance across different hardware/software configurations.

## 📂 Project Structure
- `controllers/PositionControllerPan/`: C code for the Pan-axis PID controller (`pan_*`).
- `controllers/PositionControllerTilt/`: C code for the Tilt-axis PID controller (`tilt_*`).
- `*_model.c/h`: Contains the mathematical equations of the PID.
- `*_submod.c/h`: Wrapper functions to initialize and step the model.
- `*_main.c`: Template main loop (reference only).

## 🚀 Integration Steps

### 1. Unified Controller Wrapper
You need to create a single C application that manages both controllers simultaneously.
- **Initialize**: Call `XXInitializeSubmodel` for both Pan and Tilt at startup.
- **Input Mapping**: 
    - Pan Inputs (`u` array): `u[0]` = setpoint (desired angle), `u[1]` = feedback (current encoder value).
    - Tilt Inputs (`u` array): `u[0]` = correction, `u[1]` = setpoint, `u[2]` = feedback.
- **Output Mapping**:
    - The controller output (`y[0]`) is the raw control signal (normalized -1.0 to 1.0).
    - Scale this value to an 8-bit duty cycle (0-255) and determine the direction bit for the FPGA PWM registers.

### 2. Time-Driven Execution
The controllers are discrete and expect a fixed time step (defined as `0.01s` or 100Hz in `*_model.c`).
- Implement a precise timer loop (e.g., using `clock_gettime` or `usleep` with compensation).
- In each step:
    1. Read Encoders from FPGA (Address `0x00` and `0x04`).
    2. Convert Encoder counts to Radians (SI units).
    3. Update Controller Inputs.
    4. Call `XXCalculateSubmodel`.
    5. Convert Controller Output to PWM duty/direction.
    6. Write to FPGA PWM registers (Address `0x08` and `0x0C`).

### 3. Performance Measurement (DSE)
Compare the execution on:
- **ARM on Raspberry Pi** vs **ARM on DE10-Nano**.
- Measure the time taken for the `XXCalculateSubmodel` call vs the total loop time.
- Analyze the impact of floating-point operations on real-time jitter.

## 🛠️ Compilation
Compile all relevant `pan_*.c` and `tilt_*.c` files along with your main integration code. Link against the math library:
```bash
gcc main_control.c controllers/PositionControllerPan/pan_*.c controllers/PositionControllerTilt/tilt_*.c -lm -o jiwy_controller
```

## ⚠️ Safety Notes
- **Homing**: Ensure a homing procedure is performed before enabling the PID loop to establish a zero-reference for encoders.
- **Saturation**: The controllers include `SignalLimiter` logic, but always verify duty cycle bounds in software before writing to hardware.
