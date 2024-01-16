#include "stream_parser.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#define ERROR_CONTEXT_SIZE 4096
#define DATA_BUFFER_SIZE 1024

// Define the internal state of the parser
typedef enum {
    STATE_FIND_HEADER,
    STATE_LENGTH,
    STATE_TYPE,
    STATE_BODY,
    STATE_CHECKSUM,
    STATE_FIND_TRAILER
} ParserState;

// Define the struct StreamParser
struct StreamParser {
    uint8_t *buffer;
    size_t buffer_size;
    size_t buffer_index;
    ParserState state;
    uint16_t packet_length;
    StreamParserErrorCallback error_callback;
    void *callback_data;
    char *error_context;
};

// Helper function to create a string context
static void string_context(StreamParser *parser) {
    if (!parser->error_callback) return;

    snprintf(parser->error_context, ERROR_CONTEXT_SIZE, "State: %d, Buffer Index: %zu, Packet Length: %u, Buffer Content: ",
             parser->state, parser->buffer_index, parser->packet_length);

    char byte_str[4];
    for (size_t i = 0; i < parser->buffer_index && i < parser->buffer_size; ++i) {
        snprintf(byte_str, sizeof(byte_str), "%02X ", parser->buffer[i]);
        strncat(parser->error_context, byte_str, ERROR_CONTEXT_SIZE - strlen(parser->error_context) - 1);
    }
}

// CRC32 function prototype (assuming it's provided)
uint32_t crc32(const uint8_t *data, size_t len);

StreamParser *open_stream_parser() {
    StreamParser *parser = malloc(sizeof(StreamParser));
    if (!parser) return NULL;

    parser->buffer = malloc(DATA_BUFFER_SIZE);
    if (!parser->buffer) {
        free(parser);
        return NULL;
    }

    parser->error_context = malloc(ERROR_CONTEXT_SIZE);
    if (!parser->error_context) {
        free(parser->buffer);
        free(parser);
        return NULL;
    }

    parser->buffer_size = DATA_BUFFER_SIZE;
    parser->buffer_index = 0;
    parser->state = STATE_FIND_HEADER;
    parser->packet_length = 0;
    parser->error_callback = NULL;
    parser->callback_data = NULL;

    return parser;
}

void close_stream_parser(StreamParser *parser) {
    if (parser) {
        free(parser->buffer);
        free(parser->error_context);
        free(parser);
    }
}

size_t push_byte(StreamParser *parser, uint8_t byte, StreamParserError *error) {
    if (!parser || !error) {
        if (error) *error = STREAM_PARSER_INVALID_ARG;
        return 0;
    }

    // Handle states
    switch (parser->state) {
        // TODO: Implement this according to this ICD:
        /*
The ICD:
Header: "/", "*"
Length bytes (index 2, 3): little endian uint16 of the entire packet length including header and trailer.
Packet type (indexes 4, 5, 6): 3 bytes
Body (index 4 until checksum): raw bytes
Checksum: CRC32 4 bytes after body (assume there's already a function available for calculating checksum).
Trailer: "*", "/"
        */
    }

    *error = STREAM_PARSER_OK;
    return 0; // Return the length of the packet if ready, or 0 if not
}

void stream_parser_error_callback(StreamParser *parser, StreamParserErrorCallback callback, void *callback_data) {
    if (parser) {
        parser->error_callback = callback;
        parser->callback_data = callback_data;
    }
}
