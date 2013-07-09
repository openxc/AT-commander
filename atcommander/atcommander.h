#ifndef _ATCOMMANDER_H_
#define _ATCOMMANDER_H_

#include <stdbool.h>
#include <stdint.h>
#include <stddef.h>

#define AT_PLATFORM_RN41 AT_PLATFORM_RN42

#ifdef __cplusplus
extern "C" {
#endif

static const int VALID_BAUD_RATES[] = {230400, 115200, 9600, 19200, 38400,
    57600, 460800};

typedef struct {
    const char* request_format;
    const char* expected_response;
} AtCommand;

typedef struct {
    int response_delay_ms;
    int (*baud_rate_mapper)(int baud);
    AtCommand enter_command_mode_command;
    AtCommand exit_command_mode_command;
    AtCommand set_baud_rate_command;
    AtCommand store_settings_command;
    AtCommand reboot_command;
} AtCommanderPlatform;

typedef struct {
    AtCommanderPlatform platform;
    bool connected;
    int baud;
    int device_baud;
    void* device;
    void (*baud_rate_initializer)(void* device, int);
    void (*write_function)(void* device, uint8_t);
    int (*read_function)(void* device);
    void (*delay_function)(unsigned long);
    void (*log_function)(const char*, ...);
} AtCommanderConfig;

/** Public: Switch to command mode.
 *
 * If unable to determine the current baud rate and enter command mode, returns
 * false.
 *
 * Returns true if successful, or if already in command mode.
 */
bool at_commander_enter_command_mode(AtCommanderConfig* config);

/** Public: Switch to data mode (from command mode).
 *
 * Returns true if the device was successfully switched to data mode.
 */
bool at_commander_exit_command_mode(AtCommanderConfig* config);

/** Public: Soft-reboot the attached AT device.
 *
 * After rebooting, it's good practice to wait a few hundred ms for the device
 * to restart before trying any other commands.
 *
 * Returns true if the device was successfully rebooted.
 */
bool at_commander_reboot(AtCommanderConfig* config);

/** Public: Change the UART baud rate of the attached AT device, regardless of
 *      the current baud rate.
 *
 *  Attempts to automatically determine the current baud rate in order to enter
 *  command mode and change the baud rate. After updating the baud rate
 *  successfully, it reboots the device.
 *
 *      baud - the desired baud rate.
 *
 *  Returns true if the baud rate was successfully changed.
 */
bool at_commander_set_baud(AtCommanderConfig* config, int baud);

int rn42_baud_rate_mapper(int baud);
int xbee_baud_rate_mapper(int baud);

#ifdef __cplusplus
}
#endif

#endif // _ATCOMMANDER_H_
