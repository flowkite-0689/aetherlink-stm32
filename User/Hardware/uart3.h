#ifndef __UART3_H
#define __UART3_H

#include "stm32f10x.h"
#include <stdint.h>

// 蓝牙数据缓冲区大小
#define UART3_RX_BUFFER_SIZE 256
#define UART3_TX_BUFFER_SIZE 128

// 蓝牙数据结构体
typedef struct {
    uint8_t buffer[UART3_RX_BUFFER_SIZE];  // 接收缓冲区
    uint16_t head;                         // 缓冲区头指针
    uint16_t tail;                         // 缓冲区尾指针
    uint16_t count;                        // 当前数据量
    uint8_t  data_ready;                   // 数据准备好标志
} uart3_buffer_t;

// 蓝牙命令处理回调函数类型
typedef void (*bluetooth_cmd_callback_t)(const char* cmd, uint16_t len);

// 函数声明
void My_USART3_Init(void);
void USART3_IRQHandler(void);
uint16_t UART3_Read_Data(uint8_t* data, uint16_t len);
uint16_t UART3_Get_Available_Data(void);
void UART3_Clear_Buffer(void);
uint8_t UART3_Send_Data(const uint8_t* data, uint16_t len);
uint8_t UART3_Send_String(const char* str);

// 蓝牙数据处理任务相关
void Bluetooth_Process_Task(void *pvParameters);
void Bluetooth_Register_Callback(bluetooth_cmd_callback_t callback);
void Bluetooth_Process_Command(const char* cmd, uint16_t len);

// 全局缓冲区声明
extern uart3_buffer_t uart3_rx_buffer;

#endif /* __UART3_H */