#ifndef _ATCOMMANDER_H_
#define _ATCOMMANDER_H_

#include <stdbool.h>
#include <stdint.h>
#include <stddef.h>
#include <stdarg.h>

#define AT_PLATFORM_RN41 AT_PLATFORM_RN42

#ifdef __cplusplus
extern "C" {
#endif

static const int VALID_BAUD_RATES[] = {230400, 115200, 9600, 19200, 38400,
    57600, 460800};

typedef struct {
    const char* request_format;
    const char* expected_response;
    const char* error_response;
} AtCommand;

typedef struct {
    int response_delay_ms;
    int (*baud_rate_mapper)(int baud);
    AtCommand enter_command_mode_command;
    AtCommand exit_command_mode_command;
    AtCommand set_baud_rate_command;
    AtCommand set_configuration_timer_command;
    AtCommand store_settings_command;
    AtCommand reboot_command;
    AtCommand set_name_command;
    AtCommand set_serialized_name_command;
    AtCommand get_name_command;
    AtCommand get_device_id_command;
} AtCommanderPlatform;

extern const AtCommanderPlatform AT_PLATFORM_RN42;
extern const AtCommanderPlatform AT_PLATFORM_XBEE;

typedef struct {
    AtCommanderPlatform platform;
    void (*baud_rate_initializer)(void* device, int);
    void (*write_function)(void* device, uint8_t);
    int (*read_function)(void* device);
    void (*delay_function)(unsigned long);
    void (*log_function)(const char*, ...);

    bool connected;
    int baud;
    int device_baud;
    void* device;
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

/** Public: Change the configuration timeout of the attached AT device.
 *
 *  Attempts to automatically determine the current baud rate in order to enter
 *  command mode and change the configuration timer (in seconds). A timeout of 0
 *  means that remote configuration is not allowed (configuration is always
 *  allowed via UART).
 *
 *      timeout - the desired configuration timeout in seconds.
 *
 *  Returns true if the configuration timer was successfully changed.
 */
bool at_commander_set_configuration_timer(AtCommanderConfig* config, int timeout_s);

/** Public: Change the attached AT device's name.
 *
 *  name - the desired name.
 *  serialized - if true, will request the AT device append a unique serial
 *  number to the end of the name (e.g. the last 2 digits of the MAC address).
 *
 *  Returns true if the name was set successfully.
 */
bool at_commander_set_name(AtCommanderConfig* config, const char* name,
        bool serialized);

/** Public: Retrieve the attached AT device's ID (usually MAC).
 *
 *  buffer - a string buffer to store the retrieved device ID.
 *  buflen - the length of the buffer.
 *
 *  Returns the length of the response, or -1 if an error occurred.
 */
int at_commander_get_device_id(AtCommanderConfig* config, char* buffer,
        int buflen);

/** Public: Retrieve the attached AT device's name.
 *
 *  buffer - a string buffer to store the retrieved name.
 *  buflen - the length of the buffer.
 *
 *  Returns the length of the response, or -1 if an error occurred.
 */
int at_commander_get_name(AtCommanderConfig* config, char* buffer,
        int buflen);

/** Public: Retrieve the attached AT device's name.
 *
 *  buffer - a string buffer to store the retrieved name.
 *  buflen - the length of the buffer.
 *
 *  Returns the length of the response, or -1 if an error occurred.
 */
int at_commander_get_name(AtCommanderConfig* config, char* buffer,
        int buflen);

/** Public: Send an AT "get" query, read a response, and verify it doesn't match
 * any known errors.
 *
 *  Returns the length of the response, or -1 if an error occurred.
 */
int at_commander_get(AtCommanderConfig* config, AtCommand* command,
        char* response_buffer, int response_buffer_length);

/** Public: Send an AT command, read a response, and verify it matches the
 * expected value.
 *
 * Returns true if the response matches the expected.
 */
bool at_commander_set(AtCommanderConfig* config, AtCommand* command,
        ...);

int rn42_baud_rate_mapper(int baud);
int xbee_baud_rate_mapper(int baud);

#ifdef __cplusplus
}
#endif

#endif // _ATCOMMANDER_H_
