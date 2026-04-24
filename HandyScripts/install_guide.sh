###
echo "[WARNING] DELIVERED AS IS, the TA's are not required to help you with running this script"
###

## update your packages
sudo apt update

## make project folder (from git)
sudo apt install git
git clone https://git.ram.eemcs.utwente.nl/repository-esl/laboratory-files.git
cd laboratory-files

## installing the packages
sudo apt install yosys 
sudo apt install nextpnr-ice40-qt 
sudo apt install libboost-all-dev 
sudo apt install libeigen3-dev 
sudo apt install qtcreator 
sudo apt install qtbase5-dev 
sudo apt install qt5-qmake 
sudo apt install fpga-icestorm 
sudo apt install g++ 
sudo apt install raspi-config
sudo apt install git 
sudo apt install libgpiod-dev 
sudo apt install gpiod

# get the additional tooling (icoprog to upgrade / make the fpga on board work)
git clone https://git.ram.eemcs.utwente.nl/repository-esl/icoprog.git
cd icoprog
## Create the icoprog executable
g++ src/icoprog.cpp src/gpio_interface.cpp -o icoprog -lgpiodcxx

# BEFORE YOU DO ANYTHING THE .PCF file is still missing! This is located in the Assignments
