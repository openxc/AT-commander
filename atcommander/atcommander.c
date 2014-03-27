#include "atcommander.h"

#include <stddef.h>
#include <string.h>
#include <stdio.h>

// TODO hard coded max of 128 - I've never seen one anywhere near this
// long so we're probably OK.
#define AT_COMMANDER_MAX_REQUEST_LENGTH 128
#define AT_COMMANDER_DEFAULT_RESPONSE_DELAY_MS 100
#define AT_COMMANDER_RETRY_DELAY_MS 50
#define AT_COMMANDER_MAX_RESPONSE_LENGTH 8
#define AT_COMMANDER_MAX_RETRIES 3

#define at_commander_debug(config, ...) \
    if(config->log_function != NULL) { \
        config->log_function(__VA_ARGS__); \
        config->log_function("\r\n"); \
    }

const AtCommanderPlatform AT_PLATFORM_RN42 = {
    AT_COMMANDER_DEFAULT_RESPONSE_DELAY_MS,
    rn42_baud_rate_mapper,
    { "$$$", "CMD" },
    { "---\r", "END" },
    { "SU,%d\r", "AOK" },
    { "ST,%d\r", "AOK" },
    { NULL, NULL },
    { "R,1\r", "Reboot!" },
    { "SN,%s\r", "AOK" },
    { "S-,%s\r", "AOK" },
    { "GN\r", NULL, "ERR" },
    { "GB\r", NULL, "ERR" },
};

const AtCommanderPlatform AT_PLATFORM_XBEE = {
    3000,
    xbee_baud_rate_mapper,
    { "+++", "OK" },
    { NULL, NULL },
    { "ATBD %d\r\n", "OK" },
    { "ATWR\r\n", "OK" },
    { NULL, NULL },
    { NULL, NULL },
    { NULL, NULL },
    { NULL, NULL },
    { NULL, NULL },
};

/** Private: Send an array of bytes to the AT device.
 */
void at_commander_write(AtCommanderConfig* config, const char* bytes, int size) {
    int i;
    if(config->write_function != NULL) {
        /* at_commander_debug(config, "tx: %s", bytes); */
        for(i = 0; i < size; i++) {
            config->write_function(config->device, bytes[i]);
        }
    }
}

/** Private: If a delay function is available, delay the given time, otherwise
 * just continue.
 */
void at_commander_delay_ms(AtCommanderConfig* config, unsigned long ms) {
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
    bool sawCarraigeReturn = false;
    while(bytes_read < size && (max_retries == 0 || retries < max_retries)) {
        int byte = config->read_function(config->device);
        if(byte == -1) {
            at_commander_delay_ms(config, AT_COMMANDER_RETRY_DELAY_MS);
            retries++;
        } else if(byte != '\r' && byte != '\n') {
            buffer[bytes_read++] = byte;
        }

        if(bytes_read > 1) {
            if(byte == '\r') {
                sawCarraigeReturn = true;
            } else if(sawCarraigeReturn && byte == '\n') {
                break;
            }
        }
    }
    /* if(bytes_read > 0) { */
        /* at_commander_debug(config, "rx: %s", buffer); */
    /* } */
    return bytes_read;
}

/** Private: Compare a response received from a device with some expected
 *      output.
 *
 * Returns true if the reponse matches content and length, otherwise false.
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
        at_commander_debug(config, "Expected \"%s\" (%d bytes) in response "
                "but got \"%s\" (%d bytes)", expected, expected_length,
                response, response_length);
    }
    return false;
}

/** Private: Send an AT "get" query, read a response, and verify it doesn't match
 * any known errors.
 *
 * Returns true if the response isn't a known error state.
 */
int get_request(AtCommanderConfig* config, AtCommand* command,
        char* response_buffer, int response_buffer_length) {
    at_commander_write(config, command->request_format,
            strlen(command->request_format));
    at_commander_delay_ms(config, config->platform.response_delay_ms);

    int bytes_read = at_commander_read(config, response_buffer,
            response_buffer_length - 1, AT_COMMANDER_MAX_RETRIES);
    response_buffer[bytes_read] = '\0';

    if(strncmp(response_buffer, command->error_response, strlen(command->error_response))) {
        return bytes_read;
    }
    return -1;
}


/** Private: Send an AT command, read a response, and verify it matches the
 * expected value.
 *
 * Returns true if the response matches the expected.
 */
bool set_request(AtCommanderConfig* config, const char* command, const char* expected_response) {
    at_commander_write(config, command, strlen(command));
    at_commander_delay_ms(config, config->platform.response_delay_ms);

    char response[AT_COMMANDER_MAX_RESPONSE_LENGTH];
    int bytes_read = at_commander_read(config, response, strlen(expected_response),
            AT_COMMANDER_MAX_RETRIES);

    return check_response(config, response, bytes_read, expected_response,
            strlen(expected_response));
}

bool at_commander_store_settings(AtCommanderConfig* config) {
    if(config->platform.store_settings_command.request_format != NULL
            && config->platform.store_settings_command.expected_response
                != NULL) {
        if(set_request(config,
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

bool at_commander_set(AtCommanderConfig* config, AtCommand* command, ...) {
    if(at_commander_enter_command_mode(config)) {
        va_list args;
        va_start(args, command);

        char request[AT_COMMANDER_MAX_REQUEST_LENGTH];
        vsnprintf(request, AT_COMMANDER_MAX_REQUEST_LENGTH, command->request_format, args);

        va_end(args);

        if(set_request(config, request, command->expected_response)) {
            at_commander_store_settings(config);
            return true;
        } else {
            return false;
        }
    } else {
        at_commander_debug(config,
                "Unable to enter command mode, can't make set request");
        return false;
    }
}

int at_commander_get(AtCommanderConfig* config, AtCommand* command,
        char* response_buffer, int response_buffer_length) {
    if(response_buffer == NULL || response_buffer_length <= 0) {
        at_commander_debug(config, "Buffer for query response is invalid");
        return -1;
    }

    int bytes_read = -1;
    if(at_commander_enter_command_mode(config)) {
        bytes_read = get_request(config, command, response_buffer,
                response_buffer_length);
    } else {
        at_commander_debug(config,
                "Unable to enter command mode, can't get device name");
    }
    return bytes_read;
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
        config->baud_rate_initializer(config->device, baud);
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

            if(set_request(config,
                    config->platform.enter_command_mode_command.request_format,
                    config->platform.enter_command_mode_command.expected_response)) {
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
    }
    return config->connected;
}

bool at_commander_exit_command_mode(AtCommanderConfig* config) {
    if(config->connected) {
        if(set_request(config,
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
        if(set_request(config,
                config->platform.reboot_command.request_format,
                config->platform.reboot_command.expected_response)) {
            at_commander_debug(config, "Rebooted");
            config->connected = false;
        } else {
            at_commander_debug(config, "Unable to reboot");
        }
        return true;
    } else {
        at_commander_debug(config, "Unable to enter command mode, can't reboot");
        return false;
    }
}

bool at_commander_set_configuration_timer(AtCommanderConfig* config,
        int timeout_s) {
    if(at_commander_enter_command_mode(config)) {
        char command[6];
        sprintf(command,
                config->platform.set_configuration_timer_command.request_format,
                timeout_s);
        if(set_request(config, command,
                config->platform.set_configuration_timer_command.expected_response)) {
            at_commander_debug(config, "Changed configuration timer to %d",
                    timeout_s);
            at_commander_store_settings(config);
            return true;
        } else {
            at_commander_debug(config, "Unable to change configuration timer");
            return false;
        }
    } else {
        at_commander_debug(config,
                "Unable to enter command mode, can't set configuration timer");
        return false;
    }
}

bool at_commander_set_baud(AtCommanderConfig* config, int baud) {
    int (*baud_rate_mapper)(int) = config->platform.baud_rate_mapper;
    if(at_commander_set(config, &config->platform.set_baud_rate_command,
                baud_rate_mapper(baud))) {
        at_commander_debug(config, "Changed device baud rate to %d", baud);
        config->device_baud = baud;
        return true;
    } else {
        at_commander_debug(config, "Unable to change device baud rate");
        return false;
    }
}

bool at_commander_set_name(AtCommanderConfig* config, const char* name,
        bool serialized) {
    AtCommand* command = &config->platform.set_name_command;
    if(serialized) {
        at_commander_debug(config,
                "Appending unique serial number to end of name");
        command = &config->platform.set_serialized_name_command;
    }
    if(at_commander_set(config, command, name)) {
        at_commander_debug(config, "Changed device name successfully to %s",
                name);
        return true;
    }
    return false;
}

int at_commander_get_device_id(AtCommanderConfig* config, char* buffer,
        int buflen) {
    return at_commander_get(config, &config->platform.get_device_id_command,
            buffer, buflen);
}

int at_commander_get_name(AtCommanderConfig* config, char* buffer,
        int buflen) {
    return at_commander_get(config, &config->platform.get_name_command,
            buffer, buflen);
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
