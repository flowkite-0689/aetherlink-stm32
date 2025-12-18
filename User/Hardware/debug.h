#ifndef DEBUG_H
#define DEBUG_H

#include "stm32f10x.h"                  // Device header
#include <stdio.h>

// 定义UART1接收缓冲区大小
#define UART1_BUF_SIZE 128

extern uint8_t uart1_buffer[UART1_BUF_SIZE];
extern uint8_t uart1_rx_len;

void debug_init(void);
void Usart1_Send_Sring(char *string);
void Usart1_send_bytes(uint8_t *buf, uint16_t len);

// DMA发送相关函数
uint8_t UART1_SendDataToDebug_DMA(uint8_t *data, uint16_t len);

#endif
