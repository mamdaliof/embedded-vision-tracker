###
echo "[WARNING] DELIVERED AS IS, the TA's are not required to help you with running this script"
###

set -e
echo "[IMPORTANT] Please UNDERSTAND what this script does before asking question why it doesn't work. Also run this via the terminal in the MAIN folder!"
echo "Simulate: Blinking LED [0], Full System [1], PWM [2], QuadratureEncoder [3], ..."
read INPUT



if [ "$INPUT" = "1" ]; then # If the users input is equal to variable it executes this branch and is finished afterwards!
    # For the full System MY Designed system had the TopEntity in the "main folder", so this could not work in your case"
    iverilog -o TopEntity_tb.vvp TopEntity_tb.v
    vvp TopEntity_tb.vvp
    gtkwave signals.vcd
fi

if [ "$INPUT" = "0" ]; then 
    cd assignment_01 || { echo "failed to enter folder! Does it exist?"; exit 1;} # For the full System MY Designed system had the TopEntity in the "main folder", so this could not work in your case"
    iverilog -o TopEntity_tb.vvp TopEntity_tb.v
    vvp TopEntity_tb.vvp
    gtkwave signals.vcd
fi

if [ "$INPUT" = "2" ]; then
    cd PWM || { echo "failed to enter folder! Does it exist?"; exit 1;} #Move to the relevant folder and start the relevant testbench there
    iverilog -o PWMGenerator_tb.vvp PWMGenerator_tb.v
    vvp PWMGenerator_tb.vvp
    gtkwave signals.vcd
fi

if [ "$INPUT" = "3" ]; then
    cd QuadratureEncoder || { echo "failed to enter folder! Does it exist?"; exit 1;}
    iverilog -o QuadratureEncoder_tb.vvp QuadratureEncoder_tb.v
    vvp QuadratureEncoder_tb.vvp
    gtkwave signals.vcd
fi

# Feel free to expand on this
