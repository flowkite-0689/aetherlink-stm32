#ifndef __BLUETOOTH_H
#define __BLUETOOTH_H

#include "stm32f10x.h"
#include <stdint.h>

// 蓝牙缓冲区大小
#define BLUETOOTH_RX_BUFFER_SIZE 128

// 蓝牙状态定义
#define BLUETOOTH_STATUS_DISCONNECTED    0
#define BLUETOOTH_STATUS_CONNECTED       1

// 蓝牙函数返回值
#define BLUETOOTH_OK                     0
#define BLUETOOTH_ERROR                  1

// 蓝牙任务接口函数
uint8_t Bluetooth_Init(uint32_t baudrate);
void Bluetooth_Process_Task(void);
uint8_t Bluetooth_Send_Data(uint8_t *data, uint16_t len);
uint8_t Bluetooth_Send_String(char *str);
uint8_t Bluetooth_Get_Status(void);
uint8_t Bluetooth_Send_Sensor_Data(float temperature, float humidity);
uint8_t Bluetooth_Send_System_Info(void);
uint8_t Bluetooth_Scan_Devices(void);

#endif
