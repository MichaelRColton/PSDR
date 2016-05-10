/* hal.h
 */

#ifndef HAL_H_
#define HAL_H_

#include <main.h>
#include <stdint.h>
#include <stdbool.h>
//#include <stm32f10x.h>
//#include <stm32f415xx.h>
#ifdef PSDR2
#include "stm32f429xx.h"
#include <stm32f4xx_hal_gpio.h>
#include "stm32f4xx_hal_dma.h"
#include "stm32f4xx_hal_usart.h"
#include "stm32f4xx_hal_i2c.h"
#endif

//    #include <stm32f10x_gpio.h>

#ifdef PSDR257
#include "stm32f746xx.h"
#include <stm32f7xx_hal_gpio.h>
#include "stm32f7xx_hal_dma.h"
#include "stm32f7xx_hal_usart.h"
#include "stm32f7xx_hal_i2c.h"
#endif

//#include <uart.h>

typedef struct _Gpio_Pin
{
  GPIO_TypeDef* port;
  uint16_t pin;
} Gpio_Pin;

// a timer with which channel of the timer specified
typedef struct _Timer_Channel
{
  uint8_t channel;
  TIM_TypeDef* timer;
} Timer_Channel;

// a Timer_Pin is a gpio pin that is connected to an internal timer
typedef struct _Timer_Pin
{
  Gpio_Pin gpioPin;
  Timer_Channel timer;
} Timer_Pin;

typedef void
(*hal_sysTickCallback) (void);

// hal wrappers for GPIO
// gpioPin must be a Gpio_Pin struct
// val must be either a 0 or a 1
#define hal_writeGpio(gpioPin, val) (GPIO_WriteBit((gpioPin).port, (gpioPin).pin, (val)))   // returns void
#define hal_readGpio(gpioPin)       (GPIO_ReadInputDataBit((gpioPin).port, (gpioPin).pin))  // returns uint8_t

//    // power macros
//    #define hal_isPlugged(powerState)       (!((powerState) & hal_externalPower))

__IO uint32_t timingDelay;

#define MEDIATEK_MODULE
#define hal_gpsUart USART1

// gpio pins
//    extern const Gpio_Pin RX_TO_GSM;
//    extern const Gpio_Pin TX_FROM_GSM;
//    extern const Gpio_Pin FINGER_PRINT_POWER;
//    extern const Gpio_Pin USER_BUTTON;
//    extern const Gpio_Pin FPR_RX;
//    extern const Gpio_Pin FPR_TX;
//    extern const Gpio_Pin BUZZER;
//    extern const Gpio_Pin RFID_INT;
//    extern const Gpio_Pin FINGER_PRINT_BUTTON;
extern const Gpio_Pin LCD_NSS;
//    extern const Gpio_Pin LOW_BAT;
extern const Gpio_Pin RX_TO_GPS;
extern const Gpio_Pin TX_FROM_GPS;
extern const Gpio_Pin GPS_RESET;
//extern const Gpio_Pin GPS_FIX_LED;
extern const Gpio_Pin GPS_PPS;
extern const Gpio_Pin GPS_POWER;
extern const Gpio_Pin LCD_SCK;
extern const Gpio_Pin LCD_MISO;
extern const Gpio_Pin LCD_MOSI;
extern const Gpio_Pin SPI2_SCK;
extern const Gpio_Pin SPI2_MISO;
extern const Gpio_Pin SPI2_MOSI;
extern const Gpio_Pin LCD_RESET;
extern const Gpio_Pin LCD_DC;
extern const Gpio_Pin RX_Q;
extern const Gpio_Pin RX_I;
//    extern const Gpio_Pin OLED_RESET;
//    extern const Gpio_Pin OLED_DC;
//    extern const Gpio_Pin OLED_NSS;

//CORRECT FOR PSDR1
//extern const Gpio_Pin ddsReset;
//extern const Gpio_Pin ddsSleep;
//extern const Gpio_Pin dds1Mosi;
//extern const Gpio_Pin dds1Nss;
//extern const Gpio_Pin dds1Sck;
//extern const Gpio_Pin dds2Mosi;
//extern const Gpio_Pin dds2Nss;
//extern const Gpio_Pin dds2Sck;
extern const Gpio_Pin encoderBee;
extern const Gpio_Pin encoderB;
extern const Gpio_Pin encoderP;
extern const Gpio_Pin dac1;
extern const Gpio_Pin dac2;

//extern const Gpio_Pin REF_CLOCK_ENABLE;
//extern const Gpio_Pin DDS_FSEL;
//extern const Gpio_Pin DDS_PSEL;
//extern const Gpio_Pin RX_MUX;
//extern const Gpio_Pin TX_MUX;
extern const Gpio_Pin AMP_SWITCH_A;
extern const Gpio_Pin AMP_SWITCH_B;
extern const Gpio_Pin IMP_BRIDGE_SWITCH_A;
extern const Gpio_Pin IMP_BRIDGE_SWITCH_B;
//extern const Gpio_Pin MIXER_SWITCH_A;
//extern const Gpio_Pin MIXER_SWITCH_B;
extern const Gpio_Pin TX_RF_SWITCH_A;
extern const Gpio_Pin TX_RF_SWITCH_B;

extern const Gpio_Pin AMP_POWER;
//extern const Gpio_Pin MIXER_POWER;

extern const Gpio_Pin FILTER_GAIN_POT_SCLK;
extern const Gpio_Pin FILTER_GAIN_POT_MOSI;
extern const Gpio_Pin GAIN_POT_NSS;

extern const Gpio_Pin IN_AMP_ENABLE;
extern const Gpio_Pin DAC_MUX;

extern const Gpio_Pin AUDIO_AMP_NSHTDWN;
extern const Gpio_Pin EARPHONE_NOT_INSERTED;
extern const Gpio_Pin SIDETONE; //Should be a timer pin.

extern const Gpio_Pin MIC_IN;
extern const Gpio_Pin MIC_SWITCH;
extern const Gpio_Pin PREAMP_POWER;
extern const Gpio_Pin MIC_BUTTON;

extern const Gpio_Pin PADDLE_THUMB_NO;
extern const Gpio_Pin PADDLE_INDEX_NO;
extern const Gpio_Pin PADDLE_THUMB_NC;
extern const Gpio_Pin PADDLE_INDEX_NC;

extern const Gpio_Pin FILTER_S0;
extern const Gpio_Pin FILTER_S1;

extern const Gpio_Pin TRX_SWITCH;

extern const Gpio_Pin I2C_SCL;
extern const Gpio_Pin I2C_SDA;

extern const Gpio_Pin FLIP_FLOP_ENABLE;

extern const Gpio_Pin RED_LED;
extern const Gpio_Pin FIVE_VOLT_REGULATOR_ENABLE;

//    extern const Gpio_Pin NC_1;
//    extern const Gpio_Pin DAC_SWITCHES;
//    extern const Gpio_Pin GSM_PWRKEY;
//    extern const Gpio_Pin GSM_STATUS;
//    extern const Gpio_Pin GSM_NRST;
//    extern const Gpio_Pin SERVO_PWR;
//    extern const Gpio_Pin CHARGE_STATUS2;
//    extern const Gpio_Pin POWER_GOOD;
//    extern const Gpio_Pin POWER_SWITCH;
//    extern const Gpio_Pin ACCEL_NSS;

// timer pins
//    extern const Timer_Pin LED_G;
//    extern const Timer_Pin LED_R;
//    extern const Timer_Pin LED_B;
//    extern const Timer_Pin SERVO1;
//    extern const Timer_Pin SERVO2;

extern inline bool
hal_checkTimeout (uint32_t startTime_ms, uint32_t interval_ms);
// returns true if the interval has timed out

int
hal_acquireSemaphore (uint8_t sem);

void
hal_releaseSemaphore (uint8_t sem);

void
hal_getBatteryVoltage (float* battVoltage);
// reads power voltage level

void
hal_delay_ms (uint32_t ms);
// busy wait for ms milliseconds

void
hal_setupPins (void);
// Setup gpio pins and timer pins.

void
hal_timerSetCompare (const Timer_Channel* timer, uint16_t value);
// set the timer compare register for timer

void
hal_setupTimers (void);
// Setup TIM3 and TIM4 for controlling the LEDs and Servos

uint32_t
hal_getCurrentTime_ms (void);
// get the current system millisecond count

void
hal_setupUart1 (void);
// setup usart 1 and its pins

void
hal_setupUart2 (void);
// setup usart 2 and its pins

void
hal_setupUart3 (unsigned int baudRate);
// setup usart 3 and its pins

void
hal_resetUart1 (void);
// resets usart 1 and its pins

void
hal_resetUart2 (void);
// resets usart 2 and its pins

void
hal_resetUart3 (void);
// resets usart 3 and its pins

void
hal_adcConfigure (void);
// configure and enable used ADC(s)

uint16_t
hal_readAdc1 (uint8_t channel);
// read a value on a channel of ADC1
// returns value read

void
hal_setSysTickCallback (hal_sysTickCallback callback);

//void hal_blinkLed(uint8_t led, uint8_t blinkCount, uint16_t onTime, uint16_t offTime);
// blink the given LED blinkCount times

#endif /* HAL_H_ */
