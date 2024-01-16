#include "stream_parser.h"
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <termios.h>
#include <signal.h>
#include <string.h>

volatile sig_atomic_t keep_running = 1;

void int_handler(int dummy) {
    keep_running = 0;
}

void error_callback(StreamParserError error, const char *message, void *callback_data) {
    (void)callback_data; // Unused parameter
    printf("Error [%d]: %s\n", error, message);
}

int main() {
    char port[100];
    printf("Enter the serial port to listen on (e.g., /dev/ttyS0): ");
    scanf("%99s", port);

    int fd = open(port, O_RDWR | O_NOCTTY | O_NONBLOCK);
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

    if (tcsetattr(fd, TCSANOW, &tty) != 0) {
        perror("Error from tcsetattr");
        return EXIT_FAILURE;
    }

    signal(SIGINT, int_handler);
    signal(SIGTERM, int_handler);

    StreamParser *parser = open_stream_parser();
    if (!parser) {
        fprintf(stderr, "Failed to open stream parser\n");
        return EXIT_FAILURE;
    }
    stream_parser_error_callback(parser, error_callback, NULL);

    uint8_t byte;
    StreamParserError error;
    while (keep_running) {
        int n = read(fd, &byte, 1);
        if (n > 0) {
            size_t packet_length = push_byte(parser, byte, &error);
            if (packet_length > 0) {
                printf("Received packet of length %zu\n", packet_length);
            }
        } else if (n < 0 && errno != EAGAIN) {
            perror("Error reading from serial port");
            break;
        }

        usleep(10000); // Small delay to avoid busy looping
    }

    close_stream_parser(parser);
    close(fd);
    printf("Exiting\n");
    return EXIT_SUCCESS;
}
