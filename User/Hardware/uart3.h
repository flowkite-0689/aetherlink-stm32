#ifndef __UART3_H
#define __UART3_H

#include "stm32f10x.h"
#include <stdint.h>

// 蓝牙数据缓冲区大小
#define UART3_BUF_SIZE 128

// 全局变量声明
extern uint8_t uart3_buffer[UART3_BUF_SIZE];
extern uint8_t uart3_rx_len;

// 蓝牙命令处理回调函数类型
typedef void (*bluetooth_cmd_callback_t)(const char* cmd, uint16_t len);

// 函数声明
void UART3_GPIO_Init(void);
void UART3_Init(uint32_t baudrate);
void UART3_DMA_Init(void);
void USART3_IRQHandler(void);
void UART3_DMA_RX_Init(uint32_t baudrate);
uint8_t UART3_SendDataToBluetooth_Poll(uint8_t *data, uint16_t len);

// 蓝牙数据处理任务相关
void Bluetooth_Process_Task(void *pvParameters);
void Bluetooth_Register_Callback(bluetooth_cmd_callback_t callback);
void Bluetooth_Process_Command(const char* cmd, uint16_t len);

#endif /* __UART3_H */