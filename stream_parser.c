#include "stream_parser.h"
#include "crc32.h"
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
    int64_t buffer_index;
    ParserState state;
    uint16_t packet_length;
    StreamParserErrorCallback error_callback;
    void *callback_data;
    char *error_context;
    char *general_use_buffer;
};

static void append_to_error_context(StreamParser *parser, const char *const string_to_append) {
    const int64_t length = strlen(parser->error_context);
    if (length >= ERROR_CONTEXT_SIZE) {
        return; // No room to append error message
    }
    strncat(parser->error_context + length, string_to_append, ERROR_CONTEXT_SIZE - length - 1);
}

static void clear_error_context(StreamParser *parser) {
    parser->error_context[0] = '\0';
}

// Helper function to create a string context
static void string_context(StreamParser *parser) {
    if (!parser->error_callback) return;

    parser->general_use_buffer[0] = '\0';
    snprintf(parser->general_use_buffer, GENERAL_USE_BUFFER_SIZE, "State: %d, Buffer Index: %zu, Packet Length: %u, Buffer Content: ",
             parser->state, parser->buffer_index, parser->packet_length);

    append_to_error_context(parser, parser->general_use_buffer);

    for (int64_t i = 0; i < parser->buffer_index && i < DATA_BUFFER_SIZE; ++i) {
        parser->general_use_buffer[0] = '\0';
        snprintf(parser->general_use_buffer, GENERAL_USE_BUFFER_SIZE, "%02X ", parser->buffer[i]);
        append_to_error_context(parser, parser->general_use_buffer);
    }
}

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

// header + trailer 4 bytes, length 2 bytes, message type 3 bytes, checksum 4 bytes.
#define MIN_PACKET_LENGTH 13

size_t push_byte(StreamParser *parser, uint8_t byte, StreamParserError *error) {
    if (!parser || !error) {
        if (error) *error = STREAM_PARSER_INVALID_ARG;
        if (parser->error_callback) {
            clear_error_context(parser);
            parser->general_use_buffer[0] = '\0';
            snprintf(parser->general_use_buffer, GENERAL_USE_BUFFER_SIZE, "STREAM_PARSER_INVALID_ARG: parser: %p, error: %p, byte: %03o\n", (void*)parser, (void*)error, byte);
            append_to_error_context(parser, parser->general_use_buffer);
            string_context(parser);
            parser->error_callback(STREAM_PARSER_INVALID_ARG, parser->error_context, parser->callback_data);
        }

        return 0;
    }

    // Handle states
    switch (parser->state) {
        case STATE_FIND_HEADER:
            if (parser->buffer_index == 0 && byte == '/') {
                parser->buffer[parser->buffer_index++] = byte;
            } else if (parser->buffer_index == 1 && byte == '*') {
                parser->buffer[parser->buffer_index++] = byte;
                parser->state = STATE_LENGTH;
            } else {
                *error = STREAM_PARSER_HEADER_NOT_FOUND_YET;
                if (parser->error_callback) {
                    clear_error_context(parser);
                    snprintf(parser->general_use_buffer, GENERAL_USE_BUFFER_SIZE, "STREAM_PARSER_HEADER_NOT_FOUND_YET: Expected '/' and '*', received: %03o\n", byte);
                    append_to_error_context(parser, parser->general_use_buffer);
                    string_context(parser);
                    parser->error_callback(STREAM_PARSER_HEADER_NOT_FOUND_YET, parser->error_context, parser->callback_data);
                }
                parser->buffer_index = 0; // Reset buffer index to start looking for header again
            }
            break;
        case STATE_LENGTH: {
            parser->buffer[parser->buffer_index++] = byte;
            if (parser->buffer_index == 4) { // Header + 2 length bytes
                const int64_t payload_length = ((uint32_t)parser->buffer[2]) | (((uint32_t)(parser->buffer[3])) << 8);
                parser->packet_length = payload_length + MIN_PACKET_LENGTH;
                if (parser->packet_length < MIN_PACKET_LENGTH || parser->packet_length > DATA_BUFFER_SIZE) {
                    *error = STREAM_PARSER_INVALID_PACKET;
                    if (parser->error_callback) {
                        clear_error_context(parser);
                        snprintf(parser->general_use_buffer, GENERAL_USE_BUFFER_SIZE, "STREAM_PARSER_INVALID_PACKET: Invalid packet length: %u\n", parser->packet_length);
                        append_to_error_context(parser, parser->general_use_buffer);
                        string_context(parser);
                        parser->error_callback(STREAM_PARSER_INVALID_PACKET, parser->error_context, parser->callback_data);
                    }
                    parser->buffer_index = 0; // Reset buffer index to start looking for header again
                    parser->state = STATE_FIND_HEADER;
                } else {
                    parser->state = STATE_TYPE;
                }
            }
            break;
        }
        case STATE_TYPE:
            parser->buffer[parser->buffer_index++] = byte;
            if (parser->buffer_index == 7) { // Header + 2 length bytes + 3 type bytes
                // Type bytes are successfully captured.
                if (parser->packet_length <= MIN_PACKET_LENGTH) {
                    // Stop the body state from stealing one byte in the case
                    // of a packet that has an empty body.
                    parser->state = STATE_CHECKSUM;
                } else {
                    parser->state = STATE_BODY;
                }
            }
            break;
        case STATE_BODY:
            parser->buffer[parser->buffer_index++] = byte;
            // Calculate the expected end of the body, taking into account header, length, type, checksum, and trailer bytes
            if (parser->buffer_index == parser->packet_length - (4 + 2)) { // Subtract 4 bytes for checksum and 2 bytes for trailer
                // The body is now complete. Transition to STATE_CHECKSUM.
                parser->state = STATE_CHECKSUM;
            }
            break;
        case STATE_CHECKSUM: {
            parser->buffer[parser->buffer_index++] = byte;
            if (parser->buffer_index == parser->packet_length - 2) { // Reached end of checksum, 2 bytes left for trailer
                CRC32_State hash_engine = crc32_create_engine();
                // The checksum is of the entire messsage including the header and trailer
                // except for the hash itself which isn't included in the calculation.
                crc32_update(&hash_engine, parser->buffer, parser->packet_length - 6); // Everything before checksum
                // Need to manually fill in the trailer bytes for this CRC32 calculation
                // because we haven't collected the trailer bytes yet.
                // Don't worry- we'll also verify the trailer bytes in the next state.
                static const uint8_t trailer_bytes[] = { '*', '/' };
                crc32_update(&hash_engine, trailer_bytes, 2);
                const uint32_t calculated_checksum = crc32_finalize(&hash_engine);
                const uint32_t received_checksum = ((uint32_t)parser->buffer[parser->packet_length - 6]) |
                                             ((uint32_t)parser->buffer[parser->packet_length - 5] << 8) |
                                             ((uint32_t)parser->buffer[parser->packet_length - 4] << 16) |
                                             ((uint32_t)parser->buffer[parser->packet_length - 3] << 24);

                if (calculated_checksum != received_checksum) {
                    *error = STREAM_PARSER_INVALID_PACKET;
                    if (parser->error_callback) {
                        clear_error_context(parser);
                        snprintf(parser->general_use_buffer, GENERAL_USE_BUFFER_SIZE, "STREAM_PARSER_INVALID_PACKET: Checksum mismatch. Expected: %08X, Received: %08X\n", calculated_checksum, received_checksum);
                        append_to_error_context(parser, parser->general_use_buffer);
                        string_context(parser);
                        parser->error_callback(STREAM_PARSER_INVALID_PACKET, parser->error_context, parser->callback_data);
                    }
                    parser->buffer_index = 0;
                    parser->state = STATE_FIND_HEADER;
                    return 0;
                } else {
                    // Checksum is valid. Transition to STATE_FIND_TRAILER.
                    parser->state = STATE_FIND_TRAILER;
                }
            }
            break;
        }
        case STATE_FIND_TRAILER:
            parser->buffer[parser->buffer_index++] = byte;
            if (parser->buffer_index == parser->packet_length - 1 && byte == '*') {
                // First trailer byte received, wait for the second
            } else if (parser->buffer_index == parser->packet_length && byte == '/') {
                // Complete packet received, including trailer
                parser->buffer_index = 0; // Reset buffer index for the next packet
                parser->state = STATE_FIND_HEADER; // Reset state to start looking for the next header
                *error = STREAM_PARSER_OK;
                return parser->packet_length; // Return the length of the complete packet
            } else {
                // Trailer not found or incorrect trailer sequence
                *error = STREAM_PARSER_INVALID_PACKET;
                if (parser->error_callback) {
                    clear_error_context(parser);
                    snprintf(parser->general_use_buffer, GENERAL_USE_BUFFER_SIZE, "STREAM_PARSER_INVALID_PACKET: Incorrect trailer sequence or incomplete packet\n");
                    append_to_error_context(parser, parser->general_use_buffer);
                    string_context(parser);
                    parser->error_callback(STREAM_PARSER_INVALID_PACKET, parser->error_context, parser->callback_data);
                }
                parser->buffer_index = 0; // Reset buffer index to start looking for header again
                parser->state = STATE_FIND_HEADER; // Reset state to start looking for the next header
            }
            break;
        default:
            *error = STREAM_PARSER_INTERNAL_ERROR;
            if (parser->error_callback) {
                clear_error_context(parser);
                snprintf(parser->general_use_buffer, GENERAL_USE_BUFFER_SIZE, "STREAM_PARSER_INTERNAL_ERROR: Unknown state\n");
                append_to_error_context(parser, parser->general_use_buffer);
                string_context(parser);
                parser->error_callback(STREAM_PARSER_INVALID_PACKET, parser->error_context, parser->callback_data);
            }
            parser->buffer_index = 0; // Reset buffer index to start looking for header again
            parser->state = STATE_FIND_HEADER; // Reset state to start looking for the next header
            
            return 0;
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
