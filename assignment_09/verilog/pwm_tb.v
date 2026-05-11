`timescale 1ns/1ps

module tb_pwm_generator;

    // --- Signals for DUT (Device Under Test) ---
    reg clk = 0;
    reg rst = 0;
    reg cnt_enable = 0;
    reg [7:0] duty_cycle = 0; // 8-bit duty cycle input
    reg direction = 0; // 1=CW, 0=CCW
    wire INA;
    wire INB;
    wire PWM_OUT;

    // --- Instantiate the PWM Generator Module ---
    pwm_generator dut (
        .clk(clk),
        .rst(rst),
        .cnt_enable(cnt_enable),
        .duty_cycle(duty_cycle),
        .direction(direction),
        .INA(INA),
        .INB(INB),
        .PWM_OUT(PWM_OUT)
    );

    // --- Clock Generation ---
    // 20ns period (50 MHz clock)
    always #10 clk = ~clk;

    // --- Test Sequence ---
        // --- Test Sequence ---
    initial begin
        // --- Add these two lines for VCD generation ---
        $dumpfile("pwm_tb.vcd");         // Name of the output file
        $dumpvars(0, tb_pwm_generator);  // Dump all variables in this module

        // Reset the system
        rst = 1;
        #40; // Hold reset for a few cycles
        rst = 0;

        // Enable counting and set direction to CW
        cnt_enable = 1;
        direction = 1; // CW

        // Test various duty cycles
        duty_cycle = 8'h00; // 0% duty cycle
        #50000; // Wait for a few periods

        duty_cycle = 8'h80; // 50% duty cycle
        #50000; // Wait for a few periods

        duty_cycle = 8'hFF; // 100% duty cycle (should be capped by minimum off time)
        #50000; // Wait for a few periods

        direction = 0; // CCW
        duty_cycle = 8'h40; // 25% duty cycle
        #50000; // Wait for a few periods

        $finish; // Use $finish instead of $stop to close the VCD file cleanly
    end

endmodule