module logchange (
		clk,
		rst,
		sig,
		data_valid,
		data,
		next
	);

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
	wire RdClock = clk;
	wire RdClockEn = 1;
	wire Reset = 0;
	wire WrClock = clk;
	wire WrClockEn = 1;
	wire [nsig:0] RdData;

	wire [adrbits-1:0] WrAddress_p_3 = WrAddress + 3;

	ram #(
		.databits(nsig+1),
		.adrbits(adrbits)
	) ram_ (
		.WrAddress(WrAddress),
		.RdAddress(RdAddress),
		.Data(WrData),
		.WE(WE),
		.RdClock(RdClock),
		.RdClockEn(RdClockEn),
		.Reset(Reset),
		.WrClock(WrClock),
		.WrClockEn(WrClockEn),
		.Q(RdData)
	);

	reg[nsig-1:0] timestamp;

	parameter
		idle = 0,
		overflow = 1,
		record = 2,
		stop = 3;


	reg[nsig-1:0] sig_1;

	reg sig_diff1;
	wire sig_diff0 = sig_1 != sig;

	reg[3:0] state;
	reg[3:0] byteno;

	task reset;
		begin
			state <= idle;
			byteno <= 0;
			data_valid <= 0;

			WrAddress <= 0;
			RdAddress <= 0;
			WE <= 0;
			timestamp <= 0;
			sig_diff1 <= 1'b0;
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


			case (state)
				idle:
					if (sig_diff0 || timestamp == max_timestamp) begin
						state <= record;
						WE <= 1;
						WrData <= {1'b1, timestamp};
					end
				record: begin
					WrAddress <= WrAddress + 1;
					WrData <= {1'b0, sig_1};
					if (!sig_diff0 && !sig_diff1) begin
						timestamp <= 0;
						state <= idle;
						WE <= 0;
					end					
				end
				
				overflow: begin
					WrAddress <= WrAddress + 1;
					WE <= 1;
					WrData <= {nsig+1{1'b1}};
					state <= stop;
				end
				
				stop: begin
					WE <= 0;
				end
				
			endcase
			if (WrAddress_p_3 == RdAddress && state != stop) begin
				state <= overflow;
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
