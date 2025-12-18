#ifndef _SENSORDATA_H_
#define _SENSORDATA_H_ 

//温湿度
#include "dht11.h"
//光照
#include "light.h"

#include <FreeRTOS.h>
#include <task.h>

typedef struct {
    PhotoRes_TypeDef    light_data;

} SensorData_TypeDef;
extern SensorData_TypeDef SensorData;


extern uint16_t Sensordata_delaytime; // 传感器读取间隔时间


extern uint8_t Light_ON;

extern uint8_t Light_ERR;



void SensorData_Init(void);
void SensorData_CreateTask(void);


#endif
