# embedded-vision-tracker

ESL35 is Rasberry Pi SD card with 64 GB

esl35 is for DE10-nano SD card with 8 GB

username: esl35

password: root


DE10 ip address: 10.0.15.73. Always connect via USB serial to laptop.

username: root

password: root

* To find the DE10 ip address, simply call `screen /dev/tty.usbserial-* 115200` when the DE10 is connected to the local machine via USB.

`!!!!!!! ALWAYS CONNECT THE FUCKING ETHERNET TO THE BOARD !!!!!`

--- 

# Assignment 1 - Blinking LED (icoBoard)

Steps are for execution on the RPi. Otherwise copy the local .bin to RPi.

1. cd to /assignment_01
2. `yosys -p 'synth_ice40 -top TopEntity -json ice40.json' TopEntity.v`
3. `nextpnr-ice40 --hx8k --json ice40.json --pcf ico-jiwy.pcf --asc ice40.asc`
4. `icepack ice40.asc ice40.bin & move ice40.bin to ./icoprog`
5. `./icoprog -R`
6. `./icoprog -p < ice40.bin`

---

# Assignment 2 - Blinking LED (DE10-Nano)

Done via Quartus GUI on xoc2, then on the DE10. Copied the generated .rbf file using scp.

On DE10 terminal: `scp s<studentnumber>@xoc2.ewi.utwente.nl:<path>/output.rbf .`

1. `mkdir -p fat`
2. `mount /dev/mmcblk0p1 fat`
3. `cp soc_system.rbf fat/soc_system.rbf`
4. `umount fat`
5. `sudo reboot` and you shall see the blinking LED on the DE10-Nano

---

# Assignment 3 - PulseView / Logic Analyzer

It was more about making the software work than anything else? Farhad maybe you can confirm here.

---

# Assignment 4.1 - Quadrature Encoder in Verilog

to run the testbench:

1. `iverilog -o quad_sim quadrature_encoder_counter.v quad_tb.v`
2. `vvp quad sim`
3. `to visualize it: gtkwave quad_sim.vcd`

to synthesize it on Raspberry:

1. `yosys -p 'synth_ice40 -top top -json ice40.json' TopEntity.v quadrature_encoder_counter.v`
2. `nextpnr-ice40 --hx8k --json ice40.json --pcf ico-jiwy.pcf --asc ice40.asc`
3. `icepack ice40.asc ice40.bin`

if those 3 commands were called locally, just copy->paste ice40.bin to the rpi.

now flash on the icoboard with `./icoprog -R` and then `./icoprog -p < ice40.bin`. If by moving the end-effector we see LEDs 2 and 3 flickering, we know the encoders are being read correctly. Move ice40.bin to ./icoprog.

---

# Assignment 5 - GStreamer Pipeline

it's possible to check what formats c250 supports: `v4l2-ctl -d /dev/video0 --list-formats-ext`.

* because of USB bandwidth limitations (2.0), it could drop to 15 fps instead of 30 fps. In this case, change the capsfilter in `assignment_05/webcam_module.c`

steps:

1. to compile: `gcc -Wall webcam_module.c -o webcam_module $(pkg-config --cflags --libs gstreamer-1.0)`
2. `./webcam_module /dev/video0` or whatever port the webcam is on
3. `mplayer -demuxer rawvideo -rawvideo w=640:h=480:format=yuy2 file.yuv`

---

# Assignment 6 - GStreamer Appsink Module for calculating mean brightness of the stream

1. `sudo apt install libgstreamer-plugins-base1.0-dev`
2. `gcc webcam_brightness_module.c -o webcam_module $(pkg-config --cflags --libs gstreamer-1.0 gstreamer-app-1.0)`
3. `./webcam_module /dev/video0` or whatever port the camera is on.

---

# Assignment 9 - PWM Module for DE10

Local Simulation

1. `iverilog -o pwm_sim pwm_generator.v pwm_tb.v`
2. `vvp pwm_sim`
3. `gtkwave pwm_tb.vcd` — to visualize the waveform

Synthesis (on xoc2 via X2Go)

1. Open Platform Designer, load `soc_system.qsys`
2. Re-add `esl_bus_demo` IP to pick up updated Tcl with PWM ports
3. Generate HDL -> compile in Quartus -> convert to `.rbf`

Connect to DE10

```bash
ssh -X root@10.0.15.73
password: root
```

To copy any local file (Ubuntu) to DE10 (Run on Ubuntu): `scp /home/costin/ESL/laboratory-files/assignment_09/c/main_pwm.c root@10.0.15.73:~`
To copy from xoc2 server to DE10 (run on DE10): `scp s<studentnumber>@xoc2.ewi.utwente.nl:/path/to/soc_system.rbf .` (did not test it, should work).

Deploy to DE10

```bash
mkdir -p fat
mount /dev/mmcblk0p1 fat
cp resulting_file.rbf fat/soc_system.rbf
cp socfpga_cyclone5_de0_nano_soc.dtb fat/
umount fat
reboot
```

Run on DE10

Compile and run — **duty_cycle (0-255) and direction (0=CCW, 1=CW) are required arguments**:

```bash
gcc main_pwm.c -o pwm_control
./pwm_control 128 1   # 50% speed, clockwise
./pwm_control 64 0    # 25% speed, counter-clockwise
./pwm_control 0 0     # stop
```

Verify

- Probe `YAW_PWM_VAL` pin with oscilloscope — verify 20kHz frequency
- Probe `YAW_DIRA` and `YAW_DIRB` — verify they match Table 12 from VNH2SP30-E datasheet


# Assignment 14 

to build the vision:

```bash
g++ -O3 -o vision_tracker vision_tracker.cpp $(pkg-config --cflags --libs gstreamer-1.0 gstreamer-app-1.0 opencv4)
```