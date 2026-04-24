###
echo "[WARNING] DELIVERED AS IS, the TA's are not required to help you with running this script"
###

# this script is to program the pi
# If you just want the individual commands you can look in the README.md

set -e
echo "[IMPORTANT] Please UNDERSTAND what this script does before asking question why it doesn't work. Also run this via the terminal in the MAIN folder!"
echo "program: With main.c [0], without main.c [1], ..."
read INPUT


# Start of the script
cd ~/ESL || { echo "failed to enter folder! Does it exist?"; exit 1;} #move to the main folder
yosys -p 'synth_ice40 -top TopEntity -json ice40.json' TopEntity.v #program
echo "[IMPORTANT] Please check to output of yosys for errors!"
read -p "Press any key to continue... " -n1 -s
nextpnr-ice40 --hx8k --json ice40.json --pcf ico-jiwy.pcf --asc ice40.asc
echo "[IMPORTANT] Please check to output of nextpnr for errors!"
read -p "Press any key to continue... " -n1 -s
icepack ice40.asc ice40.bin

# The creating of the binary is done!
# Now to put it in the icoprog folder and program the FPGA
cd ~/icoprog || { echo "failed to enter folder! Does it exist?"; exit 1;}
sudo dtparam spi=off
./icoprog -R
./icoprog -p < ~/ESL/ice40.bin
sudo dtparam spi=on

# The FPGA is programmed now to compile the main
if [ "$INPUT" = "0" ]; then
	cd ~/ESL/SPI || { echo "failed to enter folder! Does it exist?"; exit 1;}
	gcc -o main main.c
fi

# Garbage collection, removing these files such that on a second execution it won't grab the old binary.
rm ~/ESL/ice40.bin
rm ~/ESL/ice40.asc
rm ~/ESL/ice40.json
echo "ICE40 board programmed and main is recompiled!"
echo "[IMPORTANT] The program has NOT yet been executed!"
read -p "Press any key to continue... " -n1 -s
