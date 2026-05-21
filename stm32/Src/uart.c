#include "main.h"
#include "adc.h"
#include "uart.h"
#include "encrypt.h"
#include "status_codes.h"

#include "bignum.h"
#include <string.h>
#include <stdio.h>

#define LOG_STATE(s) printf("[STATE] %s\n", comm_state_to_str(s))

extern UART_HandleTypeDef huart3;

extern uint8_t psk[32];

const char* comm_state_to_str(comm_state_t state) {
    switch (state) {
        case 0:					return "COMM_IDLE";
        case 1:					return "COMM_HELLO";
        case 2:					return "COMM_WAIT_HELLO";
        case 3:					return "COMM_WAIT_ACK";
        case 4:     			return "COMM_HELLO_ACK";
        case 5:					return "COMM_SECURE";
        case 6:      			return "COMM_ERROR";
        default:              	return "COMM_UNKNOWN";
    }
}

void uart_parser_reset(uart_parser_t *p)
{
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
	uint8_t crc_len = 0;

 	rx[crc_len++] = START_BYTE;
    rx[crc_len++] = p->len;

   	memcpy(&rx[crc_len], p->payload, p->len);
    crc_len += p->len;

    crc_calc = crc16_kermit(rx, crc_len);
    return (crc_calc == p->crc_rx);
}

void uart_parse_frame(uart_parser_t *p, uint8_t *buffer, uint8_t len)
{

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

    switch(p->state) {

	    case PARSER_WAIT_SOF:

	        if(byte == START_BYTE) {
	        	p->index = 0;
				p->frame_ready = false;
	            p->state = PARSER_WAIT_LEN;
	        }
	        break;

	    case PARSER_WAIT_LEN:

	        if(byte == 0 || byte > MAX_PAYLOAD) {
	            p->state = PARSER_WAIT_SOF;
	            break;
	        }
	        p->len = byte;
	        p->state = (p->len == 0) ? PARSER_WAIT_CRC_L
	                                 : PARSER_WAIT_PAYLOAD;
	        break;

	    case PARSER_WAIT_PAYLOAD:

	    	if(p->index < MAX_PAYLOAD) {
	    	        p->payload[p->index++] = byte;
	    	} else {
	    	  p->state = PARSER_WAIT_SOF;
	    	  break;
	    	}

	        if(p->index >= p->len) {
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
	        	p->type = p->payload[0];
	            p->frame_ready = true;
	        }

	        p->state = PARSER_WAIT_SOF;
	        break;
    }
}

void uart_init(comm_ctx_t *ctx, uart_parser_t *parser) {

	memset(ctx, 0, sizeof(comm_ctx_t));
	ctx->state = COMM_IDLE;
	ctx->tx_pending = true;

}

void process_frame(comm_ctx_t *ctx, uart_parser_t *parser, uint8_t len)
{

	uint8_t msg  = parser->payload[1];

    switch (msg)
    {

        /* ==============================
     	* COMM_SECURE
    	* =============================== */
    	case 0x00:

			#if TO_BE_IMPLEMENTED
    		uint8_t a = parser->payload[2];
    		uint8_t b = parser->payload[3];
    		uint8_t c = parser->payload[4];
			#endif

			//decrypt_frame(&ctx->crypto, &rx_byte, sizeof(rx_byte), plaintext, plaintext_len);

			if(ctx->state != COMM_SECURE) break;

			ctx->tx_pending = true;

    		break;

    	/* ==============================
    	* COMM_HELLO_ACK
    	* =============================== */
        case 0x05:

        	if(ctx->state == COMM_HELLO_ACK) break;

        	memcpy(&ctx->client_nonce,&parser->payload[2],sizeof(ctx->client_nonce));

        	if(ctx->client_nonce != 0) {

        		derive_session_key(&ctx->crypto, psk, sizeof(psk), &ctx->client_nonce, sizeof(ctx->client_nonce), &ctx->server_nonce, sizeof(ctx->server_nonce));

        		ctx->state = COMM_HELLO_ACK;
        		ctx->tx_pending = true;
        	}

            break;

        /* ==============================
        * COMM_READY
        * =============================== */

        case 0x07:

        	if(ctx->state == COMM_SECURE) break;

        	ctx->state = COMM_SECURE;
        	ctx->tx_pending = true;

            break;

        default:

            break;
    }
}

void transmit_data(comm_ctx_t *ctx) {

	sensor_data_t sensor = ctx->sensor_data;
	static uint8_t tx[8] = {0};
	static uint8_t frame[256] = {0};

	static uint8_t out_text[256];
	size_t out_text_len = 0;

	uint16_t crc;
	size_t idx = 0;

	memset(frame, 0, sizeof(frame));
	memset(tx, 0, sizeof(tx));
	memset(out_text, 0, sizeof(out_text));

	switch(ctx->state) {

		case(COMM_IDLE):

			tx[0] = 'H';
			tx[1] =	(char)COMM_HSK_HELLO;
			tx[2] = ctx->server_nonce;
			tx[3] = sizeof(ctx->server_nonce);

			ctx->state = COMM_WAIT_ACK;

			break;

		case(COMM_HELLO_ACK):

			tx[0] = 'H';
			tx[1] =	(char)COMM_HSK_READY;

			break;

		case (COMM_SECURE):
			tx[0] = 'D';
			tx[1] = (char)COMM_OK;
			tx[2] = sensor.status;
			tx[3] = sensor.sensor_read;
			tx[4] = sensor.status_code;
			break;

		case(COMM_HELLO):
		case(COMM_WAIT_HELLO):
		case(COMM_WAIT_ACK):
		case(COMM_ERROR):

			break;
	}

	frame[idx++] = START_BYTE;
	frame[idx++] = sizeof(tx);

	memcpy(&frame[2], tx, sizeof(tx));

	idx += sizeof(tx);

	crc = crc16_kermit(frame, idx);

	frame[idx++] = crc & 0xFF;
	frame[idx++] = (crc >> 8) & 0xFF;
	frame[idx++] = STOP_BYTE;

	size_t frame_len = idx;

	if(ctx->state == COMM_WAIT_ACK || ctx->state == COMM_HELLO_ACK) {

		HAL_UART_Transmit(&huart3, frame, frame_len,100);
	}

	if(ctx->state == COMM_SECURE) {

		encrypt_frame(&ctx->crypto, frame, frame_len, out_text, &out_text_len);

		ctx->tx_pending = true;

		HAL_UART_Transmit(&huart3, out_text, out_text_len,100);
	}
}
