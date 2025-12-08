#ifndef __EXTI_H
#define	__EXTI_H


#include "stm32f10x.h"


//Òý½Å¶¨Òå
#define KEY1_INT_GPIO_PORT         GPIOB
#define KEY1_INT_GPIO_CLK          (RCC_APB2Periph_GPIOB|RCC_APB2Periph_AFIO)
#define KEY1_INT_GPIO_PIN          GPIO_Pin_14
#define KEY1_INT_EXTI_PORTSOURCE   GPIO_PortSourceGPIOB
#define KEY1_INT_EXTI_PINSOURCE    GPIO_PinSource14
#define KEY1_INT_EXTI_LINE         EXTI_Line14
#define KEY1_INT_EXTI_IRQ          EXTI15_10_IRQn

#define KEY2_INT_GPIO_PORT         GPIOB
#define KEY2_INT_GPIO_CLK          (RCC_APB2Periph_GPIOB|RCC_APB2Periph_AFIO)
#define KEY2_INT_GPIO_PIN          GPIO_Pin_15
#define KEY2_INT_EXTI_PORTSOURCE   GPIO_PortSourceGPIOB
#define KEY2_INT_EXTI_PINSOURCE    GPIO_PinSource15
#define KEY2_INT_EXTI_LINE         EXTI_Line15
#define KEY2_INT_EXTI_IRQ          EXTI15_10_IRQn

#define KEY_IRQHandler             EXTI15_10_IRQHandler

#define KEY1_STATE        		   GPIO_ReadInputDataBit(GPIOB, GPIO_Pin_14)
#define KEY2_STATE        		   GPIO_ReadInputDataBit(GPIOB, GPIO_Pin_15)


void EXTI_Key_Config(void);


#endif 
