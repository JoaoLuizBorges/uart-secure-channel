#include <stdint.h>
#include <stdio.h>
#include <stdbool.h>
#include <sys/_intsup.h>
#include <unistd.h>
#include <string.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include "esp_timer.h"
#include "freertos/idf_additions.h"
#include <esp_adc/adc_oneshot.h>
#include "include/adc.h"
#include "include/uart.h"
#include "portmacro.h"
#include "status_codes.h"

#define FRAME_SIZE     		 13
#define FRAME_MAX_SIZE  	 45

volatile uint8_t uart_frame_ready = 0;
uint8_t UART_rxBuffer[FRAME_MAX_SIZE];

TaskHandle_t tx_task_handle;
uart_parser_t uart_parser;
comm_ctx_t comm_ctx;

uint8_t frame_size;

void uart_tx_task(void *arg) {
	
    while (1) {
        ulTaskNotifyTake(pdTRUE, portMAX_DELAY);
        transmit_data(&comm_ctx);
    }
}

void uart_rx_task(void *pvParameters) {
	
    uart_event_t event;
    uint8_t rx_buf[256];
	int len;
	
    while (1) {

        if (xQueueReceive(uart_queue, &event, portMAX_DELAY)) {

            switch (event.type) {

	            case UART_DATA:
	                len = uart_read_bytes(
					    UART_NUM_2,
					    rx_buf,
					    event.size,
					    portMAX_DELAY
					);
					
					if (len == FRAME_SIZE || len == FRAME_MAX_SIZE) {
						
						if(comm_ctx.state == COMM_SECURE) {
							memcpy(UART_rxBuffer, rx_buf, FRAME_MAX_SIZE);
						} else {
							memcpy(UART_rxBuffer, rx_buf, FRAME_SIZE);
						}	
                    
                    uart_frame_ready = true;
                    
                }
	                break;
	
	            case UART_FIFO_OVF:
	            case UART_BUFFER_FULL:
	                uart_flush_input(UART_NUM_2);
	                xQueueReset(uart_queue);
	                break;
	
	            case UART_BREAK:
	            case UART_PARITY_ERR:
	            case UART_FRAME_ERR:
	                break;
	
	            default:
	                break;
            }
        }
    }
}
    
void app_main(void) {
			
	//Inicializa o estado da comunicação
	uart_queue = xQueueCreate(20, sizeof(uart_event_t));
	uart_init(&comm_ctx);

	//Inicializa tudo que precisa do contexto de criptografia
	cryp_init(&comm_ctx.crypto);
	
	//Gera o nonce do lado do "cliente""
	generate_nonce(&comm_ctx.crypto.ctr_drbg, &comm_ctx.client_nonce, sizeof(comm_ctx.client_nonce));
	
	//Criar as tasks de transmissão e recepctação do canal UART 2
	xTaskCreate(uart_tx_task, "uart_tx", 4096, NULL, 9, &tx_task_handle);
	xTaskCreate(uart_rx_task, "uart_rx", 4096, NULL, 10, NULL);
	
	//Define a coonstante de delay usada no programa
	const TickType_t xDelay = 10 / portTICK_PERIOD_MS;
	
	while(1) {
		
        if (uart_frame_ready) {

            uart_frame_ready = false;
			
			if(comm_ctx.state == COMM_SECURE) {
				
				static uint8_t plain[256];
				size_t plain_len;
				size_t rx_len = sizeof(UART_rxBuffer);
				
				int ret = decrypt_frame(&comm_ctx.crypto, UART_rxBuffer, rx_len, plain, &plain_len);
			  					  
				if(ret == 0) {
					uart_parse_frame(&uart_parser, plain, plain_len);
			  	} else {
					printf("decrypt error = %d\n", ret);  
				}
			
			} 			
			
			uart_parse_frame(&uart_parser,UART_rxBuffer,FRAME_SIZE);
			
            if (uart_parser.frame_ready == 1) {
				
                process_frame(&comm_ctx,&uart_parser,FRAME_SIZE);
                uart_parser_reset(&uart_parser);            
            }
        }
        
        if (comm_ctx.tx_pending) {
			
			comm_ctx.tx_pending = false;
			xTaskNotifyGive(tx_task_handle); 	
			
		}
		
        vTaskDelay(xDelay);
    }
}	
