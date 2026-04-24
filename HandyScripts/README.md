# embedded Systems Laboratory
Welcome to this example project file structure:

In this project an FPGA will be able to control a JIWY, and it is up to the student to determine how it can showcase this.

Below are some shortcuts listed, these are also available as shell scripts in order to not keep repeating  lines of code and save us some time.

Among other things ** ico-jiwy.pcf ** is missing!

In order to test the system yourself you can compile the program via ./program.sh, that should automagically compile and sent the data to the correct folder on the raspberry pi and also flash it.
after that ./gcc -o main main.c  can be executed to compile the program and by executing this and passing the camera through the program should start working.

to see the testbenches, ./simulate.sh can be ran and then by entering 1 2 or 3 the full system, PWM or quadrature encoder test bench is compiled and automagically opened in GTKWave.

** This readme is written for a Linux OS, if you're on Windows please install [WSL](https://learn.microsoft.com/en-us/windows/wsl/install)**

# Helpful scripts!
**In the initial design the scripts were all located from the "main" folder, so in the terminal please execute them from the main './handyScripts/compile' for example **

## Simulating the FPGA (GTKWAVE)
In order to simulate 

```
iverilog -o TopEntity_tb.vvp TopEntity_tb.v
vvp TopEntity_tb.vvp
gtkwave signals.vcd
```

## Building C code
In order to execute C, code please run the following command
```
gcc -o main main.c
```

## Building the verilog Code
Please remember that if you want to upload the Verilog Code, the SPI interface must be turned **OFF**, afters uploading it needs to be turned back **ON**
```
raspi-config
```



# Reducing your amount of work? Learn the power of scripts!

During this project you will learn that the commands above will need to be executed a bunch of time.
To avoid a lot of work you can learn a bit of scripting, [Learn to script (GeeksForGeeks)](https://www.geeksforgeeks.org/linux-unix/introduction-linux-shell-shell-scripting/) or [The shell scripting Tutorial](https://www.shellscript.sh/).
Below are some pre-made scripts. However, these will most likely need to be adapted to your file structure.
** The TA's are not required to help you adapt these scripts ** 

## Compiling the verilog code
To compile the verilog code, first create the binary, which afterwards needs to be moved to the icoprog folder.

```
yosys -p 'synth_ice40 -top TopEntity -json ice40.json' TopEntity.v
nextpnr-ice40 --hx8k --json ice40.json --pcf ico-jiwy.pcf --asc ice40.asc
icepack ice40.asc ice40.bin
```

move ice40.bin to icoprog folder [mv -i example.txt ~/Documents] or vscode

```
cd ~/icoprog
sudo dtparam spi=off
./icoprog -R
./icoprog -p < ice40.bin
sudo dtparam spi=on
```


## full script (raw)


```
cd ~/ESL
yosys -p 'synth_ice40 -top TopEntity -json ice40.json' TopEntity.v
nextpnr-ice40 --hx8k --json ice40.json --pcf ico-jiwy.pcf --asc ice40.asc
icepack ice40.asc ice40.bin
mv ice40.bin ~/icoprog
cd ~/icoprog
sudo dtparam spi=off
./icoprog -R
./icoprog -p < ice40.bin
sudo dtparam spi=on
cd ~/ESL/spi
gcc -o main main.c
```


### TLDR
You can use the scripts in the handyScripts folder to simplify your code execution.
**ASSUME** If you try to run the script in your filestructure the scripts will say **"ERROR"**, please use it as inspiration for automisation needs.



