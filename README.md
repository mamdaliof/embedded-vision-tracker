# embedded-vision-tracker

ESL35 is Rasberry Pi SD card with 64 GB

esl35 is for DE10-nano SD card with 8 GB

username: esl35

password: root


DE10 ip address: 10.0.15.73

username: root

password: root

* To find the DE10 ip address, simply call `screen /dev/tty.usbserial-* 115200` when the DE10 is connected to the local machine via USB.

--- 

# Assignment 1 - Blinking LED (icoBoard)

Steps are for execution on the RPi. Otherwise copy the local .bin to RPi.

1. cd to /assignment_01
2. yosys -p 'synth_ice40 -top TopEntity -json ice40.json' TopEntity.v
3. nextpnr-ice40 --hx8k --json ice40.json --pcf ico-jiwy.pcf --asc ice40.asc
4. icepack ice40.asc ice40.bin & move ice40.bin to ./icoprog
5. ./icoprog -R
6. ./icoprog -p < ice40.bin

---

# Assignment 2 - Blinking LED (DE10-Nano)

Done via Quartus GUI on xoc2, then on the DE10. Copied the generated .rbf file using scp.

On DE10 terminal: `scp s<studentnumber>@xoc2.ewi.utwente.nl:<path>/output.rbf .`

1. mkdir -p fat
2. mount /dev/mmcblk0p1 fat
3. cp soc_system.rbf fat/soc_system.rbf
4. umount fat
5. sudo reboot and you shall see the blinking LED on the DE10-Nano

---

# Assignment 3 - PulseView / Logic Analyzer

It was more about making the software work than anything else? Farhad maybe you can confirm here.

---

# Assignment 4.1 - Quadrature Encoder in Verilog

to run the testbench:

1. iverilog -o quad_sim quadrature_encoder_counter.v quad_tb.v
2. vvp quad sim

to synthesize it on Raspberry:

1. yosys -p 'synth_ice40 -top top -json ice40.json' TopEntity.v quadrature_encoder_counter.v
2. nextpnr-ice40 --hx8k --json ice40.json --pcf ico-jiwy.pcf --asc ice40.asc
3. icepack ice40.asc ice40.bin

if those 3 commands were called locally, just copy->paste ice40.bin to the rpi.

now flash on the icoboard with `./icoprog -R` and then `./icoprog -p < ice40.bin`. If by moving the end-effector we see LEDs 2 and 3 flickering, we know the encoders are being read correctly. Move ice40.bin to ./icoprog.

---

# Assignment 5 - GStreamer Pipeline

to compile: gcc -Wall helloworld.c -o helloworld $(pkg-config --cflags --libs gstreamer-1.0)