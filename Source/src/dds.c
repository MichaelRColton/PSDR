#include "dds.h"

void ddsCmd(uint16_t, uint16_t);

//Bitbang the SPI interface to the DDS chips, writing commands to both
//chips as close in time as possible
void ddsCmd(uint16_t data1, uint16_t data2)
{

	//Prefix
    HAL_GPIO_WritePin(DDS_1_SCK.port, DDS_1_SCK.pin, 1);
    HAL_GPIO_WritePin(DDS_2_SCK.port, DDS_2_SCK.pin, 1);
    //delay
    HAL_GPIO_WritePin(DDS_1_NSS.port, DDS_1_NSS.pin, 0);
    HAL_GPIO_WritePin(DDS_2_NSS.port, DDS_2_NSS.pin, 0);
    //Delay

    int i;
    for(i = 0; i < 16; i++)
    {
        HAL_GPIO_WritePin(DDS_1_MOSI.port, DDS_1_MOSI.pin, (data1 >> (15-i)) & 1);
        HAL_GPIO_WritePin(DDS_2_MOSI.port, DDS_2_MOSI.pin, (data2 >> (15-i)) & 1);
        //delay
        HAL_GPIO_WritePin(DDS_1_SCK.port, DDS_1_SCK.pin, 0);
        HAL_GPIO_WritePin(DDS_2_SCK.port, DDS_2_SCK.pin, 0);
        //delay
        HAL_GPIO_WritePin(DDS_1_SCK.port, DDS_1_SCK.pin, 1); //The 16th shift of this line is when execution occurs
        HAL_GPIO_WritePin(DDS_2_SCK.port, DDS_2_SCK.pin, 1); //NOT on the release of NSS!!!! GAH!
    }

    //Suffix
    HAL_GPIO_WritePin(DDS_1_NSS.port, DDS_1_NSS.pin, 1);
    HAL_GPIO_WritePin(DDS_2_NSS.port, DDS_2_NSS.pin, 1);

}

void setFreq(uint32_t frequency)
{
	uint32_t freqReg = (uint64_t)(frequency << 28) / DDS_CLK;

	HAL_GPIO_WritePin(DDS_RESET.port, DDS_RESET.pin, 1);

	//ddsCmd(0x2100, 0x2100);
	//ddsCmd(0x0010001100000000 , 0x0010001100000000);
	ddsCmd(
	    0b0010001000000000,
		0b0010001000000000
	);

	//FIXME why do we reassert reset?
	//HAL_GPIO_WritePin(DDS_RESET.port, DDS_RESET.pin, 0);
	HAL_GPIO_WritePin(DDS_RESET.port, DDS_RESET.pin, 1);
	//ddsCmd(0x50c7);
	//ddsCmd(0x4000);

	//Frequency Register LSB
	ddsCmd(
	    (uint16_t)((freqReg & 0b00000000000000000011111111111111) | 0b00000000000000000100000000000000),
	    (uint16_t)((freqReg & 0b00000000000000000011111111111111) | 0b00000000000000000100000000000000)
	);

	//Frequency Register MSB
	ddsCmd(
		(uint16_t)(((freqReg & 0b00001111111111111100000000000000) | 0b00000000000000000100000000000000) >> 14),
		(uint16_t)(((freqReg & 0b00001111111111111100000000000000) | 0b00000000000000000100000000000000) >> 14)
	);

	//Phase Register
	//ddsCmd(0xc000, 0xc000);
	ddsCmd(
		0xc000,
		0xc000 | 3072
	);
	//ddsCmd(0b1100000000000000);

	//Exit Reset
	//ddsCmd(0x2000, 0x2000);

	HAL_GPIO_WritePin(DDS_RESET.port, DDS_RESET.pin, 0);
}
