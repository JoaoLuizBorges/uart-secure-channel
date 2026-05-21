#ifndef ADC_H
#define ADC_H


#include "status_codes.h"

void adc_init();

int adc_exec();

sensor_data_t set_sensor_health();

void adc_del();

#endif // ADC_H