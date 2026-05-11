`timescale 1 ps / 1 ps
module esl_bus_demo_example #(
        parameter DATA_WIDTH = 8
    ) (
        input wire clk,
        input wire rst,
        input wire [DATA_WIDTH-1:0] in,
        input wire cnt_enable,
        output wire [DATA_WIDTH-1:0] out
    );
    localparam residual = 17108864; 

    reg [25:0] counter;
    reg counter_overflow;
    reg [DATA_WIDTH-1:0] count_down_input;
    reg [DATA_WIDTH-1:0] count_down;

    always @(posedge clk or posedge rst) begin 
        if (rst) begin
            counter <= residual;
            counter_overflow <= 1'b0;
        end else begin
            if (cnt_enable) begin 
                if (counter == 0) begin
                    counter_overflow <= 1'b1;
                    counter <= residual;
                end else begin
                    counter <= counter + 1;
                    counter_overflow <= 1'b0;
                end
            end
        end
    end

    always @(posedge clk or posedge rst) begin
        if (rst) begin
            count_down <= {DATA_WIDTH{1'b1}};
            count_down_input <= {DATA_WIDTH{1'b0}};
        end else begin
            if (cnt_enable) begin
                // Check for new input
                if (in != count_down_input) begin
                    count_down_input <= in;
                    count_down <= in;
                end
                
                // Check if the count down has reached zero
                if (count_down == 0) begin
                    count_down <= count_down_input;
                end
                
                // Enable count down only if the counter is reset.
                if (counter_overflow) begin
                    count_down <= count_down - 1;
                end
            end
        end
    end

    // Route the counter directly to the output
    assign out = count_down;

endmodule
