#include "ESP-01S.h"

// 串口初始化
void UART_Init()
{
    TMOD |= 0x20;   // 00100000定时器1，模式2 8位自动重装定时器
    TH1 = 0xFD;     // 设定波特率 9600 bps @ 11.0592MHz
    TL1 = 0xFD;
    SCON = 0x50;    // 01010000串口控制寄存器 模式1 10位异步收发器，允许接收
    TR1 = 1;        // 启动定时器1
}

// 串口发送一个字符
void SendChar(unsigned char ch)
{
    SBUF = ch;
    // 串口每发送完一个完整的字符帧后，TI被单片机硬件置1，并向CPU发出中断请求
    while (!TI)
        ;
    TI = 0;       //取消中断申请
}

// 串口发送字符串
void SendString(unsigned char *str)
{
    while (*str)
    {
        SendChar(*str++);
    }
}

void SendData()
{
    uint8_t Humi_H, Humi_L, Temp_H, Temp_L;
    char temp_str[6], humi_str[6];

    // 获取DHT11数据
    if (DHT11_GetDate(&Humi_H, &Humi_L, &Temp_H, &Temp_L) == 0)
    {
        // 温度转换（0-50度）
        if (Temp_H < 10)
        {
            // 温度小于10度：格式"X.X"
            temp_str[0] = Temp_H + '0';
            temp_str[1] = '.';
            temp_str[2] = Temp_L + '0';
            temp_str[3] = '\0';
        }
        else
        {
            // 温度10-50度：格式"XX.X"
            temp_str[0] = (Temp_H / 10) + '0';
            temp_str[1] = (Temp_H % 10) + '0';
            temp_str[2] = '.';
            temp_str[3] = Temp_L + '0';
            temp_str[4] = '\0';
        }

        // 湿度转换（20-90%）
        if (Humi_H < 10)
        {
            humi_str[0] = Humi_H + '0';
            humi_str[1] = '.';
            humi_str[2] = Humi_L + '0';
            humi_str[3] = '\0';
        }
        else
        {
            humi_str[0] = (Humi_H / 10) + '0';
            humi_str[1] = (Humi_H % 10) + '0';
            humi_str[2] = '.';
            humi_str[3] = Humi_L + '0';
            humi_str[4] = '\0';
        }

        // 发送MQTT命令
        SendString("AT+MQTTPUB=0,\"sensor/data\",\"{\\\"id\\\":\\\"client\\\"\\,\\\"t\\\":");
        SendString(temp_str);
        SendString("\\,\\\"h\\\":");
        SendString(humi_str);
        SendString("}\",0,0\r\n");
    }
    else
    {
        // 读取失败时发送默认值或错误
        SendString("AT+MQTTPUB=0,\"sensor/data\",\"0.0\\,0.0\",0,0\r\n");
    }

    // SendString("AT+MQTTPUB=0,\"sensor/data\",\"{\\\"id\\\":\\\"client\\\"\\,\\\"t\\\":
    //     21
    //     \\,\\\"h\\\":
    //     54
    //     }\",0,0\r\n");

}

void SendAT(unsigned char *cmd)
{
    SendString(cmd);
    SendChar('\r');
    SendChar('\n');
    Delay_ms(1000);
}