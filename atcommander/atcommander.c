#include "atcommander.h"

#include <stddef.h>
#include <string.h>
#include <stdio.h>

#define AT_COMMANDER_DEFAULT_RESPONSE_DELAY_MS 1000
#define AT_COMMANDER_RETRY_DELAY_MS 100
#define AT_COMMANDER_MAX_RESPONSE_LENGTH 8
#define AT_COMMANDER_MAX_RETRIES 3

#define at_commander_debug(config, ...) \
    if(config->log_function != NULL) { \
        config->log_function(__VA_ARGS__); \
        config->log_function("\r\n"); \
    }

AtCommanderPlatform AT_PLATFORM_RN42 = {
    AT_COMMANDER_DEFAULT_RESPONSE_DELAY_MS,
    rn42_baud_rate_mapper,
    { "$$$", "CMD\r\n" },
    { "---\r", "END\r\n" },
    { "SU,%d\r", "AOK\r\n" },
    { NULL, NULL },
    { "R,1\r", NULL },
};

AtCommanderPlatform AT_PLATFORM_XBEE = {
    3000,
    xbee_baud_rate_mapper,
    { "+++", "OK" },
    { NULL, NULL },
    { "ATBD %d\r\n", "OK\r\n" },
    { "ATWR\r\n", "OK\r\n" },
    { NULL, NULL },
};

/** Private: Send an array of bytes to the AT device.
 */
void at_commander_write(AtCommanderConfig* config, const char* bytes, int size) {
    int i;
    if(config->write_function != NULL) {
        for(i = 0; i < size; i++) {
            config->write_function(bytes[i]);
        }
    }
}

/** Private: If a delay function is available, delay the given time, otherwise
 * just continue.
 */
void at_commander_delay_ms(AtCommanderConfig* config, int ms) {
    if(config->delay_function != NULL) {
        config->delay_function(ms);
    }
}

/** Private: Read multiple bytes from Serial into the buffer.
 *
 * Continues to try and read each byte from Serial until a maximum number of
 * retries.
 *
 * Returns the number of bytes actually read - may be less than size.
 */
int at_commander_read(AtCommanderConfig* config, char* buffer, int size,
        int max_retries) {
    int bytes_read = 0;
    int retries = 0;
    while(bytes_read < size && retries < max_retries) {
        int byte = config->read_function();
        if(byte == -1) {
            at_commander_delay_ms(config, AT_COMMANDER_RETRY_DELAY_MS);
            retries++;
        } else {
            buffer[bytes_read++] = byte;
        }
    }
    return bytes_read;
}

/** Private: Compare a response received from a device with some expected
 *      output.
 *
 * Returns true if there reponse matches content and length, otherwise false.
 */
bool check_response(AtCommanderConfig* config, const char* response,
        int response_length, const char* expected, int expected_length) {
    if(response_length == expected_length && !strncmp(response, expected,
                expected_length)) {
        return true;
    }

    if(response_length != expected_length) {
        at_commander_debug(config,
                "Expected %d bytes in response but received %d",
                expected_length, response_length);
    }

    if(response_length > 0) {
        at_commander_debug(config, "Expected %s response but got %s", expected,
                response);
    }
    return false;
}

/** Private: Send an AT command, read a response, and verify it matches the
 * expected value.
 *
 * Returns true if the response matches the expected.
 */
bool command_request(AtCommanderConfig* config, const char* command,
        const char* expected_response) {
    at_commander_write(config, command, strlen(command));
    at_commander_delay_ms(config, config->platform.response_delay_ms);

    char response[AT_COMMANDER_MAX_RESPONSE_LENGTH];
    int bytes_read = at_commander_read(config, response, strlen(expected_response),
            AT_COMMANDER_MAX_RETRIES);

    return check_response(config, response, bytes_read, expected_response,
            strlen(expected_response));
}

/** Private: Change the baud rate of the UART interface and update the config
 * accordingly.
 *
 * This function does *not* attempt to change anything on the AT-command set
 * supporting device, it just changes the host interface.
 */
bool initialize_baud(AtCommanderConfig* config, int baud) {
    if(config->baud_rate_initializer != NULL) {
        at_commander_debug(config, "Initializing at baud %d", baud);
        config->baud_rate_initializer(baud);
        config->baud = baud;
        return true;
    }
    at_commander_debug(config,
            "No baud rate initializer set, can't change baud - trying anyway");
    return false;
}

bool at_commander_enter_command_mode(AtCommanderConfig* config) {
    int baud_index;
    if(!config->connected) {
        for(baud_index = 0; baud_index < sizeof(VALID_BAUD_RATES) /
                sizeof(int); baud_index++) {
            initialize_baud(config, VALID_BAUD_RATES[baud_index]);
            at_commander_debug(config, "Attempting to enter command mode");

            if(command_request(config,
                    config->platform.enter_command_mode_command.request_format,
                    config->platform.
                        enter_command_mode_command.expected_response)) {
                config->connected = true;
                break;
            }
        }

        if(config->connected) {
            at_commander_debug(config, "Initialized UART and entered command "
                    "mode at baud %d", config->baud);
        } else {
            at_commander_debug(config,
                    "Unable to enter command mode at any baud rate");
        }
    } else {
        at_commander_debug(config, "Already in command mode");
    }
    return config->connected;
}

bool at_commander_exit_command_mode(AtCommanderConfig* config) {
    if(config->connected) {
        if(command_request(config,
                config->platform.exit_command_mode_command.request_format,
                config->platform.exit_command_mode_command.expected_response)) {
            at_commander_debug(config, "Switched back to data mode");
            config->connected = false;
            return true;
        } else {
            at_commander_debug(config, "Unable to exit command mode");
            return false;
        }
    } else {
        at_commander_debug(config, "Not in command mode");
        return true;
    }
}

bool at_commander_reboot(AtCommanderConfig* config) {
    if(at_commander_enter_command_mode(config)) {
        at_commander_write(config, config->platform.reboot_command.request_format,
                strlen(config->platform.reboot_command.request_format));
        at_commander_debug(config, "Rebooting RN-42");
        return true;
    } else {
        at_commander_debug(config, "Unable to enter command mode, can't reboot");
        return false;
    }
}

bool at_commander_store_settings(AtCommanderConfig* config) {
    if(config->platform.store_settings_command.request_format != NULL
            && config->platform.store_settings_command.expected_response
                != NULL) {
        if(command_request(config,
                config->platform.store_settings_command.request_format,
                config->platform.store_settings_command.expected_response)) {
            at_commander_debug(config, "Stored settings into flash memory");
            return true;
        }

        at_commander_debug(config, "Unable to store settings in flash memory");
        return false;
    }
    return false;
}

bool at_commander_set_baud(AtCommanderConfig* config, int baud) {
    if(at_commander_enter_command_mode(config)) {
        char command[6];
        int (*baud_rate_mapper)(int) = config->platform.baud_rate_mapper;
        sprintf(command, config->platform.set_baud_rate_command.request_format,
                baud_rate_mapper(baud));
        if(command_request(config, command,
                config->platform.set_baud_rate_command.expected_response)) {
            at_commander_debug(config, "Changed device baud rate to %d", baud);
            config->device_baud = baud;
            at_commander_store_settings(config);
            return true;
        } else {
            at_commander_debug(config, "Unable to change device baud rate");
            return false;
        }
    } else {
        at_commander_debug(config,
                "Unable to enter command mode, can't set baud rate");
        return false;
    }
}

int rn42_baud_rate_mapper(int baud) {
    int value;
    switch(baud) {
        case 1200:
            value = 12;
            break;
        case 2300:
            value = 23;
            break;
        case 4800:
            value = 48;
            break;
        case 9600:
            value = 96;
            break;
        case 19200:
            value = 19;
            break;
        case 38400:
            value = 28;
            break;
        case 57600:
            value = 57;
            break;
        case 115200:
            value = 11;
            break;
        case 230400:
            value = 23;
            break;
        case 460800:
            value = 46;
            break;
        case 921600:
            value = 92;
            break;
    }
    return value;
}

int xbee_baud_rate_mapper(int baud) {
    int value;
    switch(baud) {
        case 1200:
            value = 0;
            break;
        case 2300:
            value = 1;
            break;
        case 4800:
            value = 2;
            break;
        case 9600:
            value = 3;
            break;
        case 19200:
            value = 4;
            break;
        case 38400:
            value = 5;
            break;
        case 57600:
            value = 6;
            break;
        case 115200:
            value = 7;
            break;
    }
    return value;
}
