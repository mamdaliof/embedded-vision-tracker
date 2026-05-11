
module soc_system (
	clk_clk,
	encoder_inputs_yaw_a,
	encoder_inputs_yaw_b,
	encoder_inputs_pitch_a,
	encoder_inputs_pitch_b,
	hps_0_h2f_reset_reset_n,
	memory_mem_a,
	memory_mem_ba,
	memory_mem_ck,
	memory_mem_ck_n,
	memory_mem_cke,
	memory_mem_cs_n,
	memory_mem_ras_n,
	memory_mem_cas_n,
	memory_mem_we_n,
	memory_mem_reset_n,
	memory_mem_dq,
	memory_mem_dqs,
	memory_mem_dqs_n,
	memory_mem_odt,
	memory_mem_dm,
	memory_oct_rzqin,
	reset_reset_n,
	pwm_outputs_ina,
	pwm_outputs_inb,
	pwm_outputs_pwm_out);	

	input		clk_clk;
	input		encoder_inputs_yaw_a;
	input		encoder_inputs_yaw_b;
	input		encoder_inputs_pitch_a;
	input		encoder_inputs_pitch_b;
	output		hps_0_h2f_reset_reset_n;
	output	[14:0]	memory_mem_a;
	output	[2:0]	memory_mem_ba;
	output		memory_mem_ck;
	output		memory_mem_ck_n;
	output		memory_mem_cke;
	output		memory_mem_cs_n;
	output		memory_mem_ras_n;
	output		memory_mem_cas_n;
	output		memory_mem_we_n;
	output		memory_mem_reset_n;
	inout	[31:0]	memory_mem_dq;
	inout	[3:0]	memory_mem_dqs;
	inout	[3:0]	memory_mem_dqs_n;
	output		memory_mem_odt;
	output	[3:0]	memory_mem_dm;
	input		memory_oct_rzqin;
	input		reset_reset_n;
	output		pwm_outputs_ina;
	output		pwm_outputs_inb;
	output		pwm_outputs_pwm_out;
endmodule
