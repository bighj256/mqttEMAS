#ifndef __ESP01S_H__
#define __ESP01S_H__

#include "DHT11.h"


void UART_Init();
void SendChar(unsigned char ch);
void SendAT(unsigned char *cmd);
// 发送温湿度数据
void SendData();
void SendString(unsigned char *str);

#endif
