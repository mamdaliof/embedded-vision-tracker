// TopEntity.v
module TopEntity (
    input  clk,
    input  SPI_CLK,
    input  SPI_PICO,
    input  SPI_CS,
    output SPI_POCI,
    output led2,
    // CHANGED: Generic port identifiers match the exact strings in ico-jiwy.pcf
    input  YAW_ENC_A,  
    input  YAW_ENC_B,
    output YAW_PWM_VAL, 
    output YAW_DIRA,
    output YAW_DIRB
);

  // Buffer registers
  reg [15:0] rx_shift_reg; 
  reg [15:0] tx_shift_reg; 
  reg [3:0]  bit_cnt;      
  reg [7:0]  latched_duty;
  reg        latched_dir;
  wire signed [15:0] encoder_count;

  // PWM Generator Submodule Hookup
  pwm_generator pwm_inst (
        .clk(clk),
        .rst(1'b0), 
        .cnt_enable(1'b1),
        .duty_cycle(latched_duty),
        .direction(latched_dir),
        .PWM_OUT(YAW_PWM_VAL), // Re-mapped
        .INA(YAW_DIRA),       // Re-mapped
        .INB(YAW_DIRB)        // Re-mapped
    );

    // Encoder Submodule Hookup
    Quad_compact encoder_inst (
        .clk(clk),
        .A(YAW_ENC_A), // Re-mapped
        .B(YAW_ENC_B), // Re-mapped
        .reset(1'b0),
        .count(encoder_count)
    );

  // Synchronize and edge detect SPI signals
  reg [2:0] SPI_CLKr;
  always @(posedge clk) SPI_CLKr <= {SPI_CLKr[1:0], SPI_CLK};
  wire SPI_CLK_risingedge = (SPI_CLKr[2:1] == 2'b01);
  wire SPI_CLK_fallingedge = (SPI_CLKr[2:1] == 2'b10);

  reg [2:0] SPI_CSr;
  always @(posedge clk) SPI_CSr <= {SPI_CSr[1:0], SPI_CS};
  wire SPI_CS_active = ~SPI_CSr[1];
  wire SPI_CS_startmessage = (SPI_CSr[2:1] == 2'b10);

  reg [1:0] SPI_PICOr;
  always @(posedge clk) SPI_PICOr <= {SPI_PICOr[0], SPI_PICO};
  wire SPI_PICO_data = SPI_PICOr[1];

  // Bidirectional Bus State Machine
  always @(posedge clk) begin
    if (~SPI_CS_active) begin
        bit_cnt <= 0;
    end else if (SPI_CS_startmessage) begin
        tx_shift_reg <= encoder_count;
    end else if (SPI_CLK_risingedge) begin
        // Shift in the new bit data from master line
        // CORRECTION: Latch logic evaluates data inline using current wire input state
        rx_shift_reg <= {rx_shift_reg[14:0], SPI_PICO_data};
        bit_cnt <= bit_cnt + 4'd1;
        
        if (bit_cnt == 4'd15) begin
            // Grab the shifted payload inline along with the final incoming serial bit
            latched_duty <= {rx_shift_reg[6:0], SPI_PICO_data};
            latched_dir  <= rx_shift_reg[7];
        end
    end
  end

  // Shift data out to MISO line on clock falling edge sequence
  always @(posedge clk) begin
    if (SPI_CLK_fallingedge && SPI_CS_active) begin
        tx_shift_reg <= {tx_shift_reg[14:0], 1'b0};
    end
  end

  assign SPI_POCI = SPI_CS_active ? tx_shift_reg[15] : 1'bz;
  assign led2 = latched_dir; 

endmodule