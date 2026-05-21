#include <stdint.h>
#include <stdio.h>
#include <stdbool.h>
#include <unistd.h>

#include <string.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

#include <esp_adc/adc_oneshot.h>
#include <hal/adc_types.h>

#include "esp_err.h"
#include "freertos/idf_additions.h"

#include "handle_status.c"

#include "include/status_codes.h"
#include "include/adc.h"

#define MIN_VOLTAGE (142) 			//Valor que o controlador é levado, devido
									//ao sensor estar em pull-down, para evitar
									//flutuação da entrada
#define MAX_VOLTAGE (1500)

TaskHandle_t ADCTaskHandle = NULL;
adc_oneshot_unit_handle_t handle;
adc_cali_handle_t cali_handle;
sensor_data_t sensor;

// ==============
// ADC em pull up
// ==============

void adc_init(){
	
	handle = NULL;
	
	adc_oneshot_unit_init_cfg_t init_config1 = {
		.unit_id = ADC_UNIT_1,
		.ulp_mode = ADC_ULP_MODE_DISABLE,
	};
	
	ESP_ERROR_CHECK(adc_oneshot_new_unit(&init_config1, &handle));
	
	adc_oneshot_chan_cfg_t config = {
		.atten = ADC_ATTEN_DB_12,
		.bitwidth = ADC_BITWIDTH_12,
	};
	
	ESP_ERROR_CHECK(adc_oneshot_config_channel(handle, ADC_CHANNEL_7, &config));
	
	// Inicializando o tipo 'sensor_health_t'
	// Aqui são retornados os vlaores padrão
	// já que sensor é declarado globalmente
	// e toda vez que o ADC for inicializado
	// a varíavel será resetada.
	
	sensor.status = 0;
	sensor.sensor_read = 0; 
	sensor.status_code = ADC_INIT;
	 
}

int adc_exec() {
	
	cali_handle = NULL;
	
	adc_cali_line_fitting_config_t cali_config = {
		.atten = ADC_ATTEN_DB_12,
		.bitwidth = ADC_BITWIDTH_12,
		.unit_id = ADC_UNIT_1,
	};
	
	ESP_ERROR_CHECK(adc_cali_create_scheme_line_fitting(&cali_config, &cali_handle));
		
	int sensor_read, sensor_cali;
	
	ESP_ERROR_CHECK(adc_oneshot_read(handle, ADC_CHANNEL_7, &sensor_read));
		
	adc_cali_raw_to_voltage(cali_handle, sensor_read, &sensor_cali);		
		
	return sensor_cali;
}

sensor_data_t set_sensor_health() {
	
	int voltage = 0;
	int avg = 0;
	int sum = 0;
			
	static int volt_array[8] = {0};
	
	for(int i=0; i < sizeof(volt_array)/sizeof(volt_array)[0]; i++) {
		
		voltage = adc_exec(); 	
				
		if(MIN_VOLTAGE >= voltage || MAX_VOLTAGE < voltage) {				//Caso a variável 'voltage'
			sensor.status = 0;
			sensor.sensor_read = 0;								//contenha um valor incorreto
			sensor.status_code = ADC_DISCONNECTED;			//faz um early return para o
			return sensor;		
			
			//Caso a variável 'voltage'
			//contenha um valor incorreto
			//faz um eartly return com o valor '0'
			//em status e sensor_read e define o sensor
			//no status_code em ADC_DISCONNECTED

		}
		volt_array[i] = voltage;
	}
	
	sensor.sensor_read = voltage;
			
	for(int i=0; i < sizeof(volt_array)/sizeof(volt_array)[0]; i++) {
			sum = sum + volt_array[i];					
	}
	
	avg = sum / (sizeof(volt_array)/sizeof(volt_array[0]));
		
	if(volt_array[sizeof(volt_array)/sizeof(volt_array)[0] -1] != 0) {
				
		if(volt_array [0] > (avg + 5)) {	
			
			sensor.status = 0;
			sensor.sensor_read = avg;
			sensor.status_code = ADC_FAIL;
			return sensor;
		}
		
		sensor.status = 1;
		sensor.sensor_read = avg;
		sensor.status_code = ADC_OK;
	}
	return sensor;
}

void adc_del(){
	adc_oneshot_del_unit(handle);
	adc_cali_delete_scheme_line_fitting(cali_handle);
}

// xTaskCreatePinnedToCore(ADCTask, "ADC Task" , 4096, NULL, 10, &ADCTaskHandle, 0);
