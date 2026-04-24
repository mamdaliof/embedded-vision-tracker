module top (
    input clk,          // Hardware clock (Pin R9)
    
    // Pitch Hardware Pins
    input PITCH_ENC_A,  // Pin B10
    input PITCH_ENC_B,  // Pin B11
    
    // Yaw Hardware Pins
    input YAW_ENC_A,    // Pin C3
    input YAW_ENC_B,    // Pin B4
    
    // Debug LEDs
    output reg led1 = 0, // Heartbeat blinker (Pin C8)
    output led2,         // Pitch debug (Pin F7)
    output led3          // Yaw debug (Pin K9)
);

    // =========================================================
    // 1. THE ENCODER LOGIC
    // =========================================================
    wire [31:0] pitch_position;
    wire [31:0] yaw_position;

    // Instance 1: Pitch Encoder
    quadrature_encoder pitch_encoder_inst (
        .clk(clk),
        .enc_a(PITCH_ENC_A),
        .enc_b(PITCH_ENC_B),
        .position(pitch_position)
    );

    // Instance 2: Yaw Encoder
    quadrature_encoder yaw_encoder_inst (
        .clk(clk),
        .enc_a(YAW_ENC_A),
        .enc_b(YAW_ENC_B),
        .position(yaw_position)
    );

    // Tie led2 and led3 to the lowest bit of the counters so 
    // they flicker when you physically rotate the motors.
    assign led2 = pitch_position[0];
    assign led3 = yaw_position[0];

    // =========================================================
    // 2. THE HEARTBEAT BLINKER LOGIC
    // =========================================================
    reg [31:0] count = 0;

    always @(posedge clk) begin
        // Note: The operand below is set to ~1 second assuming a ~100MHz clock.
        // Change it back to 5 if you run this in a simulator like DigitalJS!
        if (count >= 99999999) begin   
            count <= 0;         // Reset count register
            led1 <= ~led1;      // Toggle heartbeat LED
        end else begin
            count <= count + 1; // Counts clock cycles
        end
    end

endmodule