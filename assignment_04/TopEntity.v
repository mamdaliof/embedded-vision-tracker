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
    // 1. QUADRATURE ENCODERS
    // =========================================================
    wire signed [31:0] pitch_count;
    wire signed [31:0] yaw_count;
    wire reset_n = 1'b0; // Hardwired reset to 0 (active high in module)

    // Instance 1: Pitch Encoder
    Quad_compact pitch_inst (
        .clk(clk),
        .A(PITCH_ENC_A),
        .B(PITCH_ENC_B),
        .reset(reset_n),
        .count(pitch_count)
    );

    // Instance 2: Yaw Encoder
    Quad_compact yaw_inst (
        .clk(clk),
        .A(YAW_ENC_A),
        .B(YAW_ENC_B),
        .reset(reset_n),
        .count(yaw_count)
    );

    // Tie LEDs to the LSB for instant physical feedback
    assign led2 = pitch_count[0];
    assign led3 = yaw_count[0];

    // =========================================================
    // 2. HEARTBEAT BLINKER
    // =========================================================
    localparam CLK_FREQ = 100_000_000; // 100MHz
    localparam BLINK_RATE = 1;         // 1Hz
    localparam CNT_MAX = CLK_FREQ / (2 * BLINK_RATE);

    reg [31:0] blink_cnt = 0;

    always @(posedge clk) begin
        if (blink_cnt >= CNT_MAX - 1) begin
            blink_cnt <= 0;
            led1 <= ~led1;
        end else begin
            blink_cnt <= blink_cnt + 1;
        end
    end

endmodule