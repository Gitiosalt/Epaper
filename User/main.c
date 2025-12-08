/**
  ******************************************************************************
  * @file    main.c
  * @author  iosalt
  * @version V1.0
  * @date    2025-11-29
  * @brief   
  ******************************************************************************
  * @attention
  * 
  * MCU平台:STM32F103CBT6 
  *
  ******************************************************************************
  */ 
	
#include "stm32f10x.h"   
#include "FreeRTOS.h"
#include "task.h"
#include "semphr.h"
#include "bsp_led.h"
#include "bsp_epaper.h"
#include "epaper.h"
#include "picture.h"
#include "ds1302.h"
#include "bsp_exti.h"
//#include "bsp_SysTick.h"

ErrorStatus HSEStartUpStatus;

#define APP_CREATE_TASK_PRIO   	 (tskIDLE_PRIORITY + 1)  
#define CLOCK_TASK_PRIO        	 (tskIDLE_PRIORITY + 3)  
#define LED_TASK_PRIO        	 (tskIDLE_PRIORITY + 3)  
#define KEY_TASK_PRIO        	 (tskIDLE_PRIORITY + 4)  

#define APP_CREATE_TASK_STACK  	 128
#define CLOCK_TASK_STACK         256
#define LED_TASK_STACK        	 128
#define KEY_TASK_STACK        	 128

static TaskHandle_t App_Create_Task_Handle = NULL;
static TaskHandle_t Clock_Task_Handle = NULL;
static TaskHandle_t Led_Task_Handle = NULL;
static TaskHandle_t Key_Task_Handle = NULL;

QueueHandle_t xKeyEventQueue;

BaseType_t xHigherPriorityTaskWoken = pdFALSE;

const unsigned char *num_image_arr[] = {
    gImage_num0, gImage_num1, gImage_num2, gImage_num3, gImage_num4,
    gImage_num5, gImage_num6, gImage_num7, gImage_num8, gImage_num9
};

typedef enum {
    KEY_NONE = 0,
    KEY1_PRESS,
    KEY2_PRESS
} KeyEvent_t;

void KEY_IRQHandler(void)
{
	KeyEvent_t key_event = KEY_NONE;
	
	// KEY1 PB14(Line14)
    if(EXTI_GetITStatus(EXTI_Line14) != RESET)
    {
		if(KEY1_STATE == 0){ 
			key_event = KEY1_PRESS;
			xQueueSendToBackFromISR(xKeyEventQueue ,&key_event ,&xHigherPriorityTaskWoken);
        }
        EXTI_ClearITPendingBit(EXTI_Line14);
		portYIELD_FROM_ISR(xHigherPriorityTaskWoken); 
    }

    // KEY2 PB15(Line15)
    if(EXTI_GetITStatus(EXTI_Line15) != RESET)
    {
		if(KEY2_STATE == 0){ 
			key_event = KEY2_PRESS;
			xQueueSendToBackFromISR(xKeyEventQueue ,&key_event ,&xHigherPriorityTaskWoken);
        }
        EXTI_ClearITPendingBit(EXTI_Line15);
		portYIELD_FROM_ISR(xHigherPriorityTaskWoken); 
    }
}


static void Clock_Task(void *arg){
	int second_l , second_h ,minute_l , minute_h , hour_l , hour_h= 0;
	
	TickType_t xLastWakeTime;
    const TickType_t xPeriod = 60000 / portTICK_PERIOD_MS;  
    xLastWakeTime = xTaskGetTickCount();
	
	int pot = 16;
	
	while(1)
	{
		taskENTER_CRITICAL();
		DS1302_GetTime();
		taskEXIT_CRITICAL();
//		second_l = Time[SECOND] % 10;
//		second_h = Time[SECOND] / 10;
		
		minute_l = Time[MINUTE] % 10;
		minute_h = Time[MINUTE] / 10;
		
		hour_l = Time[HOUR] % 10;
		hour_h = Time[HOUR] / 10;
		
		EPD_W21_Init();
		
		EPD_Dis_Part(75-pot, 32,num_image_arr[hour_h],33, 64, POS);	
		EPD_Dis_Part(120-pot, 32, num_image_arr[hour_l],33, 64, POS);
		EPD_Dis_Part(160-pot,55,gImage_DOT,8,8,POS);	
		EPD_Dis_Part(160-pot,77,gImage_DOT,8,8,POS);	
		EPD_Dis_Part(175-pot,32,num_image_arr[minute_h],33, 64, POS);
		EPD_Dis_Part(221-pot,32, num_image_arr[minute_l],33, 64, POS);
		
		EPD_Part_Update_and_DeepSleep();
	
        vTaskDelayUntil(&xLastWakeTime, xPeriod);
	}
									
	//Clear screen
	EPD_HW_Init(); 												
	EPD_WhiteScreen_White();  
}

static void Led_Task(void *arg){
	while(1){
//		LED1_TOGGLE;
		vTaskDelay(1500 / portTICK_PERIOD_MS);
	}	
}

void Key_Task(void *pvParameters)
{
	 KeyEvent_t recv_event;
    while(1)
    {
		if(xQueueReceive(xKeyEventQueue, &recv_event, portMAX_DELAY) == pdTRUE){
			vTaskDelay(20 / portTICK_PERIOD_MS);
            switch(recv_event)
            {
                case KEY1_PRESS:
                    if(KEY1_STATE == 0)  
                    {
                        LED1_TOGGLE;  // KEY1
                    }
                    break;

                case KEY2_PRESS:
                    if(KEY2_STATE == 0)
                    {
                        LED1_TOGGLE;  // KEY2
                    }
                    break;

                default:
                    break;
            }
		}
		
    }
}

static void AppCreate_Task(void *arg){
	portBASE_TYPE xReturn = pdPASS;
    taskENTER_CRITICAL();
	
	xReturn = xTaskCreate((TaskFunction_t)Clock_Task,       
						  (const char* )"Clock_Task",       
						  (uint16_t )CLOCK_TASK_STACK,                 
						  (void* )NULL,                   
						  (UBaseType_t )CLOCK_TASK_PRIO, 
						  (TaskHandle_t* )&Clock_Task_Handle);
						  
	xReturn = xTaskCreate((TaskFunction_t)Led_Task,       
						  (const char* )"Led_Task",       
						  (uint16_t )LED_TASK_STACK,                 
						  (void* )NULL,                   
						  (UBaseType_t )LED_TASK_PRIO, 
						  (TaskHandle_t* )&Led_Task_Handle);	

	xReturn = xTaskCreate((TaskFunction_t)Key_Task,       
						  (const char* )"Key_Task",       
						  (uint16_t )KEY_TASK_STACK,                 
						  (void* )NULL,                   
						  (UBaseType_t )KEY_TASK_PRIO, 
						  (TaskHandle_t* )&Key_Task_Handle);							  
						  
    taskEXIT_CRITICAL();
								
	vTaskDelete( App_Create_Task_Handle);
}

/**
  * @brief  主函数
  * @param  无  
  * @retval 无
  */
/////////////////////main//////////////////////////////////////
int main(void)
{
	#ifdef DEBUG
	  debug();
	#endif
//	SysTick_Init();
	LED_GPIO_Config();	 
	EXTI_Key_Config(); 
	EPAPER_GPIO_Config();
	DS1302_Init();
//	DS1302_SetTime();


///////////////////////////////////////////
//  全刷方式刷新整张图片（4灰阶）
//	EPD_HW_Init_4GRAY(); 										//EPD init 4Gray
//	EPD_WhiteScreen_ALL_4GRAY(gImage_4Gray4); 	
//    driver_delay_xms(2000);				
	
//	//全屏刷新整张图片（无灰阶）
//	EPD_HW_Init();
//	EPD_WhiteScreen_ALL(gImage_hust);
//	driver_delay_xms(2000);

	//全屏刷新刷背景图片
	EPD_HW_Init(); 
	EPD_SetRAMValue_BaseMap(gImage_base);  
	driver_delay_xms(500);

////	//    y         x       显示内容     显示宽度     显示高度     显示模式
//	EPD_W21_Init();											//hard reset
//	EPD_Dis_Part(25,104,black_block,4,8,POS);		      //  248        16      电量第二格        4            8				   OFF
//	EPD_Part_Update_and_DeepSleep();				
//	driver_delay_xms(500);				
//	while(1);

	
	xKeyEventQueue = xQueueCreate( 3 , 1 );
	xQueueReset( xKeyEventQueue );
	
	portBASE_TYPE xReturn = pdPASS;
	xReturn = xTaskCreate((TaskFunction_t)AppCreate_Task,       
						  (const char* )"AppCreate_Task",       
						  (uint16_t )APP_CREATE_TASK_STACK,                 
						  (void* )NULL,                   
						  (UBaseType_t )APP_CREATE_TASK_PRIO, 
						  (TaskHandle_t* )&App_Create_Task_Handle);
	
	if (xReturn == pdPASS)
    {
        vTaskStartScheduler();
    }

    while (1)
    {
        LED1_OFF;
    }
									

}



/*********************************************END OF FILE**********************/
