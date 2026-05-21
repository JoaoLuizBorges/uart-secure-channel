#ifndef UART_H
#define UART_H

#include <stdint.h>
#include <stdbool.h>

#include "driver/uart.h"
#include "encrypt.h"
#include "adc.h"

#define START_BYTE		0xAA
#define STOP_BYTE 		0x55
#define MAX_PAYLOAD 	255

#define CMD_CT_MOTOR 	0x01
#define INIT_CRC_VAL 	0x0000

extern QueueHandle_t uart_queue;

typedef enum {
    COMM_IDLE,             
    COMM_WAIT_HELLO,       
    COMM_HELLO_ACK,
    COMM_READY,
    COMM_SECURE,           
    COMM_ERROR
} comm_state_t;

typedef enum {
    PARSER_WAIT_SOF,
    PARSER_WAIT_LEN,
    PARSER_WAIT_PAYLOAD,
    PARSER_WAIT_CRC_H,
    PARSER_WAIT_CRC_L
} parser_state_t;

typedef struct {
    comm_state_t state;
    parser_state_t parser;
    crypt_context_t crypto;
    sensor_data_t sensor_data;
    uint8_t client_nonce;
    uint8_t server_nonce;
    bool tx_pending;
} comm_ctx_t;

typedef struct {
    parser_state_t state;
    uint16_t crc_rx;
    uint8_t type;
    uint8_t len;
    uint8_t payload[MAX_PAYLOAD];
    uint8_t index;
    bool frame_ready;
    sensor_data_t sensor_data;
} uart_parser_t;

void uart_parser_reset(uart_parser_t *p);

uint16_t crc16_kermit(const uint8_t *data, uint16_t len);

bool crc16_is_valid(const uart_parser_t *p);

void process_frame(comm_ctx_t *ctx, uart_parser_t *parser, uint8_t len);

void uart_parse_frame(uart_parser_t *p, uint8_t *buffer, uint8_t len);
              
void uart_parse_byte(uart_parser_t *p, uint8_t byte);

void uart_init(comm_ctx_t *ctx);

void transmit_data(comm_ctx_t *ctx);

#endif // UART_H