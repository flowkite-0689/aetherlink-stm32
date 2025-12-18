/**
 * @file esp8266.c
 * @author Jaychen (719095404@qq.com)
 * @brief esp8266连接巴法云TCP设备
 * @version 0.1
 * @date 2025-12-09
 * 
 * @copyright Copyright (c) 2025
 * 
 */
#include "esp8266.h"
#include <string.h>
#include <stdio.h>
#include <FreeRTOS.h>
#include <task.h>

extern uint8_t Server_connected=0;
extern uint8_t wifi_connected = 0;
//传输到云端的时间间隔
uint16_t publish_delaytime = 15;

extern uint8_t uart2_buffer[UART2_BUF_SIZE]; // uart2接收缓冲
extern uint8_t uart2_rx_len;     // uart2接收长度

void ESP8266_Receive_Start(void)
{
    uart2_rx_len = 0;
    memset(uart2_buffer, 0, sizeof(uart2_buffer));
}

/**
 * @brief 发送指令，并指定时间内接收指定的数据
 *
 * @param cmd  AT指令注意带\r\n
 * @param wait_string   等待字符串
 * @param timeout  超时时间ms
 * @return uint8_t 0：超时，1：成功
 */
uint8_t ESP8266_Send_AT_Cmd(const char *cmd, const char *wait_string, uint16_t timeout)
{
    uart2_rx_len = 0;
    memset(uart2_buffer, 0, sizeof(uart2_buffer));
    UART2_SendDataToWiFi_Poll((uint8_t *)cmd, strlen(cmd));    // 发送命令
    //printf("ESP8266 Send cmd: %s", cmd);
loop:
    while (uart2_rx_len <= 0 && timeout > 0)
    {
        timeout--;
        vTaskDelay(pdMS_TO_TICKS(1));
    }
    if (uart2_rx_len > 0 && timeout > 0 && strstr((const char *)uart2_buffer, wait_string))
    {
        //printf("uart2_rx_len = %d, uart2_buf = %s\n", uart2_rx_len, uart2_buffer);
        uart2_rx_len = 0; // 清空接收缓冲,继续接收
        return 1;         // 成功
    }
    else if (timeout > 0)
    {
        uart2_rx_len = 0; // 清空接收缓冲,继续接收
        goto loop;
    }
    uart2_rx_len = 0; // 清空接收缓冲,继续接收
    return 0;
}

// 退出透传模式
uint8_t ESP8266_Exit_Transmit_Mode(void)
{
    if (ESP8266_Send_AT_Cmd("+++", NULL, 2000) != 1) // 退出透传模式
    {
        printf("ESP8266 Exit Transmit Mode , Error\r\n");
        return 0;
    }
    printf("ESP8266 Exit Transmit Mode , Success\r\n");
    return 1;
}

uint8_t ESP8266_Connect_WiFi(const char *ssid,const char *password)
{
    uint8_t i = 0;
    char cmd[50];                                       // 指令缓冲
    ESP8266_Exit_Transmit_Mode();                      // 退出透传模式

    while (ESP8266_Send_AT_Cmd("AT\r\n", "OK", 500) != 1) // 测试模块状态
    {
        i++;
        if (i >= 3)
        {
            printf("ESP8266 Send cmd: AT , Error\r\n");
            return 0;
        }
    }
    if (ESP8266_Send_AT_Cmd("ATE0\r\n", "OK", 500) != 1) // 关闭回显
    {
        printf("ESP8266 Send cmd: ATE0 , Error\r\n");
        return 0;
    }
    
    if (ESP8266_Send_AT_Cmd("AT+CWMODE=3\r\n", "OK", 500) != 1) // 设置为station模式
    {
        printf("ESP8266 Send cmd: AT+CWMODE=3 , Error\r\n");
        return 0;
    }

    snprintf(cmd, sizeof(cmd), "AT+CWJAP=\"%s\",\"%s\"\r\n", ssid, password); // 拼接指令
    if (ESP8266_Send_AT_Cmd(cmd, "OK", 8000) != 1)                           // 连接WiFi
    {
        printf("ESP8266 Send cmd: %s, Error\r\n", cmd);
        return 0;
    }
    return 1;
}

// 连接服务器bemfa.com，TCP端口8344, MQTT端口：9501，进入透传模式
uint8_t ESP8266_Connect_Server(const char *ip,const char *port)
{
    char cmd[50];                                            // 指令缓冲
    if (ESP8266_Send_AT_Cmd("AT+CIPMODE=1\r\n", "OK", 2000) != 1) // 设置透传模式
    {
        printf("ESP8266 Send cmd: AT+CIPMODE=1 , Error\r\n");
        return 0;
    }
    // 连接服务器和端口AT+CIPSTART="TCP","bemfa.com",8344
    snprintf(cmd, sizeof(cmd), "AT+CIPSTART=\"TCP\",\"%s\",%s\r\n", ip, port);
    if (ESP8266_Send_AT_Cmd(cmd, "OK", 5000) != 1) // 连接服务器
    {
        printf("ESP8266 Send cmd: %s, Error\r\n", cmd);
        return 0;
    }
    // 进入透传模式，后面发的都会无条件传输
    if (ESP8266_Send_AT_Cmd("AT+CIPSEND\r\n", "OK", 3000) != 1) // 进入透传模式
    {
        printf("ESP8266 Send cmd: AT+CIPSEND\r\n, Error\r\n");
        return 0;
    }
    return 1;
}

// 订阅主题
uint8_t ESP8266_TCP_Subscribe(const char *uid,const char *topic)
{
    char cmd[128]; // 指令缓冲
    snprintf(cmd, sizeof(cmd), "cmd=1&uid=%s&topic=%s", uid, topic);
    if (ESP8266_Send_AT_Cmd(cmd, "cmd=1&res=1", 1000) != 1) // 等待订阅成功
    {
        return 0; // 订阅失败
    }
    return 1; // 订阅成功
}

// 发布主题
uint8_t ESP8266_TCP_Publish(const char *uid,const char *topic, char *data)
{
    char cmd[128]; // 指令缓冲
    // cmd=2&uid=4d9ec352e0376f2110a0c601a2857225&topic=light002&msg=#32#27.80#ON#
    snprintf(cmd, sizeof(cmd), "cmd=2&uid=%s&topic=%s&msg=%s", uid, topic, data);
    if (ESP8266_Send_AT_Cmd(cmd, "cmd=2&res=1", 1000) != 1) // 等待发布成功
    {
        return 0; // 失败
    }
    return 1;
}



// 发送心跳包
uint8_t ESP8266_TCP_Heartbeat(void)
{
    if (ESP8266_Send_AT_Cmd("cmd=0&msg=ping", "cmd=0&res=1", 1000) != 1) // 等待
    {
        return 0;
    }
    return 1;
}
// 获取时间
uint8_t ESP8266_TCP_GetTime(const char *uid, char *time_buffer, uint16_t buffer_size)
{
    char cmd[128];
    snprintf(cmd, sizeof(cmd), "cmd=7&uid=%s&type=1\r\n", uid);
    
    // 清空接收缓冲区
    uart2_rx_len = 0;
    memset(uart2_buffer, 0, sizeof(uart2_buffer));
    
    // 发送时间获取命令
    UART2_SendDataToWiFi_Poll((uint8_t *)cmd, strlen(cmd));
    
    // 等待响应
    uint16_t timeout = 3000;
    while (uart2_rx_len <= 0 && timeout > 0)
    {
        timeout--;
        vTaskDelay(pdMS_TO_TICKS(1));
    }
    
    if (uart2_rx_len > 0 && timeout > 0)
    {
        // 查找时间数据（格式：2021-06-11 16:39:27）
//        char *time_start = (char *)uart2_buffer;
        
        // 直接复制接收到的数据到输出缓冲区
        uint16_t copy_len = uart2_rx_len < buffer_size - 1 ? uart2_rx_len : buffer_size - 1;
        memcpy(time_buffer, uart2_buffer, copy_len);
        time_buffer[copy_len] = '\0';
        
        // 打印接收到的数据用于调试
        printf("Received time data: %s\n", time_buffer);
        
        uart2_rx_len = 0; // 清空接收缓冲
        return 1;
    }
    
    uart2_rx_len = 0; // 清空接收缓冲
    return 0;
}

// ==================================
// 新增消息解析函数
// ==================================

/**
 * @brief 解析命令字符串中的指定参数
 * @param buffer 接收到的命令字符串
 * @param topic 需要查找的topic
 * @param msg_value 存储找到的msg值
 * @return 1-成功找到参数，0-未找到
 */
uint8_t ESP8266_Parse_Command(const char *buffer, const char *topic, char *msg_value)
{
    if (buffer == NULL || topic == NULL || msg_value == NULL) {
        return 0;
    }
    
    // 创建临时字符串进行查找
    char temp_topic[64];
    snprintf(temp_topic, sizeof(temp_topic), "topic=%s", topic);
    
    // printf("Looking for topic: %s in buffer: %s\r\n", temp_topic, buffer);  // 添加调试信息
    
    // 检查是否包含指定的topic
    if (strstr(buffer, temp_topic) == NULL) {
        printf("Topic not found\r\n");  // 添加调试信息
        return 0;
    }
    
    // printf("Topic found!\r\n");  // 添加调试信息
    
    // 查找msg参数
    const char *msg_start = strstr(buffer, "msg=");
    if (msg_start == NULL) {
        printf("msg parameter not found\r\n");  // 添加调试信息
        return 0;
    }
    
    // 跳过"msg="前缀
    msg_start += 4;
    
    // 提取msg值（直到遇到&或\r或\n或字符串结束）
    int i = 0;
    while (msg_start[i] != '\0' && msg_start[i] != '&' && msg_start[i] != '\r' && msg_start[i] != '\n' && i < 31) {
        msg_value[i] = msg_start[i];
        i++;
    }
    msg_value[i] = '\0';
    
    // printf("Extracted msg_value: %s\r\n", msg_value);  // 添加调试信息
    
    return 1;
}

/**
 * @brief 处理传感器相关的远程命令
 * @param buffer 接收到的命令字符串
 * @return 1-成功处理命令，0-未处理
 */
uint8_t ESP8266_Process_Sensor_Commands(const char *buffer)
{
    if (buffer == NULL) {
        return 0;
    }
    
    printf("Processing command: %s\r\n", buffer);  // 添加调试信息
    
    char msg_value[32] = {0};
    
    // // 处理DHT11传感器
    // if (ESP8266_Parse_Command(buffer, "mydht004", msg_value)) {
    //     printf("Found DHT11 topic, msg_value: %s\r\n", msg_value);  // 添加调试信息
        
        
    //     if (strcmp(msg_value, "on") == 0) {
    //         DHT11_ON = 1;
    //         printf("DHT11 sensor turned ON via remote command, current status: %d\r\n", DHT11_ON);
    //         return 1;
    //     } else if (strcmp(msg_value, "off") == 0) {
    //         DHT11_ON = 0;
    //         printf("DHT11 sensor turned OFF via remote command, current status: %d\r\n", DHT11_ON);
    //         return 1;
    //     } else {
    //         printf("Invalid msg_value: '%s'\r\n", msg_value);
    //     }
    //     return 1; // 确保即使msg值不匹配也返回
    // }
    
    // 处理Light传感器
    if (ESP8266_Parse_Command(buffer, "myLUX004", msg_value)) {
        printf("Found Light topic, msg_value: %s\r\n", msg_value);  // 添加调试信息
        extern uint8_t Light_ON;
        if (strcmp(msg_value, "on") == 0) {
            Light_ON = 1;
            printf("Light sensor turned ON via remote command, current status: %d\r\n", Light_ON);
            return 1;
        } else if (strcmp(msg_value, "off") == 0) {
            Light_ON = 0;
            printf("Light sensor turned OFF via remote command, current status: %d\r\n", Light_ON);
            return 1;
        }
        return 1; // 确保即使msg值不匹配也返回
    }
    
    // 处理PM2.5传感器
    // if (ESP8266_Parse_Command(buffer, "myMP25004", msg_value)) {
    //     extern uint8_t PM25_ON;
    //     if (strcmp(msg_value, "on") == 0) {
    //         PM25_ON = 1;
    //         printf("PM25 sensor turned ON via remote command\r\n");
    //         return 1;
    //     } else if (strcmp(msg_value, "off") == 0) {
    //         PM25_ON = 0;
    //         printf("PM25 sensor turned OFF via remote command\r\n");
    //         return 1;
    //     }
    // }
    
    return 0;
}

static void ESP8266_Main_Task(void *pvParameters)
{
    printf("ESP8266_Main_Task start ->\n");

    uint8_t first = 1;
    TickType_t heart_tick = xTaskGetTickCount(); //
    TickType_t Publish_tick = xTaskGetTickCount();

    vTaskDelay(pdMS_TO_TICKS(2000)); // 等待ESP8266开机
    ESP8266_Receive_Start();

    uint8_t retry_count = 0;
    const uint8_t max_retries = 5;

    while (!wifi_connected && retry_count < max_retries)
    {
        retry_count++;
        printf("WiFi connection attempt %d/%d\r\n", retry_count, max_retries);

        if (ESP8266_Connect_WiFi("ElevatedNetwork.lt", "798798798") == 1) // 连接WiFi
        {
            printf("ESP8266 Connect WiFi Success\r\n");
            wifi_connected = 1;
        }
        else
        {
            printf("ESP8266 Connect WiFi Error, attempt %d/%d\r\n", retry_count, max_retries);
            if (retry_count < max_retries)
            {
                // 等待5秒后重试
                vTaskDelay(pdMS_TO_TICKS(5000));
            }
        }
    }

    if (!wifi_connected)
    {
        printf("WiFi connection failed after %d attempts, entering retry loop\r\n", max_retries);

        // 进入无限重试循环
        while (!wifi_connected)
        {
            vTaskDelay(pdMS_TO_TICKS(30000)); // 每30秒重试一次
            printf("Retrying WiFi connection...\r\n");
            if (ESP8266_Connect_WiFi("ElevatedNetwork.lt", "798798798") == 1)
            {
                printf("ESP8266 Connect WiFi Success after retry\r\n");
                wifi_connected = 1;
                OLED_Printf_Line(0, "WiFi Connected!");
                OLED_Refresh();
            }
        }
    }

    retry_count = 0;
    while (!Server_connected && retry_count < max_retries)
    {
        retry_count++;
        printf("Server connection attempt %d/%d\r\n", retry_count, max_retries);
        if (ESP8266_Connect_Server("bemfa.com", "8344") == 1) // 尝试连接服务器
        {
            printf("ESP8266 Connect Server Success\r\n");
            Server_connected = 1;
        }
        else
        {
            printf("ESP8266 Connect Server Error, attempt %d/%d\r\n", retry_count, max_retries);
            if (retry_count < max_retries)
            {
                // 等待5秒后重试
                vTaskDelay(pdMS_TO_TICKS(5000));
            }
        }
    }

    if (!Server_connected)
    {
        printf("Server connection failed after %d attempts, entering retry loop\r\n", max_retries);

        while (!Server_connected)
        {
            vTaskDelay(pdMS_TO_TICKS(30000)); // 每30秒重试一次
            printf("Retrying Server connection...\r\n");
            if (ESP8266_Connect_Server("bemfa.com", "8344") == 1)
            {
                printf("ESP8266 Connect Server Success after retry\r\n");
                Server_connected = 1;
            }
        }
    }

    printf("ESP8266 Connect Server Success\r\n");
   // ====================== 订阅主题1：mydht004（带重试） ======================
    retry_count = 0;
    uint8_t sub1_connected = 0;
    const char* uid = "4af24e3731744508bd519435397e4ab5";
    const char* topic1 = "mydht004";
    
    while (!sub1_connected && retry_count < max_retries)
    {
        retry_count++;
        printf("Subscribe %s attempt %d/%d\r\n", topic1, retry_count, max_retries);
        
        if (ESP8266_TCP_Subscribe(uid, topic1) == 1) // 订阅主题
        {
            printf("ESP8266 TCP Subscribe %s Success\r\n", topic1);
            sub1_connected = 1;
        }
        else
        {
            printf("ESP8266 TCP Subscribe %s Error, attempt %d/%d\r\n", topic1, retry_count, max_retries);
            if (retry_count < max_retries)
            {
                // 等待5秒后重试
                vTaskDelay(pdMS_TO_TICKS(5000));
            }
        }
    }

    if (!sub1_connected)
    {
        printf("Subscribe %s failed after %d attempts, entering retry loop\r\n", topic1, max_retries);

        // 进入无限重试循环
        while (!sub1_connected)
        {
            vTaskDelay(pdMS_TO_TICKS(30000)); // 每30秒重试一次
            printf("Retrying subscribe %s...\r\n", topic1);
            if (ESP8266_TCP_Subscribe(uid, topic1) == 1)
            {
                printf("ESP8266 TCP Subscribe %s Success after retry\r\n", topic1);
                sub1_connected = 1;
            }
        }
    }

    // ====================== 订阅主题2：myMP25004（带重试） ======================
    retry_count = 0;
    uint8_t sub2_connected = 0;
    const char* topic2 = "myMP25004";
    
    while (!sub2_connected && retry_count < max_retries)
    {
        retry_count++;
        printf("Subscribe %s attempt %d/%d\r\n", topic2, retry_count, max_retries);
        
        if (ESP8266_TCP_Subscribe(uid, topic2) == 1) // 订阅主题
        {
            printf("ESP8266 TCP Subscribe %s Success\r\n", topic2);
            sub2_connected = 1;
        }
        else
        {
            printf("ESP8266 TCP Subscribe %s Error, attempt %d/%d\r\n", topic2, retry_count, max_retries);
            if (retry_count < max_retries)
            {
                // 等待5秒后重试
                vTaskDelay(pdMS_TO_TICKS(5000));
            }
        }
    }

    if (!sub2_connected)
    {
        printf("Subscribe %s failed after %d attempts, entering retry loop\r\n", topic2, max_retries);

        // 进入无限重试循环
        while (!sub2_connected)
        {
            vTaskDelay(pdMS_TO_TICKS(30000)); // 每30秒重试一次
            printf("Retrying subscribe %s...\r\n", topic2);
            if (ESP8266_TCP_Subscribe(uid, topic2) == 1)
            {
                printf("ESP8266 TCP Subscribe %s Success after retry\r\n", topic2);
                sub2_connected = 1;
            }
        }
    }

    // ====================== 订阅主题3：myLUX004（带重试） ======================
    retry_count = 0;
    uint8_t sub3_connected = 0;
    const char* topic3 = "myLUX004";
    
    while (!sub3_connected && retry_count < max_retries)
    {
        retry_count++;
        printf("Subscribe %s attempt %d/%d\r\n", topic3, retry_count, max_retries);
        
        if (ESP8266_TCP_Subscribe(uid, topic3) == 1) // 订阅主题
        {
            printf("ESP8266 TCP Subscribe %s Success\r\n", topic3);
            sub3_connected = 1;
        }
        else
        {
            printf("ESP8266 TCP Subscribe %s Error, attempt %d/%d\r\n", topic3, retry_count, max_retries);
            if (retry_count < max_retries)
            {
                // 等待5秒后重试
                vTaskDelay(pdMS_TO_TICKS(5000));
            }
        }
    }

    if (!sub3_connected)
    {
        printf("Subscribe %s failed after %d attempts, entering retry loop\r\n", topic3, max_retries);

        // 进入无限重试循环
        while (!sub3_connected)
        {
            vTaskDelay(pdMS_TO_TICKS(30000)); // 每30秒重试一次
            printf("Retrying subscribe %s...\r\n", topic3);
            if (ESP8266_TCP_Subscribe(uid, topic3) == 1)
            {
                printf("ESP8266 TCP Subscribe %s Success after retry\r\n", topic3);
                sub3_connected = 1;
            }
        }
    }

    // ====================== 获取时间（带重试） ======================
    retry_count = 0;
    uint8_t get_time_success = 0;
    char time_buffer[64];
    
    while (!get_time_success && retry_count < max_retries)
    {
        retry_count++;
        printf("Get Time attempt %d/%d\r\n", retry_count, max_retries);
        
        if (ESP8266_TCP_GetTime(uid, time_buffer, sizeof(time_buffer)) == 1)
        {
            printf("ESP8266 Get Time Success: %s\r\n", time_buffer);
            get_time_success = 1;
        }
        else
        {
            printf("ESP8266 Get Time Error, attempt %d/%d\r\n", retry_count, max_retries);
            if (retry_count < max_retries)
            {
                // 等待5秒后重试
                vTaskDelay(pdMS_TO_TICKS(5000));
            }
        }
    }

    if (!get_time_success)
    {
        printf("Get Time failed after %d attempts, entering retry loop\r\n", max_retries);

        // 进入无限重试循环
        while (!get_time_success)
        {
            vTaskDelay(pdMS_TO_TICKS(30000)); // 每30秒重试一次
            printf("Retrying Get Time...\r\n");
            if (ESP8266_TCP_GetTime(uid, time_buffer, sizeof(time_buffer)) == 1)
            {
                printf("ESP8266 Get Time Success after retry: %s\r\n", time_buffer);
                get_time_success = 1;
            }
        }
    }

    // 同步到RTC
    if (RTC_SetFromNetworkTime(time_buffer) != 1)
    {
        printf("RTC Sync Failed\r\n");
    }
    else
    {
        printf("RTC Sync Success\r\n");
    }

    while (1)
    {
        if ((xTaskGetTickCount() - heart_tick) / 1000 >= 60)
        {
            // 发送心跳包云平台
            heart_tick = xTaskGetTickCount();
            ESP8266_TCP_Heartbeat();
        }

        if ((xTaskGetTickCount() - Publish_tick) / 1000 >= publish_delaytime || first)
        {
            printf("---->\r\n");
            // 发布主题
            Publish_tick = xTaskGetTickCount();
            first = 0;

            //

            char data[16];
            // 发布主题 :mydht004
            
            if (Light_ON)
            {
                // 发布主题 :myLuxGet
                snprintf(data, sizeof(data), "#%d",
                         SensorData.light_data.lux);

                if (ESP8266_TCP_Publish("4af24e3731744508bd519435397e4ab5", "myLUX004", data) != 1) // 发布主题
                {
                    printf("ESP8266 TCP Publish myLuxGet Error\r\n");
                }
                else
                {
                    printf("ESP8266 TCP Publish myLuxGet Success\r\n");
                }
            }
           
        }
        if (uart2_rx_len > 0)
        {
            uart2_rx_len = 0;
            printf("ESP8266 Receive Data: %s\r\n", uart2_buffer); // 收到巴法云下发的数据
            
            // 使用新的统一消息解析函数处理传感器命令
            uint8_t result = ESP8266_Process_Sensor_Commands((const char *)uart2_buffer);
            if (result == 1) {
                printf("Command processed successfully. Current sensor states: Light=%d\r\n", 
                        Light_ON);
            } else {
                printf("No matching sensor command found\r\n");
            }
            
        }
        vTaskDelay(pdMS_TO_TICKS(1));
    }
}
