#ifndef _ATCOMMANDER_H_
#define _ATCOMMANDER_H_

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

int STARTING_BAUD_RATE = 9600;
int MAX_BAUD_RATE_MULTIPLIER = 7;

typedef enum {
    AT_PLATFORM_RN42,
    AT_PLATFORM_RN41,
} AtCommanderPlatform;

typedef struct {
    AtCommanderPlatform platform;
    bool connected;
    int current_baud_rate;
    void (*baud_rate_initializer)(int);
    void (*write_function)(uint8_t);
    uint8_t (*read_function)();
    void (*delay_function)(int);
    void (*log_function)(const char*);
} AtCommanderConfig;

/** Public: Switch to command mode, returning true if successful.
 *
 * If unable to determine the current baud rate, returns false.
 */
bool at_commander_enter_command_mode(AtCommanderConfig* config);
bool at_commander_exit_command_mode(AtCommanderConfig* config);
bool at_commander_reboot(AtCommanderConfig* config);

/** Public: Change the UART baud rate of the attached AT device, regardless of
 *      the current baud rate.
 *
 *  Attempts to automatically determine the current baud rate in order to enter
 *  command mode and change the baud rate. After updating the baud rate
 *  successfully, it reboots the device.
 */
bool at_commander_set_baud(AtCommanderConfig* config, int baud);

#ifdef __cplusplus
}
#endif

#endif // _ATCOMMANDER_H_