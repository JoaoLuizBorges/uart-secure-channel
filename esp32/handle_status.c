#include "status_codes.h"
#include <stdint.h>
#include <stdio.h>

status_code_t check_sensor_health(void) {
	return MAKE_STATUS(MODULE_COMMON, SEV_OK, 0X00);
}

status_code_t uart_comm_ok(status_code_t code) {
	return COMM_OK;
}

status_code_t uart_comm_fail(status_code_t code) {
	return COMM_UART_FAIL;
}

void handle_status(status_code_t code) {
	if (IS_OK(code)) {
		printf("All good!\n");
		return;
	}
}