/**
  ******************************************************************************
  * @file    main.c
  * @author  iosalt
  * @version V1.0
  * @date    2025-11-29
  * @brief   电子纸时钟 - 状态机重构+位置对齐+保存逻辑优化
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
#include <stdbool.h>

// 任务优先级
#define APP_CREATE_TASK_PRIO    (tskIDLE_PRIORITY + 1)  
#define CLOCK_TASK_PRIO         (tskIDLE_PRIORITY + 3)  
#define LED_TASK_PRIO           (tskIDLE_PRIORITY + 3)  
#define KEY_TASK_PRIO           (tskIDLE_PRIORITY + 4)  
#define MENU_TASK_PRIO          (tskIDLE_PRIORITY + 3) 

// 任务栈大小
#define APP_CREATE_TASK_STACK   128
#define CLOCK_TASK_STACK        256
#define LED_TASK_STACK          128
#define KEY_TASK_STACK          128
#define MENU_TASK_STACK         256  // 增大栈空间适配逻辑

// 任务句柄
static TaskHandle_t App_Create_Task_Handle = NULL;
static TaskHandle_t Clock_Task_Handle = NULL;
static TaskHandle_t Led_Task_Handle = NULL;
static TaskHandle_t Key_Task_Handle = NULL;
static TaskHandle_t Menu_Task_Handle = NULL;

// 消息队列
QueueHandle_t xKeyEventQueue;
QueueHandle_t xMenuStateQueue;
// 互斥锁：保证多任务访问isClock的线程安全
SemaphoreHandle_t xIsClockMutex;  

// 中断优先级标记
BaseType_t xHigherPriorityTaskWoken = pdFALSE;

uint8_t menu_main_id = 0;  // 主菜单选中项索引（0=时间设置，1=图片切换，2=启动页面设置）
uint8_t picture_id = 0;  // 图片选中项索引
bool isClock = true;

// 数字/中文图像数组
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

const unsigned char *Gray_image_arr[] = {
	gImage_4Gray4,gImage_4Gray3,gImage_4Gray2,gImage_4Gray1
};

// 菜单主状态枚举
typedef enum {
    MENU_STATE_MAIN,        // 主菜单（时间设置/图片切换）
    MENU_STATE_TIME_SET,    // 时间设置总入口
    MENU_STATE_PIC_SWITCH,  // 图片切换子菜单
	MENU_STATE_BOOT_PAGE_SET,  //设置
    MENU_STATE_EXIT         // 退出菜单（回到时钟）
} MenuState_t;

// 按键事件枚举
typedef enum {
    KEY_NONE = 0,
    KEY1_PRESS,
    KEY2_PRESS
} KeyEvent_t;

// 按键行为枚举
typedef enum {
    KEY_ACTION_NONE = 0,
    KEY1_CLICK,    // 切换设置项/循环
    KEY1_LONG,     // 取消/退出
    KEY1_DOUBLE,   // 预留
    KEY2_CLICK,    // 当前项+1/确认保存
    KEY2_LONG,     // 预留
    KEY2_DOUBLE    // 当前项-1
} KeyAction_t;

// 时间设置子状态枚举
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

// 时间设置缓存结构体
typedef struct {
    uint8_t minute;         // 缓存分钟 (0-59)
    uint8_t hour;           // 缓存小时 (0-23)
    uint8_t date;           // 缓存日期 (1-31)
    uint8_t month;          // 缓存月份 (1-12)
    uint8_t year;           // 缓存年份 (0-99)
    int8_t week;           	// 缓存星期 (0-6)
    TimeSetSubState_t sub_state; // 当前调整项
} TimeSetCache_t;

TimeSetCache_t g_TimeSetCache; // 全局时间缓存

// 按键配置参数
#define KEY_LONG_TIME     800 / portTICK_PERIOD_MS  // 长按判定(800ms)
#define KEY_DOUBLE_TIME   300 / portTICK_PERIOD_MS  // 双击间隔(300ms)
#define KEY_DEBOUNCE_TIME 20 / portTICK_PERIOD_MS   // 消抖时间(20ms)

// 显示位置宏定义
#define DISP_YEAR_X       40      // 年份显示X坐标
#define DISP_YEAR_Y       8       // 年份显示Y坐标
#define DISP_MONTH_X      91      // 月份显示X坐标
#define DISP_MONTH_Y      8       // 月份显示Y坐标
#define DISP_DATE_X       124     // 日期显示X坐标
#define DISP_DATE_Y       8       // 日期显示Y坐标
#define DISP_WEEK_X       210     // 星期显示X坐标
#define DISP_WEEK_Y       8       // 星期显示Y坐标
#define DISP_HOUR_X       44      // 小时显示X坐标
#define DISP_HOUR_Y       40      // 小时显示Y坐标
#define DISP_DOT_X        142     // 冒号显示X坐标
#define DISP_DOT_Y1       63      // 冒号上Y坐标
#define DISP_DOT_Y2       85      // 冒号下Y坐标
#define DISP_MINUTE_X     169     // 分钟显示X坐标
#define DISP_MINUTE_Y     40      // 分钟显示Y坐标
#define DISP_MENU_TITLE_X 15      // 菜单标题X坐标

#define DISP_MENU_X1      220      //保存 X坐标
#define DISP_MENU_X2      60    	// 时间 X坐标
#define DISP_MENU_X3      120     // 壁纸 X坐标
#define DISP_MENU_X4  	  180      // 设置 X坐标

#define DISP_MENU_Y1      112     // 保存 Y坐标
#define DISP_MENU_Y2      112     // 时间 Y坐标
#define DISP_MENU_Y3      112     // 壁纸 Y坐标
#define DISP_MENU_Y4      112     // 设置 Y坐标

// 按键中断服务函数
void KEY_IRQHandler(void)
{
    KeyEvent_t key_event = KEY_NONE;
    
    // KEY1 PB14(EXTI_Line14)
    if(EXTI_GetITStatus(EXTI_Line14) != RESET)
    {
        if(KEY1_STATE == 0){ 
            key_event = KEY1_PRESS;
            xQueueSendToBackFromISR(xKeyEventQueue, &key_event, &xHigherPriorityTaskWoken);
        }
        EXTI_ClearITPendingBit(EXTI_Line14);
        portYIELD_FROM_ISR(xHigherPriorityTaskWoken); 
    }

    // KEY2 PB15(EXTI_Line15)
    if(EXTI_GetITStatus(EXTI_Line15) != RESET)
    {
        if(KEY2_STATE == 0){ 
            key_event = KEY2_PRESS;
            xQueueSendToBackFromISR(xKeyEventQueue, &key_event, &xHigherPriorityTaskWoken);
        }
        EXTI_ClearITPendingBit(EXTI_Line15);
        portYIELD_FROM_ISR(xHigherPriorityTaskWoken); 
    }
}

// 时钟任务（修改：使用统一宏定义，显示位置与设置任务一致）
static void Clock_Task(void *arg)
{
    int minute, hour, date, month, year, week = 0;
    TickType_t xLastWakeTime;
    const TickType_t xPeriod = 60000 / portTICK_PERIOD_MS;  
    xLastWakeTime = xTaskGetTickCount();
    
    // 初始化读取当前时间
    taskENTER_CRITICAL();
    DS1302_GetTime();
    taskEXIT_CRITICAL();
    
    g_TimeSetCache.hour = Time[HOUR];
    g_TimeSetCache.minute = Time[MINUTE];
    
    vTaskDelay(1000 / portTICK_PERIOD_MS);
    
    while(1)
    {// 获取互斥锁，判断是否需要刷新时钟
        xSemaphoreTake(xIsClockMutex, portMAX_DELAY);
        bool needRefresh = isClock;  // 临时变量缓存标志位
        xSemaphoreGive(xIsClockMutex);
        if (needRefresh) {  // 仅当isClock=true时执行刷新
			// 读取DS1302时间
			taskENTER_CRITICAL();
			DS1302_GetTime();
			taskEXIT_CRITICAL();

			year    = Time[YEAR];
			month   = Time[MONTH];
			date    = Time[DATE];
			hour    = Time[HOUR];
			minute  = Time[MINUTE];
			week    = Time[WEEK];    
			
			// 刷新电子纸显示（使用统一位置宏定义）
			EPD_W21_Init();
			// 年份显示
			EPD_Dis_Part(DISP_YEAR_X,     DISP_YEAR_Y, num_image_arr[2], 		    12, 16, POS);
			EPD_Dis_Part(DISP_YEAR_X+9,   DISP_YEAR_Y, num_image_arr[0],			12, 16, POS);
			EPD_Dis_Part(DISP_YEAR_X+18,  DISP_YEAR_Y, num_image_arr[year/10],      12, 16, POS);
			EPD_Dis_Part(DISP_YEAR_X+27,  DISP_YEAR_Y, num_image_arr[year%10],      12, 16, POS);
			EPD_Dis_Part(DISP_YEAR_X+39,  DISP_YEAR_Y, gImage_year,                 12, 16, POS);
			// 月份显示
			EPD_Dis_Part(DISP_MONTH_X,    DISP_MONTH_Y, num_image_arr[month/10],    12, 16, POS);
			EPD_Dis_Part(DISP_MONTH_X+9,  DISP_MONTH_Y, num_image_arr[month%10],    12, 16, POS);
			EPD_Dis_Part(DISP_MONTH_X+21, DISP_MONTH_Y, gImage_month,               12, 16, POS);
			// 日期显示
			EPD_Dis_Part(DISP_DATE_X,     DISP_DATE_Y, num_image_arr[date/10],      12, 16, POS);
			EPD_Dis_Part(DISP_DATE_X+9,   DISP_DATE_Y, num_image_arr[date%10],      12, 16, POS);
			EPD_Dis_Part(DISP_DATE_X+21,  DISP_DATE_Y, gImage_date,                 12, 16, POS);
			// 星期显示
			EPD_Dis_Part(DISP_WEEK_X,     DISP_WEEK_Y, gImage_week_h,               12, 16, POS);
			EPD_Dis_Part(DISP_WEEK_X+14,  DISP_WEEK_Y, gImage_week_l,               12, 16, POS);
			EPD_Dis_Part(DISP_WEEK_X+28,  DISP_WEEK_Y, CHN_image_arr[week],         12, 16, POS);
			// 小时显示
			EPD_Dis_Part(DISP_HOUR_X,     DISP_HOUR_Y, NUM_image_arr[hour/10],      33, 64, POS);
			EPD_Dis_Part(DISP_HOUR_X+46,  DISP_HOUR_Y, NUM_image_arr[hour%10],      33, 64, POS);
			// 冒号显示
			EPD_Dis_Part(DISP_DOT_X,      DISP_DOT_Y1, gImage_DOT,                   8,  8, POS);
			EPD_Dis_Part(DISP_DOT_X,      DISP_DOT_Y2, gImage_DOT,                   8,  8, POS);
			// 分钟显示
			EPD_Dis_Part(DISP_MINUTE_X,   DISP_MINUTE_Y, NUM_image_arr[minute/10],  33, 64, POS);
			EPD_Dis_Part(DISP_MINUTE_X+46,DISP_MINUTE_Y, NUM_image_arr[minute%10],  33, 64, POS);
			
			EPD_Part_Update_and_DeepSleep();
		}
        vTaskDelayUntil(&xLastWakeTime, xPeriod);
    }
}

// LED心跳任务
static void Led_Task(void *arg)
{
    while(1)
    {
//        LED1_TOGGLE;
        vTaskDelay(1000 / portTICK_PERIOD_MS);
    }   
}

// 按键解析任务
void Key_Task(void *pvParameters)
{
    KeyEvent_t recv_event;
    KeyAction_t key_action = KEY_ACTION_NONE;
    TickType_t key_press_time;

    while(1)
    {
        if(xQueueReceive(xKeyEventQueue, &recv_event, portMAX_DELAY) == pdTRUE)
        {
            vTaskDelay(KEY_DEBOUNCE_TIME); // 消抖
            switch(recv_event)
            {
                case KEY1_PRESS:
                    if(KEY1_STATE == 0) 
                    {
                        key_press_time = xTaskGetTickCount();
                        // 检测长按
                        while(KEY1_STATE == 0)
                        {
                            if((xTaskGetTickCount() - key_press_time) >= KEY_LONG_TIME)
                            {
                                key_action = KEY1_LONG;
                                goto KEY_ACTION_SEND;
                            }
                            vTaskDelay(10 / portTICK_PERIOD_MS);
                        }

                        // 检测双击
                        if(xQueuePeek(xKeyEventQueue, &recv_event, KEY_DOUBLE_TIME) == pdTRUE)
                        {
                            vTaskDelay(KEY_DEBOUNCE_TIME);
                            if(recv_event == KEY1_PRESS && KEY1_STATE == 0)
                            {
                                key_action = KEY1_DOUBLE;
                                xQueueReceive(xKeyEventQueue, &recv_event, 0);
                            }
                            else
                            {
                                key_action = KEY1_CLICK;
                            }
                        }
                        else
                        {
                            key_action = KEY1_CLICK;
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
            if(key_action != KEY_ACTION_NONE)
            {
                xQueueSendToBack(xMenuStateQueue, &key_action, 0);
                key_action = KEY_ACTION_NONE;
            }
        }
    }
}

// 刷新当前设置项显示
static void RefreshTimeSetDisplay(void)
{
    EPD_W21_Init();
    
    // 固定显示：时间设置标题
    EPD_Dis_Part(DISP_MENU_X2, DISP_MENU_Y2, txt_time_set,  32, 16, POS);
    EPD_Dis_Part(DISP_MENU_X3, DISP_MENU_Y3, txt_wallpaper,  32, 16, POS);
    EPD_Dis_Part(DISP_MENU_X1, DISP_MENU_Y1, txt_save_exit,32, 16, (g_TimeSetCache.sub_state == TIME_SET_SAVE_EXIT) ? NEG : POS);

    // 年份显示（根据子状态高亮）
    EPD_Dis_Part(DISP_YEAR_X,     DISP_YEAR_Y, num_image_arr[2],    12, 16, 
                 (g_TimeSetCache.sub_state == TIME_SET_YEAR) ? NEG : POS);
    EPD_Dis_Part(DISP_YEAR_X+9,   DISP_YEAR_Y, num_image_arr[0],12, 16, 
                 (g_TimeSetCache.sub_state == TIME_SET_YEAR) ? NEG : POS);
    EPD_Dis_Part(DISP_YEAR_X+18,  DISP_YEAR_Y, num_image_arr[g_TimeSetCache.year/10],      12, 16, 
                 (g_TimeSetCache.sub_state == TIME_SET_YEAR) ? NEG : POS);
    EPD_Dis_Part(DISP_YEAR_X+27,  DISP_YEAR_Y, num_image_arr[g_TimeSetCache.year%10],      12, 16, 
                 (g_TimeSetCache.sub_state == TIME_SET_YEAR) ? NEG : POS);
    EPD_Dis_Part(DISP_YEAR_X+39,  DISP_YEAR_Y, gImage_year,                 12, 16, POS);
    
    // 月份显示（根据子状态高亮）
    EPD_Dis_Part(DISP_MONTH_X,    DISP_MONTH_Y, num_image_arr[g_TimeSetCache.month/10],     12, 16, 
                 (g_TimeSetCache.sub_state == TIME_SET_MONTH) ? NEG : POS);
    EPD_Dis_Part(DISP_MONTH_X+9,  DISP_MONTH_Y, num_image_arr[g_TimeSetCache.month%10],     12, 16, 
                 (g_TimeSetCache.sub_state == TIME_SET_MONTH) ? NEG : POS);
    EPD_Dis_Part(DISP_MONTH_X+21, DISP_MONTH_Y, gImage_month,                12, 16, POS);
    
    // 日期显示（根据子状态高亮）
    EPD_Dis_Part(DISP_DATE_X,     DISP_DATE_Y, num_image_arr[g_TimeSetCache.date/10],      12, 16, 
                 (g_TimeSetCache.sub_state == TIME_SET_DATE) ? NEG : POS);
    EPD_Dis_Part(DISP_DATE_X+9,   DISP_DATE_Y, num_image_arr[g_TimeSetCache.date%10],      12, 16, 
                 (g_TimeSetCache.sub_state == TIME_SET_DATE) ? NEG : POS);
    EPD_Dis_Part(DISP_DATE_X+21,  DISP_DATE_Y, gImage_date,                 12, 16, POS);
    
    // 星期显示（根据子状态高亮）
    EPD_Dis_Part(DISP_WEEK_X,     DISP_WEEK_Y, gImage_week_h,                12, 16, POS);
    EPD_Dis_Part(DISP_WEEK_X+14,  DISP_WEEK_Y, gImage_week_l,                12, 16, POS);
    EPD_Dis_Part(DISP_WEEK_X+28,  DISP_WEEK_Y, CHN_image_arr[g_TimeSetCache.week],          12, 16, 
                 (g_TimeSetCache.sub_state == TIME_SET_WEEK) ? NEG : POS);

    // 小时显示（根据子状态高亮）
    EPD_Dis_Part(DISP_HOUR_X,     DISP_HOUR_Y, NUM_image_arr[g_TimeSetCache.hour/10],      33, 64, 
                 (g_TimeSetCache.sub_state == TIME_SET_HOUR) ? NEG : POS);
    EPD_Dis_Part(DISP_HOUR_X+45,  DISP_HOUR_Y, NUM_image_arr[g_TimeSetCache.hour%10],      33, 64, 
                 (g_TimeSetCache.sub_state == TIME_SET_HOUR) ? NEG : POS);
    
    // 冒号显示
    EPD_Dis_Part(DISP_DOT_X,      DISP_DOT_Y1, gImage_DOT,                   8,  8, POS);
    EPD_Dis_Part(DISP_DOT_X,      DISP_DOT_Y2, gImage_DOT,                   8,  8, POS);
    
    // 分钟显示（根据子状态高亮）
    EPD_Dis_Part(DISP_MINUTE_X,   DISP_MINUTE_Y, NUM_image_arr[g_TimeSetCache.minute/10],  33, 64, 
                 (g_TimeSetCache.sub_state == TIME_SET_MINUTE) ? NEG : POS);
    EPD_Dis_Part(DISP_MINUTE_X+46,DISP_MINUTE_Y, NUM_image_arr[g_TimeSetCache.minute%10],  33, 64, 
                 (g_TimeSetCache.sub_state == TIME_SET_MINUTE) ? NEG : POS);

    EPD_Part_Update_and_DeepSleep();
}

// 重构后的菜单任务
static void Menu_Task(void *arg)
{
    KeyAction_t recv_action;
    MenuState_t xCurrentMenuState = MENU_STATE_MAIN;

    // 初始化时间缓存
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

    // 初始化显示主菜单
    EPD_W21_Init();
    EPD_Dis_Part(DISP_MENU_X2, DISP_MENU_Y2, txt_time_set, 32, 16, POS);
    EPD_Dis_Part(DISP_MENU_X3, DISP_MENU_Y3, txt_wallpaper, 32, 16, POS);
    EPD_Part_Update_and_DeepSleep();

    while(1)
    {
        xQueueReceive(xMenuStateQueue, &recv_action, portMAX_DELAY);

        // 第一步：处理菜单主状态
        switch(xCurrentMenuState)
        {
            // 主菜单
			case MENU_STATE_MAIN:
				switch(recv_action)
				{
					// KEY1单击：循环切换选中项
					case KEY1_CLICK:    
						menu_main_id = (menu_main_id + 1) % 3; 
						EPD_W21_Init();
						EPD_Dis_Part(DISP_MENU_X2, DISP_MENU_Y2, txt_time_set, 32, 16,  (menu_main_id == 0) ? NEG : POS);
						EPD_Dis_Part(DISP_MENU_X3, DISP_MENU_Y3, txt_wallpaper,  32, 16,  (menu_main_id == 1) ? NEG : POS);
						EPD_Dis_Part(DISP_MENU_X4, DISP_MENU_Y4, txt_boot_set, 32, 16,  (menu_main_id == 2) ? NEG : POS);
						EPD_Part_Update_and_DeepSleep();
						break;

					case KEY2_CLICK:      
						// KEY2单击：根据当前选中项，进入对应子菜单
						switch(menu_main_id)
						{
							case 0:  // 选中“时间设置”
								
								xCurrentMenuState = MENU_STATE_TIME_SET;
								RefreshTimeSetDisplay(); // 显示时间设置界面
								break;
							case 1:  // 选中“图片切换”
								xCurrentMenuState = MENU_STATE_PIC_SWITCH;
								EPD_W21_Init();
								EPD_Dis_Part(DISP_MENU_X3, DISP_MENU_Y3, txt_wallpaper, 32, 16, NEG);
								EPD_Part_Update_and_DeepSleep();
								break;
							case 2:  // 选中“设置页面”
								xCurrentMenuState = MENU_STATE_BOOT_PAGE_SET;
								EPD_W21_Init();
								EPD_Dis_Part(DISP_MENU_X4, DISP_MENU_Y4, txt_boot_set, 32, 16, NEG);
								EPD_Part_Update_and_DeepSleep();
								break;
							default:
								break;
						}
						break;

					case KEY1_LONG:        // 退出菜单（回到时钟）
						xCurrentMenuState = MENU_STATE_EXIT;
						// 退出时重置选中项为默认（0=时间设置）
						menu_main_id = 0;
						break;

					default:
						break;
				}
				break;

            // 图片切换子菜单
            case MENU_STATE_PIC_SWITCH:
				// 切换壁纸时，关闭时钟刷新
				xSemaphoreTake(xIsClockMutex, portMAX_DELAY);
				isClock = false;
				xSemaphoreGive(xIsClockMutex);
                switch(recv_action)
                {
                    case KEY2_CLICK:       // 切换图片
						LED1_TOGGLE;
						picture_id = (picture_id + 1) % 2; 
//                        EPD_W21_Init();
                        EPD_HW_Init_4GRAY();  
                        EPD_WhiteScreen_ALL_4GRAY(Gray_image_arr[picture_id]); 
                        break;
                    case KEY1_CLICK:       // 返回主菜单
						LED1_TOGGLE;
						//恢复isClock=true，让时钟任务重启
						xSemaphoreTake(xIsClockMutex, portMAX_DELAY);
						isClock = true;
						xSemaphoreGive(xIsClockMutex);
                        xCurrentMenuState = MENU_STATE_MAIN;
					    // 加载背景图
						EPD_HW_Init(); 
						EPD_SetRAMValue_BaseMap(gImage_base);  
						driver_delay_xms(100);
                        EPD_W21_Init();
                        EPD_Dis_Part(DISP_MENU_X2, DISP_MENU_Y2, txt_time_set, 32, 16, POS);
                        EPD_Dis_Part(DISP_MENU_X3, DISP_MENU_Y3, txt_wallpaper, 32, 16, POS);
                        EPD_Part_Update_and_DeepSleep();
                        break;
                    default:
                        break;
                }
                break;

            // 时间设置子菜单（核心：按设置子状态处理按键）
            case MENU_STATE_TIME_SET:
//				EPD_W21_Init();
//				EPD_Dis_Part(DISP_MENU_X2, DISP_MENU_Y2, txt_time_set,  32, 16, OFF);
//				EPD_Dis_Part(DISP_MENU_X3, DISP_MENU_Y3, txt_wallpaper,   32, 16, OFF);
//				EPD_Dis_Part(DISP_MENU_X1, DISP_MENU_Y1, txt_save_exit, 32, 16, POS);
//				EPD_Part_Update_and_DeepSleep();
                // 第二步：按当前设置子状态分层处理按键
                switch(g_TimeSetCache.sub_state)
                {
                    // --------------------- 调整分钟状态 ---------------------
                    case TIME_SET_MINUTE:
                        switch(recv_action)
                        {	
                            case KEY1_CLICK:       // 切换到调整小时
                                g_TimeSetCache.sub_state = TIME_SET_HOUR;
                                RefreshTimeSetDisplay();
                                break;
                            case KEY2_CLICK:       // 分钟+1（循环）
                                g_TimeSetCache.minute = (g_TimeSetCache.minute + 1) % 60;
                                RefreshTimeSetDisplay();
                                break;
                            case KEY2_DOUBLE:      // 分钟-1（循环）
                                g_TimeSetCache.minute = (g_TimeSetCache.minute == 0) ? 59 : (g_TimeSetCache.minute - 1);
                                RefreshTimeSetDisplay();
                                break;
                            case KEY1_LONG:        // 取消设置，返回主菜单
                                xCurrentMenuState = MENU_STATE_MAIN;
                                EPD_W21_Init();
                                EPD_Dis_Part(DISP_MENU_X2, DISP_MENU_Y2, txt_time_set, 32, 16, POS);
                                EPD_Dis_Part(DISP_MENU_X3, DISP_MENU_Y3, txt_wallpaper, 32, 16, POS);
                                EPD_Part_Update_and_DeepSleep();
                                break;
                            default:
                                break;
                        }
                        break;

                    // --------------------- 调整小时状态 ---------------------
                    case TIME_SET_HOUR:
                        switch(recv_action)
                        {
                            case KEY1_CLICK:       // 切换到调整日期
                                g_TimeSetCache.sub_state =TIME_SET_WEEK;
                                RefreshTimeSetDisplay();
                                break;
                            case KEY2_CLICK:       // 小时+1（循环）
                                g_TimeSetCache.hour = (g_TimeSetCache.hour + 1) % 24;
                                RefreshTimeSetDisplay();
                                break;
                            case KEY2_DOUBLE:      // 小时-1（循环）
                                g_TimeSetCache.hour = (g_TimeSetCache.hour == 0) ? 23 : (g_TimeSetCache.hour - 1);
                                RefreshTimeSetDisplay();
                                break;
                            case KEY1_LONG:        // 取消设置，返回主菜单
                                xCurrentMenuState = MENU_STATE_MAIN;
                                EPD_W21_Init();
                                EPD_Dis_Part(DISP_MENU_X2, DISP_MENU_Y2, txt_time_set, 32, 16, POS);
                                EPD_Dis_Part(DISP_MENU_X3, DISP_MENU_Y3, txt_wallpaper, 32, 16, POS);
                                EPD_Part_Update_and_DeepSleep();
                                break;
                            default:
                                break;
                        }
                        break;

					// --------------------- 调整星期状态 ---------------------
                    case TIME_SET_WEEK:
                        switch(recv_action)
                        {
                            case KEY1_CLICK:       // 切换到保存退出
                                g_TimeSetCache.sub_state =  TIME_SET_DATE;
                                RefreshTimeSetDisplay();
                                break;
                            case KEY2_CLICK:       // 星期+1（循环）
                                g_TimeSetCache.week++;
                                if(g_TimeSetCache.week > 6) g_TimeSetCache.week = 0;
                                RefreshTimeSetDisplay();
                                break;
                            case KEY2_DOUBLE:      // 星期-1（循环）
                                g_TimeSetCache.week--;
                                if(g_TimeSetCache.week < 0) g_TimeSetCache.week = 6;
                                RefreshTimeSetDisplay();
                                break;
                            case KEY1_LONG:        // 取消设置，返回主菜单
                                xCurrentMenuState = MENU_STATE_MAIN;
                                EPD_W21_Init();
                                EPD_Dis_Part(DISP_MENU_X2, DISP_MENU_Y2, txt_time_set, 32, 16, POS);
                                EPD_Dis_Part(DISP_MENU_X3, DISP_MENU_Y3, txt_wallpaper, 32, 16, POS);
                                EPD_Part_Update_and_DeepSleep();
                                break;
                            default:
                                break;
                        }
                        break;
						
                    // --------------------- 调整日期状态 ---------------------
                    case TIME_SET_DATE:
                        switch(recv_action)
                        {
                            case KEY1_CLICK:       // 切换到调整月份
                                g_TimeSetCache.sub_state = TIME_SET_MONTH;
                                RefreshTimeSetDisplay();
                                break;
                            case KEY2_CLICK:       // 日期+1（循环）
                                g_TimeSetCache.date++;
                                if(g_TimeSetCache.date > 31) g_TimeSetCache.date = 1;
                                RefreshTimeSetDisplay();
                                break;
                            case KEY2_DOUBLE:      // 日期-1（循环）
                                g_TimeSetCache.date--;
                                if(g_TimeSetCache.date < 1) g_TimeSetCache.date = 31;
                                RefreshTimeSetDisplay();
                                break;
                            case KEY1_LONG:        // 取消设置，返回主菜单
                                xCurrentMenuState = MENU_STATE_MAIN;
                                EPD_W21_Init();
                                EPD_Dis_Part(DISP_MENU_X2, DISP_MENU_Y2, txt_time_set, 32, 16, POS);
                                EPD_Dis_Part(DISP_MENU_X3, DISP_MENU_Y3, txt_wallpaper, 32, 16, POS);
                                EPD_Part_Update_and_DeepSleep();
                                break;
                            default:
                                break;
                        }
                        break;

                    // --------------------- 调整月份状态 ---------------------
                    case TIME_SET_MONTH:
                        switch(recv_action)
                        {
                            case KEY1_CLICK:       // 切换到调整年份
                                g_TimeSetCache.sub_state = TIME_SET_YEAR;
                                RefreshTimeSetDisplay();
                                break;
                            case KEY2_CLICK:       // 月份+1（循环）
                                g_TimeSetCache.month++;
                                if(g_TimeSetCache.month > 12) g_TimeSetCache.month = 1;
                                RefreshTimeSetDisplay();
                                break;
                            case KEY2_DOUBLE:      // 月份-1（循环）
                                g_TimeSetCache.month--;
                                if(g_TimeSetCache.month < 1) g_TimeSetCache.month = 12;
                                RefreshTimeSetDisplay();
                                break;
                            case KEY1_LONG:        // 取消设置，返回主菜单
                                xCurrentMenuState = MENU_STATE_MAIN;
                                EPD_W21_Init();
                                EPD_Dis_Part(DISP_MENU_X2, DISP_MENU_Y2, txt_time_set, 32, 16, POS);
                                EPD_Dis_Part(DISP_MENU_X3, DISP_MENU_Y3, txt_wallpaper, 32, 16, POS);
                                EPD_Part_Update_and_DeepSleep();
                                break;
                            default:
                                break;
                        }
                        break;

                    // --------------------- 调整年份状态 ---------------------
                    case TIME_SET_YEAR:
                        switch(recv_action)
                        {
                            case KEY1_CLICK:       // 切换到调整星期
                                g_TimeSetCache.sub_state = TIME_SET_SAVE_EXIT;
                                RefreshTimeSetDisplay();
                                break;
                            case KEY2_CLICK:       // 年份+1
                                g_TimeSetCache.year = (g_TimeSetCache.year + 1) % 100;
                                RefreshTimeSetDisplay();
                                break;
                            case KEY2_DOUBLE:      // 年份-1
                                g_TimeSetCache.year = (g_TimeSetCache.year == 0) ? 99 : (g_TimeSetCache.year - 1);
                                RefreshTimeSetDisplay();
                                break;
                            case KEY1_LONG:        // 取消设置，返回主菜单
                                xCurrentMenuState = MENU_STATE_MAIN;
                                EPD_W21_Init();
                                EPD_Dis_Part(DISP_MENU_X2, DISP_MENU_Y2, txt_time_set, 32, 16, POS);
                                EPD_Dis_Part(DISP_MENU_X3, DISP_MENU_Y3, txt_wallpaper, 32, 16, POS);
                                EPD_Part_Update_and_DeepSleep();
                                break;
                            default:
                                break;
                        }
                        break;



                    // --------------------- 保存退出状态 ---------------------
                    case TIME_SET_SAVE_EXIT:
                        switch(recv_action)
                        {
                            case KEY1_CLICK:       // 循环：回到调整分钟
                                g_TimeSetCache.sub_state = TIME_SET_MINUTE;
                                RefreshTimeSetDisplay();
                                break;
                            case KEY2_CLICK:       // KEY2单击：确认保存并退出
                                // 写入DS1302
                                taskENTER_CRITICAL();
                                Time[MINUTE] = g_TimeSetCache.minute;
                                Time[HOUR] = g_TimeSetCache.hour;
                                Time[DATE] = g_TimeSetCache.date;
                                Time[MONTH] = g_TimeSetCache.month;
                                Time[YEAR] = g_TimeSetCache.year;
                                Time[WEEK] = g_TimeSetCache.week;
                                Time[SECOND] = 0;
                                DS1302_SetTime();
                                taskEXIT_CRITICAL();

                                // 显示保存成功并返回主菜单
                                EPD_W21_Init();
                                EPD_Dis_Part(DISP_MENU_X1, DISP_MENU_Y1, txt_save_ok, 32, 16, POS);
                                EPD_Part_Update_and_DeepSleep();
                                vTaskDelay(1000 / portTICK_PERIOD_MS);
                                xCurrentMenuState = MENU_STATE_MAIN;
                                g_TimeSetCache.sub_state = TIME_SET_MINUTE; // 重置子状态
                                
                                // 恢复主菜单显示
                                EPD_W21_Init();
                                EPD_Dis_Part(DISP_MENU_TITLE_X, DISP_MENU_Y2, txt_time_set, 32, 16, POS);
                                EPD_Dis_Part(DISP_MENU_TITLE_X, DISP_MENU_Y3, txt_wallpaper, 32, 16, POS);
                                EPD_Part_Update_and_DeepSleep();
                                break;
                            case KEY1_LONG:        // 取消保存并退出
                                xCurrentMenuState = MENU_STATE_MAIN;
                                g_TimeSetCache.sub_state = TIME_SET_MINUTE;
                                EPD_W21_Init();
                                EPD_Dis_Part(DISP_MENU_TITLE_X, DISP_MENU_Y2, txt_time_set, 32, 16, POS);
                                EPD_Dis_Part(DISP_MENU_TITLE_X, DISP_MENU_Y3, txt_wallpaper, 32, 16, POS);
                                EPD_Part_Update_and_DeepSleep();
                                break;
                            default:
                                break;
                        }
                        break;

                    default:
                        break;
                }
                break;

            // 退出菜单（回到时钟）
            case MENU_STATE_EXIT:
                xCurrentMenuState = MENU_STATE_MAIN;
                break;

            default:
                xCurrentMenuState = MENU_STATE_MAIN;
                break;
        }
    }
}

// 任务创建任务
static void AppCreate_Task(void *arg)
{
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
                                
    vTaskDelete(App_Create_Task_Handle);
}

// 主函数
int main(void)
{
    #ifdef DEBUG
      debug();
    #endif

    // 外设初始化
    LED_GPIO_Config();	 
    EXTI_Key_Config(); 
    EPAPER_GPIO_Config();
    DS1302_Init();

    // 加载背景图
    EPD_HW_Init(); 
    EPD_SetRAMValue_BaseMap(gImage_base);  
    driver_delay_xms(500);
	
	// 初始化isClock的互斥锁
    xIsClockMutex = xSemaphoreCreateMutex();
    if (xIsClockMutex == NULL) {
        // 互斥锁创建失败，LED提示）
        while(1) {
            LED1_OFF;
        }
    }
    // 创建消息队列
    xKeyEventQueue = xQueueCreate(5, sizeof(KeyEvent_t));
    xQueueReset(xKeyEventQueue);
    xMenuStateQueue = xQueueCreate(5, sizeof(KeyAction_t));
    xQueueReset(xMenuStateQueue);
    
    // 创建任务
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