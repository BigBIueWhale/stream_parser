#include "stream_parser.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#define ERROR_CONTEXT_SIZE 4096
#define GENERAL_USE_BUFFER_SIZE 4096
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
    size_t buffer_index;
    ParserState state;
    uint16_t packet_length;
    StreamParserErrorCallback error_callback;
    void *callback_data;
    char *error_context;
    char *general_use_buffer;
};

static void append_to_error_context(StreamParser *parser, const char *const string_to_append) {
    const size_t length = strlen(parser->error_context);
    if (length >= ERROR_CONTEXT_SIZE) {
        return; // No room to append error message
    }
    strncat(parser->error_context + length, string_to_append, ERROR_CONTEXT_SIZE - length - 1);
}

static void clear_error_context(StreamParser *parser, const char *const string_to_append) {
    parser->error_context[0] = '\0';
}

// Helper function to create a string context
static void string_context(StreamParser *parser) {
    if (!parser->error_callback) return;

    parser->general_use_buffer[0] = '\0';
    snprintf(parser->general_use_buffer, GENERAL_USE_BUFFER_SIZE, "State: %d, Buffer Index: %zu, Packet Length: %u, Buffer Content: ",
             parser->state, parser->buffer_index, parser->packet_length);

    append_to_error_context(parser, parser->general_use_buffer);

    for (size_t i = 0; i < parser->buffer_index && i < DATA_BUFFER_SIZE; ++i) {
        parser->general_use_buffer[0] = '\0';
        snprintf(general_use_buffer, GENERAL_USE_BUFFER_SIZE, "%02X ", parser->buffer[i]);
        append_to_error_context(parser, parser->general_use_buffer);
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

    parser->general_use_buffer = malloc(GENERAL_USE_BUFFER_SIZE); 
    if (!parser->general_use_buffer) {
        free(parser->error_context);
        free(parser->buffer);
        free(parser);
        return NULL;
    }

    parser->buffer_index = 0;
    parser->state = STATE_FIND_HEADER;
    parser->packet_length = 0;
    parser->error_callback = NULL;
    parser->callback_data = NULL;

    return parser;
}

void close_stream_parser(StreamParser *parser) {
    if (parser) {
        free(parser->general_use_buffer);
        free(parser->error_context);
        free(parser->buffer);
        free(parser);
    }
}

size_t push_byte(StreamParser *parser, uint8_t byte, StreamParserError *error) {
    if (!parser || !error) {
        if (error) *error = STREAM_PARSER_INVALID_ARG;
        if (parser->error_callback) {
            clear_error_context();
            parser->general_use_buffer[0] = '\0';
            snprintf(parser->general_use_buffer, GENERAL_USE_BUFFER_SIZE, "STREAM_PARSER_INVALID_ARG: parser: %p, error: %p, byte: %03o", parser, error, byte);
            append_to_error_context(parser, parser->general_use_buffer);
            string_context(parser);
            parser->error_callback(STREAM_PARSER_INVALID_ARG, parser->error_context, parser->callback_data)
        }

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
