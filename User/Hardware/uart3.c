#include "stm32f10x.h"
#include "uart3.h"
#include <stdio.h>
#include <string.h>
#include "FreeRTOS.h"
#include "queue.h"

// 全局接收缓冲区
uart3_buffer_t uart3_rx_buffer = {0};

// 蓝牙命令回调函数
static bluetooth_cmd_callback_t g_bluetooth_callback = NULL;

// 任务句柄（全局）
TaskHandle_t bluetooth_task_handle = NULL;

// 简单的大小写不敏感字符串比较函数
static int strcasecmp(const char *s1, const char *s2)
{
    while (*s1 && *s2) 
    {
        char c1 = *s1;
        char c2 = *s2;
        
        // 转换为小写比较
        if (c1 >= 'A' && c1 <= 'Z') c1 += 32;
        if (c2 >= 'A' && c2 <= 'Z') c2 += 32;
        
        if (c1 != c2) return c1 - c2;
        
        s1++;
        s2++;
    }
    
    // 处理其中一个字符串已经结束的情况
    char c1 = *s1 ? *s1 : 0;
    char c2 = *s2 ? *s2 : 0;
    
    if (c1 >= 'A' && c1 <= 'Z') c1 += 32;
    if (c2 >= 'A' && c2 <= 'Z') c2 += 32;
    
    return c1 - c2;
}

// 清理数据，移除不可见字符
static uint16_t clean_buffer(uint8_t* buffer, uint16_t len)
{
    uint16_t read_pos = 0;
    uint16_t write_pos = 0;
    
    while (read_pos < len)
    {
        uint8_t ch = buffer[read_pos];
        
        // 只保留可打印字符 (32-126) 和常见的控制字符
        if ((ch >= 32 && ch <= 126) || ch == '\r' || ch == '\n' || ch == '\t')
        {
            buffer[write_pos++] = ch;
        }
        
        read_pos++;
    }
    
    return write_pos;
}

void My_USART3_Init(void)  
{  
    GPIO_InitTypeDef GPIO_InitStruct;  
    USART_InitTypeDef USART_InitStruct;  
    NVIC_InitTypeDef NVIC_InitStruct;  
    
    // 1. 使能 GPIOB 和 USART3 时钟
    RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOB, ENABLE);   // GPIOB 在 APB2
    RCC_APB1PeriphClockCmd(RCC_APB1Periph_USART3, ENABLE);  // ⚠️ USART3 在 APB1！关键！

    // 2. 配置 PB10 为复用推挽输出（TX）
    GPIO_InitStruct.GPIO_Mode = GPIO_Mode_AF_PP;
    GPIO_InitStruct.GPIO_Pin = GPIO_Pin_10;
    GPIO_InitStruct.GPIO_Speed = GPIO_Speed_10MHz;
    GPIO_Init(GPIOB, &GPIO_InitStruct);

    // 3. 配置 PB11 为浮空输入（RX）
    GPIO_InitStruct.GPIO_Mode = GPIO_Mode_IN_FLOATING;
    GPIO_InitStruct.GPIO_Pin = GPIO_Pin_11;
    GPIO_Init(GPIOB, &GPIO_InitStruct);

    // 4. 配置 USART3 参数
    USART_InitStruct.USART_BaudRate = 115200;
    USART_InitStruct.USART_HardwareFlowControl = USART_HardwareFlowControl_None;
    USART_InitStruct.USART_Mode = USART_Mode_Tx | USART_Mode_Rx;
    USART_InitStruct.USART_Parity = USART_Parity_No;
    USART_InitStruct.USART_StopBits = USART_StopBits_1;
    USART_InitStruct.USART_WordLength = USART_WordLength_8b;
    USART_Init(USART3, &USART_InitStruct);

    // 5. 使能 USART3
    USART_Cmd(USART3, ENABLE);

    // 6. 使能接收中断
    USART_ITConfig(USART3, USART_IT_RXNE, ENABLE);

    // 7. 配置 NVIC 中断优先级（抢占=1，子优先级=1）
    NVIC_InitStruct.NVIC_IRQChannel = USART3_IRQn;         // ⚠️ 改为 USART3_IRQn
    NVIC_InitStruct.NVIC_IRQChannelPreemptionPriority = 1;
    NVIC_InitStruct.NVIC_IRQChannelSubPriority = 1;
    NVIC_InitStruct.NVIC_IRQChannelCmd = ENABLE;
    NVIC_Init(&NVIC_InitStruct);
}  

// 8. 中断服务函数（必须与启动文件中的向量表名一致！）
void USART3_IRQHandler(void)  
{  
    uint8_t res;  
    BaseType_t xHigherPriorityTaskWoken = pdFALSE;
    
    if (USART_GetITStatus(USART3, USART_IT_RXNE) != RESET)  
    {  
        res = USART_ReceiveData(USART3);  
        
        // 调试输出：显示接收到的字节
        printf("IRQ RX: 0x%02X ('%c'), buffer_count=%d\r\n", 
               res, (res >= 32 && res <= 126) ? res : '.', uart3_rx_buffer.count);
        
        // 将数据存入环形缓冲区
        uint16_t next_head = (uart3_rx_buffer.head + 1) % UART3_RX_BUFFER_SIZE;
        
        if (next_head != uart3_rx_buffer.tail) 
        {
            uart3_rx_buffer.buffer[uart3_rx_buffer.head] = res;
            uart3_rx_buffer.head = next_head;
            uart3_rx_buffer.count++;
            
            // 检查是否收到完整命令（以\r\n结尾）
            if (res == '\n') 
            {
                uart3_rx_buffer.data_ready = 1;
                printf("Command ready, notifying task\r\n");
                
                // 关键：通知蓝牙任务有数据到达
                if (bluetooth_task_handle != NULL)
                {
                    vTaskNotifyGiveFromISR(bluetooth_task_handle, &xHigherPriorityTaskWoken);
                    printf("Task notification sent\r\n");
                }
                else
                {
                    printf("Bluetooth task handle is NULL!\r\n");
                }
            }
        }
        else
        {
            // 缓冲区溢出处理
            printf("UART3 RX Buffer Overflow!\r\n");
        }
        
        USART_ClearITPendingBit(USART3, USART_IT_RXNE);
    }
    
    // 如果需要任务切换
    portYIELD_FROM_ISR(xHigherPriorityTaskWoken);
}  

// 读取数据从缓冲区
uint16_t UART3_Read_Data(uint8_t* data, uint16_t len)
{
    uint16_t read_count = 0;
    
    taskENTER_CRITICAL();
    while (read_count < len && uart3_rx_buffer.count > 0)
    {
        data[read_count] = uart3_rx_buffer.buffer[uart3_rx_buffer.tail];
        uart3_rx_buffer.tail = (uart3_rx_buffer.tail + 1) % UART3_RX_BUFFER_SIZE;
        uart3_rx_buffer.count--;
        read_count++;
    }
    taskEXIT_CRITICAL();
    
    return read_count;
}

// 获取可用数据量
uint16_t UART3_Get_Available_Data(void)
{
    return uart3_rx_buffer.count;
}

// 清空缓冲区
void UART3_Clear_Buffer(void)
{
    taskENTER_CRITICAL();
    uart3_rx_buffer.head = 0;
    uart3_rx_buffer.tail = 0;
    uart3_rx_buffer.count = 0;
    uart3_rx_buffer.data_ready = 0;
    taskEXIT_CRITICAL();
}

// 发送数据（非阻塞版本）
uint8_t UART3_Send_Data(const uint8_t* data, uint16_t len)
{
    uint16_t i;
    for (i = 0; i < len; i++)
    {
        // 使用超时机制，避免无限等待
        uint32_t timeout = 10000;  // 超时计数
        while (USART_GetFlagStatus(USART3, USART_FLAG_TXE) == RESET && timeout > 0)
        {
            timeout--;
            // 给其他任务运行的机会
            if (timeout % 1000 == 0)
            {
                taskYIELD();
            }
        }
        
        if (timeout == 0)
        {
            printf("UART3 Send Timeout!\r\n");
            return 0;  // 发送失败
        }
        
        USART_SendData(USART3, data[i]);
    }
    return 1;  // 发送成功
}

// 发送字符串
uint8_t UART3_Send_String(const char* str)
{
    return UART3_Send_Data((const uint8_t*)str, strlen(str));
}

// 注册蓝牙命令回调函数
void Bluetooth_Register_Callback(bluetooth_cmd_callback_t callback)
{
    g_bluetooth_callback = callback;
}

// 处理蓝牙命令
void Bluetooth_Process_Command(const char* cmd, uint16_t len)
{
    printf("Bluetooth CMD: %s (len=%d)\r\n", cmd, len);
    
    // 如果注册了回调函数，调用它
    if (g_bluetooth_callback != NULL)
    {
        g_bluetooth_callback(cmd, len);
        return;  // 如果有自定义回调，不再处理默认命令
    }
    
    // 默认处理一些常用命令
    // 使用strcasecmp进行大小写不敏感的比较
    if (strcasecmp(cmd, "AT") == 0)
    {
        // AT指令处理
        printf("Bluetooth Response: OK\r\n");
        UART3_Send_String("OK\r\n");
    }
    else if (strcasecmp(cmd, "STATUS") == 0)
    {
        // 状态查询
        printf("Bluetooth Response: STM32 Bluetooth Module Ready\r\n");
        UART3_Send_String("STM32 Bluetooth Module Ready\r\n");
    }
    else if (strcasecmp(cmd, "HELP") == 0)
    {
        // 帮助信息
        printf("Bluetooth Response: Help information sent\r\n");
        UART3_Send_String("Available commands:\r\n");
        UART3_Send_String("AT - Test connection\r\n");
        UART3_Send_String("STATUS - Get system status\r\n");
        UART3_Send_String("HELP - Show this help\r\n");
    }
    else
    {
        // 未知命令
        char response[64];
        snprintf(response, sizeof(response), "ERROR: Unknown command '%s'\r\n", cmd);
        printf("Bluetooth Response: Unknown command '%s'\r\n", cmd);
        UART3_Send_String(response);
    }
}

// 蓝牙数据处理任务（事件驱动版本）
void Bluetooth_Process_Task(void *pvParameters)
{
    uint8_t cmd_buffer[128];
    uint16_t cmd_len = 0;
    uint8_t ch;
    
    // 保存任务句柄
    bluetooth_task_handle = xTaskGetCurrentTaskHandle();
    printf("Bluetooth Process Task Started, handle=%p\r\n", bluetooth_task_handle);
    
    // 发送启动信息
    UART3_Send_String("STM32 Bluetooth Module Ready\r\n");
    UART3_Send_String("Type 'HELP' for available commands\r\n");
    
    while (1)
    {
        printf("Waiting for task notification...\r\n");
        
        // 等待任务通知，超时100ms（这样可以定期检查其他条件）
        uint32_t notification_result = ulTaskNotifyTake(pdTRUE, pdMS_TO_TICKS(5000));
        
        printf("Task notification received: %lu, buffer_count=%d, data_ready=%d\r\n", 
               notification_result, UART3_Get_Available_Data(), uart3_rx_buffer.data_ready);
        
        // 检查是否有数据可读
        cmd_len = 0;
        while (UART3_Get_Available_Data() > 0 && cmd_len < sizeof(cmd_buffer) - 1)
        {
            UART3_Read_Data(&ch, 1);
            printf("Read byte: 0x%02X ('%c')\r\n", ch, (ch >= 32 && ch <= 126) ? ch : '.');
            
            // 跳过\r和\n作为命令分隔符
            if (ch == '\r' || ch == '\n')
            {
                if (cmd_len > 0)
                {
                    printf("Command end detected\r\n");
                    break;  // 命令结束
                }
            }
            else
            {
                cmd_buffer[cmd_len++] = ch;
            }
        }
        
        // 只有当命令长度大于0时才处理
        if (cmd_len > 0)
        {
            // 清理数据，移除不可见字符
            cmd_len = clean_buffer(cmd_buffer, cmd_len);
            cmd_buffer[cmd_len] = '\0';  // 字符串结束符
            
            // 打印接收到的原始数据用于调试
            printf("Bluetooth Received: '%s' (len=%d)\r\n", (char*)cmd_buffer, cmd_len);
            
            Bluetooth_Process_Command((const char*)cmd_buffer, cmd_len);
            
            cmd_len = 0;  // 重置命令长度
            uart3_rx_buffer.data_ready = 0;
        }
        else
        {
            printf("No command data to process\r\n");
        }
    }
}

