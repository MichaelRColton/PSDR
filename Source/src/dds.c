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
    //delay

    int i;
    for(i = 0; i < 16; i++)
    {
        HAL_GPIO_WritePin(DDS_1_MOSI.port, DDS_1_MOSI.pin, (data1 >> (15-i)) & 1);
        HAL_GPIO_WritePin(DDS_2_MOSI.port, DDS_2_MOSI.pin, (data2 >> (15-i)) & 1);
        //delay
        HAL_GPIO_WritePin(DDS_1_SCK.port, DDS_1_SCK.pin, 0);
        HAL_GPIO_WritePin(DDS_2_SCK.port, DDS_2_SCK.pin, 0);
        //delay
        HAL_GPIO_WritePin(DDS_1_SCK.port, DDS_1_SCK.pin, 1);
        HAL_GPIO_WritePin(DDS_2_SCK.port, DDS_2_SCK.pin, 1);
    }

    //Suffix
    HAL_GPIO_WritePin(DDS_1_NSS.port, DDS_1_NSS.pin, 1);
    HAL_GPIO_WritePin(DDS_2_NSS.port, DDS_2_NSS.pin, 1);

}

void ddsInit()
{
	//Init the DDS chips to pin-based control
	HAL_GPIO_WritePin(DDS_RESET.port, DDS_RESET.pin, 0);
	ddsCmd(
		DDS_CTL_PINSW,
		DDS_CTL_PINSW
	);
	HAL_GPIO_WritePin(DDS_RESET.port, DDS_RESET.pin, 1);
}

void ddsFreq(uint32_t frequency)
{
	uint32_t freqReg = DDS_FREQ_HZ(frequency);

	//Assert DDS_RESET
	HAL_GPIO_WritePin(DDS_RESET.port, DDS_RESET.pin, 1);

	ddsCmd(
	    DDS_CTL_PINSW | DDS_CTL_B28,
	    DDS_CTL_PINSW | DDS_CTL_B28
	);

	//Frequency Register LSB
	ddsCmd(
	    DDS_REG_FREQ0 | DDS_FREQ_LSB(freqReg),
	    DDS_REG_FREQ0 | DDS_FREQ_LSB(freqReg)
	);

	//Frequency Register MSB
	ddsCmd(
		DDS_REG_FREQ0 | DDS_FREQ_MSB(freqReg),
		DDS_REG_FREQ0 | DDS_FREQ_MSB(freqReg)
	);

	//Phase Register
	ddsCmd(
		DDS_REG_PHASE0 | 0,
		DDS_REG_PHASE0 | 3072
	);

	//Deassert DDS_RESET
	HAL_GPIO_WritePin(DDS_RESET.port, DDS_RESET.pin, 0);
}
