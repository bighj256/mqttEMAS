#include "DHT11.h"
/*
硬件连接
DHT11   			STC89C52RC
VCC(3.3-5.5V)		5V
GND					GND
DATA				P2^0
*/

//引脚定义
sbit DHT11_DQ=P2^0;

uint8_t Humi_H,Humi_L,Temp_H,Temp_L;

void DHT11_Delay30us(void)
{
	unsigned char data i;

	i = 12;
	while (--i);
}

void DHT11_Delay40us(void)
{
	unsigned char data i;

//	_nop_();
	i = 17;
	while (--i);
}
/**
  * @brief  DHT11初始化
  * @param  无
  * @retval 1 初始化失败    0 初始化成功
  */
uint8_t DHT11_Init(void)
{
	DHT11_DQ=1;
	DHT11_DQ=0;
	Delay_ms(20);
	DHT11_DQ=1;
	DHT11_Delay30us();		//Delay 30us
	if(DHT11_DQ==0)
	{
		while(DHT11_DQ==0);			//等待响应信号完成（释放总线）
		while(DHT11_DQ==1);			//等待开始传输数据
	
		return 0;
	}
	return 1;
}


uint8_t DHT11_ReceiveByte(void)
{
	uint8_t i,Byte=0x00;
	for (i=0;i<8;i++) 
	{
		Byte<<=1; //Byte左移一位
		while(DHT11_DQ==0);	     //从1bit开始，低电平变高电平，等待低电平结束
		DHT11_Delay40us();
		if(DHT11_DQ==1)
		{
			Byte|=0x01;         //将最低位置1
		}
		while(DHT11_DQ==1);  //等待高电平结束
	}						    
	return Byte;

}

/**
  * @brief  DHT11获取温湿度数据        
  * @param  4个指针，用来返回DHT11获取到的四个数据。
  * @retval 返回值为uint8_t 0 表示数据接收成功    1 数据接收失败
  *			
  */
uint8_t DHT11_GetDate(uint8_t *Humi_H,uint8_t *Humi_L,uint8_t *Temp_H,uint8_t *Temp_L)
{
	uint8_t i,Buffer[5];
	if(DHT11_Init()==0)
	{
		
		for(i=0;i<5;i++)
		{
			Buffer[i]=DHT11_ReceiveByte();
			if(i==4)
                DHT11_Delay40us(); // Delay 40us 等待总线由上拉电阻拉高进入空闲状态
        }
		if(Buffer[0]+Buffer[1]+Buffer[2]+Buffer[3]==Buffer[4])
		{
			*Humi_H=Buffer[0];
			*Humi_L=Buffer[1];
			*Temp_H=Buffer[2];
			*Temp_L=Buffer[3];
            
            return 0;
        }
        else
            return 1;
    }
	else 
        return 1;
}

/*
代码示例
while(DHT11_Init()==1);

DHT11_GetDate(&Humi_H,&Humi_L,&Temp_H,&Temp_L);
*/
