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

ErrorStatus HSEStartUpStatus;
#define SOFT_DELAY Delay(0x1FFFFF);

void Delay(__IO u32 nCount); 




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
	/* LED 端口初始化 */
	LED_GPIO_Config();	 
	EPAPER_GPIO_Config();
	
	LED2_ON;
	 
///////////////////////////////////////////
//  全刷方式刷新整张图片（4灰阶）
	EPD_HW_Init_4GRAY(); 										//EPD init 4Gray
	EPD_WhiteScreen_ALL_4GRAY(gImage_4Gray11); 	
    driver_delay_xms(2000);				
	 
///////////////////////////////////////////
	//全屏刷新整张图片（无灰阶）
	//Full screen refresh
	EPD_HW_Init(); 													//Electronic paper initialization
	EPD_WhiteScreen_ALL(gImage_1); 					//Refresh the picture in full screen
    driver_delay_xms(1000);
/*	
	EPD_HW_Init(); 													//Electronic paper initialization
	EPD_WhiteScreen_ALL(gImage_2); 					//Refresh the picture in full screen
    driver_delay_xms(1000);

	EPD_HW_Init(); 													//Electronic paper initialization
	EPD_WhiteScreen_ALL(gImage_3); 					//Refresh the picture in full screen
    driver_delay_xms(1000);

	EPD_HW_Init(); 													//Electronic paper initialization
	EPD_WhiteScreen_ALL(gImage_4); 					//Refresh the picture in full screen
    driver_delay_xms(1000);
		
	EPD_HW_Init(); 													//Electronic paper initialization
	EPD_WhiteScreen_ALL(gImage_5); 					//Refresh the picture in full screen
    driver_delay_xms(1000);

	EPD_HW_Init(); 													//Electronic paper initialization
	EPD_WhiteScreen_ALL(gImage_6); 					//Refresh the picture in full screen
    driver_delay_xms(1000);
*/	
	EPD_HW_Init(); 													//Electronic paper initialization	
	EPD_SetRAMValue_BaseMap(gImage_base);  	//Partial refresh background（注意：此处刷背景图片的函数和全刷图片的函数不一样）
	driver_delay_xms(500);

///////////////////////////////////////////
//“摄氏度”负显，51.9°C，电量3格

//注意：单个图片刷新的时候，数据发送完直接执行 EPD_Part_Update();
//      多个图标一起刷新的时候，所有数据全都发送完毕，一次性执行 EPD_Part_Update();
//此处建议先清除不显示的部分，再发送需要显示的部分，以免因为前后顺序错误，造成该显示的不显

	EPD_HW_Init(); 													//Electronic paper initialization
																										//    y         x       显示内容     显示宽度     显示高度     显示模式
	EPD_Dis_Part(25,104,gImage_Celsius,47,16,NEG); 		//   25       104        摄氏度         47           16				   负显

	EPD_Dis_Part(29,32,gImage_minus1,33,64,OFF); 		  //   29        32          -1           33           64					 OFF
	EPD_Dis_Part(75,32,gImage_num5,33,64,POS); 		    //   75        32           5           33           64					 正显
	EPD_Dis_Part(120,32,gImage_num1,33,64,POS);		    //  120        32           1           33           64					 正显
	EPD_Dis_Part(160,88,gImage_DOT,8,8,POS);		      //  160        88        小数点          8            8					 正显
	EPD_Dis_Part(175,32,gImage_num9,33,64,POS); 	    //  175        32           9           33           64					 正显
	EPD_Dis_Part(221,32,gImage_degree,10,16,POS);     //  221        32           °           10           16			     正显		
	EPD_Dis_Part(233,32,gImage_C,33,64,POS); 			    //  233        32           C           33           64					 正显
	EPD_Dis_Part(243,16,black_block,4,8,OFF);		      //  243        16      电量最高格        4            8				   OFF

	EPD_Part_Update_and_DeepSleep();
    driver_delay_xms(500);		


///////////////////////////////////////////
//“华氏度”负显，125.4°F，电量3格

//采用局刷方式连续刷新多个显示界面的时候，从休眠状态唤醒只需要硬件复位就可以，无需重新初始化。

	EPD_W21_Init();											    //hard reset
																										//    y         x       显示内容     显示宽度     显示高度     显示模式
	EPD_Dis_Part(25,104,gImage_Celsius,47,16,POS); 		//   25       104        摄氏度         47           16				   正显
	EPD_Dis_Part(92,104,gImage_Fahrenheit,47,16,NEG); //   92       104        华氏度         47           16			     负显
	EPD_Dis_Part(25,104,gImage_Fahrenheit,47,16,POS); //   92       104        华氏度         47           16			     负显
	
	EPD_Dis_Part(29,32,gImage_num1,33,64,POS); 		    //   29        32           1           33           64					 正显
	EPD_Dis_Part(75,32,gImage_num2,33,64,POS); 		    //   75        32           2           33           64					 正显
	EPD_Dis_Part(120,32,gImage_num5,33,64,POS);		    //  120        32           5           33           64					 正显
	EPD_Dis_Part(160,88,gImage_DOT,8,8,POS);		      //  160        88        小数点          8            8					 正显
	EPD_Dis_Part(175,32,gImage_num4,33,64,POS); 	    //  175        32           4           33           64					 正显
	EPD_Dis_Part(221,32,gImage_degree,10,16,POS);     //  221        32           °           10           16				   正显	
	EPD_Dis_Part(233,32,gImage_F,33,64,POS); 			    //  233        32           F           33           64					 正显
	EPD_Dis_Part(243,16,black_block,4,8,OFF);		      //  243        16      电量最高格        4            8				   OFF

	EPD_Part_Update_and_DeepSleep();
  driver_delay_xms(500);		
		
		
//////////////////////////////////////////
//“湿度”负显，30.6%，电量2格

	EPD_W21_Init();											    //hard reset
																										//    y         x       显示内容     显示宽度     显示高度     显示模式		
	EPD_Dis_Part(25,104,gImage_Celsius,47,16,POS); 		//   25       104        摄氏度         47           16				   正显
	EPD_Dis_Part(92,104,gImage_Fahrenheit,47,16,POS); //   92       104        华氏度         47           16			     正显
	EPD_Dis_Part(159,104,gImage_humidity,47,16,NEG); 	//  159       104         湿度          47           16				   负显
	EPD_Dis_Part(25,104,gImage_humidity,47,16,NEG); 	//  159       104         湿度          47           16				   负显

	EPD_Dis_Part(29,32,gImage_num1,33,64,OFF); 		    //   29        32           1           33           64					 OFF
	EPD_Dis_Part(75,32,gImage_num3,33,64,POS); 		 		//   75        32           3           33           64					 正显
	EPD_Dis_Part(120,32,gImage_num0,33,64,POS);			  //  120        32           0           33           64					 正显
	EPD_Dis_Part(160,88,gImage_DOT,8,8,POS);		      //  160        88        小数点          8            8					 正显
	EPD_Dis_Part(175,32,gImage_num6,33,64,POS); 			//  175        32           6           33           64					 正显
  EPD_Dis_Part(221,32,gImage_degree,10,16,OFF);     //  221        32           °           10           16				   OFF	
	EPD_Dis_Part(233,32,gImage_F,33,64,OFF); 			    //  233        32           F           33           64					 OFF
	EPD_Dis_Part(223,32,gImage_Percent,44,64,POS); 		//  223        32           %           44           64					 正显
	EPD_Dis_Part(243,16,black_block,4,8,OFF);		      //  243        16      电量最高格        4            8				   OFF
	EPD_Dis_Part(248,16,black_block,4,8,OFF);		      //  248        16      电量第二格        4            8				   OFF
	
	EPD_Part_Update_and_DeepSleep();
//  driver_delay_xms(500);				


//////////////////////////////////////////
//“电量”负显，72.8%，电量3格

	EPD_W21_Init();											//hard reset
																										//    y         x       显示内容     显示宽度     显示高度     显示模式		
	EPD_Dis_Part(25,104,gImage_Celsius,47,16,POS); 		//   25       104        摄氏度         47           16				   正显
	EPD_Dis_Part(92,104,gImage_Fahrenheit,47,16,POS); //   92       104        华氏度         47           16			     正显
	EPD_Dis_Part(159,104,gImage_humidity,47,16,POS); 	//  159       104         湿度          47           16				   正显
	EPD_Dis_Part(226,104,gImage_Power,47,16,NEG); 		//  226       104         电量          47           16				   负显
	EPD_Dis_Part(25,104,gImage_Power,47,16,POS); 		//  226       104         电量          47           16				   负显

	EPD_Dis_Part(29,32,gImage_num1,33,64,OFF); 		    //   29        32           1           33           64					 OFF
	EPD_Dis_Part(75,32,gImage_num7,33,64,POS); 		 		//   75        32           7           33           64					 正显
	EPD_Dis_Part(120,32,gImage_num2,33,64,POS);			  //  120        32           2           33           64					 正显
	EPD_Dis_Part(160,88,gImage_DOT,8,8,POS);		      //  160        88        小数点          8            8					 正显
	EPD_Dis_Part(175,32,gImage_num8,33,64,POS); 			//  175        32           8           33           64					 正显
  EPD_Dis_Part(221,32,gImage_degree,10,16,OFF);     //  221        32           °           10           16				   OFF
	EPD_Dis_Part(233,32,gImage_F,33,64,OFF); 			    //  233        32           F           33           64					 OFF
	EPD_Dis_Part(223,32,gImage_Percent,44,64,POS); 		//  223        32           %           44           64					 正显
	EPD_Dis_Part(243,16,black_block,4,8,OFF);		      //  243        16      电量最高格        4            8				   OFF
	EPD_Dis_Part(248,16,black_block,4,8,OFF);		      //  248        16      电量第二格        4            8				   OFF

	EPD_Part_Update_and_DeepSleep();				
    driver_delay_xms(500);				


///////////////////////////////////////////
//注意：从局刷方式转到全刷方式的时候，休眠后一定要重新初始化

	EPD_HW_Init(); 																		//Electronic paper initialization
	EPD_WhiteScreen_White();  												//Show all white

///////////////////////////////////////////
//////////////////////Partial screen refresh/////////////////////////////////////

	EPD_HW_Init(); //Electronic paper initialization	
		
	EPD_Dis_Part(0,0,gImage_1,296,128,POS); 					//y,x,gImage_1,Resolution 296*128
	EPD_Part_Update_and_DeepSleep();	
	driver_delay_xms(500);	

///////////////////////////////////////////

	EPD_W21_Init();											//hard reset
	
	EPD_Dis_Part(0,0,gImage_base,296,128,POS);        //y,x,gImage_base,Resolution 296*128
	EPD_Part_Update_and_DeepSleep();			
    driver_delay_xms(500);			
		
////////////////////////////////////////////////////////////////////////	
////////////////////////////////////////////////////////////////////////	
	//Clear screen

	EPD_HW_Init(); 																		//Electronic paper initialization
	EPD_WhiteScreen_White();  												//Show all white

	while(1);		

}
//注意：屏幕刷新完毕必须进入休眠。
///////////////////////////////////////////////////////////


void Delay(__IO uint32_t nCount)	 //简单的延时函数
{
	for(; nCount != 0; nCount--);
}
/*********************************************END OF FILE**********************/
