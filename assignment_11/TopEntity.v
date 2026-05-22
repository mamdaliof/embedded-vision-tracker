// TopEntity.v
// Contains a verilog module called TopEntity that inplements a simple SPI bouncer.
// What it receives in transaction N, it will send back in transaction N+1.
// Look into SPI and Full-Duplex connection for more information if this is unclear
//
// Heavily insired by https://www.fpga4fun.com/SPI2.html

module TopEntity (
    input  clk,
    input  SPI_CLK,
    input  SPI_PICO,
    input  SPI_CS,
    output SPI_POCI,
    output led2,
    input ENC_A,  
    input ENC_B,
    output PWM_OUT, 
    output INA,
    output INB
);

  reg [15:0] rx_shift_reg; 
  reg [15:0] tx_shift_reg; 
  reg [3:0]  bit_cnt;      
  reg [7:0] latched_duty;
  reg       latched_dir;
  wire signed [15:0] encoder_count; // From Quad_compact

  pwm_generator pwm_inst (
        .clk(clk),
        .rst(1'b0), 
        .cnt_enable(1'b1),
        .duty_cycle(latched_duty),
        .direction(latched_dir),
        .PWM_OUT(PWM_OUT),
        .INA(INA),
        .INB(INB)
    );

    // 3. Instantiate Encoder Module
    Quad_compact encoder_inst (
        .clk(clk),
        .A(ENC_A),
        .B(ENC_B),
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
  wire SPI_CS_endmessage = (SPI_CSr[2:1] == 2'b01);

  reg [1:0] SPI_PICOr;
  always @(posedge clk) SPI_PICOr <= {SPI_PICOr[0], SPI_PICO};
  wire SPI_PICO_data = SPI_PICOr[1];

  // Bit counter and byte receiver
  reg [2:0] bitcnt;
  reg byte_received;
  reg [7:0] byte_data_received;

  always @(posedge clk) begin
    if (~SPI_CS_active) begin
        bit_cnt <= 0;
    end else if (SPI_CS_startmessage) begin
        // Load Encoder value into TX buffer when CS goes low
        tx_shift_reg <= encoder_count; 
    end else if (SPI_CLK_risingedge) begin
        // Shift data in from PICO
        rx_shift_reg <= {rx_shift_reg[14:0], SPI_PICO_data};
        bit_cnt <= bit_cnt + 1;
        
        // When 16 bits are fully received, latch the command
        if (bit_cnt == 15) begin
            latched_duty <= rx_shift_reg[7:0];
            latched_dir  <= rx_shift_reg[8];
        end
    end
  end

  // Shift data out on falling edge
  always @(posedge clk) begin
    if (SPI_CLK_fallingedge && SPI_CS_active) begin
        tx_shift_reg <= {tx_shift_reg[14:0], 1'b0};
    end
  end

  
  assign SPI_POCI = SPI_CS_active ? tx_shift_reg[15] : 1'bz;
  assign led2 = latched_dir; 

endmodule