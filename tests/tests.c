#include <check.h>
#include <stdint.h>
#include "atcommander.h"
#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>

AtCommanderConfig config;

void debug(const char* format, ...) {
    va_list args;
    va_start(args, format);
    vprintf(format, args);
    va_end(args);
}

void baud_rate_initializer(int baud) {
}

void mock_write(uint8_t byte) {
}

static char* read_message;
static int read_message_length;
static int read_index;

int mock_read() {
    if(read_message != NULL && read_index < read_message_length) {
        return read_message[read_index++];
    }
    return -1;
}

void setup() {
    config.platform = AT_PLATFORM_RN42;
    config.connected = false;
    config.baud = 9600;
    config.baud_rate_initializer = baud_rate_initializer;
    config.write_function = mock_write;
    config.read_function = mock_read;
    config.delay_function = NULL;
    config.log_function = debug;

    read_message = NULL;
    read_message_length = 0;
    read_index = 0;
}


START_TEST (test_enter_command_mode_success)
{
    char* success_message = "CMD";
    read_message = success_message;
    read_message_length = 3;

    ck_assert(!config.connected);
    at_commander_enter_command_mode(&config);
    ck_assert(config.connected);
}
END_TEST

START_TEST (test_enter_command_mode_fail_bad_response)
{
    char* success_message = "BAD";
    read_message = success_message;
    read_message_length = 3;

    ck_assert(!config.connected);
    at_commander_enter_command_mode(&config);
    ck_assert(!config.connected);
}
END_TEST

START_TEST (test_enter_command_mode_fail_no_response)
{
    read_message = NULL;
    read_message_length = 0;

    ck_assert(!config.connected);
    at_commander_enter_command_mode(&config);
    ck_assert(!config.connected);
}
END_TEST

Suite* suite(void) {
    Suite* s = suite_create("atcommander");
    TCase *tc_command_mode = tcase_create("command_mode");
    tcase_add_checked_fixture(tc_command_mode, setup, NULL);
    tcase_add_test(tc_command_mode, test_enter_command_mode_success);
    tcase_add_test(tc_command_mode, test_enter_command_mode_fail_bad_response);
    tcase_add_test(tc_command_mode, test_enter_command_mode_fail_no_response);
    suite_add_tcase(s, tc_command_mode);
    return s;
}

int main(void) {
    int numberFailed;
    Suite* s = suite();
    SRunner *sr = srunner_create(s);
    // Don't fork so we can actually use gdb
    srunner_set_fork_status(sr, CK_NOFORK);
    srunner_run_all(sr, CK_NORMAL);
    numberFailed = srunner_ntests_failed(sr);
    srunner_free(sr);
    return (numberFailed == 0) ? 0 : 1;
}
