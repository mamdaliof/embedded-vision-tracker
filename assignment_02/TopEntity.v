module TopEntity(
	input FPGA_CLK1_50,
	output reg [7:0] LED = 0
);
reg [31:0]count = 0;
always @(posedge FPGA_CLK1_50) begin
	if(count == 99999999) begin //Time is up
		count <= 0; //Reset count register
		LED[0] <= ~LED[0]; //Toggle led (in each second)
	end else begin
		count <= count + 1; //Counts 100MHz clock
	end
end
endmodule
