#ifndef APP_RUNTIME_CONFIG_TEST_SUPPORT_H
#define APP_RUNTIME_CONFIG_TEST_SUPPORT_H

#include <stdint.h>

void keyswitch_test_reset_runtime_config_host_state(void);
void keyswitch_test_clear_usb_output(void);
const char *keyswitch_test_usb_output(void);
uintptr_t keyswitch_test_config_flash_storage_address(void);

#endif