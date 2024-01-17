#include "stream_parser.h"
#include "crc32.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#define ERROR_CONTEXT_SIZE 512
#define GENERAL_USE_BUFFER_SIZE 512
#define DATA_BUFFER_SIZE 64 // Max packet size according to our ICD is 49

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
    uint8_t *packet_buffer;
    int64_t packet_buffer_index;
    ParserState state;
    uint16_t packet_length;

    StreamParserErrorCallback error_callback;
    void *error_callback_data;

    StreamParserPacketCallback packet_callback;
    void *packet_callback_data;

    char *error_context;
    char *general_use_buffer;
};

static void append_to_error_context(StreamParser *const parser, const char *const string_to_append) {
    const int64_t length = strlen(parser->error_context);
    if (length >= ERROR_CONTEXT_SIZE) {
        return; // No room to append error message
    }
    strncat(parser->error_context + length, string_to_append, ERROR_CONTEXT_SIZE - length - 1);
}

static void clear_error_context(StreamParser *const parser) {
    parser->error_context[0] = '\0';
}

// Helper function to create a string context
static void string_context(StreamParser *const parser) {
    if (!parser->error_callback) return;

    parser->general_use_buffer[0] = '\0';
    snprintf(parser->general_use_buffer, GENERAL_USE_BUFFER_SIZE, "State: %d, Buffer Index: %zu, Packet Length: %u, Buffer Content: ",
             parser->state, parser->packet_buffer_index, parser->packet_length);

    append_to_error_context(parser, parser->general_use_buffer);

    for (int64_t i = 0; i < parser->packet_buffer_index && i < DATA_BUFFER_SIZE; ++i) {
        parser->general_use_buffer[0] = '\0';
        snprintf(parser->general_use_buffer, GENERAL_USE_BUFFER_SIZE, "0x%02X ", parser->packet_buffer[i]);
        append_to_error_context(parser, parser->general_use_buffer);
    }
}

static void reset_state(StreamParser *const parser) {
    parser->packet_buffer_index = 0;
    parser->state = STATE_FIND_HEADER;
    parser->packet_length = 0;

    // For runtime determinism
    memset(parser->packet_buffer, 0, DATA_BUFFER_SIZE);
    memset(parser->error_context, 0, ERROR_CONTEXT_SIZE);
    memset(parser->general_use_buffer, 0, GENERAL_USE_BUFFER_SIZE);
}

StreamParser *stream_parser_open() {
    StreamParser *parser = (StreamParser*)calloc(1, sizeof(StreamParser));
    if (!parser) return NULL;

    parser->packet_buffer = (uint8_t*)calloc(DATA_BUFFER_SIZE, sizeof(uint8_t));
    if (!parser->packet_buffer) {
        free(parser);
        return NULL;
    }

    parser->error_context = (char*)calloc(ERROR_CONTEXT_SIZE, sizeof(char));
    if (!parser->error_context) {
        free(parser->packet_buffer);
        free(parser);
        return NULL;
    }

    parser->general_use_buffer = (char*)calloc(GENERAL_USE_BUFFER_SIZE, sizeof(char)); 
    if (!parser->general_use_buffer) {
        free(parser->error_context);
        free(parser->packet_buffer);
        free(parser);
        return NULL;
    }

    reset_state(parser);
    parser->error_callback = NULL;
    parser->error_callback_data = NULL;
    parser->packet_callback_data = NULL;

    return parser;
}

void stream_parser_close(StreamParser *parser) {
    if (parser) {
        free(parser->general_use_buffer);
        free(parser->error_context);
        free(parser->packet_buffer);
        free(parser);
    }
}

// header + trailer 4 bytes, length 2 bytes, message type 3 bytes, checksum 4 bytes.
#define MIN_PACKET_LENGTH 13

StreamParserError stream_parser_push_byte(StreamParser *const parser, const uint8_t byte) {
    if (!parser) {
        if (parser->error_callback) {
            clear_error_context(parser);
            parser->general_use_buffer[0] = '\0';
            snprintf(parser->general_use_buffer, GENERAL_USE_BUFFER_SIZE, "STREAM_PARSER_INVALID_ARG: parser: %p, byte: %03o\n", (void*)parser, byte);
            append_to_error_context(parser, parser->general_use_buffer);
            string_context(parser);
            parser->error_callback(STREAM_PARSER_INVALID_ARG, parser->error_context, parser->error_callback_data);
        }

        return STREAM_PARSER_INVALID_ARG;
    }

    StreamParserError err_ret = STREAM_PARSER_OK;

    // Handle states
    switch (parser->state) {
        case STATE_FIND_HEADER:
            if (parser->packet_buffer_index == 0 && byte == '/') {
                parser->packet_buffer[parser->packet_buffer_index++] = byte;
            } else if (parser->packet_buffer_index == 1 && byte == '*') {
                parser->packet_buffer[parser->packet_buffer_index++] = byte;
                parser->state = STATE_LENGTH;
            } else {
                err_ret = STREAM_PARSER_HEADER_NOT_FOUND_YET;
                if (parser->error_callback) {
                    clear_error_context(parser);
                    snprintf(parser->general_use_buffer, GENERAL_USE_BUFFER_SIZE, "STREAM_PARSER_HEADER_NOT_FOUND_YET: Expected '/' and '*', received: %03o\n", byte);
                    append_to_error_context(parser, parser->general_use_buffer);
                    string_context(parser);
                    parser->error_callback(STREAM_PARSER_HEADER_NOT_FOUND_YET, parser->error_context, parser->error_callback_data);
                }
                parser->packet_buffer_index = 0; // Reset buffer index to start looking for header again
            }
            break;
        case STATE_LENGTH: {
            parser->packet_buffer[parser->packet_buffer_index++] = byte;
            if (parser->packet_buffer_index == 4) { // Header + 2 length bytes
                const int64_t payload_length = ((uint32_t)parser->packet_buffer[2]) | (((uint32_t)(parser->packet_buffer[3])) << 8);
                parser->packet_length = payload_length + MIN_PACKET_LENGTH;
                if (parser->packet_length < MIN_PACKET_LENGTH || parser->packet_length > DATA_BUFFER_SIZE) {
                    err_ret = STREAM_PARSER_INVALID_PACKET;
                    if (parser->error_callback) {
                        clear_error_context(parser);
                        snprintf(parser->general_use_buffer, GENERAL_USE_BUFFER_SIZE, "STREAM_PARSER_INVALID_PACKET: Invalid packet length: %u\n", parser->packet_length);
                        append_to_error_context(parser, parser->general_use_buffer);
                        string_context(parser);
                        parser->error_callback(STREAM_PARSER_INVALID_PACKET, parser->error_context, parser->error_callback_data);
                    }
                    reset_state(parser);
                } else {
                    parser->state = STATE_TYPE;
                }
            }
            break;
        }
        case STATE_TYPE:
            parser->packet_buffer[parser->packet_buffer_index++] = byte;
            if (parser->packet_buffer_index == 7) { // Header + 2 length bytes + 3 type bytes
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
            parser->packet_buffer[parser->packet_buffer_index++] = byte;
            // Calculate the expected end of the body, taking into account header, length, type, checksum, and trailer bytes
            if (parser->packet_buffer_index == parser->packet_length - (4 + 2)) { // Subtract 4 bytes for checksum and 2 bytes for trailer
                // The body is now complete. Transition to STATE_CHECKSUM.
                parser->state = STATE_CHECKSUM;
            }
            break;
        case STATE_CHECKSUM: {
            parser->packet_buffer[parser->packet_buffer_index++] = byte;
            if (parser->packet_buffer_index == parser->packet_length - 2) { // Reached end of checksum, 2 bytes left for trailer
                CRC32_State hash_engine = crc32_create_engine();
                // The checksum is of the entire messsage including the header and trailer
                // except for the hash itself which isn't included in the calculation.
                crc32_update(&hash_engine, parser->packet_buffer, parser->packet_length - 6); // Everything before checksum
                // Need to manually fill in the trailer bytes for this CRC32 calculation
                // because we haven't collected the trailer bytes yet.
                // Don't worry- we'll also verify the trailer bytes in the next state.
                static const uint8_t trailer_bytes[] = { '*', '/' };
                crc32_update(&hash_engine, trailer_bytes, 2);
                const uint32_t calculated_checksum = crc32_finalize(&hash_engine);
                const uint32_t received_checksum = ((uint32_t)parser->packet_buffer[parser->packet_length - 6]) |
                                             ((uint32_t)parser->packet_buffer[parser->packet_length - 5] << 8) |
                                             ((uint32_t)parser->packet_buffer[parser->packet_length - 4] << 16) |
                                             ((uint32_t)parser->packet_buffer[parser->packet_length - 3] << 24);

                if (calculated_checksum != received_checksum) {
                    err_ret = STREAM_PARSER_INVALID_PACKET;
                    if (parser->error_callback) {
                        clear_error_context(parser);
                        snprintf(parser->general_use_buffer, GENERAL_USE_BUFFER_SIZE, "STREAM_PARSER_INVALID_PACKET: Checksum mismatch. Expected: %08X, Received: %08X\n", calculated_checksum, received_checksum);
                        append_to_error_context(parser, parser->general_use_buffer);
                        string_context(parser);
                        parser->error_callback(STREAM_PARSER_INVALID_PACKET, parser->error_context, parser->error_callback_data);
                    }
                    reset_state(parser);
                } else {
                    // Checksum is valid. Transition to STATE_FIND_TRAILER.
                    parser->state = STATE_FIND_TRAILER;
                }
            }
            break;
        }
        case STATE_FIND_TRAILER:
            parser->packet_buffer[parser->packet_buffer_index++] = byte;
            if (parser->packet_buffer_index == parser->packet_length - 1 && byte == '*') {
                // First trailer byte received, wait for the second
            } else if (parser->packet_buffer_index == parser->packet_length && byte == '/') {
                // Complete packet received, including trailer
                
                // $$$$$$$$$$$ Behold! The most important line of code $$$$$$$$$$$$$$$$$
                parser->packet_callback(parser->packet_buffer, parser->packet_length, parser->packet_callback_data);

                reset_state(parser);
            } else {
                // Trailer not found or incorrect trailer sequence
                err_ret = STREAM_PARSER_INVALID_PACKET;
                if (parser->error_callback) {
                    clear_error_context(parser);
                    snprintf(parser->general_use_buffer, GENERAL_USE_BUFFER_SIZE, "STREAM_PARSER_INVALID_PACKET: Incorrect trailer sequence or incomplete packet\n");
                    append_to_error_context(parser, parser->general_use_buffer);
                    string_context(parser);
                    parser->error_callback(STREAM_PARSER_INVALID_PACKET, parser->error_context, parser->error_callback_data);
                }
                reset_state(parser);
            }
            break;
        default:
            err_ret = STREAM_PARSER_INTERNAL_ERROR;
            if (parser->error_callback) {
                clear_error_context(parser);
                snprintf(parser->general_use_buffer, GENERAL_USE_BUFFER_SIZE, "STREAM_PARSER_INTERNAL_ERROR: Unknown state\n");
                append_to_error_context(parser, parser->general_use_buffer);
                string_context(parser);
                parser->error_callback(STREAM_PARSER_INVALID_PACKET, parser->error_context, parser->error_callback_data);
            }
            reset_state(parser);
    }

    return err_ret;
}

void stream_parser_register_error_callback(StreamParser *const parser, const StreamParserErrorCallback callback, void *const error_callback_data) {
    if (parser) {
        parser->error_callback = callback;
        parser->error_callback_data = error_callback_data;
    }
}

void stream_parser_register_packet_callback(StreamParser *const parser, const StreamParserPacketCallback callback, void *const packet_callback_data) {
    if (parser) {
        parser->packet_callback = callback;
        parser->packet_callback_data = packet_callback_data;
    }
}
