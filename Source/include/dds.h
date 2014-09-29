#ifndef DDS_H_

    #define DDS_H_

    #include "hal.h"

	#ifndef DDS_CLK
    	#define DDS_CLK 75000000
	#endif /* DDS_CLK */

    #define DDS_CTL_B28     (1<<13)
    #define DDS_CTL_HLB     (1<<12)
    #define DDS_CTL_FSEL    (1<<11)
    #define DDS_CTL_PSEL    (1<<10)
    #define DDS_CTL_PINSW   (1<<9)
    #define DDS_CTL_RESET   (1<<8)
    #define DDS_CTL_SLEEP1  (1<<7)
	#define DDS_CTL_SLEEP12 (1<<6)
	#define DDS_CTL_OPBITEN (1<<5)
	#define DDS_CTL_SIGNPIB (1<<4)
	#define DDS_CTL_DIV2    (1<<3)
	#define DDS_CTL_MODE    (1<<1)

	#define DDS_REG_FREQ0   (1<<14)
	#define DDS_REG_FREQ1   (1<<15)

	#define DDS_REG_PHASE0  ((1<<15)|(1<<14))
	#define DDS_REG_PHASE1  ((1<<15)|(1<<14)|(1<<13))

	#define DDS_FREQ_LSB_MASK  0b00000000000000000011111111111111
	#define DDS_FREQ_LSB(freq) (uint16_t)(freq & DDS_FREQ_LSB_MASK)

	#define DDS_FREQ_MSB_MASK  0b00001111111111111100000000000000
	#define DDS_FREQ_MSB(freq) (uint16_t)((freq & DDS_FREQ_MSB_MASK) >> 14)

	#define DDS_FREQ_HZ(freq)  (uint64_t)(freq << 28) / DDS_CLK

	void ddsInit(void);
	void ddsFreq(uint32_t);

#endif /* DDS_H_ */
