/*
 * @Descripttion: DS1302驱动文件(.h)
 * @Author: JaRyon
 * @version: v1.0
 * @Date: 2025-10-19 10:52:47
 */
#ifndef __DS1302_H
#define __DS1302_H
 
#include "stm32f10x.h"
#include "bsp_SysTick.h"
 
 /*
 *	SCL	  ->	PB8
 *	DATA  ->	PB9
 *	CE    ->	PC13
 */
 
 
// 宏定义
#define CE_GPIO_PORT    	GPIOC
#define CE_GPIO_CLK    		RCC_APB2Periph_GPIOC
#define CE_GPIO_PIN			GPIO_Pin_13

#define SCL_GPIO_PORT   	GPIOB
#define SCL_GPIO_CLK    	RCC_APB2Periph_GPIOB
#define SCL_GPIO_PIN		GPIO_Pin_8

#define DATA_GPIO_PORT   	GPIOB
#define DATA_GPIO_CLK    	RCC_APB2Periph_GPIOB
#define DATA_GPIO_PIN		GPIO_Pin_9

// DS1302寄存器写命令
#define DS1302_SECOND   0X80
#define DS1302_MINUTE   0X82
#define DS1302_HOUR     0X84
#define DS1302_DAY      0X86
#define DS1302_MONTH    0X88
#define DS1302_WEEK     0X8A
#define DS1302_YEAR     0X8C
#define DS1302_WP       0X8E
// 在一些 DS1302 的手册中，可能因为写保护寄存器对芯片数据写入控制的关键作用，就直接将其描述为控制寄存器，导致了理解上的混淆
 
 
// 时钟数据复位信号控制
#define DS1302_RST_HIGH()  GPIO_SetBits(CE_GPIO_PORT, CE_GPIO_PIN) 
#define DS1302_RST_LOW()   GPIO_ResetBits(CE_GPIO_PORT, CE_GPIO_PIN)
#define DS1302_DAT_HIGH()  GPIO_SetBits(DATA_GPIO_PORT, DATA_GPIO_PIN)
#define DS1302_DAT_LOW()   GPIO_ResetBits(DATA_GPIO_PORT, DATA_GPIO_PIN)
#define DS1302_CLK_HIGH()  GPIO_SetBits(SCL_GPIO_PORT, SCL_GPIO_PIN) 
#define DS1302_CLK_LOW()   GPIO_ResetBits(SCL_GPIO_PORT, SCL_GPIO_PIN) 

// 数据线读取
#define DS1302_READ_DAT    GPIO_ReadInputDataBit(DATA_GPIO_PORT, DATA_GPIO_PIN)

 
// 变量定义
// 日历时间枚举
typedef enum
{
    SECOND,
    MINUTE,
    HOUR,
    DATE,
    MONTH,
    WEEK,
    YEAR,
    TIME_SUM
}TIME_PARAM;
 
// IO模式枚举
typedef enum
{
    OUTPUT_MODE,
    INPUT_MODE
}IO_MODE;
 
// 外部变量声明
extern int8_t Time[TIME_SUM];
 
// 函数声明
void DS1302_Init(void);
void DS1302_WriteData(uint8_t cmd, uint8_t data);
uint8_t DS1302_ReadData(uint8_t cmd);
 
void DS1302_SetTime(void);
void DS1302_GetTime(void);
 
#endif