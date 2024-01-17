#ifndef STREAM_PARSER_H
#define STREAM_PARSER_H

#include <stdint.h>
#include <stddef.h>

// Forward declaration of the opaque struct.
typedef struct StreamParser StreamParser;

// Error codes
typedef enum {
    STREAM_PARSER_OK = 0,
    STREAM_PARSER_INTERNAL_ERROR,
    STREAM_PARSER_INVALID_ARG,
    STREAM_PARSER_HEADER_NOT_FOUND_YET,
    STREAM_PARSER_INVALID_PACKET // Covers checksum failure as well
} StreamParserError;

// Callback function type for error reporting.
typedef void (*StreamParserErrorCallback)(const StreamParserError error, const char *message, void *const error_callback_data);

// Callback function for any collected packet
typedef void (*StreamParserPacketCallback)(const uint8_t *const packet_buffer, int64_t packet_size, void *const packet_callback_data);

// Function to open and initialize the parser.
extern StreamParser *stream_parser_open();

// Function to close and free the parser.
extern void stream_parser_close(StreamParser *parser);

// Function to push a byte into the parser state machine
// Upon error- calls the error callback if initialized
// Upon collecting entire packet- calls the packet_callback if initialized
extern StreamParserError stream_parser_push_byte(StreamParser *parser, uint8_t byte);


// Register error callback function for detailed error reporting.
// If called twice- replaces previous callback.
// If called with (parser, NULL, NULL), removes callback.
// If provided, this function will be called with a nicely formatted string describing
// an error code before the error code is returned.

// The error string that you'll be called with is not persistent (same buffer reused for next time)
// and you don't own it.
extern void stream_parser_register_error_callback(StreamParser *parser, StreamParserErrorCallback callback, void *error_callback_data);


// Register packet callback function for collecting completed packets from the state machine.
// If called twice- replaces previous callback.
// If called with (parser, NULL, NULL), removes callback.
// If provided, this function will be called with a buffer and length that is a single packet
// with a valid header, trailer, length, and checksum.

// The packet buffer that you'll be called with is not persistent (same buffer reused for next time),
// and you don't own it.
extern void stream_parser_register_packet_callback(StreamParser *parser, StreamParserPacketCallback callback, void *packet_callback_data);

#endif // STREAM_PARSER_H
