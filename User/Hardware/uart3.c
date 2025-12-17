#include "uart3.h"
#include <stdio.h>
#include <string.h>
#include "FreeRTOS.h"
#include "task.h"

// 全局接收缓冲区
uint8_t uart3_buffer[UART3_BUF_SIZE];
uint8_t uart3_rx_len;

// 蓝牙命令回调函数
static bluetooth_cmd_callback_t g_bluetooth_callback = NULL;

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

/**
 * @brief  GPIO初始化，PB10=TX3，PB11=RX3
 */
void UART3_GPIO_Init(void)
{
    GPIO_InitTypeDef GPIO_InitStruct;
    
    // 使能GPIOB时钟
    RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOB, ENABLE);
    
    // PB10(TX3) 复用推挽输出
    GPIO_InitStruct.GPIO_Pin = GPIO_Pin_10;
    GPIO_InitStruct.GPIO_Mode = GPIO_Mode_AF_PP;
    GPIO_InitStruct.GPIO_Speed = GPIO_Speed_50MHz;
    GPIO_Init(GPIOB, &GPIO_InitStruct);
    
    // PB11(RX3) 浮空输入
    GPIO_InitStruct.GPIO_Pin = GPIO_Pin_11;
    GPIO_InitStruct.GPIO_Mode = GPIO_Mode_IN_FLOATING;
    GPIO_Init(GPIOB, &GPIO_InitStruct);
}

/**
 * @brief  UART3初始化，使能RXNE中断（用于测试）
 * @param  baudrate: 波特率（如9600、115200等）
 */
void UART3_Init(uint32_t baudrate)
{
    USART_InitTypeDef USART_InitStruct;
    NVIC_InitTypeDef NVIC_InitStruct;
    
    // 1. 使能UART3时钟
    RCC_APB1PeriphClockCmd(RCC_APB1Periph_USART3, ENABLE);
    
    // 2. UART配置
    USART_InitStruct.USART_BaudRate = baudrate;
    USART_InitStruct.USART_WordLength = USART_WordLength_8b; // 8位数据位
    USART_InitStruct.USART_StopBits = USART_StopBits_1;      // 1位停止位
    USART_InitStruct.USART_Parity = USART_Parity_No;         // 无校验
    USART_InitStruct.USART_HardwareFlowControl = USART_HardwareFlowControl_None; // 无硬件流控
    USART_InitStruct.USART_Mode = USART_Mode_Rx | USART_Mode_Tx; // 收发模式
    USART_Init(USART3, &USART_InitStruct);
    
    // 3. 使能UART3
    USART_Cmd(USART3, ENABLE);
    
    // 4. 暂时使用RXNE中断进行测试
    USART_ITConfig(USART3, USART_IT_RXNE, ENABLE);
    
    // 5. 配置UART3中断优先级（NVIC配置）
    NVIC_InitStruct.NVIC_IRQChannel = USART3_IRQn;
    NVIC_InitStruct.NVIC_IRQChannelPreemptionPriority = 5; // 抢占优先级5
    NVIC_InitStruct.NVIC_IRQChannelSubPriority = 0;        // 子优先级0
    NVIC_InitStruct.NVIC_IRQChannelCmd = ENABLE;
    NVIC_Init(&NVIC_InitStruct);
    
    // 6. 调试信息
    printf("UART3 Initialized: Baud=%d, RXNE_IRQ=ENABLED (test mode)\r\n", baudrate);
}

/**
 * @brief  DMA初始化，UART3_RX -> 循环模式
 */
void UART3_DMA_Init(void)
{
    DMA_InitTypeDef DMA_InitStruct;
    
    // 1. 使能DMA2时钟（UART3_RX对应DMA2_Channel2）
    RCC_AHBPeriphClockCmd(RCC_AHBPeriph_DMA2, ENABLE);
    
    // 2. 复位DMA2_Channel2，等待配置完成
    DMA_DeInit(DMA2_Channel2);
    
    // 3. 先配置为8位数据模式，确保数据正确接收
    DMA_InitStruct.DMA_PeripheralBaseAddr = (uint32_t)&USART3->DR; // 外设地址：UART3数据寄存器
    DMA_InitStruct.DMA_MemoryBaseAddr = (uint32_t)uart3_buffer;    // 内存地址：接收缓冲区
    DMA_InitStruct.DMA_DIR = DMA_DIR_PeripheralSRC;                // 数据方向：外设->内存
    DMA_InitStruct.DMA_BufferSize = UART3_BUF_SIZE;                 // 缓冲区大小：128字节
    DMA_InitStruct.DMA_PeripheralInc = DMA_PeripheralInc_Disable;  // 外设地址不递增
    DMA_InitStruct.DMA_MemoryInc = DMA_MemoryInc_Enable;           // 内存地址递增
    DMA_InitStruct.DMA_PeripheralDataSize = DMA_PeripheralDataSize_Byte; // 外设数据宽度：字节
    DMA_InitStruct.DMA_MemoryDataSize = DMA_MemoryDataSize_Byte;   // 内存数据宽度：字节
    DMA_InitStruct.DMA_Mode = DMA_Mode_Circular;                   // 循环模式（重复使用）
    DMA_InitStruct.DMA_Priority = DMA_Priority_Medium;             // 中等优先级
    DMA_InitStruct.DMA_M2M = DMA_M2M_Disable;                      // 禁止内存到内存
    
    DMA_Init(DMA2_Channel2, &DMA_InitStruct);
    
    // 4. 使能DMA2_Channel2
    DMA_Cmd(DMA2_Channel2, ENABLE);
    
    // 5. 使能UART3的DMA接收
    USART_DMACmd(USART3, USART_DMAReq_Rx, ENABLE);
    
    // 6. 清空缓冲区，初始化接收长度
    memset(uart3_buffer, 0, UART3_BUF_SIZE);
    uart3_rx_len = 0;
    
    // 7. 调试信息
    printf("UART3 DMA Initialized: Channel2, Size=%d\r\n", UART3_BUF_SIZE);
}

/**
 * @brief  UART3中断服务函数（RXNE中断测试）
 */
void USART3_IRQHandler(void)
{
    uint32_t temp;
    
    // 检查是否是RXNE中断（接收数据寄存器非空）
    if(USART_GetITStatus(USART3, USART_IT_RXNE) != RESET)
    {
        // 读取接收到的数据
        uint8_t rx_data = USART_ReceiveData(USART3);
        
        // 存储到缓冲区（防止溢出）
        static uint16_t rx_index = 0;
        if(rx_index < UART3_BUF_SIZE - 1)
        {
            uart3_buffer[rx_index] = rx_data;
            rx_index++;
            uart3_rx_len = rx_index;
        }
        
        printf("RXNE: 0x%02X ('%c')\r\n", rx_data, 
               (rx_data >= 32 && rx_data <= 126) ? rx_data : '.');
        
        // 检查是否收到完整命令（\r\n）
        if(rx_data == '\n' || rx_data == '\r')
        {
            if(rx_index > 1) // 至少有数据才处理
            {
                uart3_buffer[rx_index-1] = '\0'; // 替换\r\n为字符串结束符
                printf("Command complete: '%s' (len=%d)\r\n", 
                       uart3_buffer, rx_index-1);
                rx_index = 0; // 重置索引
            }
            else
            {
                rx_index = 0; // 重置索引
            }
        }
        
        USART_ClearITPendingBit(USART3, USART_IT_RXNE);
    }
    
    // 检测是否是空闲中断（保留DMA功能）
    else if(USART_GetITStatus(USART3, USART_IT_IDLE) != RESET)
    {
        // 清除IDLE中断标志
        temp = USART3->SR;
        temp = USART3->DR;
        (void)temp;
        
        printf("IDLE interrupt detected\r\n");
    }
}

/**
 * @brief  初始化接收（测试模式）
 */
void UART3_DMA_RX_Init(uint32_t baudrate)
{
    UART3_GPIO_Init();    // GPIO初始化
    UART3_Init(baudrate); // UART初始化（使用RXNE中断测试）
    // 暂时禁用DMA，先测试基本UART功能
    // UART3_DMA_Init();     // DMA初始化
    
    // 清空缓冲区
    memset(uart3_buffer, 0, UART3_BUF_SIZE);
    uart3_rx_len = 0;
    
    printf("UART3 Test Mode: RXNE interrupt enabled\r\n");
}

/**
 * @brief  轮询式发送UART3数据给蓝牙模块（适合少量数据）
 * @param  data: 要发送的数据缓冲区指针
 * @param  len:  要发送数据的长度（字节）
 * @retval 0: 成功；1: 失败
 */
uint8_t UART3_SendDataToBluetooth_Poll(uint8_t *data, uint16_t len)
{
    // 参数校验
    if(data == NULL || len == 0 || len > UART3_BUF_SIZE)
    {
        return 1;
    }
    
    // 轮询发送每个字节
    for(uint16_t i = 0; i < len; i++)
    {
        // 等待发送缓冲区为空（TXE标志位复位）
        while(USART_GetFlagStatus(USART3, USART_FLAG_TXE) == RESET);
        // 发送当前字节
        USART_SendData(USART3, data[i]);
    }
    
    // 等待最后一个字节发送完成（TC标志位置位），确保数据完全发送
    while(USART_GetFlagStatus(USART3, USART_FLAG_TC) == RESET);
    
    return 0;
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
    if (strcasecmp(cmd, "AT") == 0)
    {
        // AT指令处理
        printf("Bluetooth Response: OK\r\n");
        UART3_SendDataToBluetooth_Poll((uint8_t*)"OK\r\n", 4);
    }
    else if (strcasecmp(cmd, "STATUS") == 0)
    {
        // 状态查询
        printf("Bluetooth Response: STM32 Bluetooth Module Ready\r\n");
        UART3_SendDataToBluetooth_Poll((uint8_t*)"STM32 Bluetooth Module Ready\r\n", 30);
    }
    else if (strcasecmp(cmd, "HELP") == 0)
    {
        // 帮助信息
        printf("Bluetooth Response: Help information sent\r\n");
        UART3_SendDataToBluetooth_Poll((uint8_t*)"Available commands:\r\n", 19);
        UART3_SendDataToBluetooth_Poll((uint8_t*)"AT - Test connection\r\n", 21);
        UART3_SendDataToBluetooth_Poll((uint8_t*)"STATUS - Get system status\r\n", 28);
        UART3_SendDataToBluetooth_Poll((uint8_t*)"HELP - Show this help\r\n", 23);
    }
    else
    {
        // 未知命令
        char response[64];
        snprintf(response, sizeof(response), "ERROR: Unknown command '%s'\r\n", cmd);
        printf("Bluetooth Response: Unknown command '%s'\r\n", cmd);
        UART3_SendDataToBluetooth_Poll((uint8_t*)response, strlen(response));
    }
}

// 蓝牙数据处理任务（基于IDLE中断和DMA）
void Bluetooth_Process_Task(void *pvParameters)
{
    uint8_t cmd_buffer[128];
    uint16_t cmd_len = 0;
    
    printf("Bluetooth Process Task Started (DMA+IDLE mode)\r\n");
    
    // 发送启动信息
    UART3_SendDataToBluetooth_Poll((uint8_t*)"STM32 Bluetooth Module Ready\r\n", 29);
    UART3_SendDataToBluetooth_Poll((uint8_t*)"Type 'HELP' for available commands\r\n", 37);
    
    while (1)
    {
        // 检查是否有新数据到达
        if (uart3_rx_len > 0)
        {
            // 处理接收到的数据
            printf("Processing UART3 data (len=%d): ", uart3_rx_len);
            
            // 查找命令结束符（\r\n）
            uint8_t cmd_found = 0;
            for (uint16_t i = 0; i < uart3_rx_len; i++)
            {
                // 跳过\r和\n作为命令分隔符
                if (uart3_buffer[i] == '\r' || uart3_buffer[i] == '\n')
                {
                    if (cmd_len > 0)
                    {
                        cmd_buffer[cmd_len] = '\0';  // 字符串结束符
                        printf("Command: '%s'\r\n", cmd_buffer);
                        
                        Bluetooth_Process_Command((const char*)cmd_buffer, cmd_len);
                        cmd_len = 0;
                        cmd_found = 1;
                    }
                }
                else
                {
                    if (cmd_len < sizeof(cmd_buffer) - 1)
                    {
                        cmd_buffer[cmd_len++] = uart3_buffer[i];
                    }
                }
            }
            
            // 如果没有找到命令结束符但还有数据，直接处理
            if (!cmd_found && cmd_len > 0)
            {
                cmd_buffer[cmd_len] = '\0';
                printf("Direct command: '%s'\r\n", cmd_buffer);
                Bluetooth_Process_Command((const char*)cmd_buffer, cmd_len);
                cmd_len = 0;
            }
            
            // 清空接收长度
            uart3_rx_len = 0;
        }
        
        vTaskDelay(pdMS_TO_TICKS(10));  // 10ms延时
    }
}

