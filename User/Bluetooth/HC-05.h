#ifndef __HC05_H
#define __HC05_H

#include "stm32f10x.h"
#include <stdint.h>

// HC-05状态定义
#define HC05_STATUS_DISCONNECTED   0
#define HC05_STATUS_CONNECTED      1

// HC-05连接超时时间(ms)
#define HC05_CONNECTION_TIMEOUT    5000

// HC-05函数返回值
#define HC05_OK                    0
#define HC05_ERROR                 1

// HC-05命令接口函数
uint8_t HC05_Init(uint32_t baudrate);
uint8_t HC05_Set_Slave_Mode(void);
uint8_t HC05_Set_Master_Mode(void);
uint8_t HC05_Connect_Device(uint8_t *device_name, uint16_t timeout);
uint8_t HC05_Scan_Devices(char device_list[][32], uint8_t max_devices, uint16_t timeout);
uint8_t HC05_Get_Paired_Devices(char device_list[][32], uint8_t max_devices);
uint8_t HC05_Clear_Paired_Devices(void);
uint8_t HC05_Disconnect(void);
uint8_t HC05_Set_Connection_Timeout(uint8_t timeout_seconds);
uint8_t HC05_Get_Module_Info(void);
uint8_t HC05_Send_AT_Cmd(const char *cmd, const char *wait_string, uint16_t timeout);
uint8_t HC05_Send_Data(uint8_t *data, uint16_t len);
uint8_t HC05_Send_String(char *str);
uint8_t HC05_Set_Name(char *name);
uint8_t HC05_Set_PIN(char *pin);
uint8_t HC05_Discover(void);
uint8_t HC05_Get_Status(void);
uint8_t HC05_Check_Connection(void);
void HC05_Receive_Start(void);

// HC-05数据处理函数
uint8_t HC05_Parse_Command(const char *buffer, const char *topic, char *msg_value);
uint8_t HC05_Process_Commands(const char *buffer);

#endif
