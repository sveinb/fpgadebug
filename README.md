# fpgadebug
IP core + PC program to transfer signals from an FPGA via serial line to a vcd file to view and debug.

Works like a logic analyzer. Doesn't support triggers, but includes run-length encoding, so useful for signals that
only change in short bursts.

# overview
The IP core consists of three verilog modules:

* logchange
* xmit_rs232

The logchange module instantiates a ram module which acts as the buffer for the logged signals. The logchange outputs
8 bit parallel data. These can be connected to the xmit_rs232 module to serialize the data on a serial output pin, which
in turn can be connected to an rs232 port on a pc.

The data is transmitted in binary form over the serial line. The readdump program opens and configures the serial port and
converts the data to vcd or human-readable ascii. The vcd file can be viewed with gtkwave or another vcd viewer.
