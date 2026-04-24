module quadrature_encoder (
    input clk,             // System clock
    input enc_a,           // Generic Channel A
    input enc_b,           // Generic Channel B
    output reg [31:0] position =0 // 32-bit output for the count
);

    reg prev_a;

    always @(posedge clk) begin
        // 1x Counting: Only check for the rising edge of Channel A
        if (enc_a == 1'b1 && prev_a == 1'b0) begin
            
            // Check Channel B to determine direction
            if (enc_b == 1'b0) begin
                position <= position + 1;
            end else begin
                position <= position - 1;
            end
            
        end
        
        // Remember the state of A for the next clock cycle
        prev_a <= enc_a;
    end

endmodule