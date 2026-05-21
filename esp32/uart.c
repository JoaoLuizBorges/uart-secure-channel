#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdbool.h>
#include <sys/_intsup.h>
#include <sys/types.h>
#include <unistd.h>

#include "esp_err.h"
#include "hal/uart_types.h"
#include "include/encrypt.h"
#include "string.h"
#include "include/status_codes.h"
#include "include/uart.h"

#define LOG_STATE(s) printf("[STATE] %s\n", comm_state_to_str(s))

QueueHandle_t uart_queue = NULL;

extern uint8_t psk[32];

const char* comm_state_to_str(comm_state_t state) {
	
    switch (state) {
        case COMM_IDLE:       return "COMM_IDLE";
        case COMM_WAIT_HELLO: return "COMM_WAIT_HELLO";
        case COMM_HELLO_ACK:  return "COMM_HELLO_ACK";
        case COMM_SECURE:     return "COMM_SECURE";
        case COMM_ERROR:      return "COMM_ERROR";
        default:              return "COMM_UNKNOWN";
    }
}

void uart_parser_reset(uart_parser_t *p) {
	
    p->state = PARSER_WAIT_SOF;
    p->index = 0;
    p->len = 0;
    p->crc_rx = 0;
    p->frame_ready = false;
}

uint16_t crc16_kermit(const uint8_t *data, uint16_t len) {
	
    uint16_t crc = 0x0000;

    while (len--) {
        crc ^= *data++;
        for (uint8_t i = 0; i < 8; i++) {
            if (crc & 1)
                crc = (crc >> 1) ^ 0x8408;
            else
                crc >>= 1;
        }
    }

    return crc;
}

bool crc16_is_valid(const uart_parser_t *p) {
	
    uint16_t crc_calc;
	static uint8_t rx[3 + MAX_PAYLOAD];
	size_t crc_len = 0;
	
 	rx[crc_len++] = START_BYTE;
    rx[crc_len++] = p->len;
    	
	memcpy(&rx[crc_len], p->payload, p->len);
    crc_len += p->len;
    crc_calc = crc16_kermit(rx, crc_len);
    
    return (crc_calc == p->crc_rx);
}

void process_frame(comm_ctx_t *ctx, uart_parser_t *parser, uint8_t len) {
	
    uint8_t msg  = parser->payload[1];

    switch (msg)
    {
		
		/* ==============================
     	* FRAME SEGURO
    	* =============================== */
    	case 0x00: {

			#if TO_BE_IMPLEMENTED
    		uint8_t a = parser->payload[2];
    		uint8_t b = parser->payload[3];
    		uint8_t c = parser->payload[4];
			#endif		
			
			if(ctx->state != COMM_SECURE) break;			
			ctx->tx_pending = true;
    		break;
		}
		
		/* ==============================
     	* COMM_WAIT_HELLO
    	* =============================== */
        case 0x04:
						
			if(ctx->state == COMM_HELLO_ACK) break;
			
        	memcpy(&ctx->server_nonce,&parser->payload[2],sizeof(ctx->server_nonce));
						
        	if(ctx->server_nonce != 0) {
				
        		ctx->state = COMM_HELLO_ACK;
				
				derive_session_key(&ctx->crypto, psk, 
							   sizeof(psk), 
						  &ctx->client_nonce, 
				      sizeof(ctx->client_nonce), 
						  &ctx->server_nonce, 
					  sizeof(ctx->server_nonce));
        		
        		ctx->tx_pending = true;
        	}

            break;
        
        /* ==============================
     	* COMM_READY
    	* =============================== */    
		case 0x06:
						
			if(ctx->state == COMM_READY) break;
						
        	ctx->state = COMM_READY;
			ctx->tx_pending = true;
            break;
            
        default:

            break;
    }
}

void uart_parse_frame(uart_parser_t *p, uint8_t *buffer, uint8_t len) {
	
	if (!p || !buffer) return;
	
    for (uint8_t i = 0; i < len; i++)
    {
        uart_parse_byte(p, buffer[i]);

        if (p->frame_ready)
        {
        	break;
        }
    }
}

void uart_parse_byte(uart_parser_t *p, uint8_t byte) { 
    
    switch (p->state) {

	    case PARSER_WAIT_SOF:
	        if (byte == START_BYTE) {
	            p->state = PARSER_WAIT_LEN;
	        }
	        break;
	        	
	    case PARSER_WAIT_LEN:
	        if (byte > MAX_PAYLOAD) {
	            p->state = PARSER_WAIT_SOF;
	            break;
	        }
	        p->len = byte;
	        p->index = 0;
	        p->state = (p->len == 0) ? PARSER_WAIT_CRC_L
	                                 : PARSER_WAIT_PAYLOAD;
	        break;
	        
	    case PARSER_WAIT_PAYLOAD:
	        p->payload[p->index++] = byte;
	        if (p->index >= p->len) {
	            p->state = PARSER_WAIT_CRC_L;
	        }
	        break;
	
	    case PARSER_WAIT_CRC_L:
		    p->crc_rx = byte;
		    p->state = PARSER_WAIT_CRC_H;
		    break;
	
	    case PARSER_WAIT_CRC_H:
	        p->crc_rx |= ((uint16_t)byte) << 8;
	    
	        if (crc16_is_valid(p)) {
	            p->frame_ready = true;
	        }       
	 
	        p->state = PARSER_WAIT_SOF;
	        break;
    }
}

void uart_init(comm_ctx_t *ctx) {
	
	const int uart_buffer_size = (1024*2);
	
	const uart_port_t uart_num = UART_NUM_2;
	uart_config_t uart_config = {
		.baud_rate = 115200,
		.data_bits = UART_DATA_8_BITS,
		.parity = UART_PARITY_DISABLE,
		.stop_bits = UART_STOP_BITS_1,
		.flow_ctrl = UART_HW_FLOWCTRL_DISABLE,
	};
	
	ESP_ERROR_CHECK(uart_param_config(uart_num, &uart_config));
	ESP_ERROR_CHECK(uart_set_pin(UART_NUM_2, 17, 16, UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE));
	
	ESP_ERROR_CHECK(uart_driver_install(UART_NUM_2, 
								  uart_buffer_size, 
								  uart_buffer_size, 
									  10, 
									  &uart_queue, 
							    0));

	memset(ctx, 0, sizeof(comm_ctx_t));

	ctx->state = COMM_WAIT_HELLO;
	ctx->tx_pending = false;
	
}

void transmit_data(comm_ctx_t *ctx) {
	
	sensor_data_t sensor = ctx->sensor_data;
			
	static uint8_t  tx[8] = {0};
	static uint8_t frame[256];
	uint16_t crc;
	static uint8_t out_text[256];
	size_t out_text_len = 0;
	memset(out_text, 0, sizeof(out_text));
	
	size_t idx = 0;	
	memset(frame, 0, sizeof(frame));
	memset(tx, 0, sizeof(tx));
	
	switch (ctx->state) {
		
		case (COMM_HELLO_ACK): 
			
			tx[0] = 'H';
		    tx[1] = (char)COMM_HSK_HELLO_ACK;
		    tx[2] = ctx->client_nonce;
		    tx[3] = sizeof(ctx->client_nonce);
		    break;
		    
		case (COMM_SECURE):
			
			tx[0] = 'D';
			tx[1] = (char)COMM_OK;
			tx[2] = sensor.status;
			tx[3] = sensor.sensor_read;
			tx[4] = sensor.status_code;
			break;
			
		case(COMM_IDLE):
		
		case(COMM_READY):
			
			tx[0] = 'H';
			tx[1] =	(char)COMM_HSK_READY_ACK;

			break;
			
		case(COMM_ERROR):
		case(COMM_WAIT_HELLO):
		
			#if NOT_YET_IMPLEMENTED
			
			#endif
			break;
	}
	
	#if NOT_YET_IMPLEMENTED
	//tx[5] = TEST_BYTE;
	//tx[6] = TEST_BYTE;
	//tx[7] = TEST_BYTE;
	#endif
	
	frame[idx++] = START_BYTE;
	frame[idx++] = sizeof(tx);
	
	memcpy(&frame[2], tx, sizeof(tx));
	
	idx += sizeof(tx);
	
	crc = crc16_kermit(frame, idx);
			
	frame[idx++] = crc & 0xFF;
	frame[idx++] = (crc >> 8) & 0xFF;
	frame[idx++] = STOP_BYTE;
	
	size_t frame_len = idx;
	
	if(ctx->state == COMM_HELLO_ACK) { 
				
		uart_write_bytes(UART_NUM_2, (const char*)frame, frame_len);
		
	    ESP_ERROR_CHECK(uart_wait_tx_done(UART_NUM_2, 100));

		return;
	}
	
	if(ctx->state == COMM_READY) {
				
		uart_write_bytes(UART_NUM_2, (const char*)frame, frame_len);
		
		ctx->state = COMM_SECURE;
		ctx->tx_pending = true;
					
	    ESP_ERROR_CHECK(uart_wait_tx_done(UART_NUM_2, 100));

		return;
	}
	
	if(ctx->state == COMM_SECURE) {
			
		encrypt_frame(&ctx->crypto, frame, frame_len, out_text, &out_text_len);
			
		uart_write_bytes(UART_NUM_2, (const char*)out_text, out_text_len);
					
		ctx->tx_pending = true;
		
		ESP_ERROR_CHECK(uart_wait_tx_done(UART_NUM_2, 100));
	
	}

}

