#include "atcommander.h"

#include <stddef.h>

void debug(AtCommanderConfig* config, const char* message) {
    if(config->log_function != NULL) {
        config->log_function(message);
    }
}

/** Private: Send an array of bytes to the AT device.
 */
void write(AtCommanderConfig* config, uint8_t* bytes, int size) {
}

void delay(AtCommanderConfig* config, int ms) {
    if(config->delay_function != NULL) {
        config->delay_function(ms);
    }
}

bool at_commander_enter_command_mode(AtCommanderConfig* config) {
    if(!config->command_mode_active) {
        for(int i = STARTING_BAUD_RATE; i < MAX_BAUD_RATE_MULTIPLIER; i *= 2) {
            write(config, "$$$", 3);
            delay(config, 100);
            // TODO see if we can read "OK", otherwise return false
        }

        if(config->connected) {
            // TODO sprintf to get this param
            debug("Initialized UART at baud %d and entered command mode");
        } else {
            debug("Unable to initialze UART, can't enter command mode");
        }
    } else {
        debug("Already in command mode");
    }
}

bool at_commander_exit_command_mode(AtCommanderConfig* config) {
}

bool at_commander_reset(AtCommanderConfig* config) {
    if(at_commander_enter_command_mode(config)) {
    } else {
        debug("Unable to enter command mode, can't set baud rate");
    }
}

bool at_commander_set_baud(AtCommanderConfig* config, int baud) {
    if(at_commander_enter_command_mode(config)) {
    } else {
        debug("Unable to enter command mode, can't set baud rate");
    }
}
