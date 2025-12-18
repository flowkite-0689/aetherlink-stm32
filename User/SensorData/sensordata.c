#include "sensordata.h"
#include "debug.h"
#include "Delay.h"
#include "stm32f10x_rcc.h"
#include "stm32f10x_gpio.h"

static TaskHandle_t sensordate_handle = NULL;
uint8_t DHT11_ON = 1;
uint8_t Light_ON = 1;

uint8_t DHT11_ERR = 0 ;
uint8_t Light_ERR = 0 ;
SensorData_TypeDef SensorData;
 uint16_t Sensordata_delaytime = 3000;
void SensorData_Init(void)
{
    DHT11_Init();
    Light_ADC_Init(); // ADC1 初始化用于光照传感器
}

static void SensorData_Task(void *pvParameters)
{
    printf("SensorData_Task start ->\n");

    // 初始延时，确保系统稳定
    vTaskDelay(pdMS_TO_TICKS(1000));

    while (1)
    {
       
        if (DHT11_ON)
        {
            taskENTER_CRITICAL();
            Read_DHT11(&SensorData.dht11_data);
            if (SensorData.dht11_data.temp_deci==0&&SensorData.dht11_data.humi_int==0&&SensorData.dht11_data.temp_int==0)
            {
                DHT11_ERR=1;
            }
            
            taskEXIT_CRITICAL();
        }

        // 读取光照强度数据
        // 配置ADC通道1 (PA1) 用于光照传感器
        if (Light_ON)
        {
            taskENTER_CRITICAL();
            ADC_RegularChannelConfig(ADC1, ADC_Channel_1, 1, ADC_SampleTime_55Cycles5);
            uint16_t lux_value = Light_GetLux();
            SensorData.light_data.lux = lux_value;
            taskEXIT_CRITICAL();
        }
      

        // 每3秒读取一次传感器数据
        vTaskDelay(pdMS_TO_TICKS(Sensordata_delaytime));
    }
}

void SensorData_CreateTask(void)
{
    xTaskCreate((TaskFunction_t)SensorData_Task,     /* 任务函数 */
                (const char *)"SensorData",          /* 任务名称 */
                (uint16_t)620,                       /* 任务堆栈大小 */
                (void *)NULL,                        /* 任务函数参数 */
                (UBaseType_t)3,                      /* 任务优先级 */
                (TaskHandle_t *)&sensordate_handle); /* 任务控制句柄 */
}
