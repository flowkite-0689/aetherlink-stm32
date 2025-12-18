#ifndef __UART3_H
#define __UART3_H

#include "stm32f10x.h"
#include <stdarg.h>

#define UART3_BUF_SIZE 128

extern uint8_t uart3_buffer[UART3_BUF_SIZE];
extern uint8_t uart3_rx_len;

void UART3_DMA_RX_Init(uint32_t baudrate);
uint8_t UART3_SendDataToBLE_Poll(uint8_t *data, uint16_t len);   // 专为蓝牙封装

// UART3 printf功能
int uart3_printf(const char *format, ...);

#endif
