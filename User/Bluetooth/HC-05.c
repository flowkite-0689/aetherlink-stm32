#include "HC-05.h"
#include "uart3.h"
#include "Delay.h"
#include "FreeRTOS.h"
#include "task.h"
#include <string.h>
#include <stdlib.h>
#include "debug.h"

// HC-05状态变量
static uint8_t hc05_connection_status = HC05_STATUS_DISCONNECTED;

/**
 * @brief  初始化HC-05蓝牙模块
 * @param  baudrate: 波特率
 * @retval HC05_OK: 成功, HC05_ERROR: 失败
 */
uint8_t HC05_Init(uint32_t baudrate)
{
    // 初始化UART3 - 类似于参考代码中的USART1_Init
    UART3_DMA_RX_Init(baudrate);
    
    // 延时等待模块启动
    Delay_ms(1000);
    
    printf("HC-05 UART3 initialized at %d baud\r\n", baudrate);
    
    // 初始化状态为未连接
    hc05_connection_status = HC05_STATUS_DISCONNECTED;
    
    return HC05_OK; // 直接返回成功，不需要AT指令测试
}

/**
 * @brief  设置HC-05为从模式（等待连接）
 * @param  None
 * @retval HC05_OK: 成功, HC05_ERROR: 失败
 */
uint8_t HC05_Set_Slave_Mode(void)
{
    // 清空接收缓冲区
    memset(uart3_buffer, 0, UART3_BUF_SIZE);
    uart3_rx_len = 0;
    
    // 设置为从模式
    if(HC05_Send_AT_Cmd("AT+ROLE=0", "OK", 1000) == HC05_OK)
    {
        printf("HC-05 set to slave mode successfully\r\n");
        return HC05_OK;
    }
    else
    {
        printf("Failed to set HC-05 to slave mode (may already be connected)\r\n");
        return HC05_ERROR;
    }
}

/**
 * @brief  HC-05扫描附近的蓝牙设备
 * @param  device_list: 设备列表存储缓冲区
 * @param  max_devices: 最大存储设备数量
 * @param  timeout: 扫描超时时间(ms)
 * @retval 实际发现的设备数量
 */
uint8_t HC05_Scan_Devices(char device_list[][32], uint8_t max_devices, uint16_t timeout)
{
    uint8_t device_count = 0;
    uint32_t start_time;
    
    // 清空接收缓冲区
    memset(uart3_buffer, 0, UART3_BUF_SIZE);
    uart3_rx_len = 0;
    
    // 发送查询指令
    HC05_Send_String("AT+INQ\r\n");
    
    // 等待扫描结果
    start_time = xTaskGetTickCount();
    while((xTaskGetTickCount() - start_time) < pdMS_TO_TICKS(timeout) && 
          device_count < max_devices)
    {
        if(uart3_rx_len > 0)
        {
            char *buffer = (char*)uart3_buffer;
            char *line = strtok(buffer, "\r\n");
            
            while(line != NULL && device_count < max_devices)
            {
                // 查找+INQ开头的行
                if(strncmp(line, "+INQ:", 5) == 0)
                {
                    // 提取设备地址信息（简化处理）
                    sscanf(line, "+INQ:%s", device_list[device_count]);
                    device_count++;
                }
                line = strtok(NULL, "\r\n");
            }
        }
        
        Delay_ms(100);
    }
    
    printf("Bluetooth scan completed, found %d devices\r\n", device_count);
    for(uint8_t i = 0; i < device_count; i++)
    {
        printf("Device %d: %s\r\n", i+1, device_list[i]);
    }
    
    return device_count;
}

/**
 * @brief  HC-05查询已配对的设备列表
 * @param  device_list: 设备列表存储缓冲区
 * @param  max_devices: 最大存储设备数量
 * @retval 实际发现的设备数量
 */
uint8_t HC05_Get_Paired_Devices(char device_list[][32], uint8_t max_devices)
{
    uint8_t device_count = 0;
    
    // 清空接收缓冲区
    memset(uart3_buffer, 0, UART3_BUF_SIZE);
    uart3_rx_len = 0;
    
    // 发送查询配对设备指令
    HC05_Send_String("AT+ADCN?\r\n");
    Delay_ms(1000);
    
    if(uart3_rx_len > 0)
    {
        char *buffer = (char*)uart3_buffer;
        char *line = strtok(buffer, "\r\n");
        
        while(line != NULL && device_count < max_devices)
        {
            // 查找+ADCN开头的行
            if(strncmp(line, "+ADCN:", 6) == 0)
            {
                // 提取设备信息
                strncpy(device_list[device_count], line + 7, 31);
                device_list[device_count][31] = '\0';
                device_count++;
            }
            line = strtok(NULL, "\r\n");
        }
    }
    
    printf("Found %d paired devices\r\n", device_count);
    for(uint8_t i = 0; i < device_count; i++)
    {
        printf("Paired device %d: %s\r\n", i+1, device_list[i]);
    }
    
    return device_count;
}

/**
 * @brief  HC-05取消所有配对设备
 * @retval HC05_OK: 成功, HC05_ERROR: 失败
 */
uint8_t HC05_Clear_Paired_Devices(void)
{
    printf("Clearing all paired devices...\r\n");
    
    // 发送清除配对指令
    if(HC05_Send_AT_Cmd("AT+RMAAD", "OK", 2000) == HC05_OK)
    {
        printf("All paired devices cleared successfully\r\n");
        return HC05_OK;
    }
    else
    {
        printf("Failed to clear paired devices\r\n");
        return HC05_ERROR;
    }
}

/**
 * @brief  HC-05断开当前连接
 * @retval HC05_OK: 成功, HC05_ERROR: 失败
 */
uint8_t HC05_Disconnect(void)
{
    printf("Disconnecting current Bluetooth connection...\r\n");
    
    // 发送断开连接指令
    if(HC05_Send_AT_Cmd("AT+DISC", "OK", 2000) == HC05_OK)
    {
        hc05_connection_status = HC05_STATUS_DISCONNECTED;
        printf("Bluetooth disconnected successfully\r\n");
        return HC05_OK;
    }
    else
    {
        printf("Failed to disconnect Bluetooth\r\n");
        return HC05_ERROR;
    }
}

/**
 * @brief  HC-05设置连接超时时间
 * @param  timeout_seconds: 超时时间（秒）
 * @retval HC05_OK: 成功, HC05_ERROR: 失败
 */
uint8_t HC05_Set_Connection_Timeout(uint8_t timeout_seconds)
{
    char cmd[32];
    sprintf(cmd, "AT+IAC=%s", "9E8B33"); // 设置查询接入码
    if(HC05_Send_AT_Cmd(cmd, "OK", 1000) != HC05_OK)
    {
        return HC05_ERROR;
    }
    
    sprintf(cmd, "AT+CNCTO=%d", timeout_seconds);
    if(HC05_Send_AT_Cmd(cmd, "OK", 1000) == HC05_OK)
    {
        printf("Connection timeout set to %d seconds\r\n", timeout_seconds);
        return HC05_OK;
    }
    
    return HC05_ERROR;
}

/**
 * @brief  获取HC-05模块信息
 * @retval HC05_OK: 成功, HC05_ERROR: 失败
 */
uint8_t HC05_Get_Module_Info(void)
{
    printf("=== HC-05 Module Information ===\r\n");
    
    // 获取版本信息
    if(HC05_Send_AT_Cmd("AT+VERSION?", "+VERSION:", 1000) == HC05_OK)
    {
        if(uart3_rx_len > 0)
        {
            char *buffer = (char*)uart3_buffer;
            char *version = strstr(buffer, "+VERSION:");
            if(version)
            {
                printf("Version: %s\r\n", version + 9);
            }
        }
    }
    
    // 获取设备地址
    memset(uart3_buffer, 0, UART3_BUF_SIZE);
    uart3_rx_len = 0;
    if(HC05_Send_AT_Cmd("AT+ADDR?", "+ADDR:", 1000) == HC05_OK)
    {
        if(uart3_rx_len > 0)
        {
            char *buffer = (char*)uart3_buffer;
            char *addr = strstr(buffer, "+ADDR:");
            if(addr)
            {
                printf("Address: %s\r\n", addr + 6);
            }
        }
    }
    
    // 获取当前角色
    memset(uart3_buffer, 0, UART3_BUF_SIZE);
    uart3_rx_len = 0;
    if(HC05_Send_AT_Cmd("AT+ROLE?", "+ROLE:", 1000) == HC05_OK)
    {
        if(uart3_rx_len > 0)
        {
            char *buffer = (char*)uart3_buffer;
            char *role = strstr(buffer, "+ROLE:");
            if(role)
            {
                uint8_t role_num = atoi(role + 6);
                printf("Role: %s\r\n", role_num == 0 ? "Slave" : "Master");
            }
        }
    }
    
    printf("================================\r\n");
    return HC05_OK;
}

/**
 * @brief  HC-05设置为主模式
 * @param  None
 * @retval HC05_OK: 成功, HC05_ERROR: 失败
 */
uint8_t HC05_Set_Master_Mode(void)
{
    // 设置为主模式
    if(HC05_Send_AT_Cmd("AT+ROLE=1", "OK", 1000) == HC05_OK)
    {
        printf("HC-05 set to master mode successfully\r\n");
        return HC05_OK;
    }
    else
    {
        printf("Failed to set HC-05 to master mode\r\n");
        return HC05_ERROR;
    }
}

/**
 * @brief  HC-05发送AT指令
 * @param  cmd: AT指令字符串
 * @param  wait_string: 等待响应的字符串
 * @param  timeout: 超时时间(ms)
 * @retval HC05_OK: 成功, HC05_ERROR: 失败
 */
uint8_t HC05_Send_AT_Cmd(const char *cmd, const char *wait_string, uint16_t timeout)
{
    uint16_t len;
    char *response;
    uint32_t start_time;
    
    // 清空接收缓冲区
    memset(uart3_buffer, 0, UART3_BUF_SIZE);
    uart3_rx_len = 0;
    
    // 发送AT指令
    len = strlen(cmd);
    printf("Sending AT command: %s\r\n", cmd);
    UART3_SendDataToBLE_Poll((uint8_t*)cmd, len);
    UART3_SendDataToBLE_Poll((uint8_t*)"\r\n", 2);
    
    // 等待响应 - 使用FreeRTOS的tick计数
    start_time = xTaskGetTickCount();
    while((xTaskGetTickCount() - start_time) < pdMS_TO_TICKS(timeout))
    {
        if(uart3_rx_len > 0)
        {
            response = (char*)uart3_buffer;
            printf("HC-05 response: %s\r\n", response);
            if(strstr(response, wait_string) != NULL)
            {
                return HC05_OK;
            }
            // 检查是否返回ERROR
            if(strstr(response, "ERROR") != NULL)
            {
                return HC05_ERROR;
            }
        }
        Delay_ms(10);
    }
    
    printf("AT command timeout after %dms\r\n", timeout);
    return HC05_ERROR;
}

/**
 * @brief  设置HC-05蓝牙名称
 * @param  name: 蓝牙名称
 * @retval HC05_OK: 成功, HC05_ERROR: 失败
 */
uint8_t HC05_Set_Name(char *name)
{
    char cmd[32];
    
    // 清空接收缓冲区
    memset(uart3_buffer, 0, UART3_BUF_SIZE);
    uart3_rx_len = 0;
    
    sprintf(cmd, "AT+NAME=%s", name);
    if(HC05_Send_AT_Cmd(cmd, "OK", 1000) == HC05_OK)
    {
        printf("Bluetooth name set successfully to: %s\r\n", name);
        return HC05_OK;
    }
    else
    {
        printf("Failed to set Bluetooth name\r\n");
        return HC05_ERROR;
    }
}

/**
 * @brief  设置HC-05蓝牙PIN码
 * @param  pin: PIN码
 * @retval HC05_OK: 成功, HC05_ERROR: 失败
 */
uint8_t HC05_Set_PIN(char *pin)
{
    char cmd[16];
    
    // 清空接收缓冲区
    memset(uart3_buffer, 0, UART3_BUF_SIZE);
    uart3_rx_len = 0;
    
    sprintf(cmd, "AT+PSWD=%s", pin);
    if(HC05_Send_AT_Cmd(cmd, "OK", 1000) == HC05_OK)
    {
        printf("Bluetooth PIN set successfully to: %s\r\n", pin);
        return HC05_OK;
    }
    else
    {
        printf("Failed to set Bluetooth PIN\r\n");
        return HC05_ERROR;
    }
}

/**
 * @brief  HC-05开始发现设备
 * @retval HC05_OK: 成功, HC05_ERROR: 失败
 */
uint8_t HC05_Discover(void)
{
    // 在主模式下，HC-05会自动进入可发现模式
    // 也可以发送AT+INQ查询设备
    return HC05_Send_AT_Cmd("AT+INQ", "OK", 3000);
}

/**
 * @brief  HC-05连接指定设备
 * @param  device_name: 设备名称
 * @param  timeout: 超时时间(ms)
 * @retval HC05_OK: 成功, HC05_ERROR: 失败
 */
uint8_t HC05_Connect_Device(uint8_t *device_name, uint16_t timeout)
{
    uint32_t start_time;
    
    // 清空接收缓冲区
    memset(uart3_buffer, 0, UART3_BUF_SIZE);
    uart3_rx_len = 0;
    
    // 等待连接 - 使用FreeRTOS的tick计数
    start_time = xTaskGetTickCount();
    while((xTaskGetTickCount() - start_time) < pdMS_TO_TICKS(timeout))
    {
        if(uart3_rx_len > 0)
        {
            // 检查连接成功标识
            if(strstr((char*)uart3_buffer, "CONNECT") != NULL)
            {
                hc05_connection_status = HC05_STATUS_CONNECTED;
                return HC05_OK;
            }
        }
        Delay_ms(100);
    }
    
    hc05_connection_status = HC05_STATUS_DISCONNECTED;
    return HC05_ERROR;
}

/**
 * @brief  HC-05发送数据（直接发送，不检查连接状态）
 * @param  data: 数据缓冲区
 * @param  len: 数据长度
 * @retval HC05_OK: 成功, HC05_ERROR: 失败
 */
uint8_t HC05_Send_Data(uint8_t *data, uint16_t len)
{
    // 使用UART3发送数据，类似于参考代码中的USART_SendData
    return UART3_SendDataToBLE_Poll(data, len);
}

/**
 * @brief  HC-05发送字符串（直接发送，不检查连接状态）
 * @param  str: 字符串
 * @retval HC05_OK: 成功, HC05_ERROR: 失败
 */
uint8_t HC05_Send_String(char *str)
{
    // 使用UART3发送字符串，类似于参考代码
    return UART3_SendDataToBLE_Poll((uint8_t*)str, strlen(str));
}

/**
 * @brief  HC-05获取连接状态（检查是否有数据传输）
 * @param  None
 * @retval HC05_STATUS_CONNECTED: 已连接, HC05_STATUS_DISCONNECTED: 未连接
 */
uint8_t HC05_Check_Connection_By_Data(void)
{
    // 简单的连接检测：如果接收到数据就认为已连接
    // HC-05蓝牙模块连接后不会发送CONNECT字符串
    // 只要有数据传输就说明连接成功
    return hc05_connection_status;
}

/**
 * @brief  获取HC-05连接状态
 * @retval HC05_STATUS_CONNECTED: 已连接, HC05_STATUS_DISCONNECTED: 未连接
 */
uint8_t HC05_Get_Status(void)
{
    return HC05_Check_Connection_By_Data();
}

/**
 * @brief  检测HC-05连接状态
 * @param  None
 * @retval HC05_STATUS_CONNECTED: 已连接, HC05_STATUS_DISCONNECTED: 未连接
 */
uint8_t HC05_Check_Connection(void)
{
    // 检查接收缓冲区中的连接状态信息
    if(uart3_rx_len > 0)
    {
        // 有数据接收说明已经连接（HC-05连接后不会发送CONNECT字符串）
        // 只要接收到数据就认为连接成功
        hc05_connection_status = HC05_STATUS_CONNECTED;
        
        // 检查断开连接标识
        char *buffer = (char*)uart3_buffer;
        if(strstr(buffer, "DISCONNECT") != NULL || 
           strstr(buffer, "ERROR") != NULL)
        {
            hc05_connection_status = HC05_STATUS_DISCONNECTED;
        }
    }
    
    return hc05_connection_status;
}

/**
 * @brief  启动HC-05数据接收
 * @retval None
 */
void HC05_Receive_Start(void)
{
    
}

/**
 * @brief  HC-05解析命令
 * @param  buffer: 接收缓冲区
 * @param  topic: 命令主题
 * @param  msg_value: 消息值
 * @retval HC05_OK: 成功, HC05_ERROR: 失败
 */
uint8_t HC05_Parse_Command(const char *buffer, const char *topic, char *msg_value)
{
    char *start;
    char *end;
    int len;
    
    // 查找主题
    start = strstr(buffer, topic);
    if(start == NULL)
    {
        return HC05_ERROR;
    }
    
    // 移动到值部分
    start = strchr(start, ':');
    if(start == NULL)
    {
        return HC05_ERROR;
    }
    start++; // 跳过':'
    
    // 查找结束符
    end = strchr(start, '\n');
    if(end == NULL)
    {
        end = strchr(start, '\r');
        if(end == NULL)
        {
            return HC05_ERROR;
        }
    }
    
    // 提取值
    len = end - start;
    strncpy(msg_value, start, len);
    msg_value[len] = '\0';
    
    return HC05_OK;
}

/**
 * @brief  HC-05处理命令
 * @param  buffer: 接收缓冲区
 * @retval HC05_OK: 成功, HC05_ERROR: 失败
 */
uint8_t HC05_Process_Commands(const char *buffer)
{
    // 解析LED控制命令
    char msg_value[64];
    
    if(HC05_Parse_Command(buffer, "LED", msg_value) == HC05_OK)
    {
        if(strstr(msg_value, "ON") != NULL)
        {
            // 打开LED
            GPIO_WriteBit(GPIOC, GPIO_Pin_13, Bit_RESET);
        }
        else if(strstr(msg_value, "OFF") != NULL)
        {
            // 关闭LED
            GPIO_WriteBit(GPIOC, GPIO_Pin_13, Bit_SET);
        }
        return HC05_OK;
    }
    
    // 解析其他命令...
    
    return HC05_ERROR;
}
