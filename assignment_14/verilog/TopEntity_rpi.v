// TopEntity.v
// Dual-axis SPI controller for the Raspberry Pi + icoBoard setup (Assignment 14).
// Extends the single-axis version from Assignment 11 to handle both Pan (Yaw)
// and Tilt (Pitch) axes over a single 4-byte full-duplex SPI frame.

// SPI frame layout (MSB first, 32 bits total):
//   TX (RPi -> icoBoard):  [duty_pan 8b][dir_pan 8b][duty_tilt 8b][dir_tilt 8b]
//   RX (icoBoard -> RPi):  [enc_pan_hi 8b][enc_pan_lo 8b][enc_tilt_hi 8b][enc_tilt_lo 8b]

module TopEntity (
    input  clk,

    // SPI bus (single shared bus, RPi is master)
    input  SPI_CLK,
    input  SPI_PICO,
    input  SPI_CS,
    output SPI_POCI,

    // Pan (Yaw) axis
    input  YAW_ENC_A,
    input  YAW_ENC_B,
    output YAW_PWM_VAL,
    output YAW_DIRA,
    output YAW_DIRB,

    // Tilt (Pitch) axis
    input  PITCH_ENC_A,
    input  PITCH_ENC_B,
    output PITCH_PWM_VAL,
    output PITCH_DIRA,
    output PITCH_DIRB
);

// Synchronize and edge detect SPI signals
reg [2:0] SPI_CLKr;
always @(posedge clk) SPI_CLKr <= {SPI_CLKr[1:0], SPI_CLK};
wire SPI_CLK_rising             = (SPI_CLKr[2:1] == 2'b01);
wire SPI_CLK_falling            = (SPI_CLKr[2:1] == 2'b10);

reg [2:0] SPI_CSr;
always @(posedge clk) SPI_CSr  <= {SPI_CSr[1:0], SPI_CS};
wire SPI_CS_active              = ~SPI_CSr[1];
wire SPI_CS_startmessage        = (SPI_CSr[2:1] == 2'b10);

reg [1:0] SPI_PICOr;
always @(posedge clk) SPI_PICOr <= {SPI_PICOr[0], SPI_PICO};
wire SPI_PICO_data = SPI_PICOr[1];

// Encoders
wire signed [15:0] enc_pan;
wire signed [15:0] enc_tilt;

// PWM generators
pwm_generator pwm_pan (
    .clk        (clk),
    .rst        (1'b0),
    .cnt_enable (1'b1),
    .duty_cycle (latched_duty_pan),
    .direction  (latched_dir_pan),
    .PWM_OUT    (YAW_PWM_VAL),
    .INA        (YAW_DIRA),
    .INB        (YAW_DIRB)
);

pwm_generator pwm_tilt (
    .clk        (clk),
    .rst        (1'b0),
    .cnt_enable (1'b1),
    .duty_cycle (latched_duty_tilt),
    .direction  (latched_dir_tilt),
    .PWM_OUT    (PITCH_PWM_VAL),
    .INA        (PITCH_DIRA),
    .INB        (PITCH_DIRB)
);

// Encoder Submodules
Quad_compact enc_pan_inst (
    .clk   (clk),
    .A     (YAW_ENC_A),
    .B     (YAW_ENC_B),
    .reset (1'b0),
    .count (enc_pan)
);

Quad_compact enc_tilt_inst (
    .clk   (clk),
    .A     (PITCH_ENC_A),
    .B     (PITCH_ENC_B),
    .reset (1'b0),
    .count (enc_tilt)
);

// Buffer registers (32-bit frame for this assignment, 2 bytes per axis)
reg [31:0] rx_shift;
reg [31:0] tx_shift;
reg [4:0]  bit_cnt;   // 0-31

// Latch registers for decoded PWM outputs
reg [7:0]  latched_duty_pan;
reg        latched_dir_pan;
reg [7:0]  latched_duty_tilt;
reg        latched_dir_tilt;

always @(posedge clk) begin
    if (~SPI_CS_active) begin
        bit_cnt  <= 0;
    end else if (SPI_CS_startmessage) begin
        // Pre-load the TX shift register with both encoder values
        tx_shift <= {enc_pan, enc_tilt};
    end else if (SPI_CLK_rising) begin
        rx_shift <= {rx_shift[30:0], SPI_PICO_data};
        bit_cnt  <= bit_cnt + 5'd1;

        // On the last bit (31), latch the complete received frame
        if (bit_cnt == 5'd31) begin
            // Frame layout: [duty_pan 31:24][dir_pan 23:16][duty_tilt 15:8][dir_tilt 7:0]
            // dir bytes are effectively 1-bit (LSB used), rest ignored
            latched_duty_pan  <= {rx_shift[22:16], SPI_PICO_data}; // bits 31:24 of final frame
            latched_dir_pan   <= rx_shift[23];                       // bit 23
            latched_duty_tilt <= rx_shift[15:8];
            latched_dir_tilt  <= rx_shift[0];
        end
    end
end

// Shift MISO data out on falling edge
always @(posedge clk) begin
    if (SPI_CLK_falling && SPI_CS_active)
        tx_shift <= {tx_shift[30:0], 1'b0};
end

assign SPI_POCI = SPI_CS_active ? tx_shift[31] : 1'bz;
assign led2 = latched_dir_pan;

endmodule