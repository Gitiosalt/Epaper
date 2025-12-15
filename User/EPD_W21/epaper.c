#include "epaper.h"

const unsigned char LUT_DATA_4Gray[159] =    //159bytes

{											
0x40,	0x48,	0x80,	0x0,	0x0,	0x0,	0x0,	0x0,	0x0,	0x0,	0x0,	0x0,
0x8,	0x48,	0x10,	0x0,	0x0,	0x0,	0x0,	0x0,	0x0,	0x0,	0x0,	0x0,
0x2,	0x48,	0x4,	0x0,	0x0,	0x0,	0x0,	0x0,	0x0,	0x0,	0x0,	0x0,
0x20,	0x48,	0x1,	0x0,	0x0,	0x0,	0x0,	0x0,	0x0,	0x0,	0x0,	0x0,
0x0,	0x0,	0x0,	0x0,	0x0,	0x0,	0x0,	0x0,	0x0,	0x0,	0x0,	0x0,
0xA,	0x19,	0x0,	0x3,	0x8,	0x0,	0x0,					
0x14,	0x1,	0x0,	0x14,	0x1,	0x0,	0x3,					
0xA,	0x3,	0x0,	0x8,	0x19,	0x0,	0x0,					
0x1,	0x0,	0x0,	0x0,	0x0,	0x0,	0x1,					
0x0,	0x0,	0x0,	0x0,	0x0,	0x0,	0x0,					
0x0,	0x0,	0x0,	0x0,	0x0,	0x0,	0x0,					
0x0,	0x0,	0x0,	0x0,	0x0,	0x0,	0x0,					
0x0,	0x0,	0x0,	0x0,	0x0,	0x0,	0x0,					
0x0,	0x0,	0x0,	0x0,	0x0,	0x0,	0x0,					
0x0,	0x0,	0x0,	0x0,	0x0,	0x0,	0x0,					
0x0,	0x0,	0x0,	0x0,	0x0,	0x0,	0x0,					
0x0,	0x0,	0x0,	0x0,	0x0,	0x0,	0x0,					
0x22,	0x22,	0x22,	0x22,	0x22,	0x22,	0x0,	0x0,	0x0,			
0x22,	0x17,	0x41,	0x0,	0x32,	0x1C};						
			




void driver_delay_xms(unsigned long xms)	
{	
    unsigned long i = 0 , j=0;

    for(j=0;j<xms;j++)
	{
        for(i=0; i<256*40; i++);
    }
}


void Epaper_Spi_WriteByte(unsigned char TxData)
{				   			 
	unsigned char TempData;
	unsigned char scnt;
	TempData=TxData;

  EPD_W21_CLK_0;  
	for(scnt=0;scnt<8;scnt++)
	{ 
		if(TempData&0x80)
		  EPD_W21_MOSI_1 ;
		else
		  EPD_W21_MOSI_0 ;
		EPD_W21_CLK_1;  
	  EPD_W21_CLK_0;  
		TempData=TempData<<1;

  }

}

void Epaper_READBUSY(void)
{ 
  while(1)
  {	 //=1 BUSY
     if(isEPD_W21_BUSY==0) break;;
  }  
}

void Epaper_Write_Command(unsigned char cmd)
{
	EPD_W21_CS_1;
	EPD_W21_CS_0;
	EPD_W21_DC_0;  // D/C#   0:command  1:data

	Epaper_Spi_WriteByte(cmd);
	EPD_W21_CS_1;
}

void Epaper_Write_Data(unsigned char data)
{
	EPD_W21_CS_1;
	EPD_W21_CS_0;
	EPD_W21_DC_1;  // D/C#   0:command  1:data

	Epaper_Spi_WriteByte(data);
	EPD_W21_CS_1;
}




//////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////4GRAY/////////////////////////////////////

void EPD_select_LUT(unsigned char * wave_data)
{        
     unsigned char count;
     Epaper_Write_Command(0x32);
	 for(count=0;count<227;count++)Epaper_Write_Data(wave_data[count]);

}	

/***************************4灰阶初始化（FPC朝下显示）**************************************/

void EPD_HW_Init_4GRAY(void)
{
	int XStart,XEnd,YStart_L,YStart_H,YEnd_L,YEnd_H;	
	
	XStart=0x00;
	XEnd=(EPD_HEIGHT/8-1);
	
	YStart_L=0x00;
	YStart_H=0x00;	
	YEnd_L=(EPD_WIDTH-1)%256;
	YEnd_H=(EPD_WIDTH-1)/256;	
	
	EPD_W21_Init();							//hard reset 

	Epaper_READBUSY();
	Epaper_Write_Command(0x12); // soft reset
	Epaper_READBUSY();
	
	Epaper_Write_Command(0x01); //Driver output control      
	Epaper_Write_Data((EPD_WIDTH-1)%256);
	Epaper_Write_Data((EPD_WIDTH-1)/256);
	Epaper_Write_Data(0x00);
	
	Epaper_Write_Command(0x11); //data entry mode       
	Epaper_Write_Data(0x03);		//Y increment, X increment

	Epaper_Write_Command(0x3C); //BorderWavefrom
	Epaper_Write_Data(0x00);	

	Epaper_Write_Command(0x21); //  Display update control
	Epaper_Write_Data(0x00);		
  Epaper_Write_Data(0x80);		//Available Source from S8 to S167		

	Epaper_Write_Command(0x2C);     //VCOM Voltage
	Epaper_Write_Data(LUT_DATA_4Gray[158]);    

	Epaper_Write_Command(0x3F); //EOPQ    
	Epaper_Write_Data(LUT_DATA_4Gray[153]);
	
	Epaper_Write_Command(0x03); //VGH      
	Epaper_Write_Data(LUT_DATA_4Gray[154]);

	Epaper_Write_Command(0x04); //      
	Epaper_Write_Data(LUT_DATA_4Gray[155]); //VSH1   
	Epaper_Write_Data(LUT_DATA_4Gray[156]); //VSH2   
	Epaper_Write_Data(LUT_DATA_4Gray[157]); //VSL   
   
	EPD_select_LUT((unsigned char *)LUT_DATA_4Gray); //LUT
	
	Epaper_Write_Command(0x44); //set Ram-X address start/end position   
	Epaper_Write_Data(XStart);		//0x00-->0			
	Epaper_Write_Data(XEnd);    //0x16-->(22+1)*8=184		

	Epaper_Write_Command(0x45); //set Ram-Y address start/end position          
	Epaper_Write_Data(YStart_L);   	
	Epaper_Write_Data(YStart_H);		//0x017F-->(383+1)=384
	Epaper_Write_Data(YEnd_L);		
	Epaper_Write_Data(YEnd_H); 
	
	Epaper_Write_Command(0x4E);   // set RAM x address count
	Epaper_Write_Data(XStart);
	Epaper_Write_Command(0x4F);   // set RAM y address count
	Epaper_Write_Data(YStart_L);
	Epaper_Write_Data(YStart_H);
	Epaper_READBUSY();
	
}



u8 In2bytes_Out1byte_RAM1(u8 data1,u8 data2)
{
  u32 i; 
	u8 TempData1,TempData2;
	u8 outdata=0x00;
	TempData1=data1;
	TempData2=data2;
	
    for(i=0;i<4;i++)
     { 
        outdata=outdata<<1;
        if( ((TempData1&0xC0)==0xC0) || ((TempData1&0xC0)==0x40))
           outdata=outdata|0x01;
        else 
          outdata=outdata|0x00;

        TempData1=TempData1<<2;

     }

    for(i=0;i<4;i++)
     { 
        outdata=outdata<<1;
         if((TempData2&0xC0)==0xC0||(TempData2&0xC0)==0x40)
           outdata=outdata|0x01;
        else 
          outdata=outdata|0x00;

        TempData2=TempData2<<2;

     }
		 return outdata;
}
u8 In2bytes_Out1byte_RAM2(u8 data1,u8 data2)
{
  u32 i; 
  u8 TempData1,TempData2;
	u8 outdata=0x00;
TempData1=data1;
TempData2=data2;
	
    for(i=0;i<4;i++)
     { 
        outdata=outdata<<1;
        if( ((TempData1&0xC0)==0xC0) || ((TempData1&0xC0)==0x80))
           outdata=outdata|0x01;
        else 
          outdata=outdata|0x00;

        TempData1=TempData1<<2;

     }

    for(i=0;i<4;i++)
     { 
        outdata=outdata<<1;
         if((TempData2&0xC0)==0xC0||(TempData2&0xC0)==0x80)
           outdata=outdata|0x01;
        else 
          outdata=outdata|0x00;

        TempData2=TempData2<<2;

     }
		 return outdata;
}


void EPD_WhiteScreen_ALL_4GRAY(const unsigned char *datas)
{
   unsigned int i;
	 u8 tempOriginal;   
	
	
    Epaper_Write_Command(0x24);   //write RAM for black(0)/white (1)
   for(i=0;i<ALLSCREEN_GRAGHBYTES*2;i+=2)
   {               
		tempOriginal= In2bytes_Out1byte_RAM1( *(datas+i),*(datas+i+1));
		 Epaper_Write_Data(~tempOriginal); 
   }
	 
	 Epaper_Write_Command(0x26);   //write RAM for black(0)/white (1)
   for(i=0;i<ALLSCREEN_GRAGHBYTES*2;i+=2)
   {               
		tempOriginal= In2bytes_Out1byte_RAM2( *(datas+i),*(datas+i+1));
		 Epaper_Write_Data(~tempOriginal); 
   }
	 
   EPD_Update_4GRAY_and_DeepSleep();	 
}



void EPD_Update_4GRAY_and_DeepSleep(void)
{   
  Epaper_Write_Command(0x22); 
  Epaper_Write_Data(0xC7);   
  Epaper_Write_Command(0x20); 
  Epaper_READBUSY();  
	
  Epaper_Write_Command(0x10); //enter deep sleep
  Epaper_Write_Data(0x01); 
  driver_delay_xms(100);	

}





/***************************无灰阶初始化（FPC朝左边显示）************************************/

void EPD_HW_Init(void)
{
	int XStart,XEnd,YStart_L,YStart_H,YEnd_L,YEnd_H;	

	XStart=0x00;
	XEnd=(EPD_HEIGHT/8-1);

	YStart_L=(EPD_WIDTH-1)%256;
	YStart_H=(EPD_WIDTH-1)/256;
	YEnd_L=0x00;
	YEnd_H=0x00;
	
	EPD_W21_Init();							//hard reset 

	Epaper_READBUSY();
	Epaper_Write_Command(0x12); // soft reset
	Epaper_READBUSY();

	Epaper_Write_Command(0x01); //Driver output control      
	Epaper_Write_Data((EPD_WIDTH-1)%256);
	Epaper_Write_Data((EPD_WIDTH-1)/256);
	Epaper_Write_Data(0x00);

	Epaper_Write_Command(0x11); //data entry mode       
	Epaper_Write_Data(0x01);		//Y decrement, X increment
	
	Epaper_Write_Command(0x3C); //BorderWavefrom
	Epaper_Write_Data(0x01);	

	Epaper_Write_Command(0x21); //  Display update control
	Epaper_Write_Data(0x00);		
	Epaper_Write_Data(0x80);		//Available Source from S8 to S167	
	
	Epaper_Write_Command(0x18); //Temperature Sensor Selection
	Epaper_Write_Data(0x80);	  //Internal temperature sensor
	
	Epaper_Write_Command(0x44); //set Ram-X address start/end position   
	Epaper_Write_Data(XStart);	//0x00-->0			
	Epaper_Write_Data(XEnd);    //0x16-->(22+1)*8=184		

	Epaper_Write_Command(0x45); //set Ram-Y address start/end position          
	Epaper_Write_Data(YStart_L);   	
	Epaper_Write_Data(YStart_H);		
	Epaper_Write_Data(YEnd_L);		//0x017F-->(383+1)=384
	Epaper_Write_Data(YEnd_H); 
	
	Epaper_Write_Command(0x4E);   // set RAM x address count
	Epaper_Write_Data(XStart);
	Epaper_Write_Command(0x4F);   // set RAM y address count
	Epaper_Write_Data(YStart_L);
	Epaper_Write_Data(YStart_H);
	Epaper_READBUSY();
	
}


void EPD_W21_Init(void)
{
	EPD_W21_RST_0;     
	driver_delay_xms(100); 
	EPD_W21_RST_1; 							//hard reset  
	driver_delay_xms(100); 
}


//////////////////////////////All screen update////////////////////////////////////////////
void EPD_WhiteScreen_ALL(const unsigned char *datas)
{
   unsigned int i;
    Epaper_Write_Command(0x24);   //write RAM for black(0)/white (1)
   for(i=0;i<ALLSCREEN_GRAGHBYTES;i++)
   {               
     Epaper_Write_Data(*datas);
			datas++;
   }
   EPD_Update_and_DeepSleep();	 
}

/////////////////////////////////////////////////////////////////////////////////////////
void EPD_Update_and_DeepSleep(void)
{   
  Epaper_Write_Command(0x22); 
  Epaper_Write_Data(0xF7);   
  Epaper_Write_Command(0x20); 
  Epaper_READBUSY();   
	
  Epaper_Write_Command(0x10); //enter deep sleep
  Epaper_Write_Data(0x01); 
  driver_delay_xms(100);
}


void EPD_Part_Update_and_DeepSleep(void)
{
	Epaper_Write_Command(0x22); 
	Epaper_Write_Data(0xFF);   
	Epaper_Write_Command(0x20); 
	Epaper_READBUSY(); 	

  Epaper_Write_Command(0x10); //enter deep sleep
  Epaper_Write_Data(0x01); 
  driver_delay_xms(50);	
}




void EPD_SetRAMValue_BaseMap( const unsigned char * datas)
{
	unsigned int i;   
	const unsigned char  *datas_flag;   
	datas_flag=datas;
  Epaper_Write_Command(0x24);   //Write Black and White image to RAM
   for(i=0;i<ALLSCREEN_GRAGHBYTES;i++)
   {               
     Epaper_Write_Data(*datas);
			datas++;
   }
	 datas=datas_flag;
  Epaper_Write_Command(0x26);   //Write Black and White image to RAM
   for(i=0;i<ALLSCREEN_GRAGHBYTES;i++)
   {               
     Epaper_Write_Data(*datas);
			datas++;
   }
   EPD_Update_and_DeepSleep();		 
	 
}


///////////////////////////Part update//////////////////////////////////////////////
//The x axis is reduced by one byte, and the y axis is reduced by one pixel.

//mode==POS , 正显；
//mode==NEG , 负显；
//mode==OFF , 清除；
//v和PART_HEIGHT 必须是8的整数倍。
void EPD_Dis_Part(int h_start,int v_start,const unsigned char * datas,int PART_WIDTH,int PART_HEIGHT,unsigned char mode)
{
	int i;  
	int vend,hstart_H,hstart_L,hend,hend_H,hend_L;
	
	v_start=v_start/8;						//转换成字节
	vend=v_start+PART_HEIGHT/8-1; 
	
	h_start=295-h_start;
		hstart_H=h_start/256;
		hstart_L=h_start%256;

	hend=h_start-PART_WIDTH+1;
		hend_H=hend/256;
		hend_L=hend%256;		
	
	Epaper_Write_Command(0x44);       // set RAM x address start/end
	Epaper_Write_Data(v_start);    		// RAM x address start;
	Epaper_Write_Data(vend);    			// RAM x address end
	Epaper_Write_Command(0x45);       // set RAM y address start/end
	Epaper_Write_Data(hstart_L);    	// RAM y address start Low
	Epaper_Write_Data(hstart_H);    	// RAM y address start High
	Epaper_Write_Data(hend_L);    		// RAM y address end Low
	Epaper_Write_Data(hend_H);    		// RAM y address end High


	Epaper_Write_Command(0x4E);   		// set RAM x address count
	Epaper_Write_Data(v_start); 
	Epaper_Write_Command(0x4F);   		// set RAM y address count
	Epaper_Write_Data(hstart_L);
	Epaper_Write_Data(hstart_H);
	Epaper_READBUSY();	
	
	 Epaper_Write_Command(0x24);   //Write Black and White image to RAM
	
		for(i=0;i<PART_WIDTH*PART_HEIGHT/8;i++)
			{                         
				if (mode==POS)
					{
						Epaper_Write_Data(*datas);
						datas++;
					}

				if (mode==NEG)
					{
						Epaper_Write_Data(~*datas);
						datas++;
					}	

			  if (mode==OFF)
				  {
						Epaper_Write_Data(0xFF);
					}		
				
			} 	
}


/////////////////////////////////Single display////////////////////////////////////////////////

void EPD_WhiteScreen_White(void)

{
   unsigned int k;
    Epaper_Write_Command(0x24);   //write RAM for black(0)/white (1)
		for(k=0;k<ALLSCREEN_GRAGHBYTES;k++)
		{
			Epaper_Write_Data(0xff);
		}
	
    Epaper_Write_Command(0x26);   //write RAM for black(0)/white (1)
		for(k=0;k<ALLSCREEN_GRAGHBYTES;k++)
		{
			Epaper_Write_Data(0xff);
		}	
		EPD_Update_and_DeepSleep();
}


//////////////////////////////////////////////////////////////////////////////////////
