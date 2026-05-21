#ifndef STATUS_CODES_H
#define STATUS_CODES_H

#include <stdint.h>
#include <stdbool.h>

typedef uint16_t status_code_t;

// ==================
// Modules IDs
// ==================

#define MODULE_COMMON 	(0x0 << 12)
#define MODULE_LIDAR  	(0x1 << 12)
#define MODULE_ADC	  	(0x2 << 12)
#define MODULE_IMU		(0x3 << 12)
#define MODULE_POWER 	(0x4 << 12)
#define MODULE_COMM		(0x5 << 12)
#define MODULE_SYSTEM	(0xF << 12)

// ==================
// Severity levels
// ==================

#define SEV_OK			(0x0 << 8)
#define SEV_WARN		(0x1 << 8)
#define SEV_ERROR		(0x2 << 8)
#define SEV_FATAL		(0x3 << 8)

// ==================
// Macro to build and extract codes
// ==================

#define MAKE_STATUS(mod, sev, sub) 	((status_code_t)((mod)|(sev)|((sub) & 0xFF)))
#define GET_MODULE(code)			(((code) >> 12) & 0xF)
#define GET_SEVERITY(code)			(((code) >> 8) & 0xF)
#define GET_SUBCODE(code)			((code) & 0xF)

#define IS_OK(code)					(GET_SEVERITY(code) == 0)
#define IS_ERROR(code)				(GET_SEVERITY(code) >= 2)

// ==================
// COMMON CODES (0x0---)
// ==================

#define STATUS_OK 					MAKE_STATUS(MODULE_COMMON, SEV_OK, 	  0X00)
#define STATUS_UNKNOWN				MAKE_STATUS(MODULE_COMMON, SEV_ERROR, 0X01)
#define STATUS_TIMEOUT				MAKE_STATUS(MODULE_COMMON, SEV_ERROR, 0x02)
#define STATUS_INVALID_DATA			MAKE_STATUS(MODULE_COMMON, SEV_ERROR, 0x03)
#define STATUS_MEMMOY_FAIL 			MAKE_STATUS(MODULE_COMMON, SEV_FATAL, 0x04)
#define STATUS_INIT_FAIL			MAKE_STATUS(MODULE_COMMON, SEV_ERROR, 0x05)
#define STATUS_UNSUPPORTED_FEATURE	MAKE_STATUS(MODULE_COMMON, SEV_WARN,  0x06)

// =================
// LIDAR MODULE (0x1---)
// =================

// =================
// ADC MODULE (0X2---)
// =================

#define ADC_OK						MAKE_STATUS(MODULE_ADC, SEV_OK,    0X00)
#define ADC_FAIL					MAKE_STATUS(MODULE_ADC, SEV_ERROR, 0X01)
#define ADC_OUT_OF_RANGE			MAKE_STATUS(MODULE_ADC, SEV_WARN,  0X02)
#define ADC_DISCONNECTED			MAKE_STATUS(MODULE_ADC, SEV_FATAL, 0X03)
#define ADC_INIT					MAKE_STATUS(MODULE_ADC, SEV_OK,    0X04)
// =================
// MOTOR MODULE (0x2---)
// =================

// =================
// IMU MODULE (0x3---)
// =================

// =================
// POWER MODULE (0x4---)
// =================

// ==================
// COMMUNICATION MODULE (0X5---)
// ==================

#define COMM_OK					MAKE_STATUS(MODULE_COMM, SEV_OK, 	0X00)
#define COMM_UART_FAIL			MAKE_STATUS(MODULE_COMM, SEV_ERROR, 0x01)
#define COMM_I2C_FAIL			MAKE_STATUS(MODULE_COMM, SEV_ERROR, 0x02)
#define COMM_SPI_FAIL			MAKE_STATUS(MODULE_COMM, SEV_ERROR, 0x03)
#define COMM_HSK_HELLO		    MAKE_STATUS(MODULE_COMM, SEV_OK, 0x04)
#define COMM_HSK_HELLO_ACK	    MAKE_STATUS(MODULE_COMM, SEV_OK, 0x05)
#define COMM_HSK_READY		    MAKE_STATUS(MODULE_COMM, SEV_OK, 0x06)
#define COMM_HSK_READY_ACK		MAKE_STATUS(MODULE_COMM, SEV_OK, 0x07)
#define COMM_HSK_DEF			MAKE_STATUS(MODULE_COMM, SEV_OK, 0x08)

// ==================
// SYSTEM-WIDE FATALS
// ==================

#define SYSTEM_PANIC			MAKE_STATUS(MODULE_SYSTEM, SEV_FATAL, 0x01)
#define SYSTEM_STACK_OVERFLOW	MAKE_STATUS(MODULE_SYSTEM, SEV_FATAL, 0x02)

#endif //STATUS_CODES_H
