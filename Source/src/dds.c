
#include "dds.h"

void ddsPrefix()
{
//    HAL_GPIO_WritePin(dds1Sck.port, dds1Sck.pin, 1);
//    HAL_GPIO_WritePin(dds2Sck.port, dds2Sck.pin, 1);
//    //delay
//    HAL_GPIO_WritePin(dds1Nss.port, dds1Nss.pin, 0);
//    HAL_GPIO_WritePin(dds2Nss.port, dds2Nss.pin, 0);
//    //Delay
}

void sendToDds(uint16_t data1, uint16_t data2)
{
//    int i;
//    for(i = 0; i < 16; i++)
//    {
//        HAL_GPIO_WritePin(dds1Mosi.port, dds1Mosi.pin, (data1 >> (15-i)) & 1);
//        HAL_GPIO_WritePin(dds2Mosi.port, dds2Mosi.pin, (data2 >> (15-i)) & 1);
//        //delay
//        HAL_GPIO_WritePin(dds1Sck.port, dds1Sck.pin, 0);
//        HAL_GPIO_WritePin(dds2Sck.port, dds2Sck.pin, 0);
//        //delay
//        HAL_GPIO_WritePin(dds1Sck.port, dds1Sck.pin, 1); //The 16th shift of this line is when execution occurs
//        HAL_GPIO_WritePin(dds2Sck.port, dds2Sck.pin, 1); //NOT on the release of NSS!!!! GAH!
//    }
}

void ddsSuffix()
{
//    HAL_GPIO_WritePin(dds1Nss.port, dds1Nss.pin, 1);
//    HAL_GPIO_WritePin(dds2Nss.port, dds2Nss.pin, 1);
//    //HAL_GPio
}

long long freqToReg(long long frequency)
{
	  //frequency = 6000000;
	  long long freqIn = 75000000;
	  long long bits28 = 268435456;
	  long long intStep = frequency * bits28;
	  return intStep / freqIn;
}


//void setFreq(long frequency)
//{
//	long long freg = freqToReg(frequency);
//
//
//	//HAL_GPIO_WritePin(ddsReset.port, ddsReset.pin, 1);
//	  ddsPrefix();
//	  //sendToDds(0x2100, 0x2100);
//	  //sendToDds(0x0010001100000000 , 0x0010001100000000);
//	  sendToDds(0b0010001000000000 , 0b0010001000000000);
//	  ddsSuffix();
//
//	  //HAL_GPIO_WritePin(ddsReset.port, ddsReset.pin, 0);
//	  //HAL_GPIO_WritePin(ddsReset.port, ddsReset.pin, 1);
//	  //sendToDds(0x50c7);
//	  //sendToDds(0x4000);
//	  ddsPrefix();
//	  sendToDds(((freg & 0b0011111111111111) | 0b0100000000000000),
//			  ((freg & 0b0011111111111111) | 0b0100000000000000));
//	  ddsSuffix();
//	  ddsPrefix();
//	  sendToDds(((freg >> 14 & 0b0011111111111111) | 0b0100000000000000),
//			  (((freg >> 14) & 0b0011111111111111) | 0b0100000000000000));
//	  ddsSuffix();
//	  ddsPrefix();
//	  //sendToDds(0xc000, 0xc000);
//	  sendToDds(0xc000, 0xc000 | 3072);
//	  ddsSuffix();
//	//  ddsPrefix();
//	//  sendToDds(0b1100000000000000);
//	//  ddsSuffix();
//
//
//
//	//  ddsPrefix();
//	//  sendToDds(0x2000, 0x2000);
//	//  ddsSuffix();
//
////	  HAL_GPIO_WritePin(ddsReset.port, ddsReset.pin, 0);
//}
