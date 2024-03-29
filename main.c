#include "stream_parser.h"
#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <termios.h>
#include <unistd.h>
#include <signal.h>
#include <string.h>
#include <errno.h>

volatile sig_atomic_t keep_running = 1;

static void int_handler(const int dummy) {
    (void)dummy;
    keep_running = 0;
}

static void error_callback(const StreamParserError error, const char *message, void *const error_callback_data) {
    (void)error_callback_data; // Unused parameter
    printf("Error [%d]: %s\n", error, message);
    fflush(stdout);
}

static void packet_callback(const uint8_t *const packet_buffer, int64_t packet_size, void *const packet_callback_data) {
    (void)packet_callback_data; // Unused parameter

    printf("Received packet with length %lld bytes and contents: [", (long long)packet_size);
    for (int64_t i = 0; i < packet_size; ++i) {
        printf(" 0x%02x", packet_buffer[i]);
        if (i < packet_size - 1) {
            printf(",");
        }
    }
    printf(" ]\n");
    fflush(stdout);
}

int main(int argc, char *argv[]) {
    char *port = NULL;

    // Iterate through command line arguments to find the --port argument
    for (int i = 1; i < argc - 1; i++) {
        if (strcmp(argv[i], "--port") == 0) {
            port = argv[i + 1];
            break;
        }
    }

    // Check if the port argument was provided
    if (port != NULL) {
        printf("Listening on serial port: %s\n", port);
    } else {
        printf("Error: Please specify the serial port using the --port argument.\n");
        printf("Usage: program_name --port <port_name>\n");
        fflush(stdout);
        return EXIT_FAILURE;
    }

    const int fd = open(port, O_RDWR | O_NOCTTY | O_NONBLOCK);
    if (fd < 0) {
        perror("Error opening port");
        return EXIT_FAILURE;
    }

    struct termios tty;
    memset(&tty, 0, sizeof tty);
    if (tcgetattr(fd, &tty) != 0) {
        perror("Error from tcgetattr");
        return EXIT_FAILURE;
    }

    cfsetospeed(&tty, B9600);
    cfsetispeed(&tty, B9600);

    tty.c_cflag &= ~PARENB; // Clear parity bit, disabling parity
    tty.c_cflag &= ~CSTOPB; // Clear stop field, only one stop bit used in communication
    tty.c_cflag &= ~CSIZE; // Clear all bits that set the data size
    tty.c_cflag |= CS8; // 8 bits per byte
    tty.c_cflag &= ~CRTSCTS; // Disable RTS/CTS hardware flow control
    tty.c_cflag |= CREAD | CLOCAL; // Turn on READ & ignore ctrl lines

    tty.c_lflag &= ~ICANON;
    tty.c_lflag &= ~ECHO; // Disable echo
    tty.c_lflag &= ~ECHOE; // Disable erasure
    tty.c_lflag &= ~ECHONL; // Disable new-line echo
    tty.c_lflag &= ~ISIG; // Disable interpretation of INTR, QUIT and SUSP

    tty.c_iflag &= ~(IXON | IXOFF | IXANY); // Turn off s/w flow ctrl
    tty.c_iflag &= ~(IGNBRK | BRKINT | PARMRK | ISTRIP | INLCR | IGNCR | ICRNL); // Disable any special handling of received bytes

    tty.c_oflag &= ~OPOST; // Prevent special interpretation of output bytes (e.g. newline chars)
    tty.c_oflag &= ~ONLCR; // Prevent conversion of newline to carriage return/line feed

    tty.c_cc[VTIME] = 1; // Wait for up to 1 deciseconds, returning as soon as any data is received
    tty.c_cc[VMIN] = 0;

    {
        const int tcsetattr_ret = tcsetattr(fd, TCSANOW, &tty);
        if (tcsetattr_ret != 0) {
            printf("Error from tcsetattr: %d\n", tcsetattr_ret);
            fflush(stdout);
            return EXIT_FAILURE;
        }
    }

    signal(SIGINT, int_handler);
    signal(SIGTERM, int_handler);

    StreamParser *parser = stream_parser_open();
    if (!parser) {
        printf("Failed to open stream parser\n");
        fflush(stdout);
        return EXIT_FAILURE;
    }
    stream_parser_register_error_callback(parser, error_callback, NULL);
    stream_parser_register_packet_callback(parser, packet_callback, NULL);

    while (keep_running) {
        uint8_t byte = 0;
        const int n = read(fd, &byte, 1);
        if (n > 0) {
            printf("Processing byte... : %c  0x%x\n", (char)byte, (int)byte);
            fflush(stdout);
            const StreamParserError error = stream_parser_push_byte(parser, byte);
            if (error) {
                printf("Error code returned by stream_parser_push_byte: %d\n", (int)error);
                fflush(stdout);
            }
        } else if (n < 0 && errno != EAGAIN) {
            printf("Error reading from serial port\n");
            fflush(stdout);
            break;
        }

        usleep(10000); // Small delay to avoid busy looping
    }

    stream_parser_close(parser);
    close(fd);
    printf("Exiting\n");
    fflush(stdout);
    return EXIT_SUCCESS;
}
