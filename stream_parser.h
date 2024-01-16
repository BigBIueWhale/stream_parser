#ifndef STREAM_PARSER_H
#define STREAM_PARSER_H

#include <stdint.h>
#include <stddef.h>

// Forward declaration of the opaque struct.
typedef struct StreamParser StreamParser;

// Error codes
typedef enum {
    STREAM_PARSER_OK = 0,
    STREAM_PARSER_ERROR,
    STREAM_PARSER_INVALID_ARG,
    STREAM_PARSER_OUT_OF_MEMORY,
    STREAM_PARSER_HEADER_NOT_FOUND_YET,
    STREAM_PARSER_INVALID_PACKET // Covers checksum failure as well
} StreamParserError;

// Callback function type for error reporting.
typedef void (*StreamParserErrorCallback)(StreamParserError error, const char *message, void *callback_data);

// Function to open and initialize the parser.
StreamParser *open_stream_parser(size_t buffer_size);

// Function to close and free the parser.
void close_stream_parser(StreamParser *parser);

// Function to push a byte into the parser. Returns the length of the packet if ready, or 0 if not.
size_t push_byte(StreamParser *parser, uint8_t byte, StreamParserError *error);

// Function to set an error callback for detailed error reporting.
// If provided, this function will be called with a nicely formatted string of the received byte so far, upon any error.
void stream_parser_error_callback(StreamParser *parser, StreamParserErrorCallback callback, void *callback_data);

#endif // STREAM_PARSER_H
