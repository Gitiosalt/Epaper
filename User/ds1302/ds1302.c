/*
 * @Descripttion: DS1302驱动文件(.c)
 * @Author: JaRyon
 * @version: v1.0
 * @Date: 2025-10-19 10:52:52
 */
#include "ds1302.h"


/**
 * DS1302 引脚连接方式
 * 
 * PC13  ----->  RST
 * PB9   ----->  DAT
 * PB8   ----->  CLK
 * 
*/

 
// 时间参数 秒、分、时、日、月、星期、年
int8_t Time[TIME_SUM] = {10, 45, 22, 19, 12, 7, 25};
 
static void Delay_us(uint16_t us)
{
    uint32_t ticks = 0;
    uint32_t reload = SysTick->LOAD; 
    ticks = us * (SystemCoreClock / 1000000); 
    SysTick->CTRL &= ~SysTick_CTRL_ENABLE_Msk;
    while(ticks--)
    {
        __NOP(); 
    }
    SysTick->CTRL |= SysTick_CTRL_ENABLE_Msk; 
} 



/**
 * @brief DS1302初始化
 * @param  void 无
 * @return void
 */
void DS1302_Init(void)
{
	GPIO_InitTypeDef GPIO_InitStructure;

	RCC_APB2PeriphClockCmd( CE_GPIO_CLK | SCL_GPIO_CLK | DATA_GPIO_CLK , ENABLE);	
	GPIO_InitStructure.GPIO_Mode = GPIO_Mode_Out_PP;    
	GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz; 
	
	GPIO_InitStructure.GPIO_Pin = CE_GPIO_PIN;
	GPIO_Init(CE_GPIO_PORT, &GPIO_InitStructure);	
	
	GPIO_InitStructure.GPIO_Pin = SCL_GPIO_PIN;
	GPIO_Init(SCL_GPIO_PORT, &GPIO_InitStructure);	
	
	GPIO_InitStructure.GPIO_Pin = DATA_GPIO_PIN;
	GPIO_Init(DATA_GPIO_PORT, &GPIO_InitStructure);	
	// 初始复位，时钟拉低
	DS1302_RST_LOW();
    DS1302_CLK_LOW();
}
 
 
/**
 * @brief      设置数据线输入输出模式
 * @param      IO_MODE mode 设置的模式类型枚举常量
 * @return     void 无
 * @example    DS1302_SetMode(OUTPUT_MODE);
 * @attention  该函数仅内部调用
 */
static void DS1302_SetMode(IO_MODE mode)
{
    GPIO_InitTypeDef GPIO_InitStruct;  
    
    GPIO_InitStruct.GPIO_Pin = DATA_GPIO_PIN;  
    GPIO_InitStruct.GPIO_Speed = GPIO_Speed_50MHz;  
    
    switch (mode)
    {
    case OUTPUT_MODE:

        GPIO_InitStruct.GPIO_Mode = GPIO_Mode_Out_PP;
        GPIO_Init(DATA_GPIO_PORT, &GPIO_InitStruct); 
        break;
        
    case INPUT_MODE:

        GPIO_InitStruct.GPIO_Mode = GPIO_Mode_IPU;
        GPIO_Init(DATA_GPIO_PORT, &GPIO_InitStruct);  
        break;
        
    default:
        break;
    }   
}

 
/**
 * @brief      向DS1302写入指令数据
 * @param      uint8_t cmd 待写入命令
 * @param      uint8_t data 待写入数据
 * @return     void
 * @example    DS1302_WriteData(DS1302_WP, 0x00);
 * @attention  
 */
void DS1302_WriteData(uint8_t cmd, uint8_t data)
{
//	DS1302_SetMode(OUTPUT_MODE);
    // 1. RST拉高，准备写入
    DS1302_RST_HIGH();
    Delay_us(3);
	
    // 2. 开始写命令
    for (uint8_t i = 0; i < 8; i++)
    {
        // 2.1 写入命令
        if (cmd & 0x01)
        {
            DS1302_DAT_HIGH();
        }
        else
        {
            DS1302_DAT_LOW();
        }
        cmd >>= 1;
        Delay_us(1);
		
 
        // 2.2 CLK拉高，等待接收
        DS1302_CLK_HIGH();
        Delay_us(1);
		
 
        // 2.3 CLK拉低
        DS1302_CLK_LOW();
        Delay_us(1);
		
    }
 
    // 3. 开始写数据
    for (uint8_t i = 0; i < 8; i++)
    {
        // 3.1 写入数据
        if (data & 0x01)
        {
            DS1302_DAT_HIGH();
        }
        else
        {
            DS1302_DAT_LOW();
        }
        data >>= 1;
        Delay_us(1);
		
 
        // 3.2 CLK拉高，等待接收
        DS1302_CLK_HIGH();
        Delay_us(1);
 		
		
        // 3.3 CLK拉低
        DS1302_CLK_LOW();
        Delay_us(1);
		
    }
 
    // 4. RST拉低，复位
    DS1302_RST_LOW();
}
 
 
/**
 * @brief      向DS1302读取数据
 * @param      uint8_t cmd  待读数据需写入的访问命令
 * @return     uint8_t 返回一字节读取的数据
 * @example    uint8_t data = DS1302_ReadData(DS1302_SECOND);
 * @attention  读取命令比写入命令数值上+1, 读完数据后数据线拉低避免数据错乱
 */
uint8_t DS1302_ReadData(uint8_t cmd)
{
    uint8_t byte = 0x00;
    cmd |= 0x01;
 
    // 1. 拉高RST
    DS1302_RST_HIGH();
    Delay_us(3);

    // 2. 开始写命令
    for (uint8_t i = 0; i < 8; i++)
    {
        // 2.1 写入命令
        if (cmd & 0x01)
        {
            DS1302_DAT_HIGH();
        }
        else
        {
            DS1302_DAT_LOW();
        }
        cmd >>= 1;
        Delay_us(1);
		
        // 2.2 CLK拉高，等待接收
        DS1302_CLK_HIGH();
        Delay_us(1);
		
        // 2.3 CLK拉低
        DS1302_CLK_LOW();
        Delay_us(1);
		
    }
 
    // 3. 开始读取数据 低位先行
    DS1302_SetMode(INPUT_MODE);
    Delay_us(5);

    for (uint8_t i = 0; i < 8; i++)
    {
        if (DS1302_READ_DAT)
        {
            byte |= (0x01 << i);
        }
        else
        {
            byte &= ~(0x01 << i);
        }
 
        // 3.1 CLK拉高
        DS1302_CLK_HIGH();
        Delay_us(1);
		
        // 3.2 CLK拉低，开始数据读取
        DS1302_CLK_LOW();
        Delay_us(1);
		
    }
    
    // 4. RST拉低，DAT置0
    DS1302_RST_LOW();
    DS1302_SetMode(OUTPUT_MODE);
    DS1302_DAT_LOW();
 
    return byte;
}
 
 
/**
 * @brief      设置DS1302初始时间
 * @param      void 无
 * @return     void
 * @example    DS1302_SetTime();
 * @attention  设置DS1302时间相关寄存器时，需将实际数值转换成十位和个位的BCD码后进行写入
 */
void DS1302_SetTime(void)
{
    // 解除写保护
    DS1302_WriteData(DS1302_WP, 0x00);
    // 十进制转为BCD码存储
    // 1. 设置秒
    DS1302_WriteData(DS1302_SECOND, Time[SECOND] / 10 * 16 + Time[SECOND] % 10);
    // 2. 设置分
    DS1302_WriteData(DS1302_MINUTE, Time[MINUTE] / 10 * 16 + Time[MINUTE] % 10);
    // 3. 设置时 默认24小时制
    DS1302_WriteData(DS1302_HOUR, Time[HOUR] /10 * 16 + Time[HOUR] % 10);
    // 4. 设置日
    DS1302_WriteData(DS1302_DAY, Time[DATE] / 10 * 16 + Time[DATE] % 10);
    // 5. 设置月
    DS1302_WriteData(DS1302_MONTH, Time[MONTH] / 10 * 16 + Time[MONTH] % 10);
    // 6. 设置星期
    DS1302_WriteData(DS1302_WEEK, Time[WEEK]);
    // 7. 设置年
    DS1302_WriteData(DS1302_YEAR, Time[YEAR] / 10 * 16 + Time[YEAR] % 10);
 
    // 开启写保护
    DS1302_WriteData(DS1302_WP, 0x80);
}
 
 
/**
 * @brief      获取当前DS1302的时间
 * @param      void 无
 * @return     void
 * @example    DS1302_GetTime();
 * @attention  读取DS1302时间相关寄存器时，需将寄存器中的BCD码转换成十位和个位的十进制数后进行读取
 */
void DS1302_GetTime(void)
{
    uint8_t temp = 0;
    // 读出BCD码，转为十进制记录
    temp = DS1302_ReadData(DS1302_SECOND);  Time[SECOND] = temp / 16 * 10 + temp % 16;
    temp = DS1302_ReadData(DS1302_MINUTE);  Time[MINUTE] = temp / 16 * 10 + temp % 16;
    temp = DS1302_ReadData(DS1302_HOUR);    Time[HOUR] = temp / 16 * 10 + temp % 16;
    temp = DS1302_ReadData(DS1302_DAY);     Time[DATE] = temp / 16 * 10 + temp % 16;
    temp = DS1302_ReadData(DS1302_MONTH);   Time[MONTH] = temp / 16 * 10 + temp % 16;
    temp = DS1302_ReadData(DS1302_WEEK);    Time[WEEK] = temp / 16 * 10 + temp % 16;
    temp = DS1302_ReadData(DS1302_YEAR);    Time[YEAR] = temp / 16 * 10 + temp % 16;
}
 