#include "Bluetooth.h"
#include "HC-05.h"
#include "uart3.h"
#include "Delay.h"
#include "FreeRTOS.h"
#include "task.h"
#include "stdio.h"
#include <string.h>

// 蓝牙接收缓冲区
static uint8_t bluetooth_rx_buffer[BLUETOOTH_RX_BUFFER_SIZE];
static uint16_t bluetooth_rx_len = 0;

// 蓝牙状态
static uint8_t bluetooth_status = BLUETOOTH_STATUS_DISCONNECTED;

/**
 * @brief  蓝牙任务初始化
 * @param  baudrate: 波特率
 * @retval BLUETOOTH_OK: 成功, BLUETOOTH_ERROR: 失败
 */
uint8_t Bluetooth_Init(uint32_t baudrate)
{
    printf("Initializing Bluetooth on UART3...\r\n");
    
    // 初始化HC-05蓝牙模块 - 只初始化UART3，不发送AT指令
    if(HC05_Init(baudrate) != BLUETOOTH_OK)
    {
        printf("HC-05 initialization failed\r\n");
        return BLUETOOTH_ERROR;
    }
    
    // 启动数据接收
    HC05_Receive_Start();
    
    bluetooth_status = BLUETOOTH_STATUS_DISCONNECTED;
    printf("Bluetooth initialization complete - ready for connection\r\n");
    
    return BLUETOOTH_OK;
}

/**
 * @brief  蓝牙任务处理函数
 * @param  None
 * @retval None
 */
void Bluetooth_Process_Task(void)
{
    // 检查是否有数据接收
    if(uart3_rx_len > 0)
    {
        // 复制接收到的数据
        if(uart3_rx_len <= BLUETOOTH_RX_BUFFER_SIZE)
        {
            memcpy(bluetooth_rx_buffer, uart3_buffer, uart3_rx_len);
            bluetooth_rx_len = uart3_rx_len;
            
            // 打印接收到的数据
            printf("Bluetooth received %d bytes: ", bluetooth_rx_len);
            for(uint16_t i = 0; i < bluetooth_rx_len; i++)
            {
                printf("%c", bluetooth_rx_buffer[i]);
            }
            printf("\r\n");
            
            // 处理蓝牙命令
            HC05_Process_Commands((char*)bluetooth_rx_buffer);
            
            // 清空UART3接收缓冲区
            memset(uart3_buffer, 0, UART3_BUF_SIZE);
            uart3_rx_len = 0;
        }
        else
        {
            // 数据过长，清空缓冲区
            printf("Bluetooth data overflow, cleared\r\n");
            memset(uart3_buffer, 0, UART3_BUF_SIZE);
            uart3_rx_len = 0;
        }
    }
    
    // 检查蓝牙连接状态
    uint8_t current_status = HC05_Get_Status();
    if(current_status == HC05_STATUS_CONNECTED && 
       bluetooth_status == BLUETOOTH_STATUS_DISCONNECTED)
    {
        bluetooth_status = BLUETOOTH_STATUS_CONNECTED;
        printf("Bluetooth connected - Ready for communication\r\n");
    }
    else if(current_status == HC05_STATUS_DISCONNECTED && 
            bluetooth_status == BLUETOOTH_STATUS_CONNECTED)
    {
        bluetooth_status = BLUETOOTH_STATUS_DISCONNECTED;
        printf("Bluetooth disconnected - Waiting for connection\r\n");
    }
    else if(bluetooth_status == BLUETOOTH_STATUS_DISCONNECTED)
    {
        // 定期显示等待连接状态（每10秒显示一次）
        static uint32_t last_status_time = 0;
        uint32_t current_time = xTaskGetTickCount();
        if((current_time - last_status_time) > pdMS_TO_TICKS(10000))
        {
            printf("Bluetooth waiting for connection...\r\n");
            last_status_time = current_time;
        }
    }
}

/**
 * @brief  蓝牙发送数据
 * @param  data: 数据缓冲区
 * @param  len: 数据长度
 * @retval BLUETOOTH_OK: 成功, BLUETOOTH_ERROR: 失败
 */
uint8_t Bluetooth_Send_Data(uint8_t *data, uint16_t len)
{
    if(HC05_Send_Data(data, len) == HC05_OK)
    {
        return BLUETOOTH_OK;
    }
    return BLUETOOTH_ERROR;
}

/**
 * @brief  蓝牙发送字符串
 * @param  str: 字符串
 * @retval BLUETOOTH_OK: 成功, BLUETOOTH_ERROR: 失败
 */
uint8_t Bluetooth_Send_String(char *str)
{
    if(HC05_Send_String(str) == HC05_OK)
    {
        return BLUETOOTH_OK;
    }
    return BLUETOOTH_ERROR;
}

/**
 * @brief  获取蓝牙连接状态
 * @retval BLUETOOTH_STATUS_CONNECTED: 已连接, BLUETOOTH_STATUS_DISCONNECTED: 未连接
 */
uint8_t Bluetooth_Get_Status(void)
{
    return bluetooth_status;
}

/**
 * @brief  蓝牙发送传感器数据
 * @param  temperature: 温度值
 * @param  humidity: 湿度值
 * @retval BLUETOOTH_OK: 成功, BLUETOOTH_ERROR: 失败
 */
uint8_t Bluetooth_Send_Sensor_Data(float temperature, float humidity)
{
    char sensor_data[64];
    int len;
    
    // 格式化传感器数据
    len = snprintf(sensor_data, sizeof(sensor_data), 
                   "{\"temperature\":%.2f,\"humidity\":%.2f}\r\n", 
                   temperature, humidity);
    
    // 发送数据
    if(len > 0)
    {
        return Bluetooth_Send_String(sensor_data);
    }
    
    return BLUETOOTH_ERROR;
}

/**
 * @brief  蓝牙扫描附近的设备
 * @param  None
 * @retval 发现的设备数量
 */
uint8_t Bluetooth_Scan_Devices(void)
{
    char device_list[8][32]; // 最多存储8个设备信息
    uint8_t device_count;
    
    printf("Starting Bluetooth device scan...\r\n");
    
    // 扫描附近的蓝牙设备
    device_count = HC05_Scan_Devices(device_list, 8, 10000); // 10秒超时
    
    // 获取已配对设备
    printf("\r\nChecking paired devices...\r\n");
    HC05_Get_Paired_Devices(device_list, 8);
    
    return device_count;
}

/**
 * @brief  蓝牙发送系统信息
 * @param  None
 * @retval BLUETOOTH_OK: 成功, BLUETOOTH_ERROR: 失败
 */
uint8_t Bluetooth_Send_System_Info(void)
{
    char sys_info[128];
    int len;
    
    // 获取系统时间 - 使用FreeRTOS的tick计数
    uint32_t tick = xTaskGetTickCount();
    
    // 格式化系统信息
    len = snprintf(sys_info, sizeof(sys_info), 
                   "{\"device\":\"STM32_HC05\",\"uptime\":%lu,\"status\":\"%s\"}\r\n", 
                   tick,
                   bluetooth_status == BLUETOOTH_STATUS_CONNECTED ? "connected" : "disconnected");
    
    // 发送数据
    if(len > 0)
    {
        return Bluetooth_Send_String(sys_info);
    }
    
    return BLUETOOTH_ERROR;
}
