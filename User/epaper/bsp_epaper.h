#ifndef __BSP_EPAPER_H
#define	__BSP_EPAPER_H


#include "stm32f10x.h"

/* 定义Epaper连接的GPIO端口 */
/*
* BUSY ->   PB1
* RST  ->   PB0
* DC   ->   PA7
* CS   ->   PA6
* SCK  ->   PA5
* SDI  ->   PA4
*/

#define BUSY_GPIO_PORT    	GPIOB
#define BUSY_GPIO_CLK    	RCC_APB2Periph_GPIOB
#define BUSY_GPIO_PIN		GPIO_Pin_1

#define RST_GPIO_PORT    	GPIOB
#define RST_GPIO_CLK    	RCC_APB2Periph_GPIOB
#define RST_GPIO_PIN		GPIO_Pin_0

#define DC_GPIO_PORT    	GPIOA
#define DC_GPIO_CLK    		RCC_APB2Periph_GPIOA
#define DC_GPIO_PIN			GPIO_Pin_7

#define CS_GPIO_PORT    	GPIOA
#define CS_GPIO_CLK    		RCC_APB2Periph_GPIOA
#define CS_GPIO_PIN			GPIO_Pin_6

#define SCK_GPIO_PORT    	GPIOA
#define SCK_GPIO_CLK    	RCC_APB2Periph_GPIOA
#define SCK_GPIO_PIN		GPIO_Pin_5

#define SDI_GPIO_PORT    	GPIOA
#define SDI_GPIO_CLK    	RCC_APB2Periph_GPIOA
#define SDI_GPIO_PIN		GPIO_Pin_4


/** the macro definition to trigger the led on or off 
  * 1 - off
  *0 - on
  */
#define ON  0
#define OFF 1


void EPAPER_GPIO_Config(void);

#endif /* __EPAPER_H */
