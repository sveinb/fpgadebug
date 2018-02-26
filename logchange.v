module logchange (
		clk,
		rst,
		sig,
		data_valid,
		data,
		next
	);
	/* synthesis syn_ramstyle=block_ram */

	parameter nsig = 12;
//	parameter timebits = 4*nsig;
	parameter adrbits = 12;

	input clk;
	input rst;
	input [nsig-1:0] sig;
	output reg data_valid;
	output [7:0] data;
	input next;

	reg[nsig:0] dataReg;
	assign data = dataReg[7:0];

	reg [adrbits-1:0] WrAddress;
	reg [adrbits-1:0] RdAddress;
	reg [nsig:0] WrData;
	reg WE;
	wire [nsig:0] RdData;

	wire [adrbits-1:0] WrAddress_p_4 = WrAddress + 4;

 	reg [nsig+1-1:0] mem [0:(1<<adrbits)-1];

	// Memory Write Block 
	// Write Operation : When we = 1, cs = 1
	always @ (posedge clk) begin
		if (WE) begin
       		mem[WrAddress] <= WrData;
	   	end
	end

	assign RdData = mem[RdAddress];
	

	reg[nsig-1:0] timestamp;

	parameter
		idle = 0,
		record = 1,
		overflow1 = 2,
		overflow2 = 3,
		stop = 4;


	reg[nsig-1:0] sig_1;

	reg sig_diff1;
	wire sig_diff0 = sig_1 != sig;

	reg[3:0] state;
	reg[3:0] byteno;

	task reset;
		begin
			state <= record;
			byteno <= 0;
			data_valid <= 0;

			WrAddress <= 0;
			RdAddress <= 0;
			WE <= 1;
			timestamp <= 0;
			sig_diff1 <= 1'b1;
			sig_1 <= {nsig{1'b0}};
		end
	endtask
			
	initial reset;

    parameter max_timestamp = {nsig{1'b1}}-1;

	always @(posedge clk) begin
		if (rst)
			reset;
		else begin
			sig_1 <= sig;

			sig_diff1 <= sig_diff0;

			timestamp <= timestamp + 1;

			if (next && data_valid)
				data_valid <= 0;

			if (WE)
				WrAddress <= WrAddress + 1;

			case (state)
				idle:
					if (sig_diff0 || timestamp == max_timestamp) begin
						state <= record;
						WE <= 1;
						WrData <= {1'b1, timestamp};
					end
				record: begin
					WrData <= {1'b0, sig_1};
					if (!sig_diff0 && !sig_diff1) begin
						timestamp <= 0;
						state <= idle;
						WE <= 0;
					end					
				end
				
				overflow1: begin
					WE <= 1;
					WrData <= {nsig+1{1'b1}};
					state <= overflow2;
				end
				
				overflow2: begin
					WE <= 1;
					WrData <= 0;
					state <= stop;
				end
				
				stop: begin
					WE <= 0;
				end
				
			endcase

			if (WrAddress_p_4 == RdAddress && state != stop) begin
				state <= overflow1;
			end



			if (RdAddress != WrAddress && !data_valid) begin
				if (byteno==0)
					dataReg <= RdData;
				else
					dataReg <= dataReg >> 8;

				if (byteno * 8 + 8 < nsig + 1)
					byteno <= byteno + 1;
				else begin
					RdAddress <= RdAddress + 1;
					byteno <= 0;
				end
				data_valid <= 1;
			end

		end
	end
	
endmodule
