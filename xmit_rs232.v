module xmit_rs232(rst, clk, tx, data, data_valid, data_ready);
  input rst;
  input clk;
  output tx;
  input[7:0] data;
  input data_valid;
  output reg data_ready;

  parameter bps=115200;
  parameter parity=0; // 0=odd, 1=even, -1=none
  parameter fclk=26000000;

  // derived parameters:
  parameter bitperiod=fclk/bps;
  parameter has_parity=parity!=-1;

  reg[3:0] bitno; /* 0=idle, 1=start, 2-8=bits, 10=parity, 11=stop */
  reg[$clog2(bitperiod):0] baudclk;

  reg[10:0] shreg;
  reg p;

  assign tx = bitno == 10 ? !p : !shreg[0];

  task dataready;
    begin
      data_ready <= bitno==0;// && !data_valid;
    end
  endtask
  
  task baudrate;
    begin
      if (bitno == 0)
	baudclk <= 0;
      else if (baudclk < bitperiod-1)
	baudclk <= baudclk + 1;
      else
	baudclk <= 0;
    end
  endtask

  task cnt_bitno;
    begin
      if (bitno==0 && data_valid)
	     bitno <= 1;
      else if (baudclk >= bitperiod-1) begin
	if (bitno < 10)
		bitno <= bitno + 1;
	else if (bitno == 10 && has_parity)
		     bitno <= bitno + 1;
	else if (bitno == 10 && !has_parity)
		     bitno <= 0;
	else
	     bitno <= 0;
      end
    end
  endtask

  task shiftdata;
    begin
      if (bitno==0 && data_valid)
	shreg <= {2'b00, ~data, 1'b1};
      else if (baudclk >= bitperiod-1)
	shreg <= {1'b0, shreg[10:1]};
    end
  endtask

  task calcparity;
    begin
      if (bitno == 0)
	p <= parity;
      else if (baudclk == 0 && bitno != 10 && bitno != 1)
	p <= p ^ shreg[0];
    end
  endtask

  task reset;
    begin
      bitno <= 0;
      baudclk <= 0;
      shreg <= 0;
      p <= 0;
      data_ready <= 0;
    end
  endtask

  initial reset;

  always @(posedge clk) begin
    if (rst)
      reset;
    else begin
      baudrate;
      cnt_bitno;
      shiftdata;
      calcparity;
      dataready;
    end
  end

endmodule
