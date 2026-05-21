#ifndef ADC_H
#define ADC_H


#include <stdint.h>
#include <stdbool.h>

typedef uint16_t status_code_t;

typedef struct {
	bool status;
	uint8_t sensor_read;
	status_code_t status_code;
} sensor_data_t;


#endif // ADC_H
