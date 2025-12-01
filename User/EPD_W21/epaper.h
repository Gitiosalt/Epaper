#ifndef __EPAPER_H
#define __EPAPER_H
#include "stm32f10x.h"
//296*128///////////////////////////////////////

#define POS 1
#define NEG 2 
#define OFF 3 


#define EPD_WIDTH  296
#define EPD_HEIGHT 128

#define ALLSCREEN_GRAGHBYTES	EPD_WIDTH*EPD_HEIGHT/8

///////////////////////////GPIO Settings//////////////////////////////////////////////////////

#define EPD_W21_MOSI_0	GPIO_ResetBits(GPIOA, GPIO_Pin_4)
#define EPD_W21_MOSI_1	GPIO_SetBits(GPIOA, GPIO_Pin_4)

#define EPD_W21_CLK_0	GPIO_ResetBits(GPIOA, GPIO_Pin_5)
#define EPD_W21_CLK_1	GPIO_SetBits(GPIOA, GPIO_Pin_5)

#define EPD_W21_CS_0	GPIO_ResetBits(GPIOA, GPIO_Pin_6)
#define EPD_W21_CS_1	GPIO_SetBits(GPIOA, GPIO_Pin_6)

#define EPD_W21_DC_0	GPIO_ResetBits(GPIOA, GPIO_Pin_7)
#define EPD_W21_DC_1	GPIO_SetBits(GPIOA, GPIO_Pin_7)

#define EPD_W21_RST_0	GPIO_ResetBits(GPIOB, GPIO_Pin_0)
#define EPD_W21_RST_1	GPIO_SetBits(GPIOB, GPIO_Pin_0)

#define EPD_W21_BUSY_LEVEL 0
#define isEPD_W21_BUSY GPIO_ReadInputDataBit(GPIOB, GPIO_Pin_1) 


void driver_delay_xms(unsigned long xms);			
//void EpaperIO_Init(void);
void Epaper_READBUSY(void);
void Epaper_Spi_WriteByte(unsigned char TxData);
void Epaper_Write_Command(unsigned char cmd);
void Epaper_Write_Data(unsigned char data);

void EPD_HW_Init_4GRAY(void);			//Electronic paper initialization£¨4»Ò½×£©
void EPD_HW_Init(void); //Electronic paper initialization
void EPD_W21_Init(void); //Electronic paper hard reset

void EPD_Update_4GRAY_and_DeepSleep(void);
void EPD_Update_and_DeepSleep(void);
void EPD_Part_Update_and_DeepSleep(void); 
	
void EPD_WhiteScreen_White(void);

//Display 
void EPD_WhiteScreen_ALL_4GRAY(const unsigned char *datas);
void EPD_WhiteScreen_ALL(const unsigned char *datas);
void EPD_SetRAMValue_BaseMap(const unsigned char * datas);
void EPD_Dis_Part(int h_start,int v_start,const unsigned char * datas,int PART_WIDTH,int PART_HEIGHT,unsigned char mode);


#endif


