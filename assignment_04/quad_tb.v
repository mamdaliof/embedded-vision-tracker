`timescale 1ns/1ps

module tb_quadrature_encoder;

    // --- Signals for DUT (Device Under Test) ---
    reg clk = 0;
    reg A = 0;
    reg B = 0;
    reg reset = 0;
    wire signed [31:0] count;

    // --- Instantiate the Encoder Module ---
    // Ensure the module name matches your source file exactly
    Quad_compact dut (
        .clk(clk),
        .A(A),
        .B(B),
        .reset(reset),
        .count(count)
    );

    // --- Clock Generation ---
    // 10ns period (100 MHz clock)
    always #5 clk = ~clk;

    // --- Internal Testbench State Tracker ---
    reg [1:0] enc_state = 2'b00;

    // --- Task: Turn Right (Clockwise) ---
    // Gray code sequence for +1: 00 -> 01 -> 11 -> 10
    task turn_right(input integer steps);
        integer i;
        begin
            for (i = 0; i < steps; i = i + 1) begin
                case (enc_state)
                    2'b00: enc_state = 2'b01;
                    2'b01: enc_state = 2'b11;
                    2'b11: enc_state = 2'b10;
                    2'b10: enc_state = 2'b00;
                endcase
                {A, B} = enc_state;
                // Hold state for 4 clock cycles to allow the 
                // 2-stage synchronizer to process the input safely
                #40; 
            end
        end
    endtask

    // --- Task: Turn Left (Counter-Clockwise) ---
    // Gray code sequence for -1: 00 -> 10 -> 11 -> 01
    task turn_left(input integer steps);
        integer i;
        begin
            for (i = 0; i < steps; i = i + 1) begin
                case (enc_state)
                    2'b00: enc_state = 2'b10;
                    2'b10: enc_state = 2'b11;
                    2'b11: enc_state = 2'b01;
                    2'b01: enc_state = 2'b00;
                endcase
                {A, B} = enc_state;
                #40; 
            end
        end
    endtask

    // --- Main Simulation Sequence ---
    initial begin
        // Setup Waveform Recording
        $dumpfile("quad_sim.vcd");
        $dumpvars(0, tb_quadrature_encoder);
        
        // Print output to the terminal only when 'count' or 'reset' changes
        $monitor("Time: %0dns | Reset: %b | A:%b B:%b | Count: %0d", 
                 $time, reset, A, B, count);

        // --- Step 1: Initialize and Reset ---
        reset = 1;
        enc_state = 2'b00;
        {A, B} = enc_state;
        #50; // Hold reset high
        reset = 0;
        #50;

        // --- Step 2: Turn 150 pulses right ---
        $display("\n--- Turning 150 pulses right ---");
        turn_right(150);
        #100; // Pause briefly before next action

        // --- Step 3: Turn 70 pulses left ---
        $display("\n--- Turning 70 pulses left ---");
        turn_left(70);
        #100;

        // --- Step 4: Reset the counter ---
        $display("\n--- Asserting Reset ---");
        reset = 1;
        #50;
        reset = 0;
        #50;

        // --- Step 5: Turn 70 pulses left ---
        $display("\n--- Turning 70 pulses left ---");
        turn_left(70);
        #100;

        $display("\n--- Simulation Complete ---");
        $finish;
    end

endmodule