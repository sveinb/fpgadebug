module ram (
		WrAddress,
		RdAddress,
		Data,
		WE,
		RdClock,
		RdClockEn,
		Reset,
		WrClock,
		WrClockEn,
		Q
	);
	/* synthesis syn_ramstyle=block_ram */
 
	parameter adrbits = 12;
	parameter databits = 16;
 
	input[adrbits-1:0] WrAddress, RdAddress;
	input[databits-1:0] Data;
	output [databits-1:0] Q;
	input WE;
	input RdClock, RdClockEn;
	input Reset;
	input WrClock, WrClockEn;

 	reg [databits-1:0] mem [0:(1<<adrbits)-1];

	// Memory Write Block 
	// Write Operation : When we = 1, cs = 1
	always @ (posedge WrClock) begin
		if ( WrClockEn && WE ) begin
       		mem[WrAddress] <= Data;
	   	end
	end

	// Memory Read Block 
	// Read Operation : When we = 0, oe = 1, cs = 1
/*	always @ (posedge RdClock) begin
	  if (RdClockEn && !WE) begin
	    Q <= mem[RdAddress];
	  end
	end
*/
	assign Q = mem[RdAddress];

endmodule
