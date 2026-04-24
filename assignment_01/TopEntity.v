module TopEntity(
    input clk,
    output reg led1 = 0
);

reg[31:0] count = 0;

always @(posedge clk) begin
    // Note: The operand below is changed to 5 for DigitalJS simulation
    if (count >= 99999999) begin   // Time is up
        count <= 0;         // Reset count register
        led1 <= ~led1;      // Toggle led
    end else begin
        count <= count + 1; // Counts clock cycles
    end
end

endmodule