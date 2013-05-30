#include "atcommander.h"

#include <stddef.h>
#include <string.h>
#include <stdio.h>

#define RETRY_DELAY_MS 100

#define debug(config, ...) \
    if(config->log_function != NULL) { \
        config->log_function(__VA_ARGS__); \
        config->log_function("\r\n"); \
    }

/** Private: Send an array of bytes to the AT device.
 */
void write(AtCommanderConfig* config, const char* bytes, int size) {
    if(config->write_function != NULL) {
        for(int i = 0; i < size; i++) {
            config->write_function(bytes[i]);
        }
    }
}

/** Private: Read multiple bytes from Serial into the buffer.
 *
 * Continues to try and read each byte from Serial until a maximum number of
 * retries.
 *
 * Returns the number of bytes actually read - may be less than size.
 */
int read(AtCommanderConfig* config, char* buffer, int size,
        int max_retries) {
    int bytes_read = 0;
    int retries = 0;
    while(bytes_read < size && retries < max_retries) {
        int byte = config->read_function();
        if(byte == -1) {
            if(config->delay_function != NULL) {
                config->delay_function(RETRY_DELAY_MS);
            }
            retries++;
        } else {
            buffer[bytes_read++] = byte;
        }
    }
    return bytes_read;
}

bool check_response(AtCommanderConfig* config, char* response,
        int response_length, char* expected, int expected_length) {
    if(response_length == 3 && !strncmp(response, expected, expected_length)) {
        return true;
    }

    if(response_length != 3) {
        debug(config, "Expected %d bytes in response but received %d", 3,
                response_length);
    }

    if(response_length > 0) {
        debug(config, "Invalid command mode request response: %s", response);
    }
    return false;
}


void delay(AtCommanderConfig* config, int ms) {
    if(config->delay_function != NULL) {
        config->delay_function(ms);
    }
}

bool initialize_baud(AtCommanderConfig* config, int baud) {
    if(config->baud_rate_initializer != NULL) {
        debug(config, "Initializing at baud %d", baud);
        config->baud_rate_initializer(baud);
        config->baud = baud;
        return true;
    }
    debug(config, "No baud rate initializer set, can't change baud - trying anyway");
    return false;
}

bool at_commander_enter_command_mode(AtCommanderConfig* config) {
    if(!config->connected) {
        for(int baud_index = 0; baud_index < sizeof(VALID_BAUD_RATES) /
                sizeof(int); baud_index++) {
            initialize_baud(config, VALID_BAUD_RATES[baud_index]);
            debug(config, "Attempting to enter command mode");

            write(config, "$$$", 3);
            delay(config, 100);
            char response[3];
            int bytes_read = read(config, response, 3, 3);
            if(check_response(config, response, bytes_read, "CMD", 3)) {
                config->connected = true;
                break;
            }
        }

        if(config->connected) {
            debug(config, "Initialized UART and entered command mode at baud %d",
                    config->baud);
        } else {
            debug(config, "Unable to enter command mode at any baud rate");
        }
    } else {
        debug(config, "Already in command mode");
    }
    return config->connected;
}

bool at_commander_exit_command_mode(AtCommanderConfig* config) {
    if(config->connected) {
        write(config, "---", 3);
        delay(config, 100);
        char response[3];
        int bytes_read = read(config, response, 3, 3);
        if(check_response(config, response, bytes_read, "END", 3)) {
            debug(config, "Switched back to data mode");
            config->connected = false;
        } else {
            debug(config, "Unable to exit command mode");
        }
    } else {
        debug(config, "Not in command mode");
    }
}

bool at_commander_reboot(AtCommanderConfig* config) {
    if(at_commander_enter_command_mode(config)) {
        write(config, "R,1\r\n", 5);
    } else {
        debug(config, "Unable to enter command mode, can't reboot");
    }
}

bool at_commander_set_baud(AtCommanderConfig* config, int baud) {
    if(at_commander_enter_command_mode(config)) {
        char command[5];
        sprintf(command, "SU,%d\r\n", baud);
        write(config, command, strnlen(command, 11));
    } else {
        debug(config, "Unable to enter command mode, can't set baud rate");
    }
}
