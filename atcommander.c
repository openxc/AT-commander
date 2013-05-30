#include "atcommander.h"

#include <stddef.h>
#include <string.h>
#include <stdio.h>

#define RETRY_DELAY_MS 100

void debug(AtCommanderConfig* config, const char* message) {
    if(config->log_function != NULL) {
        config->log_function(message);
        config->log_function("\r\n");
    }
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
 * Returns true if all bytes were read, otherwise false.
 */
bool read(AtCommanderConfig* config, char* buffer, int size,
        int max_retries) {
    int bytesRead = 0;
    int retries = 0;
    while(bytesRead < size && retries < max_retries) {
        uint8_t byte = config->read_function();
        if(byte == -1) {
            config->delay_function(RETRY_DELAY_MS);
            retries++;
        } else {
            buffer[bytesRead++] = byte;
        }
    }
}

void delay(AtCommanderConfig* config, int ms) {
    if(config->delay_function != NULL) {
        config->delay_function(ms);
    }
}

bool at_commander_enter_command_mode(AtCommanderConfig* config) {
    if(!config->connected) {
        for(int i = STARTING_BAUD_RATE;
                i < STARTING_BAUD_RATE * MAX_BAUD_RATE_MULTIPLIER; i *= 2) {
            debug(config, "Attempting to enter command mode");
            write(config, "$$$", 3);
            delay(config, 100);
            char response[3];
            read(config, response, 3, 3);
            if(!strcmp(response, "CMD")) {
                config->connected = true;
                break;
            } else {
                debug(config, "Command mode request gave the wrong response");
                config->connected = false;
            }
        }

        if(config->connected) {
            debug(config, "Initialized UART and entered command mode");
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
