#include "stm32f10x.h" // Device header
#include <FreeRTOS.h>
#include <task.h>
#include "hardware_def.h"
#include "Key.h"
#include "debug.h"
#include "beep.h"
#include "oled_print.h"
#include "rtc_date.h"
#include "queue.h"
#include "unified_menu.h"
#include "index.h"
#include "esp8266.h"
#include "uart2.h"
#include "uart3.h"
#include "HC-05.h"
#include "light.h"
#include "rtc_date.h"
#include "sensordata.h"
// 创建队列来存储按键事件
QueueHandle_t keyQueue; // 按键队列

// 验证传感器状态变量的可访问性
extern uint8_t Light_ON;

static TaskHandle_t Menu_handle = NULL;
static TaskHandle_t Key_handle = NULL;
static TaskHandle_t ESP8266_handle = NULL;
static TaskHandle_t Bluetooth_handle = NULL;

/* 任务函数声明 */
static void Menu_Main_Task(void *pvParameters);
static void Key_Main_Task(void *pvParameters);
static void ESP8266_Main_Task(void *pvParameters);
static void Bluetooth_Main_Task(void *pvParameters);

int main(void)
{
    // 系统初始化开始
    TIM2_Delay_Init();
    debug_init();
    OLED_Init();
    HC05_Init(115200);
    HC05_Send_String("test1\n");
    HC05_Send_String("AT\r\n");
    OLED_Show_many_Tupian(tjbg, 8, 1);

    // if(HC05_Send_AT_Cmd("AT\r\n", "OK", 1000))
    // {
    //     printf("AT OK\r\n");
    //     HC05_Send_String("AT OK\r\n");
    // }
    OLED_Refresh();
    Key_Init();

    // 初始化UART2，用于ESP8266通信
    UART2_DMA_RX_Init(115200);

    
    printf("UART3 (Bluetooth) DMA+IDLE initialization complete\r\n");

    // 读取Flash大小寄存器 (0x1FFFF7E22)
    uint16_t flash_size = *((uint16_t *)0x1FFFF7E0);

    printf("Flash Size: %d KB\n", flash_size);
    Light_ADC_Init();
    printf("Light_ADC_Init OK\n");

    OLED_Refresh();
    Delay_s(1);
    OLED_Clear();

    // printf("ESP8266初始化将在独立任务中进行...\r\n");
    // OLED_Printf_Line(0, "WiFi Connecting...");
    // OLED_Printf_Line(1, "Please Wait...");
    OLED_Refresh();

    // 初始化传感器数据模块
    SensorData_Init();
    printf("Sensor data initialization complete\r\n");

    // 初始化菜单系统
    if (menu_system_init() != 0)
    {
        printf("Menu system initialization failed\r\n");
        return -1;
    }

    // 创建并初始化首页
    menu_item_t *index_menu = index_init();
    if (index_menu == NULL)
    {
        printf("Index page initialization failed\r\n");
        return -1;
    }

    // 设置首页为根菜单
    g_menu_sys.root_menu = index_menu;
    g_menu_sys.current_menu = index_menu;

    /* 创建菜单任务 */
    xTaskCreate((TaskFunction_t)Menu_Main_Task, /* 任务函数 */
                (const char *)"Menu_Main",      /* 任务名称 */
                (uint16_t)2024,                 /* 任务堆栈大小 */
                (void *)NULL,                   /* 任务函数参数 */
                (UBaseType_t)4,                 /* 任务优先级 */
                (TaskHandle_t *)&Menu_handle);  /* 任务控制句柄 */
    xTaskCreate(Key_Main_Task, "KeyMain", 96, NULL, 4, &Key_handle);

    // 创建蓝牙数据处理任务
    xTaskCreate((TaskFunction_t)Bluetooth_Main_Task,
                (const char *)"Bluetooth_Main",
                (uint16_t)384,
                (void *)NULL,
                (UBaseType_t)2,
                (TaskHandle_t *)&Bluetooth_handle);

    printf("------------------\n");
    printf("creat task OK\n");
    printf("------------------\n");

    // 创建传感器数据采集任务
    SensorData_CreateTask();
    printf("SensorData task created\n");

    // 打印传感器初始状态
    printf("Initial sensor states: Light=%d\n", Light_ON);

    // // 创建ESP8266任务
    // xTaskCreate((TaskFunction_t)ESP8266_Main_Task, /* 任务函数 */
    //             (const char *)"ESP8266_Main",      /* 任务名称 */
    //             (uint16_t)384,                     /* 任务堆栈大小 */
    //             (void *)NULL,                      /* 任务参数 */
    //             (UBaseType_t)2,                    /* 任务优先级 */
    //             (TaskHandle_t *)&ESP8266_handle);  /* 任务句柄 */

    // 添加调试信息，确认调度器启动
    printf("Starting scheduler...\n");

    /* 启动调度器 */
    vTaskStartScheduler();

    // 如果调度器启动失败，会执行到这里
    printf("ERROR: Scheduler failed to start!\n");
    // LED2_ON();
}

static void Menu_Main_Task(void *pvParameters)
{
    printf("Menu_Main_Task start ->\n");
    // 直接调用统一菜单框架的任务
    menu_task(pvParameters);
}

static void Key_Main_Task(void *pvParameters)
{
    // 直接调用统一菜单功能的按键处理
    menu_key_task(pvParameters);
}

static void Bluetooth_Main_Task(void *pvParameters)
{
    printf("Bluetooth_Main_Task start ->\n");

    // 初始化蓝牙模块（如果main中没有初始化）
    // if(Bluetooth_Init(9600) != BLUETOOTH_OK)
    // {
    //     printf("Bluetooth initialization failed in task\r\n");
    //     vTaskDelete(NULL);
    //     return;
    // }

    // 蓝牙任务主循环
    while (1)
    {
//   UART3_SendDataToBLE_Poll("hello\n",sizeof("hello\n"));
        // HC05_Send_String("hello\n");
        if (uart3_rx_len > 0)
        {
            uart3_rx_len = 0;
            printf("HC-05 Receive Data: %s\r\n", uart3_buffer); 

            
        }
        // 延时10ms
        vTaskDelay(pdMS_TO_TICKS(10));
    }
}
