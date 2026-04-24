`timescale 1 ps / 1 ps
module esl_bus_demo #(
		parameter LED_WIDTH = 8,
        parameter DATA_WIDTH = 32
	) (
		input  wire [7:0]  slave_address,     //      avs_s0.address
		input  wire        slave_read,        //            .read
		output reg  [DATA_WIDTH-1:0] slave_readdata,    //            .readdata
		input  wire        slave_write,       //            .write
		input  wire [DATA_WIDTH-1:0] slave_writedata,   //            .writedata
		input  wire        clk,          //       clock.clk
		input  wire        reset,        //       reset.reset
        input  wire [(DATA_WIDTH/8)-1:0] slave_byteenable,
		output wire [LED_WIDTH-1:0]  user_output         // user_output.new_signal
	);

    // Internal memory for the system and a subset for the IP
    reg [31:0] mem;
    wire [LED_WIDTH-1:0] mem_masked;
    wire enable;

    // Definition of the counter
    esl_bus_demo_example #(
        .DATA_WIDTH(LED_WIDTH)
    ) my_ip (
        .clk(clk),
        .rst(reset),
        .in(mem_masked),
        .cnt_enable(enable),
        .out(user_output)
    );

    assign mem_masked = mem[LED_WIDTH-1:0];
    assign enable = mem[31];

    always @(posedge clk or posedge reset) begin
        if (reset) begin
            mem <= 32'b0;
        end else begin
            if (slave_read) begin
                slave_readdata <= mem;
            end
            if (slave_write) begin
                mem <= slave_writedata;
            end;
        end;
    end



endmodule