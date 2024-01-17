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
typedef void (*StreamParserErrorCallback)(StreamParserError error, const char *message, void *callback_data);

// Function to open and initialize the parser.
extern StreamParser *stream_parser_open();

// Function to close and free the parser.
extern void stream_parser_close(StreamParser *parser);

// Function to push a byte into the parser. Returns the length of the packet if ready, or 0 if not.
extern size_t stream_parser_push_byte(StreamParser *parser, uint8_t byte, StreamParserError *error);

// Register error callback function for detailed error reporting.
// If provided, this function will be called with a nicely formatted string describing
// an error code before it's returned.
extern void stream_parser_error_callback(StreamParser *parser, StreamParserErrorCallback callback, void *callback_data);

#endif // STREAM_PARSER_H
