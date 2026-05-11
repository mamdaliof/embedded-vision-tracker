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
        
        // Conduits to physical switches / encoder pins
        input  wire  yaw_A, 
        input  wire  yaw_B,
        input  wire  pitch_A, 
        input  wire  pitch_B        
    );

    // 1. Wires to carry the output from the instantiated engines.
    // They MUST be wires, not regs.
    wire signed [31:0] yaw_count;
    wire signed [31:0] pitch_count;

    // 2. Instantiate the Yaw Encoder
    Quad_compact encoder_yaw (
        .clk(clk),
        .A(yaw_A), 
        .B(yaw_B),
        .reset(reset),
        .count(yaw_count) // Correctly mapped to the module's actual port
    );

    // 3. Instantiate the Pitch Encoder
    Quad_compact encoder_pitch (
        .clk(clk),
        .A(pitch_A), 
        .B(pitch_B),
        .reset(reset),
        .count(pitch_count) // Correctly mapped to the module's actual port
    );

    // 4. The Avalon-MM Read/Write Protocol
    // Only variables declared as 'reg' (like slave_readdata) can be modified here.
    always @(posedge clk or posedge reset) begin
        if (reset) begin
            slave_readdata <= 32'b0;
        end else begin
            // Handle ARM Read Requests via Address Decoding
            if (slave_read) begin
                case (slave_address)
                    8'h00: slave_readdata <= yaw_count;   // Base Address + 0
                    8'h01: slave_readdata <= pitch_count; // Base Address + 4 bytes
                    default: slave_readdata <= 32'b0;
                endcase
            end
            
            // Handle ARM Write Requests (e.g., for PWM later)
            // Left empty for now as we are only reading encoders.
            if (slave_write) begin
                // Future PWM logic goes here.
            end
        end
    end

endmodule