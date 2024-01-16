
# Stream Parser
Example of how to implement a stream parsing (serial, TCP, etc) parsing state machine in idiomatic C.

## Rationale
You always want to packetize. It's always a good idea to define a robust packetizing API at a layer above the raw communication that you're using.

This C library is a reference implementation of how to design an idiomatic state machine API for such a purpose. 

## Interface Control Document (ICD) Format

I chose a dummy API with this format:

The ICD format is a structured way of encoding data packets. It consists of the following components:

- **Header**: The packet begins with a header denoted by `"/", "*"`.
  
- **Length Bytes**: Located at index 2 and 3, this is a little-endian encoded `uint16` value that represents the total length of the packet, including the header and trailer.

- **Packet Type**: This field spans indexes 4, 5, and 6 and is composed of 3 bytes that identify the packet type.

- **Body**: Starting from index 7, the body contains the raw bytes of the packet's content. The length of the body varies depending on the packet.

- **Checksum**: A CRC32 checksum is calculated for the packet body. This 4-byte checksum is appended immediately after the body. It's assumed that a function for calculating the CRC32 checksum is available.

- **Trailer**: The packet ends with a trailer, indicated by `"*", "/"`.

### Notes
- The packet's `Length Bytes` include the size of the header and trailer.
- Ensure that the packet type and body are correctly formatted as per the specifications for each packet type.
- The CRC32 checksum helps in verifying the integrity of the packet's data.

## Compiling
The code is valid POSIX-compliant C11
