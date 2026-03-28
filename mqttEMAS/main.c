#include <REGX52.H>
#include "Delay.h"
#include "ESP-01S.h"

void main()
{
    UART_Init();
    Delay_ms(2000); // 等待ESP启动

    // 初始化ESP-01S
    SendAT("ATE0");
    SendAT("AT+CWMODE=1");
    Delay_ms(2000);

    // 连接WiFi
    SendString("AT+CWJAP=\"OnePlus Ace 5\",\"1234567890\"\r\n");
    Delay_ms(5000);

    // 配置MQTT
    SendAT("AT+MQTTUSERCFG=0,1,\"client\",\"\",\"\",0,0,\"\"");
    Delay_ms(2000);

    // 连接MQTT服务器
    SendString("AT+MQTTCONN=0,\"82.157.129.239\",1883,0\r\n");
    Delay_ms(3000);

    while (1)
    {
        SendData();
        Delay_ms(3000);
    }
}
