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
  * MCUƽ̨:STM32F103CBT6 
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

//ErrorStatus HSEStartUpStatus;

//任务优先级
#define APP_CREATE_TASK_PRIO   	 (tskIDLE_PRIORITY + 1)  
#define CLOCK_TASK_PRIO        	 (tskIDLE_PRIORITY + 3)  
#define LED_TASK_PRIO        	 (tskIDLE_PRIORITY + 3)  
#define KEY_TASK_PRIO        	 (tskIDLE_PRIORITY + 4)  
#define MENU_TASK_PRIO        	 (tskIDLE_PRIORITY + 3) 

//任务栈大小
#define APP_CREATE_TASK_STACK  	 128
#define CLOCK_TASK_STACK         256
#define LED_TASK_STACK        	 128
#define KEY_TASK_STACK        	 128
#define MENU_TASK_STACK        	 128

//任务句柄
static TaskHandle_t App_Create_Task_Handle = NULL;
static TaskHandle_t Clock_Task_Handle = NULL;
static TaskHandle_t Led_Task_Handle = NULL;
static TaskHandle_t Key_Task_Handle = NULL;
static TaskHandle_t Menu_Task_Handle = NULL;

//消息队列
QueueHandle_t xKeyEventQueue;
QueueHandle_t xMenuStateQueue;

//
BaseType_t xHigherPriorityTaskWoken = pdFALSE;

const unsigned char *num_image_arr[] = {
    gImage_num0, gImage_num1, gImage_num2, gImage_num3, gImage_num4,
    gImage_num5, gImage_num6, gImage_num7, gImage_num8, gImage_num9
};

const unsigned char *NUM_image_arr[] = {
    gImage_NUM0, gImage_NUM1, gImage_NUM2, gImage_NUM3, gImage_NUM4,
    gImage_NUM5, gImage_NUM6, gImage_NUM7, gImage_NUM8, gImage_NUM9
};

const unsigned char *CHN_image_arr[] = {
    gImage_one, gImage_two, gImage_three, gImage_four, gImage_five,
    gImage_six, gImage_sky
};

// 菜单状态枚举
typedef enum {
    MENU_STATE_MAIN,        // 主菜单（显示“时间设置/图片切换”）
    MENU_STATE_TIME_SET,    // 时间设置子菜单（调整时/分）
    MENU_STATE_PIC_SWITCH,  // 图片切换子菜单
    MENU_STATE_EXIT         // 退出菜单（回到时钟显示）
} MenuState_t;

//按键状态枚举类型
typedef enum {
    KEY_NONE = 0,
    KEY1_PRESS,
    KEY2_PRESS
} KeyEvent_t;

//按键行为枚举类型
typedef enum {
    KEY_ACTION_NONE = 0,
    KEY1_CLICK,    // KEY1单击（确认/进入下一级）
    KEY1_LONG,     // KEY1长按（快速调整数值/保存）
    KEY1_DOUBLE,   // KEY1双击（可选：一键退出）
    KEY2_CLICK,    // KEY2单击（切换选项/返回上一级）
    KEY2_LONG,     // KEY2长按（快速切换/重置）
    KEY2_DOUBLE    // KEY2双击（可选：一键确认）
} KeyAction_t;

// 时间设置子状态枚举（细化时间调整项）
typedef enum {
    TIME_SET_MINUTE,        // 调整分钟
    TIME_SET_HOUR,          // 调整小时
    TIME_SET_DATE,          // 调整日期
    TIME_SET_MONTH,         // 调整月份
    TIME_SET_YEAR,          // 调整年份
    TIME_SET_WEEK,          // 调整星期
    TIME_SET_SAVE_EXIT,     // 保存退出
    TIME_SET_ITEM_MAX       // 枚举边界
} TimeSetSubState_t;


// 全局时间设置缓存（避免直接修改DS1302实时时间）
typedef struct {
    uint8_t minute;         // 缓存分钟 (0-59)
    uint8_t hour;           // 缓存小时 (0-23)
    uint8_t date;           // 缓存日期 (1-31)
    uint8_t month;          // 缓存月份 (1-12)
    uint8_t year;           // 缓存年份 (0-99)
    uint8_t week;           // 缓存星期 (1-7)
    TimeSetSubState_t sub_state; // 当前调整项
} TimeSetCache_t;

TimeSetCache_t g_TimeSetCache; // 时间设置缓存结构体

// 按键配置参数
#define KEY_LONG_TIME   	 800 / portTICK_PERIOD_MS  // 长按判定时间（700ms）
#define KEY_DOUBLE_TIME  	 600 / portTICK_PERIOD_MS   // 双击间隔（300ms）
#define KEY_DEBOUNCE_TIME 	 20 / portTICK_PERIOD_MS   // 消抖时间

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
	int minute,hour,date,month,year,week= 0;

	TickType_t xLastWakeTime;
    const TickType_t xPeriod = 60000 / portTICK_PERIOD_MS;  
    xLastWakeTime = xTaskGetTickCount();
	
	int NUM_Offset = 20;    //大数字偏移
	int num_Offset = 40;	//小数字偏移
	
	taskENTER_CRITICAL();
	DS1302_GetTime();
	taskEXIT_CRITICAL();
	
	g_TimeSetCache.hour = Time[HOUR];
	g_TimeSetCache.minute = Time[MINUTE];
	while(1)
	{
		vTaskDelay(1000 / portTICK_PERIOD_MS);
	}
	while(1)
	{
		taskENTER_CRITICAL();
		DS1302_GetTime();
		taskEXIT_CRITICAL();

		year 	= Time[YEAR];
		month 	= Time[MONTH];
		date	= Time[DATE];
		hour	= Time[HOUR];
		minute 	= Time[MINUTE];
		week 	= Time[WEEK];	
		
		EPD_W21_Init();
		//日期
		EPD_Dis_Part(9* 0+num_Offset, 8,num_image_arr[year / 1000],		   12, 16, POS);	
		EPD_Dis_Part(9* 1+num_Offset, 8,num_image_arr[(year / 100) % 10],		   12, 16, POS);		
		EPD_Dis_Part(9* 2+num_Offset, 8,num_image_arr[year / 10],     12, 16, POS);	
		EPD_Dis_Part(9* 3+num_Offset, 8,num_image_arr[year % 10],     12, 16, POS);		
		EPD_Dis_Part(9* 4+num_Offset+3, 8,gImage_year,			   12, 16, POS);
		EPD_Dis_Part(9* 5+num_Offset+6, 8,num_image_arr[month / 10],  12, 16, POS);	
		EPD_Dis_Part(9* 6+num_Offset+6, 8,num_image_arr[month % 10],  12, 16, POS);	
		EPD_Dis_Part(9* 7+num_Offset+6+3, 8,gImage_month,		   12, 16, POS);
		EPD_Dis_Part(9* 8+num_Offset+6+6, 8,num_image_arr[date / 10], 12, 16, POS);	
		EPD_Dis_Part(9* 9+num_Offset+6+6, 8,num_image_arr[date  % 10], 12, 16, POS);	
		EPD_Dis_Part(9*10+num_Offset+6+6+3, 8,gImage_date,		   12, 16, POS);
		//星期
		EPD_Dis_Part(220, 8,gImage_week_h,			12, 16, POS);
		EPD_Dis_Part(220 +14, 8,gImage_week_l,			12, 16, POS);
		EPD_Dis_Part(220 +28, 8,CHN_image_arr[week], 12, 16, POS);	
		//时分
		EPD_Dis_Part(75- NUM_Offset, 32+8,NUM_image_arr[hour / 10],33, 64,   POS);	
		EPD_Dis_Part(120-NUM_Offset, 32+8, NUM_image_arr[hour  % 10],33, 64,  POS);
		EPD_Dis_Part(160-NUM_Offset, 55+8,gImage_DOT,8,8,POS);	
		EPD_Dis_Part(160-NUM_Offset, 77+8,gImage_DOT,8,8,POS);	
		EPD_Dis_Part(175-NUM_Offset, 32+8,NUM_image_arr[minute / 10],33, 64, POS);
		EPD_Dis_Part(221-NUM_Offset, 32+8, NUM_image_arr[minute  % 10],33, 64,POS);
		
		EPD_Part_Update_and_DeepSleep();
        vTaskDelayUntil(&xLastWakeTime, xPeriod);
	}
									
	//Clear screen
	EPD_HW_Init(); 												
	EPD_WhiteScreen_White();  
}

static void Led_Task(void *arg){
	while(1){
		LED1_TOGGLE;
		vTaskDelay(1000 / portTICK_PERIOD_MS);
	}	
}

void Key_Task(void *pvParameters)
{
    KeyEvent_t recv_event;
    KeyAction_t key_action = KEY_ACTION_NONE;
    TickType_t key_press_time;
    BaseType_t is_double_click = pdFALSE;

    while(1)
    {
        if(xQueueReceive(xKeyEventQueue, &recv_event, portMAX_DELAY) == pdTRUE)
        {
            vTaskDelay(KEY_DEBOUNCE_TIME); // 消抖
            switch(recv_event)
            {
                case KEY1_PRESS:
                    if(KEY1_STATE == 0) // 确认按键真的按下
                    {
                        key_press_time = xTaskGetTickCount();
                        // 检测长按：等待期间持续检测按键是否保持按下
                        while(KEY1_STATE == 0)
                        {
                            if((xTaskGetTickCount() - key_press_time) >= KEY_LONG_TIME)
                            {
                                key_action = KEY1_LONG;
                                goto KEY_ACTION_SEND; // 触发长按，跳出循环
                            }
                            vTaskDelay(10 / portTICK_PERIOD_MS); // 轮询间隔
                        }

                        // 不是长按，检测双击：等待第二击
                        if(xQueuePeek(xKeyEventQueue, &recv_event, KEY_DOUBLE_TIME) == pdTRUE)//不消耗队列事件的前提下，检测 “第二击” 是否存在
                        {
                            vTaskDelay(KEY_DEBOUNCE_TIME);
                            if(recv_event == KEY1_PRESS && KEY1_STATE == 0)
                            {
                                key_action = KEY1_DOUBLE;
                                xQueueReceive(xKeyEventQueue, &recv_event, 0); // 清空第二击事件
                            }
                            else
                            {
                                key_action = KEY1_CLICK; // 无第二击，判定单击
                            }
                        }
                        else
                        {
                            key_action = KEY1_CLICK; // 超时无第二击，判定单击
                        }
                    }
                    break;

                case KEY2_PRESS:
                    if(KEY2_STATE == 0)
                    {
                        key_press_time = xTaskGetTickCount();
                        // 检测长按
                        while(KEY2_STATE == 0)
                        {
                            if((xTaskGetTickCount() - key_press_time) >= KEY_LONG_TIME)
                            {
                                key_action = KEY2_LONG;
                                goto KEY_ACTION_SEND;
                            }
                            vTaskDelay(10 / portTICK_PERIOD_MS);
                        }

                        // 检测双击
                        if(xQueuePeek(xKeyEventQueue, &recv_event, KEY_DOUBLE_TIME) == pdTRUE)
                        {
                            vTaskDelay(KEY_DEBOUNCE_TIME);
                            if(recv_event == KEY2_PRESS && KEY2_STATE == 0)
                            {
                                key_action = KEY2_DOUBLE;
                                xQueueReceive(xKeyEventQueue, &recv_event, 0);
                            }
                            else
                            {
                                key_action = KEY2_CLICK;
                            }
                        }
                        else
                        {
                            key_action = KEY2_CLICK;
                        }
                    }
                    break;

                default:
                    key_action = KEY_ACTION_NONE;
                    break;
            }

        KEY_ACTION_SEND:
            // 发送细分的按键行为到菜单队列
            if(key_action != KEY_ACTION_NONE)
            {
                xQueueSendToBack(xMenuStateQueue, &key_action, 0);
                key_action = KEY_ACTION_NONE; // 重置
            }
        }
    }
}

static void Menu_Task(void *arg){
    KeyAction_t recv_action;
    MenuState_t xCurrentMenuState = MENU_STATE_MAIN; // 默认初始化为主菜单
    // 初始化时间缓存（从DS1302读取当前时间）
    taskENTER_CRITICAL();
    DS1302_GetTime();
    g_TimeSetCache.minute = Time[MINUTE];
    g_TimeSetCache.hour = Time[HOUR];
    g_TimeSetCache.date = Time[DATE];
    g_TimeSetCache.month = Time[MONTH];
    g_TimeSetCache.year = Time[YEAR];
    g_TimeSetCache.week = Time[WEEK];
    g_TimeSetCache.sub_state = TIME_SET_MINUTE; // 默认先调整分钟
    taskEXIT_CRITICAL();
	
	EPD_W21_Init();
	EPD_Dis_Part(15,  80, txt_time_set, 32, 16, POS); // 显示"时间"
	EPD_Dis_Part(15, 104, txt_picture, 32, 16, POS); //	显示"壁纸"

	EPD_Part_Update_and_DeepSleep();
    while(1)
    {
        xQueueReceive( xMenuStateQueue, &recv_action, portMAX_DELAY );
        switch(xCurrentMenuState)
        {
			
            // ------------------------ 主菜单逻辑 ------------------------
            case MENU_STATE_MAIN:    
                switch(recv_action)
                {
                    case KEY1_CLICK: // 单击：进入时间设置子菜单
                        xCurrentMenuState = MENU_STATE_TIME_SET;
                        
                        EPD_W21_Init();
                        EPD_Dis_Part(15, 80, txt_time_set, 32, 16, NEG); // 显示"时间设置"
						EPD_Dis_Part(15, 104, txt_picture, 32, 16, POS); //	显示"图片设置"
						EPD_Dis_Part(15, 60, txt_save_exit, 32, 16, POS); // 显示"保存"
                        // 显示时间设置初始界面（分钟闪烁/高亮）
						// 时分
                        EPD_Dis_Part(60,  40, NUM_image_arr[g_TimeSetCache.hour/10], 33, 64, POS);
                        EPD_Dis_Part(105, 40, NUM_image_arr[g_TimeSetCache.hour%10], 33, 64, POS);
                        EPD_Dis_Part(150, 63, gImage_DOT, 8, 8, POS); // 显示冒号
                        EPD_Dis_Part(150, 85, gImage_DOT, 8, 8, POS); // 显示冒号
                        EPD_Dis_Part(175, 40, NUM_image_arr[g_TimeSetCache.minute/10], 33, 64, NEG);
                        EPD_Dis_Part(220, 40, NUM_image_arr[g_TimeSetCache.minute%10], 33, 64, NEG);
						// 年月日周
						EPD_Dis_Part( 40,  8, num_image_arr[g_TimeSetCache.year/1000], 12, 16, POS);
						EPD_Dis_Part( 49,  8, num_image_arr[(g_TimeSetCache.year/100)%10], 12, 16, POS);
						EPD_Dis_Part( 58,  8, num_image_arr[g_TimeSetCache.year/10], 12, 16, POS);
						EPD_Dis_Part( 67,  8, num_image_arr[g_TimeSetCache.year%10], 12, 16, POS);
						EPD_Dis_Part( 79,  8, gImage_year, 12, 16, POS);
						EPD_Dis_Part( 91,  8, num_image_arr[g_TimeSetCache.month/10], 12, 16, POS);
						EPD_Dis_Part(100,  8, num_image_arr[g_TimeSetCache.month%10], 12, 16, POS);
						EPD_Dis_Part(112,  8, gImage_month, 12, 16, POS);
						EPD_Dis_Part(124,  8, num_image_arr[g_TimeSetCache.date/10], 12, 16, POS);
						EPD_Dis_Part(133,  8, num_image_arr[g_TimeSetCache.date%10], 12, 16, POS);
						EPD_Dis_Part(145,  8, gImage_date, 12, 16, POS);
						
						EPD_Dis_Part(220, 8, gImage_week_h,			12, 16, POS);
						EPD_Dis_Part(234, 8, gImage_week_l,			12, 16, POS);
						EPD_Dis_Part(248, 8, CHN_image_arr[g_TimeSetCache.week], 12, 16, POS);
						EPD_Part_Update_and_DeepSleep();
                        break;
                    
                    case KEY2_CLICK: // KEY2单击：进入图片切换子菜单
                        xCurrentMenuState = MENU_STATE_PIC_SWITCH;
                        EPD_W21_Init();
                        EPD_Dis_Part(15, 80, txt_time_set, 32, 16, POS); // 显示"时间设置"
						EPD_Dis_Part(15, 104, txt_picture, 32, 16, NEG); //	显示"图片设置"
                        EPD_Part_Update_and_DeepSleep();
                        break;
                    
                    case KEY1_LONG: // KEY1长按：退出菜单（回到时钟）
                        xCurrentMenuState = MENU_STATE_EXIT;
                        break;
                    
                    default:
                        break;
                }
                break;

            // ------------------------ 时间设置子菜单 ------------------------
            case MENU_STATE_TIME_SET:   
                switch(recv_action)
                {
                    // KEY1单击：切换调整项（小时→分钟→保存退出）
                    case KEY1_CLICK: 
                        switch(g_TimeSetCache.sub_state)
                        {
							    case TIME_SET_MINUTE:
								g_TimeSetCache.sub_state = TIME_SET_HOUR;
								// 高亮显示"保存"提示
                                EPD_W21_Init();
							    EPD_Dis_Part(15, 80, txt_time_set, 32, 16, NEG); // 显示"时间设置"
								EPD_Dis_Part(15, 104, txt_picture, 32, 16, POS); //	显示"图片设置"
								EPD_Dis_Part(15, 60, txt_save_exit, 32, 16, POS); // 显示"保存"
								EPD_Dis_Part(60,  32+8, NUM_image_arr[g_TimeSetCache.hour/10], 33, 64, NEG);
								EPD_Dis_Part(105, 32+8, NUM_image_arr[g_TimeSetCache.hour%10], 33, 64, NEG);
								EPD_Dis_Part(150, 55+8, gImage_DOT, 8, 8, POS); // 显示冒号
								EPD_Dis_Part(150, 77+8, gImage_DOT, 8, 8, POS); // 显示冒号
								EPD_Dis_Part(175, 32+8, NUM_image_arr[g_TimeSetCache.minute/10], 33, 64, POS);
								EPD_Dis_Part(220, 32+8, NUM_image_arr[g_TimeSetCache.minute%10], 33, 64, POS);
								// 年月日周
								EPD_Dis_Part( 40,  8, num_image_arr[g_TimeSetCache.year/1000], 12, 16, POS);
								EPD_Dis_Part( 49,  8, num_image_arr[(g_TimeSetCache.year/100)%10], 12, 16, POS);
								EPD_Dis_Part( 58,  8, num_image_arr[g_TimeSetCache.year/10], 12, 16, POS);
								EPD_Dis_Part( 67,  8, num_image_arr[g_TimeSetCache.year%10], 12, 16, POS);
								EPD_Dis_Part( 79,  8, gImage_year, 12, 16, POS);
								EPD_Dis_Part( 91,  8, num_image_arr[g_TimeSetCache.month/10], 12, 16, POS);
								EPD_Dis_Part(100,  8, num_image_arr[g_TimeSetCache.month%10], 12, 16, POS);
								EPD_Dis_Part(112,  8, gImage_month, 12, 16, POS);
								EPD_Dis_Part(124,  8, num_image_arr[g_TimeSetCache.date/10], 12, 16, POS);
								EPD_Dis_Part(133,  8, num_image_arr[g_TimeSetCache.date%10], 12, 16, POS);
								EPD_Dis_Part(145,  8, gImage_date, 12, 16, POS);			
								EPD_Dis_Part(220, 8, gImage_week_h,			12, 16, POS);
								EPD_Dis_Part(234, 8, gImage_week_l,			12, 16, POS);
								EPD_Dis_Part(248, 8, CHN_image_arr[g_TimeSetCache.week], 12, 16, POS);
                                EPD_Part_Update_and_DeepSleep();
                                break;
							
                            case TIME_SET_HOUR:
								g_TimeSetCache.sub_state = TIME_SET_DATE;
                                // 高亮分钟，取消小时高亮（闪烁效果用反显模拟）
                                EPD_W21_Init();
//							    EPD_Dis_Part(15, 80, txt_time_set, 32, 16, NEG); // 显示"时间设置"
//								EPD_Dis_Part(15, 104, txt_picture, 32, 16, POS); //	显示"图片设置"
								EPD_Dis_Part(15, 60, txt_save_exit, 32, 16, POS); // 显示"保存"
								EPD_Dis_Part(60,  32+8, NUM_image_arr[g_TimeSetCache.hour/10], 33, 64, POS);
								EPD_Dis_Part(105, 32+8, NUM_image_arr[g_TimeSetCache.hour%10], 33, 64, POS);
//								EPD_Dis_Part(150, 55+8, gImage_DOT, 8, 8, POS); // 显示冒号
//								EPD_Dis_Part(150, 77+8, gImage_DOT, 8, 8, POS); // 显示冒号
								EPD_Dis_Part(175, 32+8, NUM_image_arr[g_TimeSetCache.minute/10], 33, 64, POS);
								EPD_Dis_Part(220, 32+8, NUM_image_arr[g_TimeSetCache.minute%10], 33, 64, POS);
								// 年月日周
//								EPD_Dis_Part( 40,  8, num_image_arr[g_TimeSetCache.year/1000], 12, 16, POS);
//								EPD_Dis_Part( 49,  8, num_image_arr[(g_TimeSetCache.year/100)%10], 12, 16, POS);
//								EPD_Dis_Part( 58,  8, num_image_arr[g_TimeSetCache.year/10], 12, 16, POS);
//								EPD_Dis_Part( 67,  8, num_image_arr[g_TimeSetCache.year%10], 12, 16, POS);
//								EPD_Dis_Part( 79,  8, gImage_year, 12, 16, POS);
//								EPD_Dis_Part( 91,  8, num_image_arr[g_TimeSetCache.month/10], 12, 16, POS);
//								EPD_Dis_Part(100,  8, num_image_arr[g_TimeSetCache.month%10], 12, 16, POS);
//								EPD_Dis_Part(112,  8, gImage_month, 12, 16, POS);
								EPD_Dis_Part(124,  8, num_image_arr[g_TimeSetCache.date/10], 12, 16, NEG);
								EPD_Dis_Part(133,  8, num_image_arr[g_TimeSetCache.date%10], 12, 16, NEG);
//								EPD_Dis_Part(145,  8, gImage_date, 12, 16, POS);
//								
//								EPD_Dis_Part(220, 8, gImage_week_h,			12, 16, POS);
//								EPD_Dis_Part(234, 8, gImage_week_l,			12, 16, POS);
//								EPD_Dis_Part(248, 8, CHN_image_arr[g_TimeSetCache.week], 12, 16, POS);
								
								EPD_Part_Update_and_DeepSleep();
                                break;
                            
							case TIME_SET_DATE:
								g_TimeSetCache.sub_state = TIME_SET_MONTH;
								// 高亮显示"保存"提示
                                EPD_W21_Init();
//							    EPD_Dis_Part(15, 80, txt_time_set, 32, 16, NEG); // 显示"时间设置"
//								EPD_Dis_Part(15, 104, txt_picture, 32, 16, POS); //	显示"图片设置"
//								EPD_Dis_Part(15, 60, txt_save_exit, 32, 16, POS); // 显示"保存"
								EPD_Dis_Part(60,  32+8, NUM_image_arr[g_TimeSetCache.hour/10], 33, 64, POS);
								EPD_Dis_Part(105, 32+8, NUM_image_arr[g_TimeSetCache.hour%10], 33, 64, POS);
//								EPD_Dis_Part(150, 55+8, gImage_DOT, 8, 8, POS); // 显示冒号
//								EPD_Dis_Part(150, 77+8, gImage_DOT, 8, 8, POS); // 显示冒号
								EPD_Dis_Part(175, 32+8, NUM_image_arr[g_TimeSetCache.minute/10], 33, 64, POS);
								EPD_Dis_Part(220, 32+8, NUM_image_arr[g_TimeSetCache.minute%10], 33, 64, POS);
								// 年月日周
//								EPD_Dis_Part( 40,  8, num_image_arr[g_TimeSetCache.year/1000], 12, 16, POS);
//								EPD_Dis_Part( 49,  8, num_image_arr[(g_TimeSetCache.year/100)%10], 12, 16, POS);
//								EPD_Dis_Part( 58,  8, num_image_arr[g_TimeSetCache.year/10], 12, 16, POS);
//								EPD_Dis_Part( 67,  8, num_image_arr[g_TimeSetCache.year%10], 12, 16, POS);
//								EPD_Dis_Part( 79,  8, gImage_year, 12, 16, POS);
								EPD_Dis_Part( 91,  8, num_image_arr[g_TimeSetCache.month/10], 12, 16, NEG);
								EPD_Dis_Part(100,  8, num_image_arr[g_TimeSetCache.month%10], 12, 16, NEG);
//								EPD_Dis_Part(112,  8, gImage_month, 12, 16, POS);
								EPD_Dis_Part(124,  8, num_image_arr[g_TimeSetCache.date/10], 12, 16, POS);
								EPD_Dis_Part(133,  8, num_image_arr[g_TimeSetCache.date%10], 12, 16, POS);
//								EPD_Dis_Part(145,  8, gImage_date, 12, 16, POS);
//								EPD_Dis_Part(220, 8, gImage_week_h,			12, 16, POS);
//								EPD_Dis_Part(234, 8, gImage_week_l,			12, 16, POS);
//								EPD_Dis_Part(248, 8, CHN_image_arr[g_TimeSetCache.week], 12, 16, POS);
                                EPD_Part_Update_and_DeepSleep();
                                break;
							
							case TIME_SET_MONTH:
								g_TimeSetCache.sub_state = TIME_SET_YEAR;
								// 高亮显示"保存"提示
                                EPD_W21_Init();
//							    EPD_Dis_Part(15, 80, txt_time_set, 32, 16, NEG); // 显示"时间设置"
//								EPD_Dis_Part(15, 104, txt_picture, 32, 16, POS); //	显示"图片设置"
								EPD_Dis_Part(15, 60, txt_save_exit, 32, 16, POS); // 显示"保存"
								EPD_Dis_Part(60,  32+8, NUM_image_arr[g_TimeSetCache.hour/10], 33, 64, POS);
								EPD_Dis_Part(105, 32+8, NUM_image_arr[g_TimeSetCache.hour%10], 33, 64, POS);
//								EPD_Dis_Part(150, 55+8, gImage_DOT, 8, 8, POS); // 显示冒号
//								EPD_Dis_Part(150, 77+8, gImage_DOT, 8, 8, POS); // 显示冒号
								EPD_Dis_Part(175, 32+8, NUM_image_arr[g_TimeSetCache.minute/10], 33, 64, POS);
								EPD_Dis_Part(220, 32+8, NUM_image_arr[g_TimeSetCache.minute%10], 33, 64, POS);
								// 年月日周
								EPD_Dis_Part( 40,  8, num_image_arr[g_TimeSetCache.year/1000], 12, 16, NEG);
								EPD_Dis_Part( 49,  8, num_image_arr[(g_TimeSetCache.year/100)%10], 12, 16, NEG);
								EPD_Dis_Part( 58,  8, num_image_arr[g_TimeSetCache.year/10], 12, 16, NEG);
								EPD_Dis_Part( 67,  8, num_image_arr[g_TimeSetCache.year%10], 12, 16, NEG);
//								EPD_Dis_Part( 79,  8, gImage_year, 12, 16, POS);
								EPD_Dis_Part( 91,  8, num_image_arr[g_TimeSetCache.month/10], 12, 16, POS);
								EPD_Dis_Part(100,  8, num_image_arr[g_TimeSetCache.month%10], 12, 16, POS);
//								EPD_Dis_Part(112,  8, gImage_month, 12, 16, POS);
								EPD_Dis_Part(124,  8, num_image_arr[g_TimeSetCache.date/10], 12, 16, POS);
								EPD_Dis_Part(133,  8, num_image_arr[g_TimeSetCache.date%10], 12, 16, POS);
//								EPD_Dis_Part(145,  8, gImage_date, 12, 16, POS);
//								EPD_Dis_Part(220, 8, gImage_week_h,			12, 16, POS);
//								EPD_Dis_Part(234, 8, gImage_week_l,			12, 16, POS);
//								EPD_Dis_Part(248, 8, CHN_image_arr[g_TimeSetCache.week], 12, 16, POS);
                                EPD_Part_Update_and_DeepSleep();
                                break;

							case TIME_SET_YEAR:
								g_TimeSetCache.sub_state = TIME_SET_WEEK;
								// 高亮显示"保存"提示
                                EPD_W21_Init();
//							    EPD_Dis_Part(15, 80, txt_time_set, 32, 16, NEG); // 显示"时间设置"
//								EPD_Dis_Part(15, 104, txt_picture, 32, 16, POS); //	显示"图片设置"
								EPD_Dis_Part(15, 60, txt_save_exit, 32, 16, POS); // 显示"保存"
								EPD_Dis_Part(60,  32+8, NUM_image_arr[g_TimeSetCache.hour/10], 33, 64, POS);
								EPD_Dis_Part(105, 32+8, NUM_image_arr[g_TimeSetCache.hour%10], 33, 64, POS);
//								EPD_Dis_Part(150, 55+8, gImage_DOT, 8, 8, POS); // 显示冒号
//								EPD_Dis_Part(150, 77+8, gImage_DOT, 8, 8, POS); // 显示冒号
								EPD_Dis_Part(175, 32+8, NUM_image_arr[g_TimeSetCache.minute/10], 33, 64, POS);
								EPD_Dis_Part(220, 32+8, NUM_image_arr[g_TimeSetCache.minute%10], 33, 64, POS);
								// 年月日周
								EPD_Dis_Part( 40,  8, num_image_arr[g_TimeSetCache.year/1000], 12, 16, POS);
								EPD_Dis_Part( 49,  8, num_image_arr[(g_TimeSetCache.year/100)%10], 12, 16, POS);
								EPD_Dis_Part( 58,  8, num_image_arr[g_TimeSetCache.year/10], 12, 16, POS);
								EPD_Dis_Part( 67,  8, num_image_arr[g_TimeSetCache.year%10], 12, 16, POS);
//								EPD_Dis_Part( 79,  8, gImage_year, 12, 16, POS);
								EPD_Dis_Part( 91,  8, num_image_arr[g_TimeSetCache.month/10], 12, 16, POS);
								EPD_Dis_Part(100,  8, num_image_arr[g_TimeSetCache.month%10], 12, 16, POS);
//								EPD_Dis_Part(112,  8, gImage_month, 12, 16, POS);
//								EPD_Dis_Part(124,  8, num_image_arr[g_TimeSetCache.date/10], 12, 16, POS);
//								EPD_Dis_Part(133,  8, num_image_arr[g_TimeSetCache.date%10], 12, 16, POS);
//								EPD_Dis_Part(145,  8, gImage_date, 12, 16, POS);
//								EPD_Dis_Part(220, 8, gImage_week_h,			12, 16, POS);
//								EPD_Dis_Part(234, 8, gImage_week_l,			12, 16, POS);
								EPD_Dis_Part(248, 8, CHN_image_arr[g_TimeSetCache.week], 12, 16, NEG);
                                EPD_Part_Update_and_DeepSleep();
                                break;

							case TIME_SET_WEEK:
								g_TimeSetCache.sub_state = TIME_SET_SAVE_EXIT;
								// 高亮显示"保存"提示
                                EPD_W21_Init();
//							    EPD_Dis_Part(15, 80, txt_time_set, 32, 16, NEG); // 显示"时间设置"
//								EPD_Dis_Part(15, 104, txt_picture, 32, 16, POS); //	显示"图片设置"
								EPD_Dis_Part(15, 60, txt_save_exit, 32, 16, NEG); // 显示"保存"
								EPD_Dis_Part(60,  32+8, NUM_image_arr[g_TimeSetCache.hour/10], 33, 64, POS);
								EPD_Dis_Part(105, 32+8, NUM_image_arr[g_TimeSetCache.hour%10], 33, 64, POS);
//								EPD_Dis_Part(150, 55+8, gImage_DOT, 8, 8, POS); // 显示冒号
//								EPD_Dis_Part(150, 77+8, gImage_DOT, 8, 8, POS); // 显示冒号
								EPD_Dis_Part(175, 32+8, NUM_image_arr[g_TimeSetCache.minute/10], 33, 64, POS);
								EPD_Dis_Part(220, 32+8, NUM_image_arr[g_TimeSetCache.minute%10], 33, 64, POS);
								// 年月日周
								EPD_Dis_Part( 40,  8, num_image_arr[g_TimeSetCache.year/1000], 12, 16, POS);
								EPD_Dis_Part( 49,  8, num_image_arr[(g_TimeSetCache.year/100)%10], 12, 16, POS);
								EPD_Dis_Part( 58,  8, num_image_arr[g_TimeSetCache.year/10], 12, 16, POS);
								EPD_Dis_Part( 67,  8, num_image_arr[g_TimeSetCache.year%10], 12, 16, POS);
//								EPD_Dis_Part( 79,  8, gImage_year, 12, 16, POS);
//								EPD_Dis_Part( 91,  8, num_image_arr[g_TimeSetCache.month/10], 12, 16, POS);
//								EPD_Dis_Part(100,  8, num_image_arr[g_TimeSetCache.month%10], 12, 16, POS);
//								EPD_Dis_Part(112,  8, gImage_month, 12, 16, POS);
//								EPD_Dis_Part(124,  8, num_image_arr[g_TimeSetCache.date/10], 12, 16, POS);
//								EPD_Dis_Part(133,  8, num_image_arr[g_TimeSetCache.date%10], 12, 16, POS);
//								EPD_Dis_Part(145,  8, gImage_date, 12, 16, POS);
//								EPD_Dis_Part(220, 8, gImage_week_h,			12, 16, POS);
//								EPD_Dis_Part(234, 8, gImage_week_l,			12, 16, POS);
								EPD_Dis_Part(248, 8, CHN_image_arr[g_TimeSetCache.week], 12, 16, POS);
							
                                EPD_Part_Update_and_DeepSleep();
                                break;							
							
                            case TIME_SET_SAVE_EXIT:                            
								g_TimeSetCache.sub_state = TIME_SET_MINUTE;
								// 取消高亮显示"保存"提示
                                // 高亮分钟
                                EPD_W21_Init();
//							    EPD_Dis_Part(15, 80, txt_time_set, 32, 16, NEG); // 显示"时间设置"
//								EPD_Dis_Part(15, 104, txt_picture, 32, 16, POS); //	显示"图片设置"
								EPD_Dis_Part(15, 60, txt_save_exit, 32, 16, POS); // 显示"保存"
								EPD_Dis_Part(60,  32+8, NUM_image_arr[g_TimeSetCache.hour/10], 33, 64, POS);
								EPD_Dis_Part(105, 32+8, NUM_image_arr[g_TimeSetCache.hour%10], 33, 64, POS);
//								EPD_Dis_Part(150, 55+8, gImage_DOT, 8, 8, POS); // 显示冒号
//								EPD_Dis_Part(150, 77+8, gImage_DOT, 8, 8, POS); // 显示冒号
								EPD_Dis_Part(175, 32+8, NUM_image_arr[g_TimeSetCache.minute/10], 33, 64, NEG);
								EPD_Dis_Part(220, 32+8, NUM_image_arr[g_TimeSetCache.minute%10], 33, 64, NEG);
								// 年月日周
//								EPD_Dis_Part( 40,  8, num_image_arr[g_TimeSetCache.year/1000], 12, 16, POS);
//								EPD_Dis_Part( 49,  8, num_image_arr[(g_TimeSetCache.year/100)%10], 12, 16, POS);
//								EPD_Dis_Part( 58,  8, num_image_arr[g_TimeSetCache.year/10], 12, 16, POS);
//								EPD_Dis_Part( 67,  8, num_image_arr[g_TimeSetCache.year%10], 12, 16, POS);
//								EPD_Dis_Part( 79,  8, gImage_year, 12, 16, POS);
//								EPD_Dis_Part( 91,  8, num_image_arr[g_TimeSetCache.month/10], 12, 16, POS);
//								EPD_Dis_Part(100,  8, num_image_arr[g_TimeSetCache.month%10], 12, 16, POS);
//								EPD_Dis_Part(112,  8, gImage_month, 12, 16, POS);
//								EPD_Dis_Part(124,  8, num_image_arr[g_TimeSetCache.date/10], 12, 16, POS);
//								EPD_Dis_Part(133,  8, num_image_arr[g_TimeSetCache.date%10], 12, 16, POS);
//								EPD_Dis_Part(145,  8, gImage_date, 12, 16, POS);
//								EPD_Dis_Part(220, 8, gImage_week_h,			12, 16, POS);
//								EPD_Dis_Part(234, 8, gImage_week_l,			12, 16, POS);
								EPD_Dis_Part(248, 8, CHN_image_arr[g_TimeSetCache.week], 12, 16, POS);
								EPD_Part_Update_and_DeepSleep();
                                break;
                        }
                    break;
                                       
                    // KEY2单击：微调当前项（每次+1，越界循环）
                    case KEY2_CLICK: 
                        switch(g_TimeSetCache.sub_state)
                        {
                            case TIME_SET_HOUR:
                                g_TimeSetCache.hour = (g_TimeSetCache.hour + 1) % 24;
								EPD_W21_Init();
								EPD_Dis_Part(60,  32+8, NUM_image_arr[g_TimeSetCache.hour/10], 33, 64, NEG);
								EPD_Dis_Part(105, 32+8, NUM_image_arr[g_TimeSetCache.hour%10], 33, 64, NEG);
//								EPD_Part_Update_and_DeepSleep();
                                break;
                            
                            case TIME_SET_MINUTE:
                                g_TimeSetCache.minute = (g_TimeSetCache.minute + 1) % 60;
								EPD_W21_Init();
								EPD_Dis_Part(175, 32+8, NUM_image_arr[g_TimeSetCache.minute/10], 33, 64, NEG);
								EPD_Dis_Part(220, 32+8, NUM_image_arr[g_TimeSetCache.minute%10], 33, 64, NEG);
//								EPD_Part_Update_and_DeepSleep();
                                break;

                            case TIME_SET_DATE:
                                g_TimeSetCache.date = (g_TimeSetCache.date + 1);
								if(g_TimeSetCache.date ==32) g_TimeSetCache.date = 1;
								EPD_W21_Init();
								EPD_Dis_Part(124,  8, num_image_arr[g_TimeSetCache.date/10], 12, 16, NEG);
								EPD_Dis_Part(133,  8, num_image_arr[g_TimeSetCache.date%10], 12, 16, NEG);
//								EPD_Part_Update_and_DeepSleep();
                                break;

                            case TIME_SET_MONTH:
                                g_TimeSetCache.month = (g_TimeSetCache.month + 1) ;
								if(g_TimeSetCache.month ==13) g_TimeSetCache.month = 1;
								EPD_W21_Init();
								EPD_Dis_Part( 91,  8, num_image_arr[g_TimeSetCache.month/10], 12, 16, NEG);
								EPD_Dis_Part(100,  8, num_image_arr[g_TimeSetCache.month%10], 12, 16, NEG);
//							EPD_Part_Update_and_DeepSleep();
                                break;

                            case TIME_SET_YEAR:
                                g_TimeSetCache.year = (g_TimeSetCache.year + 1) ;
								EPD_W21_Init();
								EPD_Dis_Part( 40,  8, num_image_arr[g_TimeSetCache.year/1000], 12, 16, NEG);
								EPD_Dis_Part( 49,  8, num_image_arr[(g_TimeSetCache.year/100)%10], 12, 16, NEG);
								EPD_Dis_Part( 58,  8, num_image_arr[g_TimeSetCache.year/10], 12, 16, NEG);
								EPD_Dis_Part( 67,  8, num_image_arr[g_TimeSetCache.year%10], 12, 16, NEG);
//								EPD_Part_Update_and_DeepSleep();
                                break;						

                            case TIME_SET_WEEK:
                                g_TimeSetCache.week = (g_TimeSetCache.week + 1) ;
								if(g_TimeSetCache.week == 8) g_TimeSetCache.week = 1;
								EPD_W21_Init();
								EPD_Dis_Part(248, 8, CHN_image_arr[g_TimeSetCache.week], 12, 16, NEG);
//								EPD_Part_Update_and_DeepSleep();
                                break;								
							
                            case TIME_SET_SAVE_EXIT:
								// 退出时间设置，回到主菜单
                                xCurrentMenuState = MENU_STATE_EXIT;
							    // 保存时间到DS1302
                                taskENTER_CRITICAL();
                                Time[MINUTE] = g_TimeSetCache.minute;
                                Time[HOUR] = g_TimeSetCache.hour;
                                Time[DATE] = g_TimeSetCache.date;
                                Time[MONTH] = g_TimeSetCache.month;
                                Time[YEAR] = g_TimeSetCache.year;
                                Time[WEEK] = g_TimeSetCache.week;
                                Time[SECOND] = 0; // 秒归0
                                DS1302_SetTime(); // 写入DS1302
                                taskEXIT_CRITICAL();
							
                                EPD_W21_Init();
                                EPD_Dis_Part(15, 60, txt_save_ok, 32, 16, POS); // 显示"成功"
								EPD_Dis_Part(60,  32+8, NUM_image_arr[g_TimeSetCache.hour/10], 33, 64, OFF);
								EPD_Dis_Part(105, 32+8, NUM_image_arr[g_TimeSetCache.hour%10], 33, 64, OFF);
		                        EPD_Dis_Part(150, 55+8, gImage_DOT, 8, 8, OFF); // 显示冒号
		                        EPD_Dis_Part(150, 77+8, gImage_DOT, 8, 8, OFF); // 显示冒号
								EPD_Dis_Part(175, 32+8, NUM_image_arr[g_TimeSetCache.minute/10], 33, 64, OFF);
								EPD_Dis_Part(220, 32+8, NUM_image_arr[g_TimeSetCache.minute%10], 33, 64, OFF);
								// 年月日周
								EPD_Dis_Part( 40,  8, num_image_arr[g_TimeSetCache.year/1000], 12, 16, OFF);
								EPD_Dis_Part( 49,  8, num_image_arr[(g_TimeSetCache.year/100)%10], 12, 16, OFF);
								EPD_Dis_Part( 58,  8, num_image_arr[g_TimeSetCache.year/10], 12, 16, OFF);
								EPD_Dis_Part( 67,  8, num_image_arr[g_TimeSetCache.year%10], 12, 16, OFF);
								EPD_Dis_Part( 79,  8, gImage_year, 12, 16, OFF);
								EPD_Dis_Part( 91,  8, num_image_arr[g_TimeSetCache.month/10], 12, 16, OFF);
								EPD_Dis_Part(100,  8, num_image_arr[g_TimeSetCache.month%10], 12, 16, OFF);
								EPD_Dis_Part(112,  8, gImage_month, 12, 16, OFF);
								EPD_Dis_Part(124,  8, num_image_arr[g_TimeSetCache.date/10], 12, 16, OFF);
								EPD_Dis_Part(133,  8, num_image_arr[g_TimeSetCache.date%10], 12, 16, OFF);
								EPD_Dis_Part(145,  8, gImage_date, 12, 16, OFF);
								EPD_Dis_Part(220, 8, gImage_week_h,			12, 16, OFF);
								EPD_Dis_Part(234, 8, gImage_week_l,			12, 16, OFF);
								EPD_Dis_Part(248, 8, CHN_image_arr[g_TimeSetCache.week], 12, 16, OFF);
//								EPD_Part_Update_and_DeepSleep();

                                break;				

                            default:
                                break;
                        }
                        // 刷新显示调整后的值
//						EPD_W21_Init();
						EPD_Dis_Part(15, 60, txt_save_exit, 32, 16, POS); // 显示"保存"
						EPD_Part_Update_and_DeepSleep();
                        break;
						
						
						
                    // KEY2双击：快速调整当前项（每次-1，越界循环）
                    case KEY2_DOUBLE: 
                        switch(g_TimeSetCache.sub_state)
                        {
                            case TIME_SET_HOUR:
                                g_TimeSetCache.hour = (g_TimeSetCache.hour - 1) % 24;
								EPD_W21_Init();
								EPD_Dis_Part(60,  32+8, NUM_image_arr[g_TimeSetCache.hour/10], 33, 64, NEG);
								EPD_Dis_Part(105, 32+8, NUM_image_arr[g_TimeSetCache.hour%10], 33, 64, NEG);
								EPD_Part_Update_and_DeepSleep();
                                break;
                            
                            case TIME_SET_MINUTE:
                                g_TimeSetCache.minute = (g_TimeSetCache.minute - 1) % 6;
								EPD_W21_Init();
								EPD_Dis_Part(175, 32+8, NUM_image_arr[g_TimeSetCache.minute/10], 33, 64, NEG);
								EPD_Dis_Part(220, 32+8, NUM_image_arr[g_TimeSetCache.minute%10], 33, 64, NEG);
								EPD_Part_Update_and_DeepSleep();
                                break;

                            case TIME_SET_DATE:
                                g_TimeSetCache.date = (g_TimeSetCache.date - 1);
								if(g_TimeSetCache.date ==0) g_TimeSetCache.date =31;
								EPD_W21_Init();
								EPD_Dis_Part(124,  8, num_image_arr[g_TimeSetCache.date/10], 12, 16, NEG);
								EPD_Dis_Part(133,  8, num_image_arr[g_TimeSetCache.date%10], 12, 16, NEG);
								EPD_Part_Update_and_DeepSleep();
                                break;

                            case TIME_SET_MONTH:
                                g_TimeSetCache.month = (g_TimeSetCache.month - 1);
								if(g_TimeSetCache.month ==0) g_TimeSetCache.month = 12;
								EPD_W21_Init();
								EPD_Dis_Part( 91,  8, num_image_arr[g_TimeSetCache.month/10], 12, 16, NEG);
								EPD_Dis_Part(100,  8, num_image_arr[g_TimeSetCache.month%10], 12, 16, NEG);
								EPD_Part_Update_and_DeepSleep();
                                break;

                            case TIME_SET_YEAR:
                                g_TimeSetCache.year = (g_TimeSetCache.year - 1) ;
								EPD_W21_Init();
								EPD_Dis_Part( 40,  8, num_image_arr[g_TimeSetCache.year/1000], 12, 16, NEG);
								EPD_Dis_Part( 49,  8, num_image_arr[(g_TimeSetCache.year/100)%10], 12, 16, NEG);
								EPD_Dis_Part( 58,  8, num_image_arr[g_TimeSetCache.year/10], 12, 16, NEG);
								EPD_Dis_Part( 67,  8, num_image_arr[g_TimeSetCache.year%10], 12, 16, NEG);
								EPD_Part_Update_and_DeepSleep();
                                break;						

                            case TIME_SET_WEEK:
                                g_TimeSetCache.week = (g_TimeSetCache.week - 1);
								if(g_TimeSetCache.week ==0) g_TimeSetCache.week = 7;
								EPD_W21_Init();
								EPD_Dis_Part(248, 8, CHN_image_arr[g_TimeSetCache.week], 12, 16, NEG);
								EPD_Part_Update_and_DeepSleep();
                                break;								
							
                            case TIME_SET_SAVE_EXIT:
								// 退出时间设置，回到主菜单
                                xCurrentMenuState = MENU_STATE_EXIT;
							    // 保存时间到DS1302
                                taskENTER_CRITICAL();
                                Time[MINUTE] = g_TimeSetCache.minute;
                                Time[HOUR] = g_TimeSetCache.hour;
                                Time[DATE] = g_TimeSetCache.date;
                                Time[MONTH] = g_TimeSetCache.month;
                                Time[YEAR] = g_TimeSetCache.year;
                                Time[WEEK] = g_TimeSetCache.week;
                                Time[SECOND] = 0; // 秒归0
                                DS1302_SetTime(); // 写入DS1302
                                taskEXIT_CRITICAL();
							
                                EPD_W21_Init();
                                EPD_Dis_Part(15, 60, txt_save_ok, 32, 16, POS); // 显示"成功"
								EPD_Dis_Part(60,  32+8, NUM_image_arr[g_TimeSetCache.hour/10], 33, 64, OFF);
								EPD_Dis_Part(105, 32+8, NUM_image_arr[g_TimeSetCache.hour%10], 33, 64, OFF);
		                        EPD_Dis_Part(150, 55+8, gImage_DOT, 8, 8, OFF); // 显示冒号
		                        EPD_Dis_Part(150, 77+8, gImage_DOT, 8, 8, OFF); // 显示冒号
								EPD_Dis_Part(175, 32+8, NUM_image_arr[g_TimeSetCache.minute/10], 33, 64, OFF);
								EPD_Dis_Part(220, 32+8, NUM_image_arr[g_TimeSetCache.minute%10], 33, 64, OFF);
								// 年月日周
								EPD_Dis_Part( 40,  8, num_image_arr[g_TimeSetCache.year/1000], 12, 16, OFF);
								EPD_Dis_Part( 49,  8, num_image_arr[(g_TimeSetCache.year/100)%10], 12, 16, OFF);
								EPD_Dis_Part( 58,  8, num_image_arr[g_TimeSetCache.year/10], 12, 16, OFF);
								EPD_Dis_Part( 67,  8, num_image_arr[g_TimeSetCache.year%10], 12, 16, OFF);
								EPD_Dis_Part( 79,  8, gImage_year, 12, 16, OFF);
								EPD_Dis_Part( 91,  8, num_image_arr[g_TimeSetCache.month/10], 12, 16, OFF);
								EPD_Dis_Part(100,  8, num_image_arr[g_TimeSetCache.month%10], 12, 16, OFF);
								EPD_Dis_Part(112,  8, gImage_month, 12, 16, OFF);
								EPD_Dis_Part(124,  8, num_image_arr[g_TimeSetCache.date/10], 12, 16, OFF);
								EPD_Dis_Part(133,  8, num_image_arr[g_TimeSetCache.date%10], 12, 16, OFF);
								EPD_Dis_Part(145,  8, gImage_date, 12, 16, OFF);
								EPD_Dis_Part(220, 8, gImage_week_h,			12, 16, OFF);
								EPD_Dis_Part(234, 8, gImage_week_l,			12, 16, OFF);
								EPD_Dis_Part(248, 8, CHN_image_arr[g_TimeSetCache.week], 12, 16, OFF);
								EPD_Part_Update_and_DeepSleep();

                                break;				

                            default:
                                break;
                        }
                        // 刷新显示调整后的值
						EPD_W21_Init();
						EPD_Dis_Part(15, 60, txt_save_exit, 32, 16, POS); // 显示"保存"
						EPD_Part_Update_and_DeepSleep();
                        break;
						
                    // KEY1长按：取消设置（不保存，直接退出）
                    case KEY1_LONG: 
                        xCurrentMenuState = MENU_STATE_EXIT;
                        EPD_W21_Init();
                        EPD_Dis_Part(25, 60, txt_cancel, 32, 16, NEG); // 显示"取消设置"
                        EPD_Part_Update_and_DeepSleep();
                        vTaskDelay(1000 / portTICK_PERIOD_MS);
                        break;
                    
                    default:
                        break;
                }
                break;

            // ------------------------ 图片切换子菜单 ------------------------
            case MENU_STATE_PIC_SWITCH:  
                switch(recv_action)
                {
                    case KEY1_CLICK: // 切换图片
                        EPD_W21_Init();
                        EPD_HW_Init_4GRAY();  
                        EPD_WhiteScreen_ALL_4GRAY(gImage_4Gray4); 
                        EPD_Part_Update_and_DeepSleep();
                        break;
                    
                    case KEY2_CLICK: // 返回主菜单
                        xCurrentMenuState = MENU_STATE_MAIN;
                        EPD_W21_Init();
                        EPD_Dis_Part(25, 104, txt_clock, 32, 16, POS);
                        EPD_Dis_Part(85, 104, txt_picture, 32, 16, NEG);
                        EPD_Part_Update_and_DeepSleep();
                        break;
                    
                    default:
                        break;
                }
                break;

            // ------------------------ 退出菜单（回到时钟） ------------------------
            case MENU_STATE_EXIT:         
                xCurrentMenuState = MENU_STATE_MAIN;
                // 触发时钟任务刷新界面
//                vTaskNotifyGive(Clock_Task_Handle); // 通知时钟任务立即刷新
                break;	
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

	xReturn = xTaskCreate((TaskFunction_t)Menu_Task,       
						  (const char* )"Menu_Task",       
						  (uint16_t )MENU_TASK_STACK,                 
						  (void* )NULL, 
						  (UBaseType_t )MENU_TASK_PRIO, 
						  (TaskHandle_t* )&Menu_Task_Handle);	
						  
    taskEXIT_CRITICAL();
								
	vTaskDelete( App_Create_Task_Handle);
}



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
//	//4灰阶刷新全图
//	EPD_HW_Init_4GRAY(); 										//EPD init 4Gray
//	EPD_WhiteScreen_ALL_4GRAY(gImage_4Gray4); 	
//    driver_delay_xms(2000);				
	
//	//黑白刷新全图
//	EPD_HW_Init();
//	EPD_WhiteScreen_ALL(gImage_hust);
//	driver_delay_xms(2000);

	//加载背景图
	EPD_HW_Init(); 
	EPD_SetRAMValue_BaseMap(gImage_base);  
	driver_delay_xms(500);

//	//    y    x   图像  宽度  高度    正反显
//	EPD_W21_Init();											//hard reset
//	EPD_Dis_Part(25,104,txt_clock,32,16,POS);		      
//	EPD_Dis_Part(85,104,txt_picture,32,16,POS);		     
//	EPD_Part_Update_and_DeepSleep();				
//	driver_delay_xms(500);				


	xKeyEventQueue = xQueueCreate( 5, 1);
	xQueueReset( xKeyEventQueue);
	xMenuStateQueue = xQueueCreate( 5, 1);
	xQueueReset( xMenuStateQueue);
	
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
