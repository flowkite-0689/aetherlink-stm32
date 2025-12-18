#include "uart3.h"
#include <stdio.h>

uint8_t uart3_buffer[UART3_BUF_SIZE];
uint8_t uart3_rx_len;

static void UART3_GPIO_Init(void)
{
    GPIO_InitTypeDef GPIO_InitStruct;
    RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOB, ENABLE);

    GPIO_InitStruct.GPIO_Pin   = GPIO_Pin_10;                 // PB10 TX
    GPIO_InitStruct.GPIO_Mode  = GPIO_Mode_AF_PP;
    GPIO_InitStruct.GPIO_Speed = GPIO_Speed_50MHz;
    GPIO_Init(GPIOB, &GPIO_InitStruct);

    GPIO_InitStruct.GPIO_Pin  = GPIO_Pin_11;                  // PB11 RX
    GPIO_InitStruct.GPIO_Mode = GPIO_Mode_IN_FLOATING;
    GPIO_Init(GPIOB, &GPIO_InitStruct);
}

static void UART3_Init(uint32_t baudrate)
{
    USART_InitTypeDef USART_InitStruct;
    NVIC_InitTypeDef  NVIC_InitStruct;

    RCC_APB1PeriphClockCmd(RCC_APB1Periph_USART3, ENABLE);

    USART_InitStruct.USART_BaudRate            = baudrate;
    USART_InitStruct.USART_WordLength          = USART_WordLength_8b;
    USART_InitStruct.USART_StopBits            = USART_StopBits_1;
    USART_InitStruct.USART_Parity              = USART_Parity_No;
    USART_InitStruct.USART_HardwareFlowControl = USART_HardwareFlowControl_None;
    USART_InitStruct.USART_Mode                = USART_Mode_Rx | USART_Mode_Tx;
    USART_Init(USART3, &USART_InitStruct);

    // 启用接收中断和IDLE中断
    USART_ITConfig(USART3, USART_IT_RXNE, ENABLE);
    USART_ITConfig(USART3, USART_IT_IDLE, ENABLE);

    NVIC_InitStruct.NVIC_IRQChannel                   = USART3_IRQn;
    NVIC_InitStruct.NVIC_IRQChannelPreemptionPriority = 0;
    NVIC_InitStruct.NVIC_IRQChannelSubPriority        = 0;
    NVIC_InitStruct.NVIC_IRQChannelCmd                = ENABLE;
    NVIC_Init(&NVIC_InitStruct);

    USART_Cmd(USART3, ENABLE);
    
    printf("UART3 initialized with baudrate: %d\r\n", baudrate);
}

static void UART3_DMA_Init(void)
{
    DMA_InitTypeDef DMA_InitStruct;
    RCC_AHBPeriphClockCmd(RCC_AHBPeriph_DMA1, ENABLE);

    // 正确配置：UART3_RX使用DMA1_Channel3
    DMA_DeInit(DMA1_Channel3);
    DMA_InitStruct.DMA_PeripheralBaseAddr = (uint32_t)&USART3->DR;
    DMA_InitStruct.DMA_MemoryBaseAddr     = (uint32_t)uart3_buffer;
    DMA_InitStruct.DMA_DIR                = DMA_DIR_PeripheralSRC;
    DMA_InitStruct.DMA_BufferSize         = UART3_BUF_SIZE;
    DMA_InitStruct.DMA_PeripheralInc      = DMA_PeripheralInc_Disable;
    DMA_InitStruct.DMA_MemoryInc          = DMA_MemoryInc_Enable;
    DMA_InitStruct.DMA_PeripheralDataSize = DMA_PeripheralDataSize_Byte;
    DMA_InitStruct.DMA_MemoryDataSize     = DMA_MemoryDataSize_Byte;
    DMA_InitStruct.DMA_Mode               = DMA_Mode_Circular;
    DMA_InitStruct.DMA_Priority           = DMA_Priority_Medium;
    DMA_InitStruct.DMA_M2M                = DMA_M2M_Disable;
    DMA_Init(DMA1_Channel3, &DMA_InitStruct);

    DMA_Cmd(DMA1_Channel3, ENABLE);
    USART_DMACmd(USART3, USART_DMAReq_Rx, ENABLE);
}

void USART3_IRQHandler(void)
{
    uint32_t temp;
    
    // 处理RXNE中断（接收数据寄存器非空）
    if (USART_GetITStatus(USART3, USART_IT_RXNE) != RESET)
    {
        // 读取数据以清除RXNE标志
        temp = USART_ReceiveData(USART3);
        (void)temp;
        USART_ClearITPendingBit(USART3, USART_IT_RXNE);
    }
    
    // 处理IDLE中断（线路空闲）
    if (USART_GetITStatus(USART3, USART_IT_IDLE) != RESET)
    {
        // 清除IDLE中断标志位（重要！）
        temp = USART3->SR;
        temp = USART3->DR;
        (void)temp;

        DMA_Cmd(DMA1_Channel3, DISABLE);
        uart3_rx_len = UART3_BUF_SIZE - DMA_GetCurrDataCounter(DMA1_Channel3);
        DMA_SetCurrDataCounter(DMA1_Channel3, UART3_BUF_SIZE);
        DMA_Cmd(DMA1_Channel3, ENABLE);
        
        // 清除IDLE中断标志位
        USART_ClearITPendingBit(USART3, USART_IT_IDLE);
        
        // 调试信息
        if (uart3_rx_len > 0)
        {
            printf("UART3 IDLE Interrupt: Received %d bytes\r\n", uart3_rx_len);
        }
    }
}

void UART3_DMA_RX_Init(uint32_t baudrate)
{
    UART3_GPIO_Init();
    UART3_Init(baudrate);
    UART3_DMA_Init();
}

/* 蓝牙发送接口 */
uint8_t UART3_SendDataToBLE_Poll(uint8_t *data, uint16_t len)
{
    if (data == NULL || len == 0 || len > UART3_BUF_SIZE)
        return 1;

    for (uint16_t i = 0; i < len; i++)
    {
        while (USART_GetFlagStatus(USART3, USART_FLAG_TXE) == RESET);
        USART_SendData(USART3, data[i]);
    }
    while (USART_GetFlagStatus(USART3, USART_FLAG_TC) == RESET);
    return 0;
}

/* UART3 printf函数实现 */
int uart3_printf(const char *format, ...)
{
    va_list args;
    char buffer[UART3_BUF_SIZE];
    int len;
    
    // 解析可变参数
    va_start(args, format);
    len = vsnprintf(buffer, sizeof(buffer), format, args);
    va_end(args);
    
    // 检查格式化结果
    if (len < 0)
    {
        return -1; // 格式化错误
    }
    
    // 检查缓冲区是否足够大
    if (len >= (int)sizeof(buffer))
    {
        len = sizeof(buffer) - 1; // 截断到缓冲区大小
        buffer[len] = '\0';       // 确保字符串终止
    }
    
    // 通过UART3发送数据
    if (UART3_SendDataToBLE_Poll((uint8_t*)buffer, len) != 0)
    {
        return -1; // 发送失败
    }
    
    return len; // 返回实际发送的字符数
}
