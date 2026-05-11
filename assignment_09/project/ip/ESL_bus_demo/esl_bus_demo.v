`timescale 1 ps / 1 ps
module esl_bus_demo #(
        parameter DATA_WIDTH = 32
    ) (
        input  wire [7:0]  slave_address,
        input  wire        slave_read,
        output reg  [DATA_WIDTH-1:0] slave_readdata,
        input  wire        slave_write,
        input  wire [DATA_WIDTH-1:0] slave_writedata,
        input  wire        clk,
        input  wire        reset,
        input  wire [(DATA_WIDTH/8)-1:0] slave_byteenable,

        // Encoder pins from assignment 7
        input  wire yaw_A,
        input  wire yaw_B,
        input  wire pitch_A,
        input  wire pitch_B,

        // PWM pins
        output wire INA,
        output wire INB,
        output wire PWM_OUT
    );

    // Encoder wires
    wire signed [31:0] yaw_count;
    wire signed [31:0] pitch_count;

    // PWM control registers
    reg [7:0] duty_cycle;
    reg       direction;
    reg       cnt_enable;

    // Yaw Encoder
    Quad_compact encoder_yaw (
        .clk(clk),
        .A(yaw_A),
        .B(yaw_B),
        .reset(reset),
        .count(yaw_count)
    );

    //  Pitch Encoder
    Quad_compact encoder_pitch (
        .clk(clk),
        .A(pitch_A),
        .B(pitch_B),
        .reset(reset),
        .count(pitch_count)
    );

    //  PWM Generator
    pwm_generator pwm (
        .clk(clk),
        .rst(reset),
        .cnt_enable(cnt_enable),
        .duty_cycle(duty_cycle),
        .direction(direction),
        .INA(INA),
        .INB(INB),
        .PWM_OUT(PWM_OUT)
    );

    // Avalon-MM Read/Write Protocol
    always @(posedge clk or posedge reset) begin
        if (reset) begin
            slave_readdata <= 32'b0;
            duty_cycle     <= 8'b0;
            direction      <= 1'b0;
            cnt_enable     <= 1'b0;
        end else begin

            // Handle ARM Read Requests
            if (slave_read) begin
                case (slave_address)
                    8'h00: slave_readdata <= yaw_count;   // read yaw encoder. Base Address + 0
                    8'h01: slave_readdata <= pitch_count; // read pitch encoder. Base Address + 4 bytes
                    default: slave_readdata <= 32'b0;
                endcase
            end

            // Handle ARM Write Requests
            if (slave_write) begin
                case (slave_address)
                    8'h02: begin
                        // bit[31]   = cnt_enable
                        // bit[8]    = direction (1=CW, 0=CCW)
                        // bit[7:0]  = duty_cycle (0-255)
                        cnt_enable <= slave_writedata[31];
                        direction  <= slave_writedata[8];
                        duty_cycle <= slave_writedata[7:0];
                    end
                    default: ; // ignore other addresses
                endcase
            end

        end
    end

endmodule