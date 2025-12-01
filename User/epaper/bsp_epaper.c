/**
  ******************************************************************************
  * @file    bsp_epaper.c
  * @author  iosalt
  * @version V1.0
  * @date    2025-11-29
  * @brief   墨水屏驱动函数接口
  ******************************************************************************
  * @attention
  *
  ******************************************************************************
  */
  
#include "bsp_epaper.h"   


 /**
  * @brief  初始化控制Epaper的IO
  * @param  无
  * @retval 无
  */
void EPAPER_GPIO_Config(void)
{		
	GPIO_InitTypeDef  GPIO_InitStructure;
 	
	RCC_APB2PeriphClockCmd(BUSY_GPIO_CLK, ENABLE);
	RCC_APB2PeriphClockCmd(RST_GPIO_CLK, ENABLE);
	RCC_APB2PeriphClockCmd(DC_GPIO_CLK, ENABLE);
	RCC_APB2PeriphClockCmd(CS_GPIO_CLK, ENABLE);	
	RCC_APB2PeriphClockCmd(SCK_GPIO_CLK, ENABLE);	
	RCC_APB2PeriphClockCmd(SDI_GPIO_CLK, ENABLE);
	
	 //CS
	GPIO_InitStructure.GPIO_Pin = CS_GPIO_PIN;		//Port configuration
	GPIO_InitStructure.GPIO_Mode = GPIO_Mode_Out_PP; 		 			
	GPIO_InitStructure.GPIO_Speed = GPIO_Speed_2MHz;		 		
	GPIO_Init(CS_GPIO_PORT, &GPIO_InitStructure);	  	

	//SCK
	GPIO_InitStructure.GPIO_Pin = SCK_GPIO_PIN;		//Port configuration
	GPIO_InitStructure.GPIO_Mode = GPIO_Mode_Out_PP; 		 			
	GPIO_InitStructure.GPIO_Speed = GPIO_Speed_2MHz;		 		
	GPIO_Init(SCK_GPIO_PORT, &GPIO_InitStructure);		
	
	//SDI
	GPIO_InitStructure.GPIO_Pin = SDI_GPIO_PIN;		//Port configuration
	GPIO_InitStructure.GPIO_Mode = GPIO_Mode_Out_PP; 		 			
	GPIO_InitStructure.GPIO_Speed = GPIO_Speed_2MHz;		 		
	GPIO_Init(SDI_GPIO_PORT, &GPIO_InitStructure);	
	
	 // DC
	GPIO_InitStructure.GPIO_Pin = DC_GPIO_PIN;		//Port configuration
	GPIO_InitStructure.GPIO_Mode = GPIO_Mode_Out_PP; 		 			
	GPIO_InitStructure.GPIO_Speed = GPIO_Speed_2MHz;		 		
	GPIO_Init(DC_GPIO_PORT, &GPIO_InitStructure);
	
	// RST
	GPIO_InitStructure.GPIO_Pin = RST_GPIO_PIN;		//Port configuration
	GPIO_InitStructure.GPIO_Mode = GPIO_Mode_Out_PP; 		 			
	GPIO_InitStructure.GPIO_Speed = GPIO_Speed_2MHz;		 		
	GPIO_Init(RST_GPIO_PORT, &GPIO_InitStructure);	 
	
	// BUSY
	GPIO_InitStructure.GPIO_Pin  = BUSY_GPIO_PIN;
	GPIO_InitStructure.GPIO_Mode = GPIO_Mode_IN_FLOATING; //Floating input
	GPIO_InitStructure.GPIO_Speed = GPIO_Speed_2MHz;
	GPIO_Init(BUSY_GPIO_PORT, &GPIO_InitStructure);    
	
}

/*********************************************END OF FILE**********************/
