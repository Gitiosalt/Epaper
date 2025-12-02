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
#include "bsp_led.h"
#include "bsp_epaper.h"
#include "epaper.h"
#include "picture.h"
#include "ds1302.h"
#include "bsp_SysTick.h"

ErrorStatus HSEStartUpStatus;

#define APP_CREATE_TASK_PRIO   	 (tskIDLE_PRIORITY + 1)  
#define CLOCK_TASK_PRIO        	 (tskIDLE_PRIORITY + 2)  

#define APP_CREATE_TASK_STACK  	 128
#define CLOCK_TASK_STACK         128

static TaskHandle_t App_Create_Task_Handle = NULL;
static TaskHandle_t Clock_Task_Handle = NULL;

const unsigned char *num_image_arr[] = {
    gImage_num0, gImage_num1, gImage_num2, gImage_num3, gImage_num4,
    gImage_num5, gImage_num6, gImage_num7, gImage_num8, gImage_num9
};

static void Clock_Task(void *arg){
	LED2_ON;
	int second_l , second_h ,minute_l , minute_h , hour_l , hour_h= 0;
	
	EPD_W21_Init();	
	EPD_HW_Init(); 									//Electronic paper initialization
	EPD_WhiteScreen_ALL(gImage_1); 					//Refresh the picture in full screen
	driver_delay_xms(500);
	
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
		
		taskENTER_CRITICAL();
		EPD_HW_Init();
		taskEXIT_CRITICAL();

		if (hour_l >= 0 && hour_l <= 9) {
			EPD_Dis_Part(120, 32, num_image_arr[hour_l],33, 64, POS);
		}
		if (hour_h >= 0 && hour_h <= 9) {
			EPD_Dis_Part(75, 32,num_image_arr[hour_h],33, 64, POS);
		}
		
		EPD_Dis_Part(160,55,gImage_DOT,8,8,POS);	
		EPD_Dis_Part(160,77,gImage_DOT,8,8,POS);	
		
		if (minute_l >= 0 && minute_l <= 9) {
			EPD_Dis_Part(221,32, num_image_arr[minute_l],33, 64, POS);
		}
		if (minute_h >= 0 && minute_h <= 9) {
			EPD_Dis_Part(175,32,num_image_arr[minute_h],33, 64, POS);
		}
		
		taskENTER_CRITICAL();
		EPD_Part_Update_and_DeepSleep();
		taskEXIT_CRITICAL();

		driver_delay_xms(59900);
	}
									
	//Clear screen
	taskENTER_CRITICAL();
	EPD_HW_Init(); 												
	EPD_WhiteScreen_White();  
	taskEXIT_CRITICAL();

///////////////////////////////////////////
	//全屏刷新整张图片（无灰阶）
	//Full screen refresh
//	EPD_HW_Init(); 													//Electronic paper initialization
//	EPD_WhiteScreen_ALL(gImage_1); 					//Refresh the picture in full screen
//	driver_delay_xms(500);
////////////////////////////////////////////////////////////////////////	
	while(1);		

}

static void AppCreate_Task(void *arg){
	portBASE_TYPE xReturn = pdPASS;
	
    taskENTER_CRITICAL();
	
	xReturn = xTaskCreate(
							(TaskFunction_t)Clock_Task,       
							(const char* )"Clock_Task",       
							(uint16_t )CLOCK_TASK_STACK,                 
							(void* )NULL,                   
							(UBaseType_t )CLOCK_TASK_PRIO, 
							(TaskHandle_t* )&Clock_Task_Handle);
		
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
	SysTick_Init();
	LED_GPIO_Config();	 
	EPAPER_GPIO_Config();
	DS1302_Init();
//	DS1302_SetTime();

	
	portBASE_TYPE xReturn = pdPASS;
	xReturn = xTaskCreate(
									(TaskFunction_t)AppCreate_Task,       
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
        LED2_OFF;
    }
									

}



/*********************************************END OF FILE**********************/
