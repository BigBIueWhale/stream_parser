
# Stream Parser
Example of how to implement a stream parsing (serial, TCP, etc) parsing state machine in idiomatic C.

## Rationale
You always want to packetize. It's always a good idea to define a robust packetizing API at a layer above the raw communication that you're using.

This C library is a reference implementation of how to design an idiomatic state machine API for such a purpose. 

## Interface Control Document (ICD) Format

I chose a dummy API with this format:

The ICD format is a structured way of encoding data packets. It consists of the following components:

- **Header**: The packet begins with a header denoted by `"/", "*"`.
  
- **Length Bytes**: Located at index 2 and 3, this is a little-endian encoded `uint16` value that represents the length of the payload (only the payload itself, not including headers, trailer, length bytes, message type, and checksum).

- **Packet Type**: This field spans indexes 4, 5, and 6 and is composed of 3 bytes that identify the packet type.

- **Body**: Starting from index 7, the body contains the raw bytes of the packet's content. The length of the body varies depending on the packet.

- **Checksum**: A CRC32 checksum is calculated for the packet body. This 4-byte checksum is appended immediately after the body.

- **Trailer**: The packet ends with a trailer, indicated by `"*", "/"`.

## Compiling
Compile with `make` command on a GNU / Linux system.

## Example usage
After compiling, you can start listening and parsing packets with a USB to serial port hardware device.

```terminal
user@pop-os:~/Desktop/stream_parser$ dmesg | grep tty
dmesg: read kernel buffer failed: Operation not permitted
user@pop-os:~/Desktop/stream_parser$ sudo dmesg | grep tty
[    1.369361] printk: console [tty0] enabled
[    2.556569] 00:05: ttyS0 at I/O 0x3f8 (irq = 4, base_baud = 115200) is a 16550A
[ 3668.358955] usb 3-2: FTDI USB Serial Device converter now attached to ttyUSB0
[ 3668.364066] usb 3-2: FTDI USB Serial Device converter now attached to ttyUSB1
[ 4017.386382] ftdi_sio ttyUSB0: FTDI USB Serial Device converter now disconnected from ttyUSB0
[ 4017.387617] ftdi_sio ttyUSB1: FTDI USB Serial Device converter now disconnected from ttyUSB1
[ 4133.859300] usb 3-2: FTDI USB Serial Device converter now attached to ttyUSB0
[ 4133.868296] usb 3-2: FTDI USB Serial Device converter now attached to ttyUSB1
user@pop-os:~/Desktop/stream_parser$ sudo ./stream_parser
Enter the serial port to listen on (e.g., /dev/ttyS0): ttyUSB0              
Error opening port: No such file or directory
user@pop-os:~/Desktop/stream_parser$ sudo ./stream_parser
Enter the serial port to listen on (e.g., /dev/ttyS0): /dev/ttyUSB0
Error [4]: STREAM_PARSER_INVALID_PACKET: Invalid packet length: 0
State: 1, Buffer Index: 4, Packet Length: 0, Buffer Content: 2F 2A 00 00 
Error [3]: STREAM_PARSER_HEADER_NOT_FOUND_YET: Expected '/' and '*', received: 122
State: 0, Buffer Index: 0, Packet Length: 0, Buffer Content: 
Error [3]: STREAM_PARSER_HEADER_NOT_FOUND_YET: Expected '/' and '*', received: 107
State: 0, Buffer Index: 0, Packet Length: 0, Buffer Content: 
Error [3]: STREAM_PARSER_HEADER_NOT_FOUND_YET: Expected '/' and '*', received: 060
State: 0, Buffer Index: 0, Packet Length: 0, Buffer Content: 
Error [3]: STREAM_PARSER_HEADER_NOT_FOUND_YET: Expected '/' and '*', received: 214
State: 0, Buffer Index: 0, Packet Length: 0, Buffer Content: 
Error [3]: STREAM_PARSER_HEADER_NOT_FOUND_YET: Expected '/' and '*', received: 317
State: 0, Buffer Index: 0, Packet Length: 0, Buffer Content: 
Error [3]: STREAM_PARSER_HEADER_NOT_FOUND_YET: Expected '/' and '*', received: 066
State: 0, Buffer Index: 0, Packet Length: 0, Buffer Content: 
Error [3]: STREAM_PARSER_HEADER_NOT_FOUND_YET: Expected '/' and '*', received: 167
State: 0, Buffer Index: 0, Packet Length: 0, Buffer Content: 
Error [3]: STREAM_PARSER_HEADER_NOT_FOUND_YET: Expected '/' and '*', received: 052
State: 0, Buffer Index: 0, Packet Length: 0, Buffer Content: 
```
