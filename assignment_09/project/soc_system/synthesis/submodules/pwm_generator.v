module pwm_generator #(
        parameter DATA_WIDTH = 8
    ) (
        input wire clk,
        input wire rst,
        input wire cnt_enable,
        input wire [DATA_WIDTH-1:0] duty_cycle, // 0 to 255 for 8-bit resolution
        input wire direction, // 1=CW, 0=CCW
        output reg INA,
        output reg INB,
        output reg PWM_OUT
    );
    parameter PERIOD = 2500; // 50us period (20kHz target) at 50MHz clock. basically, clock_freq / desired_pwm_freq
    parameter MIN_OFF_CYCLES = 00; // 6us minimum off time at 50MHz clock. This is to ensure the motor driver can properly switch states and avoid damage. (see Table 9 footnote).
    reg [12:0] counter; // 13 bits to count up to 2500

    // Calculate maximum threshold outside the posedge block.
    wire [12:0] raw_threshold;
    assign raw_threshold = (duty_cycle * PERIOD) / 256;
    // Ensure the threshold respects the minimum off time. Basically ensure 6us off time at 50MHz clock, which is 300 cycles.
    /*
    If duty_cycle requests 88% - 100% power, it artificially caps the PWM_OUT high time at 2200 cycles, 
    guaranteeing the signal drops low for exactly 300 cycles (6μs) before the period restarts the next period.
    */
    wire [12:0] max_threshold;
    assign max_threshold = (raw_threshold > (PERIOD - MIN_OFF_CYCLES)) ? 
                           (PERIOD - MIN_OFF_CYCLES) : raw_threshold; // if the calculated threshold exceeds the maximum allowed by the minimum off time, cap it at that maximum.

    always @(posedge clk or posedge rst) begin 
        if (rst) begin // Strictly check only the asynchronous reset
            counter <= 0;
            INA <= 0;
            INB <= 0;
            PWM_OUT <= 0;
        
        end else if (!cnt_enable) begin // Synchronous check for enable
            counter <= 0;
            INA <= 0;
            INB <= 0;
            PWM_OUT <= 0;
            
        end else begin // normal operation

            // Increment counter and reset at the end of the period
            if (counter < (PERIOD - 1)) begin
                counter <= counter + 13'd1; // Explicitly size the adder to prevent truncation warnings
            end else begin
                counter <= 0;
            end

            // set the pwm output relative to the proportional threshold
            if (counter < max_threshold) begin // Scale duty cycle to period
                PWM_OUT <= 1;
            end else begin
                PWM_OUT <= 0;
            end

            // set the direction pins based on table 12
            if (direction) begin // CW
                INA <= 1;
                INB <= 0;
            end else begin // CCW
                INA <= 0;
                INB <= 1;
            end
        end
    end

endmodule
