module Quad_compact(
    input clk, 
    input A, input B, 
    input reset, 
    output reg signed[15:0] count);

(*ASYNC_REG="TRUE"*)reg[1:0] sync, AB; // synchronization registers
reg[1:0] os; // old state
wire[1:0] tmp, ns/*new state*/;

always @(posedge clk) // two-stage input synchronizer
    begin
        sync <= {A,B};
        AB <= sync;        
    end

// OR signal[0] with signal[1] - creates pulses on every edge of quad signal
assign tmp = {AB[1],AB[1] ^ AB[0]}; 

assign ns = tmp - os; // Get new state

always @(posedge clk)
    begin 
        if(reset) begin
            count <= 0;
            os <= 0;
        end else
            if(ns[0] == 1'b1)
                begin
                    os <= os + ns; // set current state to old state  
                    if (ns[1] == 1'b1) begin
                        count <= count - 1'b1;
                    end else begin
                        count <= count + 1'b1;
                    end 
               end
    end
endmodule