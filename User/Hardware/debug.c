#include "debug.h"
#include "FreeRTOS.h"
#include "task.h"

// UART1接收缓冲区和长度
uint8_t uart1_buffer[UART1_BUF_SIZE];
uint8_t uart1_rx_len;

/**
 * @brief  DMA发送初始化
 */
static void UART1_DMA_TX_Init(void)
{
    DMA_InitTypeDef DMA_InitStruct;
    
    // 使能DMA1时钟（UART1_TX对应DMA1_Channel4）
    RCC_AHBPeriphClockCmd(RCC_AHBPeriph_DMA1, ENABLE);
    
    // 复位DMA1_Channel4
    DMA_DeInit(DMA1_Channel4);
    
    // DMA配置
    DMA_InitStruct.DMA_PeripheralBaseAddr = (uint32_t)&USART1->DR; // 外设地址：UART1数据寄存器
    DMA_InitStruct.DMA_MemoryBaseAddr = 0;                         // 内存地址：动态设置
    DMA_InitStruct.DMA_DIR = DMA_DIR_PeripheralDST;                // 数据方向：内存->外设
    DMA_InitStruct.DMA_BufferSize = 0;                             // 缓冲区大小：动态设置
    DMA_InitStruct.DMA_PeripheralInc = DMA_PeripheralInc_Disable;  // 外设地址不递增
    DMA_InitStruct.DMA_MemoryInc = DMA_MemoryInc_Enable;           // 内存地址递增
    DMA_InitStruct.DMA_PeripheralDataSize = DMA_PeripheralDataSize_Byte; // 外设数据宽度：字节
    DMA_InitStruct.DMA_MemoryDataSize = DMA_MemoryDataSize_Byte;   // 内存数据宽度：字节
    DMA_InitStruct.DMA_Mode = DMA_Mode_Normal;                     // 正常模式（单次传输）
    DMA_InitStruct.DMA_Priority = DMA_Priority_High;               // 高优先级
    DMA_InitStruct.DMA_M2M = DMA_M2M_Disable;                      // 禁用内存到内存
    
    DMA_Init(DMA1_Channel4, &DMA_InitStruct);
    
    // 先禁用DMA通道，等待使用时再启用
    DMA_Cmd(DMA1_Channel4, DISABLE);
}

//7）在USART接收中断服务函数实现数据接收和发送。
void USART1_IRQHandler(void)
{   
    uint16_t  temp = 0;        
    //判断接收标志位是否置1
    if(USART_GetITStatus(USART1, USART_IT_RXNE) == SET)
    {
        USART_ClearITPendingBit(USART1, USART_IT_RXNE); //清除接收标志位
        
        temp = USART_ReceiveData(USART1);    // 读取数据（读一个字节）
        if (temp == '1')
		{
			GPIO_ResetBits(GPIOB, GPIO_Pin_3);
			vTaskDelay(200);
			GPIO_SetBits(GPIOB, GPIO_Pin_3);
		}
	
        USART_SendData(USART1, temp);        // 将读到的数据发送回去
    }
}

// PA9-TX, PA10-RX
void debug_init(void)
{
    GPIO_InitTypeDef GPIO_InitStruct;
    USART_InitTypeDef USART_InitStruct;
    NVIC_InitTypeDef NVIC_InitStruct;
    
    // 1）使能RX和TX引脚GPIO时钟和USART时钟；
    RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOA, ENABLE);    // GPIO时钟
    RCC_APB2PeriphClockCmd(RCC_APB2Periph_USART1, ENABLE); //for USART1 and USART6 

    // 2）初始化GPIO，并将GPIO复用到USART上
    GPIO_InitStruct.GPIO_Pin = GPIO_Pin_9 | GPIO_Pin_10;
    GPIO_InitStruct.GPIO_Mode = GPIO_Mode_AF_PP;      // 复用推挽输出模式
    GPIO_InitStruct.GPIO_Speed = GPIO_Speed_50MHz; // 高速
 
    GPIO_Init(GPIOA, &GPIO_InitStruct);
    
//    GPIO_PinAFConfig(GPIOA, GPIO_PinSource9, GPIO_AF_USART1);
//    GPIO_PinAFConfig(GPIOA, GPIO_PinSource10, GPIO_AF_USART1);

    // 3）配置USART参数；
    USART_InitStruct.USART_BaudRate = 115200;                        // 波特率
    USART_InitStruct.USART_WordLength = USART_WordLength_8b;        // 8位有效数据位
    USART_InitStruct.USART_StopBits = USART_StopBits_1;              // 1位停止位
    USART_InitStruct.USART_Parity = USART_Parity_No;                // 无校验
    USART_InitStruct.USART_Mode = USART_Mode_Rx | USART_Mode_Tx;    // 接收和发送数据模式
    USART_InitStruct.USART_HardwareFlowControl = USART_HardwareFlowControl_None; // 无硬件控制流
    USART_Init(USART1, &USART_InitStruct);

    // 4）配置中断控制器并使能USART接收中断
    USART_ITConfig(USART1, USART_IT_RXNE, ENABLE);

    // 5）设置中断优先级（如果需要开启串口中断才需要这个步骤）
    NVIC_InitStruct.NVIC_IRQChannel = USART1_IRQn;         // 中断通道(中断源)
    NVIC_InitStruct.NVIC_IRQChannelPreemptionPriority = 1;
    NVIC_InitStruct.NVIC_IRQChannelSubPriority = 2;
    NVIC_InitStruct.NVIC_IRQChannelCmd = ENABLE;
    NVIC_Init(&NVIC_InitStruct);
    
    // 6）使能USART；
    USART_Cmd(USART1, ENABLE);
    
    // 7）初始化DMA发送
    UART1_DMA_TX_Init();
}

/**
 * @brief  DMA方式发送数据到调试串口
 * @param  data: 待发送的数据缓冲区指针
 * @param  len:  发送数据的长度（字节）
 * @retval 0: 成功；1: 参数错误
 */
uint8_t UART1_SendDataToDebug_DMA(uint8_t *data, uint16_t len)
{
    // 参数校验
    if(data == NULL || len == 0 || len > UART1_BUF_SIZE)
    {
        return 1;
    }
    
    // 等待DMA传输完成（如果正在传输）
    while(DMA_GetFlagStatus(DMA1_FLAG_TC4) == RESET);
    
    // 清除DMA传输完成标志
    DMA_ClearFlag(DMA1_FLAG_TC4);
    
    // 禁用DMA通道
    DMA_Cmd(DMA1_Channel4, DISABLE);
    
    // 重新配置DMA
    DMA1_Channel4->CNDTR = len;                    // 设置传输数据长度
    DMA1_Channel4->CMAR = (uint32_t)data;          // 设置内存地址
    
    // 使能DMA通道
    DMA_Cmd(DMA1_Channel4, ENABLE);
    
    // 使能UART1的DMA发送请求
    USART_DMACmd(USART1, USART_DMAReq_Tx, ENABLE);
    
    // 等待DMA传输完成
    while(DMA_GetFlagStatus(DMA1_FLAG_TC4) == RESET);
    
    // 清除DMA传输完成标志
    DMA_ClearFlag(DMA1_FLAG_TC4);
    
    // 禁用UART1的DMA发送请求
    USART_DMACmd(USART1, USART_DMAReq_Tx, DISABLE);
    
    return 0;
}

// 通过串口1，单片机发送字符串
void Usart1_Send_Sring(char *string)
{
    
    while(*string != '\0')
    {
        USART_SendData(USART1, *string++);	// 库函数
        // 等待发送数据寄存器空
        while(USART_GetFlagStatus(USART1, USART_FLAG_TXE) == RESET);
    }
    // 等待发送完成
    while(USART_GetFlagStatus(USART1, USART_FLAG_TC) == RESET);
}

//重定向c库函数printf到串口，重定向后可使用printf函数
int fputc(int ch, FILE *f)
{
    /* 发送一个字节数据到串口 */
    USART_SendData(USART1, (uint8_t) ch);

    /* 等待发送完毕 */
    while (USART_GetFlagStatus(USART1, USART_FLAG_TXE) == RESET);

    return (ch);
}

// USART1发送字节数组函数
void Usart1_send_bytes(uint8_t *buf, uint16_t len)
{
    uint16_t i;
    for(i = 0; i < len; i++) {
        /* 发送一个字节数据到串口 */
        USART_SendData(USART1, buf[i]);
        
        /* 等待发送寄存器空 */
        while(USART_GetFlagStatus(USART1, USART_FLAG_TXE) == RESET);
    }
    /* 等待发送完成 */
    while(USART_GetFlagStatus(USART1, USART_FLAG_TC) == RESET);
}
