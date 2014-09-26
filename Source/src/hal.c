/* hal.c
 *
 */

#include "hal.h"
//#include <stm32f10x_tim.h>
//#include <stm32f10x_adc.h>
//#include <stm32f4xx_hal_adc.h>


//
//enum
//{
//    adcChannel_powerLevel        =   11,
//    adcChannel_switches          =   15,
//    adcChannel_intRefVoltage     =   17,
//    adcChannel_temperatureSensor =   16,
//    openServoDelay_ms            =  800,     // how many ms to wait after opening servos before closing them
//    adcMax                       = 4095,
//};
//
//static const float adcRefVoltage      = 3.3;
//static const float adcVoltsPerValue   = 3.3 / adcMax;
//static const float idealIntRefVoltage = 1.2;

//#define debounceThreshold 40 //40 is too low, or my code sucks.... never mind, it was a hardware issue!

// gpio pins (most are named as on schematic with some noted exceptions)
//const Gpio_Pin RX_TO_GSM            = { GPIOA, GPIO_Pin_2  };   // tx/rx view from gsm
//const Gpio_Pin TX_FROM_GSM          = { GPIOA, GPIO_Pin_3  };
//const Gpio_Pin FINGER_PRINT_POWER   = { GPIOA, GPIO_Pin_4  };   // currently labeled BUZZER on schem. (TODO)
//const Gpio_Pin USER_BUTTON          = { GPIOA, GPIO_Pin_5  };   // id button
//const Gpio_Pin FPR_RX               = { GPIOA, GPIO_Pin_9  };   // tx/rx view from fpr
//const Gpio_Pin FPR_TX               = { GPIOA, GPIO_Pin_10 };
//const Gpio_Pin BUZZER               = { GPIOA, GPIO_Pin_11 };   // currently labeld EXTRA on schem (TODO)
//const Gpio_Pin RFID_INT             = { GPIOB, GPIO_Pin_5  };
//const Gpio_Pin FINGER_PRINT_BUTTON  = { GPIOB, GPIO_Pin_6  };

//const Gpio_Pin LOW_BAT              = { GPIOB, GPIO_Pin_8  };
//const Gpio_Pin RX_TO_GPS            = { GPIOB, GPIO_Pin_10 };
//const Gpio_Pin TX_FROM_GPS          = { GPIOB, GPIO_Pin_11 };

//const Gpio_Pin OLED_RESET			= { GPIOA, GPIO_PIN_5 };
//const Gpio_Pin OLED_DC				= { GPIOA, GPIO_PIN_9 };
//const Gpio_Pin OLED_NSS				= { GPIOA, GPIO_PIN_8 };

//CORRECT FOR PSDR1
const Gpio_Pin LCD_MOSI				= { GPIOB, GPIO_PIN_5  };
const Gpio_Pin LCD_MISO				= { GPIOB, GPIO_PIN_4  };
const Gpio_Pin LCD_SCK				= { GPIOB, GPIO_PIN_3  };
const Gpio_Pin LCD_NSS              = { GPIOA, GPIO_PIN_15 };
const Gpio_Pin LCD_LED				= { GPIOB, GPIO_PIN_1  };
const Gpio_Pin LCD_DC				= { GPIOB, GPIO_PIN_11 };
const Gpio_Pin LCD_RESET			= { GPIOB, GPIO_PIN_10 };
const Gpio_Pin SD_SCK				= { GPIOB, GPIO_PIN_13 };
const Gpio_Pin SD_MISO				= { GPIOB, GPIO_PIN_14 };
const Gpio_Pin SD_MOSI				= { GPIOB, GPIO_PIN_15 };
const Gpio_Pin SD_NSS				= { GPIOB, GPIO_PIN_12 };
const Gpio_Pin DDS_RESET 			= { GPIOC, GPIO_PIN_12 };
const Gpio_Pin DDS_SLEEP 			= { GPIOD, GPIO_PIN_2  };
const Gpio_Pin DDS_1_MOSI 			= { GPIOC, GPIO_PIN_11 };
const Gpio_Pin DDS_1_NSS 			= { GPIOC, GPIO_PIN_0  };
const Gpio_Pin DDS_1_SCK 			= { GPIOC, GPIO_PIN_10 };
const Gpio_Pin DDS_2_MOSI 			= { GPIOB, GPIO_PIN_6  };
const Gpio_Pin DDS_2_NSS 			= { GPIOB, GPIO_PIN_8  };
const Gpio_Pin DDS_2_SCK 			= { GPIOB, GPIO_PIN_7  };
const Gpio_Pin ENCODER_A			= { GPIOC, GPIO_PIN_2  };
const Gpio_Pin ENCODER_B			= { GPIOC, GPIO_PIN_3  };
const Gpio_Pin ENCODER_P			= { GPIOC, GPIO_PIN_15 };
const Gpio_Pin ADC_0				= { GPIOA, GPIO_PIN_0  };
const Gpio_Pin ADC_1				= { GPIOA, GPIO_PIN_1  };
const Gpio_Pin ADC_2				= { GPIOA, GPIO_PIN_2  };
const Gpio_Pin DAC_1				= { GPIOA, GPIO_PIN_4  };
const Gpio_Pin DAC_2				= { GPIOA, GPIO_PIN_5  };
const Gpio_Pin TX_MUX				= { GPIOC, GPIO_PIN_5  };
const Gpio_Pin RX_MUX				= { GPIOB, GPIO_PIN_0  };

//const Gpio_Pin NC_1                 = { GPIOC, GPIO_Pin_0  };	// this is the Closure Sensor Pin near the 3v3 regulator, fyi
//const Gpio_Pin DAC_SWITCHES         = { GPIOC, GPIO_Pin_5  };   // currently labeled LIGHT_SENSOR on schem (TODO)
//const Gpio_Pin GSM_PWRKEY           = { GPIOC, GPIO_Pin_8  };
//const Gpio_Pin GSM_NRST             = { GPIOA, GPIO_Pin_12 };
//const Gpio_Pin GSM_STATUS           = { GPIOC, GPIO_Pin_9  };
//const Gpio_Pin SERVO_PWR            = { GPIOC, GPIO_Pin_10 };
//const Gpio_Pin CHARGE_STATUS2       = { GPIOC, GPIO_Pin_13 };
//const Gpio_Pin POWER_GOOD           = { GPIOC, GPIO_Pin_14 };
//const Gpio_Pin POWER_SWITCH         = { GPIOC, GPIO_Pin_15 };
//const Gpio_Pin ACCEL_NSS            = { GPIOD, GPIO_Pin_2  };   // currently labeled SPI2_NSS on schem
//const Gpio_Pin GPS_RESET			= { GPIOC, GPIO_Pin_4  };
//const Gpio_Pin GPS_FIX_LED 			= { GPIOC, GPIO_Pin_3  };
//const Gpio_Pin GPS_PPS				= { GPIOC, GPIO_Pin_2  };

//// timer pins
//const Timer_Pin LED_G =
//{
//    { GPIOB, GPIO_Pin_1 },
//    { 4, TIM3 }
//};
//const Timer_Pin LED_R =
//{
//    { GPIOB, GPIO_Pin_0 },
//    { 3, TIM3 }
//};
//const Timer_Pin LED_B =
//{
//    { GPIOA, GPIO_Pin_7 },
//    { 2, TIM3 }
//};
//const Timer_Pin SERVO1 =
//{
//    { GPIOA, GPIO_Pin_6 },
//    { 1, TIM3 }
//};
//const Timer_Pin SERVO2 =
//{
//    { GPIOB, GPIO_Pin_9 },
//    { 4, TIM4 }
//};

static uint32_t halMilliseconds;
//static uint32_t timingDelay;

static hal_sysTickCallback sysTickCallback = 0; //Was NULL, but NULL isn't defined? Where is it defined?









void hal_setSysTickCallback(hal_sysTickCallback callback)
{
    sysTickCallback = callback;
}

//void SysTick_Handler(void)
//{
//    // this is the system timer tick ISR
//    // WARNING: This function may be called at ANY TIME
//    // Make sure that everything this function does cannot possibly mess with other functionality
//    // For instance, currently I do not allow it to access the SPI2 bus for accelerometer if RFID in use
//    // Same goes for the sysTickCallback function
//
//    halMilliseconds++;
//
//    if (timingDelay != 0)
//        --timingDelay;
//
//    if (sysTickCallback)
//        sysTickCallback();
//}

uint32_t hal_getCurrentTime_ms(void)
{
    // get the current system millisecond count

    return halMilliseconds;
}

void hal_delay_ms(uint32_t ms)
{
    // busy wait for ms milliseconds

	////TEMP
	//int delay, extra;
	//for(delay = 0; delay < ms; delay++)
	//	for(extra = 0; extra < 1000; extra++);

    timingDelay = ms;
    while (timingDelay)
    	;
}


void hal_setupPins(void)
{
    // Setup gpio pins and timer pins.
    // Peripheral specific pins are set up in their setup functions.
    // Since GPIO_Init takes a pointer to a GPIO_InitTyepDef (not const), to be safe, we re-set all fields
    // of that struct.

    //TODO: Are the SPI Enable lines not here?! WTF?

    GPIO_InitTypeDef gpioInitStructure;

    // Buzzer
    // The buzzer starts ON when board powers up
    // For some reason, setting buzzer to input pull-down is the only way to make it quiet initially
//    gpioInitStructure.GPIO_Pin   = BUZZER.pin;
//    gpioInitStructure.GPIO_Speed = GPIO_Speed_2MHz;
//    gpioInitStructure.GPIO_Mode  = GPIO_Mode_IPD;
//    GPIO_Init(BUZZER.port, &gpioInitStructure);

    //hal_delay_ms(1); // Make sure the buzzer has quieted down

    // Now that the buzzer has quieted down, we can use it as an output
    gpioInitStructure.Pin   = DAC_1.pin;
    gpioInitStructure.Speed = GPIO_SPEED_FAST;
    gpioInitStructure.Mode  = GPIO_MODE_AF_PP;
    gpioInitStructure.Pull  = GPIO_PULLDOWN;
    gpioInitStructure.Alternate = 1;
    HAL_GPIO_Init(DAC_1.port, &gpioInitStructure);
    //GPIO_WriteBit(DAC1.port, DAC1.pin, 0);

    gpioInitStructure.Pin   = DAC_2.pin;
    gpioInitStructure.Speed = GPIO_SPEED_FAST;
    gpioInitStructure.Mode  = GPIO_MODE_AF_PP;
    gpioInitStructure.Pull  = GPIO_PULLDOWN;
    gpioInitStructure.Alternate = 1;
    HAL_GPIO_Init(DAC_2.port, &gpioInitStructure);
    //GPIO_WriteBit(DAC1.port, DAC1.pin, 0);

    gpioInitStructure.Pin   = LCD_LED.pin;
    gpioInitStructure.Speed = GPIO_SPEED_FAST;
    gpioInitStructure.Mode  = GPIO_MODE_OUTPUT_PP;
    gpioInitStructure.Alternate = 0;
    gpioInitStructure.Pull  = GPIO_PULLUP;
    HAL_GPIO_Init(LCD_LED.port, &gpioInitStructure);

    gpioInitStructure.Pin   = LCD_DC.pin;
    gpioInitStructure.Speed = GPIO_SPEED_FAST;
    gpioInitStructure.Mode  = GPIO_MODE_OUTPUT_PP;
    gpioInitStructure.Alternate = 0;
    gpioInitStructure.Pull  = GPIO_NOPULL;
    HAL_GPIO_Init(LCD_DC.port, &gpioInitStructure);

    gpioInitStructure.Pin   = TX_MUX.pin;
    gpioInitStructure.Speed = GPIO_SPEED_FAST;
    gpioInitStructure.Mode  = GPIO_MODE_OUTPUT_PP;
    gpioInitStructure.Alternate = 0;
    gpioInitStructure.Pull  = GPIO_PULLUP;
    HAL_GPIO_Init(TX_MUX.port, &gpioInitStructure);

    gpioInitStructure.Pin   = RX_MUX.pin;
    gpioInitStructure.Speed = GPIO_SPEED_FAST;
    gpioInitStructure.Mode  = GPIO_MODE_OUTPUT_PP;
    gpioInitStructure.Alternate = 0;
    gpioInitStructure.Pull  = GPIO_PULLUP;
    HAL_GPIO_Init(RX_MUX.port, &gpioInitStructure);

//    gpioInitStructure.GPIO_Pin   = LCD_NSS.pin;
//    gpioInitStructure.GPIO_Speed = GPIO_Speed_2MHz;
//    gpioInitStructure.GPIO_Mode  = GPIO_Mode_Out_PP;
//    GPIO_Init(LCD_DC.port, &gpioInitStructure);

    gpioInitStructure.Pin   = LCD_RESET.pin;
    gpioInitStructure.Speed = GPIO_SPEED_FAST;
    gpioInitStructure.Mode  = GPIO_MODE_OUTPUT_PP;
    gpioInitStructure.Alternate = 0;
    gpioInitStructure.Pull  = GPIO_NOPULL;
    HAL_GPIO_Init(LCD_RESET.port, &gpioInitStructure);


    // 'DAC' switches
//    gpioInitStructure.GPIO_Pin   = DAC_SWITCHES.pin;
//    gpioInitStructure.GPIO_Speed = GPIO_Speed_2MHz;
//    gpioInitStructure.GPIO_Mode  = GPIO_Mode_AIN;
//    GPIO_Init(DAC_SWITCHES.port, &gpioInitStructure);

    // 'DAC' switches
    gpioInitStructure.Pin   = ADC_0.pin;
    gpioInitStructure.Speed = GPIO_SPEED_FAST;
    gpioInitStructure.Mode  = GPIO_MODE_ANALOG;
    gpioInitStructure.Pull  = GPIO_NOPULL;
    gpioInitStructure.Alternate = 0;
    HAL_GPIO_Init(ADC_0.port, &gpioInitStructure);

    //EncoderA
    gpioInitStructure.Pin   = ENCODER_A.pin;
    gpioInitStructure.Speed = GPIO_SPEED_FAST;
    gpioInitStructure.Mode  = GPIO_MODE_INPUT;
    gpioInitStructure.Pull  = GPIO_PULLUP;
    gpioInitStructure.Alternate = 0;
    HAL_GPIO_Init(ENCODER_A.port, &gpioInitStructure);

    //EncoderB
    gpioInitStructure.Pin   = ENCODER_B.pin;
    gpioInitStructure.Speed = GPIO_SPEED_FAST;
    gpioInitStructure.Mode  = GPIO_MODE_INPUT;
    gpioInitStructure.Pull  = GPIO_PULLUP;
    gpioInitStructure.Alternate = 0;
    HAL_GPIO_Init(ENCODER_B.port, &gpioInitStructure);

    //EncoderP
    gpioInitStructure.Pin   = ENCODER_P.pin;
    gpioInitStructure.Speed = GPIO_SPEED_FAST;
    gpioInitStructure.Mode  = GPIO_MODE_INPUT;
    gpioInitStructure.Pull  = GPIO_PULLUP;
    gpioInitStructure.Alternate = 0;
    HAL_GPIO_Init(ENCODER_P.port, &gpioInitStructure);

    //ADC1
    gpioInitStructure.Pin   = ADC_1.pin;
    gpioInitStructure.Speed = GPIO_SPEED_FAST;
    gpioInitStructure.Mode  = GPIO_MODE_ANALOG;
    gpioInitStructure.Pull  = GPIO_NOPULL;
    gpioInitStructure.Alternate = 0;
    HAL_GPIO_Init(ADC_1.port, &gpioInitStructure);

    //ADC2
    gpioInitStructure.Pin   = ADC_2.pin;
    gpioInitStructure.Speed = GPIO_SPEED_FAST;
    gpioInitStructure.Mode  = GPIO_MODE_ANALOG;
    gpioInitStructure.Pull  = GPIO_NOPULL;
    gpioInitStructure.Alternate = 0;
    HAL_GPIO_Init(ADC_2.port, &gpioInitStructure);


    //DDS Pins
    //Reset
    	gpioInitStructure.Pin = DDS_RESET.pin;
    	gpioInitStructure.Mode = GPIO_MODE_OUTPUT_PP;
    	gpioInitStructure.Speed = GPIO_SPEED_FAST;
    	gpioInitStructure.Pull = GPIO_PULLUP;
    	HAL_GPIO_Init(DDS_RESET.port, &gpioInitStructure);
        HAL_GPIO_WritePin(DDS_RESET.port, DDS_RESET.pin, 0);

    	//Sleep
    	gpioInitStructure.Pin = DDS_SLEEP.pin;
    	gpioInitStructure.Mode = GPIO_MODE_OUTPUT_PP;
    	gpioInitStructure.Speed = GPIO_SPEED_FAST;
    	gpioInitStructure.Pull = GPIO_PULLUP;
    	HAL_GPIO_Init(DDS_SLEEP.port, &gpioInitStructure);
        HAL_GPIO_WritePin(DDS_SLEEP.port, DDS_SLEEP.pin, 0);

    	//DDS1 MOSI
    	gpioInitStructure.Pin = DDS_1_MOSI.pin;
    	gpioInitStructure.Mode = GPIO_MODE_OUTPUT_PP;
    	gpioInitStructure.Speed = GPIO_SPEED_FAST;
    	gpioInitStructure.Pull = GPIO_PULLUP;
    	HAL_GPIO_Init(DDS_1_MOSI.port, &gpioInitStructure);
        HAL_GPIO_WritePin(DDS_1_MOSI.port, DDS_1_MOSI.pin, 0);

    	//DDS1 SCK
    	gpioInitStructure.Pin = DDS_1_SCK.pin;
    	gpioInitStructure.Mode = GPIO_MODE_OUTPUT_PP;
    	gpioInitStructure.Speed = GPIO_SPEED_FAST;
    	gpioInitStructure.Pull = GPIO_PULLUP;
    	HAL_GPIO_Init(DDS_1_SCK.port, &gpioInitStructure);
        HAL_GPIO_WritePin(DDS_1_SCK.port, DDS_1_SCK.pin, 1);

    	//DDS1 NSS
    	gpioInitStructure.Pin = DDS_1_NSS.pin;
    	gpioInitStructure.Mode = GPIO_MODE_OUTPUT_PP;
    	gpioInitStructure.Speed = GPIO_SPEED_FAST;
    	gpioInitStructure.Pull = GPIO_PULLUP;
    	HAL_GPIO_Init(DDS_1_NSS.port, &gpioInitStructure);
        HAL_GPIO_WritePin(DDS_1_NSS.port, DDS_1_NSS.pin, 1);

    	//DDS2 MOSI
    	gpioInitStructure.Pin = DDS_2_MOSI.pin;
    	gpioInitStructure.Mode = GPIO_MODE_OUTPUT_PP;
    	gpioInitStructure.Speed = GPIO_SPEED_FAST;
    	gpioInitStructure.Pull = GPIO_PULLUP;
    	HAL_GPIO_Init(DDS_2_MOSI.port, &gpioInitStructure);
        HAL_GPIO_WritePin(DDS_2_MOSI.port, DDS_2_MOSI.pin, 0);

    	//DDS2 SCK
    	gpioInitStructure.Pin = DDS_2_SCK.pin;
    	gpioInitStructure.Mode = GPIO_MODE_OUTPUT_PP;
    	gpioInitStructure.Speed = GPIO_SPEED_FAST;
    	gpioInitStructure.Pull = GPIO_PULLUP;
    	HAL_GPIO_Init(DDS_2_SCK.port, &gpioInitStructure);
        HAL_GPIO_WritePin(DDS_2_SCK.port, DDS_2_SCK.pin, 1);

    	//DDS1 NSS
    	gpioInitStructure.Pin = DDS_2_NSS.pin;
    	gpioInitStructure.Mode = GPIO_MODE_OUTPUT_PP;
    	gpioInitStructure.Speed = GPIO_SPEED_FAST;
    	gpioInitStructure.Pull = GPIO_PULLUP;
    	HAL_GPIO_Init(DDS_2_NSS.port, &gpioInitStructure);
        HAL_GPIO_WritePin(DDS_2_NSS.port, DDS_2_NSS.pin, 1);



        gpioInitStructure.Pin = DAC_1.pin | DAC_2.pin;
        gpioInitStructure.Mode = GPIO_MODE_ANALOG;
        gpioInitStructure.Pull = GPIO_NOPULL;
        HAL_GPIO_Init(DAC_1.port, &gpioInitStructure);

    // Power Switch
//    gpioInitStructure.GPIO_Pin   = POWER_SWITCH.pin;
//    gpioInitStructure.GPIO_Speed = GPIO_Speed_2MHz;
//    gpioInitStructure.GPIO_Mode  = GPIO_Mode_IPU;
//    GPIO_Init(POWER_SWITCH.port, &gpioInitStructure);

    // low battery / power stat1
//    gpioInitStructure.GPIO_Pin   = LOW_BAT.pin;
//    gpioInitStructure.GPIO_Speed = GPIO_Speed_2MHz;
//    gpioInitStructure.GPIO_Mode  = GPIO_Mode_IPU;
//    GPIO_Init(LOW_BAT.port, &gpioInitStructure);

    // CHARGE_STATUS2 / POWER STAT2
//    gpioInitStructure.GPIO_Pin   = CHARGE_STATUS2.pin;
//    gpioInitStructure.GPIO_Speed = GPIO_Speed_2MHz;
//    gpioInitStructure.GPIO_Mode  = GPIO_Mode_IPU;
//    GPIO_Init(CHARGE_STATUS2.port, &gpioInitStructure);

    // POWER GOOD
//    gpioInitStructure.GPIO_Pin   = POWER_GOOD.pin;
//    gpioInitStructure.GPIO_Speed = GPIO_Speed_2MHz;
//    gpioInitStructure.GPIO_Mode  = GPIO_Mode_IPU;
//    GPIO_Init(POWER_GOOD.port, &gpioInitStructure);

    // Servo power / Muscle wire control
//    gpioInitStructure.GPIO_Pin   = SERVO_PWR.pin;
//    gpioInitStructure.GPIO_Speed = GPIO_Speed_2MHz;
//    gpioInitStructure.GPIO_Mode  = GPIO_Mode_Out_OD;
//    GPIO_Init(SERVO_PWR.port, &gpioInitStructure);

    //TODO: Make an initServo method
    //Servo power defaults to off
//    GPIO_WriteBit(SERVO_PWR.port, SERVO_PWR.pin, (BitAction) 1);

    // Finger Print Power Control
//    gpioInitStructure.GPIO_Pin   = FINGER_PRINT_POWER.pin;
//    gpioInitStructure.GPIO_Speed = GPIO_Speed_2MHz;
//    gpioInitStructure.GPIO_Mode  = GPIO_Mode_Out_OD;
//    GPIO_Init(FINGER_PRINT_POWER.port, &gpioInitStructure);

    // start with finger print reader powered off
//    GPIO_WriteBit(FINGER_PRINT_POWER.port, FINGER_PRINT_POWER.pin,
//            (BitAction) 1);

    //User Button
//    gpioInitStructure.GPIO_Pin   = USER_BUTTON.pin;
//    gpioInitStructure.GPIO_Speed = GPIO_Speed_2MHz;
//    gpioInitStructure.GPIO_Mode  = GPIO_Mode_IPU;
//    GPIO_Init(USER_BUTTON.port, &gpioInitStructure);

    // RFID_INT (RFID irq in, IRQ_IN is from the point of view of the RFID Reader)
//    GPIO_WriteBit(RFID_INT.port, RFID_INT.pin, (BitAction) 1);
//    gpioInitStructure.GPIO_Pin   = RFID_INT.pin;
//    gpioInitStructure.GPIO_Speed = GPIO_Speed_50MHz;
//    gpioInitStructure.GPIO_Mode  = GPIO_Mode_Out_OD;
//    GPIO_Init(RFID_INT.port, &gpioInitStructure);

    // GSM Power Key
//    gpioInitStructure.GPIO_Pin   = GSM_PWRKEY.pin;
//    gpioInitStructure.GPIO_Speed = GPIO_Speed_2MHz;
//    gpioInitStructure.GPIO_Mode  = GPIO_Mode_Out_OD;
//    GPIO_Init(GSM_PWRKEY.port, &gpioInitStructure);

	//VERY IMPORTANT! But it doesn't need to happen right here as long as it's pretty early in execution
//	GPIO_WriteBit(GSM_PWRKEY.port, GSM_PWRKEY.pin, (BitAction) 1);

    // GSM Power Key
//    gpioInitStructure.GPIO_Pin   = GSM_NRST.pin;
//    gpioInitStructure.GPIO_Speed = GPIO_Speed_2MHz;
//    gpioInitStructure.GPIO_Mode  = GPIO_Mode_Out_OD;
//    GPIO_Init(GSM_NRST.port, &gpioInitStructure);
//
//	GPIO_WriteBit(GSM_NRST.port, GSM_NRST.pin, (BitAction) 1);

    // GSM Status
//    gpioInitStructure.GPIO_Pin   = GSM_STATUS.pin;
//    gpioInitStructure.GPIO_Speed = GPIO_Speed_2MHz;
//    gpioInitStructure.GPIO_Mode  = GPIO_Mode_IN_FLOATING;
//    GPIO_Init(GSM_STATUS.port, &gpioInitStructure);

    // Fingerprint Button
//    gpioInitStructure.GPIO_Pin   = FINGER_PRINT_BUTTON.pin;
//    gpioInitStructure.GPIO_Speed = GPIO_Speed_2MHz;
//    gpioInitStructure.GPIO_Mode  = GPIO_Mode_IPU;
//    GPIO_Init(FINGER_PRINT_BUTTON.port, &gpioInitStructure);

    // Timer pins

    // red LED
//    gpioInitStructure.GPIO_Pin   = LED_R.gpioPin.pin;
//    gpioInitStructure.GPIO_Speed = GPIO_Speed_2MHz;
//    gpioInitStructure.GPIO_Mode  = GPIO_Mode_AF_PP;
//    GPIO_Init(LED_R.gpioPin.port, &gpioInitStructure);

    // green LED
//    gpioInitStructure.GPIO_Pin   = LED_G.gpioPin.pin;
//    gpioInitStructure.GPIO_Speed = GPIO_Speed_2MHz;
//    gpioInitStructure.GPIO_Mode  = GPIO_Mode_AF_PP;
//    GPIO_Init(LED_G.gpioPin.port, &gpioInitStructure);

    // blue LED
//    gpioInitStructure.GPIO_Pin   = LED_B.gpioPin.pin;
//    gpioInitStructure.GPIO_Speed = GPIO_Speed_2MHz;
//    gpioInitStructure.GPIO_Mode  = GPIO_Mode_AF_PP;
//    GPIO_Init(LED_B.gpioPin.port, &gpioInitStructure);

    // Servo1 PWM
//    gpioInitStructure.GPIO_Pin   = SERVO1.gpioPin.pin;
//    gpioInitStructure.GPIO_Speed = GPIO_Speed_2MHz;
//    gpioInitStructure.GPIO_Mode  = GPIO_Mode_AF_PP;
//    GPIO_Init(SERVO1.gpioPin.port, &gpioInitStructure);

    // Servo2 PWM
//    gpioInitStructure.GPIO_Pin   = SERVO2.gpioPin.pin;
//    gpioInitStructure.GPIO_Mode  = GPIO_Mode_AF_PP;
//    gpioInitStructure.GPIO_Speed = GPIO_Speed_2MHz;
//    GPIO_Init(SERVO2.gpioPin.port, &gpioInitStructure);

    // unused GPIO - Sorta, it's the closure sensor input near the 3v3 regulator
//    gpioInitStructure.GPIO_Pin   = NC_1.pin;
//    gpioInitStructure.GPIO_Speed = GPIO_Speed_2MHz;
//    gpioInitStructure.GPIO_Mode  = GPIO_Mode_IPD;
//    GPIO_Init(NC_1.port, &gpioInitStructure);

//    gpioInitStructure.GPIO_Pin 	 = GPS_RESET.pin;
//    gpioInitStructure.GPIO_Speed = GPIO_Speed_2MHz;
//    gpioInitStructure.GPIO_Mode  = GPIO_Mode_Out_PP; //I don't know if the module has ANYTHING hooked up to these pins yet.
//    GPIO_Init(GPS_RESET.port, &gpioInitStructure);

//    gpioInitStructure.GPIO_Pin 	 = GPS_FIX_LED.pin;
//    gpioInitStructure.GPIO_Speed = GPIO_Speed_2MHz;
//    gpioInitStructure.GPIO_Mode  = GPIO_Mode_Out_PP; //I don't know if the module has ANYTHING hooked up to these pins yet.
//    GPIO_Init(GPS_FIX_LED.port, &gpioInitStructure);

//    gpioInitStructure.GPIO_Pin 	 = GPS_PPS.pin;
//    gpioInitStructure.GPIO_Speed = GPIO_Speed_2MHz;
//    gpioInitStructure.GPIO_Mode  = GPIO_Mode_IN_FLOATING; //I don't know if the module has ANYTHING hooked up to these pins yet.
//    GPIO_Init(GPS_PPS.port, &gpioInitStructure);
}
//
//void hal_timerSetCompare(const Timer_Channel* timer, uint16_t value)
//{
//    // set the timer compare register for timer
//
//    switch( timer->channel )
//    {
//    case 1:
//        TIM_SetCompare1(timer->timer, value);
//        break;
//    case 2:
//        TIM_SetCompare2(timer->timer, value);
//        break;
//    case 3:
//        TIM_SetCompare3(timer->timer, value);
//        break;
//    case 4:
//        TIM_SetCompare4(timer->timer, value);
//        break;
//    default:
//        // A mistake was made, I should probably tell somebody
//        // TODO - assert
//        break;
//    }
//}

//void hal_setupTimers(void)
//{
//    // Setup TIM3 and TIM4 for controlling the LEDs and Servos
//    // TODO: Does it make more sense to init the appropriate pins here? Maybe
//
//    TIM_TimeBaseInitTypeDef timeBaseStructure;
//    TIM_OCInitTypeDef ocInitStructure;
//
//    // configure timer
//    // PWM frequency = 50 hz with 24 ,000 ,000 hz system clock
//    // 24 ,000 ,000 / 240 = 100 ,000       (24 MHz / 100KHz = 240; see TIM_Prescaler setting)
//    // 100 ,000 / 2000 = 50
//
//    // Setup timers
//    TIM_TimeBaseStructInit(&timeBaseStructure);
//    timeBaseStructure.TIM_Prescaler = SystemCoreClock / 100000 - 1; // 100KHz clk out of prescaler
//    timeBaseStructure.TIM_Period = 2000 - 1; // We need a 20ms period for the servos
//    timeBaseStructure.TIM_CounterMode = TIM_CounterMode_Up;
//    timeBaseStructure.TIM_ClockDivision = TIM_CKD_DIV1;
//    timeBaseStructure.TIM_RepetitionCounter = 0;
//    TIM_TimeBaseInit(TIM4, &timeBaseStructure);
//    TIM_TimeBaseInit(TIM3, &timeBaseStructure); // Might as well keep TIM3 the same
//
//    // Setup outputs
//    TIM_OCStructInit(&ocInitStructure);
//    ocInitStructure.TIM_OCMode = TIM_OCMode_PWM1;
//    ocInitStructure.TIM_OutputState = TIM_OutputState_Enable;
//    ocInitStructure.TIM_OutputNState = TIM_OutputNState_Disable;
//    ocInitStructure.TIM_Pulse = 0x0000;
//    ocInitStructure.TIM_OCPolarity = TIM_OCPolarity_High;
//    ocInitStructure.TIM_OCNPolarity = TIM_OCPolarity_High;
//    ocInitStructure.TIM_OCIdleState = TIM_OCIdleState_Reset;
//    ocInitStructure.TIM_OCNIdleState = TIM_OCNIdleState_Reset;
//    TIM_OC1Init(TIM3, &ocInitStructure);
//    TIM_OC2Init(TIM3, &ocInitStructure);
//    TIM_OC3Init(TIM3, &ocInitStructure);
//    TIM_OC4Init(TIM3, &ocInitStructure);
//    TIM_OC4Init(TIM4, &ocInitStructure);
//
//    // Set default values: Off/0% for LEDs, Closed for servos.
//    hal_timerSetCompare(&LED_R.timer, 0);
//    hal_timerSetCompare(&LED_G.timer, 0);
//    hal_timerSetCompare(&LED_B.timer, 0);
//
//    hal_timerSetCompare(&SERVO1.timer, 120);
//    hal_timerSetCompare(&SERVO2.timer, 180);
//
//    // Enable timers
//    // TODO: Does it work to do the above TIM_SetCompare before enabling?
//    TIM_Cmd(TIM4, ENABLE);
//    TIM_Cmd(TIM3, ENABLE);
//}

//void hal_setupUart1(void)
//{
//    // setup usart 1 and its pins
//    // connected to finger print
//
//    GPIO_InitTypeDef  gpioInitStructure;
//    USART_InitTypeDef usartInitStructure;
//    NVIC_InitTypeDef  nvicInitStructure;
//
//    // Finger Print UART Input / Output GPIO
//    gpioInitStructure.GPIO_Pin   = FPR_TX.pin;
//    gpioInitStructure.GPIO_Speed = GPIO_Speed_2MHz;
//    gpioInitStructure.GPIO_Mode  = GPIO_Mode_IN_FLOATING;
//    GPIO_Init(FPR_TX.port, &gpioInitStructure);
//
//    gpioInitStructure.GPIO_Pin   = FPR_RX.pin;
//    gpioInitStructure.GPIO_Speed = GPIO_Speed_2MHz; // TBD
//    gpioInitStructure.GPIO_Mode  = GPIO_Mode_AF_PP;
//    GPIO_Init(FPR_RX.port, &gpioInitStructure);
//    GPIO_WriteBit(FPR_RX.port, FPR_RX.pin, 0);
//
//    // Usart 1
//    USART_StructInit(&usartInitStructure);
//    usartInitStructure.USART_BaudRate = 9600; // TODO - fpr define
//    usartInitStructure.USART_Mode     = USART_Mode_Rx | USART_Mode_Tx;
//    USART_Init(USART1, &usartInitStructure);
//
//    // Setup USART1 Interrupts
//    nvicInitStructure.NVIC_IRQChannel                   = USART1_IRQn;
//    nvicInitStructure.NVIC_IRQChannelSubPriority        = 3;
//    nvicInitStructure.NVIC_IRQChannelPreemptionPriority = 0;
//    nvicInitStructure.NVIC_IRQChannelCmd                = ENABLE;
//    NVIC_Init(&nvicInitStructure);
//
//    // enable usart and rx interrupt
//    USART_Cmd(USART1, ENABLE);
//    USART_ITConfig(USART1, USART_IT_RXNE, ENABLE);
//}
//
//void hal_resetUart1(void)
//{
//    NVIC_InitTypeDef  nvicInitStructure;
//    GPIO_InitTypeDef  gpioInitStructure;
//
//    USART_ITConfig(USART1, USART_IT_RXNE, DISABLE);
//    USART_ITConfig(USART1, USART_IT_TXE, DISABLE);
//    USART_Cmd(USART1, DISABLE);
//
//    nvicInitStructure.NVIC_IRQChannel    = USART1_IRQn;
//    nvicInitStructure.NVIC_IRQChannelCmd = DISABLE;
//
//    NVIC_Init(&nvicInitStructure);
//
//    USART_DeInit(USART1);
//
//    gpioInitStructure.GPIO_Pin   = FPR_RX.pin;
//    gpioInitStructure.GPIO_Speed = GPIO_Speed_2MHz;
//    gpioInitStructure.GPIO_Mode  = GPIO_Mode_AF_OD;
//    GPIO_Init(FPR_RX.port, &gpioInitStructure);
//}
//
//void hal_setupUart2(void)
//{
//    // setup uart 2 and its pins
//    // connected to GSM
//
//    GPIO_InitTypeDef  gpioInitStructure;
//    USART_InitTypeDef usartInitStructure;
//    NVIC_InitTypeDef  nvicInitStructure;
//
//    // GSM UART Input / Output GPIO
//    gpioInitStructure.GPIO_Pin   = TX_FROM_GSM.pin;
//    gpioInitStructure.GPIO_Speed = GPIO_Speed_2MHz;
//    gpioInitStructure.GPIO_Mode  = GPIO_Mode_IN_FLOATING;
//    GPIO_Init(TX_FROM_GSM.port, &gpioInitStructure);
//
//    gpioInitStructure.GPIO_Pin   = RX_TO_GSM.pin;
//    gpioInitStructure.GPIO_Speed = GPIO_Speed_50MHz; // TBD
//    gpioInitStructure.GPIO_Mode  = GPIO_Mode_AF_PP;
//    GPIO_Init(RX_TO_GSM.port, &gpioInitStructure);
//
//    // usart 2
//    USART_StructInit(&usartInitStructure);
//    usartInitStructure.USART_BaudRate = 57600; // TODO gsm define
//    usartInitStructure.USART_Mode     = USART_Mode_Rx | USART_Mode_Tx;
//    USART_Init(USART2, &usartInitStructure);
//
//    // Setup USART2 Interupts
//    nvicInitStructure.NVIC_IRQChannel                   = USART2_IRQn;
//    nvicInitStructure.NVIC_IRQChannelSubPriority        = 3;
//    nvicInitStructure.NVIC_IRQChannelPreemptionPriority = 0;
//    nvicInitStructure.NVIC_IRQChannelCmd                = ENABLE;
//    NVIC_Init(&nvicInitStructure);
//
//    // enable usart and rx interrupt
//    USART_Cmd(USART2, ENABLE);
//    USART_ITConfig(USART2, USART_IT_RXNE, ENABLE);
//}


//
//void hal_resetUart2(void)
//{
//    NVIC_InitTypeDef  nvicInitStructure;
//    GPIO_InitTypeDef  gpioInitStructure;
//
//    USART_ITConfig(USART2, USART_IT_RXNE, DISABLE);
//    USART_ITConfig(USART2, USART_IT_TXE, DISABLE);
//    USART_Cmd(USART2, DISABLE);
//
//    nvicInitStructure.NVIC_IRQChannel    = USART2_IRQn;
//    nvicInitStructure.NVIC_IRQChannelCmd = DISABLE;
//
//    NVIC_Init(&nvicInitStructure);
//
//    USART_DeInit(USART2);
//
//    gpioInitStructure.GPIO_Pin   = RX_TO_GSM.pin;
//    gpioInitStructure.GPIO_Speed = GPIO_Speed_2MHz;
//    gpioInitStructure.GPIO_Mode  = GPIO_Mode_AF_OD;
//    GPIO_Init(RX_TO_GSM.port, &gpioInitStructure);
//}
//
//
//
//void hal_setupUart3(unsigned int baudRate)
//{
//    // setup uart 3 and its pins
//    // connected to GPS
//
//    GPIO_InitTypeDef  gpioInitStructure;
//    USART_InitTypeDef usartInitStructure;
//    NVIC_InitTypeDef  nvicInitStructure;
//
//    // GPS UART Input / Output GPIO
//    gpioInitStructure.GPIO_Pin   = TX_FROM_GPS.pin;
//    gpioInitStructure.GPIO_Speed = GPIO_Speed_2MHz;
//    gpioInitStructure.GPIO_Mode  = GPIO_Mode_IN_FLOATING;
//    GPIO_Init(TX_FROM_GPS.port, &gpioInitStructure);
//
//    gpioInitStructure.GPIO_Pin   = RX_TO_GPS.pin;
//    gpioInitStructure.GPIO_Speed = GPIO_Speed_50MHz; // TBD
//    gpioInitStructure.GPIO_Mode  = GPIO_Mode_AF_PP;
//    GPIO_Init(RX_TO_GPS.port, &gpioInitStructure);
//
//    // usart 3
//    USART_StructInit(&usartInitStructure);
//    usartInitStructure.USART_BaudRate  = baudRate; //4800 for SiRF 9600 for MT, TODO GPS define
//    usartInitStructure.USART_Mode      = USART_Mode_Rx | USART_Mode_Tx;
//    USART_Init(USART3, &usartInitStructure);
//
//    // Setup USART1 Interrupts
//    nvicInitStructure.NVIC_IRQChannel                    = USART3_IRQn;
//    nvicInitStructure.NVIC_IRQChannelSubPriority         = 3;
//    nvicInitStructure.NVIC_IRQChannelPreemptionPriority  = 0;
//    nvicInitStructure.NVIC_IRQChannelCmd                 = ENABLE;
//    NVIC_Init(&nvicInitStructure);
//
//    // enable usart and  rx interrupt
//    USART_Cmd(USART3, ENABLE);
//    USART_ITConfig(USART3, USART_IT_RXNE, ENABLE);
//}
//
//
//
//void hal_resetUart3(void)
//{
//    NVIC_InitTypeDef  nvicInitStructure;
//    GPIO_InitTypeDef  gpioInitStructure;
//
//    USART_ITConfig(USART3, USART_IT_RXNE, DISABLE);
//    USART_ITConfig(USART3, USART_IT_TXE, DISABLE);
//    USART_Cmd(USART3, DISABLE);
//
//    nvicInitStructure.NVIC_IRQChannel    = USART3_IRQn;
//    nvicInitStructure.NVIC_IRQChannelCmd = DISABLE;
//
//    NVIC_Init(&nvicInitStructure);
//
//    USART_DeInit(USART3);
//
//    gpioInitStructure.GPIO_Pin   = RX_TO_GPS.pin;
//    gpioInitStructure.GPIO_Speed = GPIO_Speed_2MHz;
//    gpioInitStructure.GPIO_Mode  = GPIO_Mode_AF_OD;
//    GPIO_Init(RX_TO_GPS.port, &gpioInitStructure);
//}



//void hal_adcConfigure(void)
//{
//    // configure and enable used ADC(s)
//    // ADC1 used
//
//    ADC_InitTypeDef adcInitStructure;
//
//    /* PCLK2 is the APB2 clock */
//    /* ADCCLK = PCLK2/6 = 72/6 = 12MHz */
//    RCC_ADCCLKConfig(RCC_PCLK2_Div6);
//
//    /* Enable ADC1 clock so that we can talk to it */
//    RCC_APB2PeriphClockCmd(RCC_APB2Periph_ADC1, ENABLE);
//
//    /* Put everything back to power-on defaults */
//    ADC_DeInit(ADC1);
//
//    /* ADC1 Configuration ------------------------------------------------------*/
//    /* ADC1 and ADC2 operate independently */
//    adcInitStructure.ADC_Mode = ADC_Mode_Independent;
//    /* Disable the scan conversion so we do one at a time */
//    adcInitStructure.ADC_ScanConvMode = DISABLE;
//    /* Don't do contimuous conversions - do them on demand */
//    adcInitStructure.ADC_ContinuousConvMode = DISABLE;
//    /* Start conversion by software, not an external trigger */
//    adcInitStructure.ADC_ExternalTrigConv = ADC_ExternalTrigConv_None;
//    /* Conversions are 12 bit - put them in the lower 12 bits of the result */
//    adcInitStructure.ADC_DataAlign = ADC_DataAlign_Right;
//    /* Say how many channels would be used by the sequencer */
//    adcInitStructure.ADC_NbrOfChannel = 1;
//
//    /* Now do the setup */
//    ADC_Init(ADC1, &adcInitStructure);
//
//    /* Enable ADC1 */
//    ADC_Cmd(ADC1, ENABLE);
//
//    ADC_TempSensorVrefintCmd(ENABLE);
//
//    /* Enable ADC1 reset calibration register */
//    ADC_ResetCalibration(ADC1);
//    /* Check the end of ADC1 reset calibration register */
//    while (ADC_GetResetCalibrationStatus(ADC1))
//        ;
//
//    /* Start ADC1 calibration */
//    ADC_StartCalibration(ADC1);
//    /* Check the end of ADC1 calibration */
//    while (ADC_GetCalibrationStatus(ADC1));
//}
//
//
//
//uint16_t hal_readAdc1(uint8_t channel)
//{
//    // read a value on a channel of ADC1
//    // returns value read
//
//    ADC_RegularChannelConfig(ADC1, channel, 1, ADC_SampleTime_239Cycles5);
//
//    // Start the conversion
//    ADC_SoftwareStartConvCmd(ADC1, ENABLE);
//    // Wait until conversion completion
//    while (ADC_GetFlagStatus(ADC1, ADC_FLAG_EOC) == RESET)  // TODO timeout, invalid value
//        ;
//    // Get the conversion value
//    return ADC_GetConversionValue(ADC1);
//}
//
//
//
//static uint16_t hal_readAdc1SingleSample(uint8_t channel)
//{
//    // read a value on a channel of ADC1
//    // returns value read
//
//    ADC_RegularChannelConfig(ADC1, channel, 1, ADC_SampleTime_1Cycles5);
//
//    // Start the conversion
//    ADC_SoftwareStartConvCmd(ADC1, ENABLE);
//    // Wait until conversion completion
//    while (ADC_GetFlagStatus(ADC1, ADC_FLAG_EOC) == RESET)  // TODO timeout, invalid value
//        ;
//    // Get the conversion value
//    return ADC_GetConversionValue(ADC1);
//}



//void hal_blinkLed(uint8_t led, uint8_t blinkCount, uint16_t onTime, uint16_t offTime)
//{
//    // blink the given LED blinkCount times
//    // TODO intensity
//    // TBD use tick count?
//
//    uint8_t i;
//
//    const Timer_Channel* timer = 0;
//
//    switch( led )
//    {
//    case hal_ledR:
//        timer = &LED_R.timer;
//        break;
//    case hal_ledG:
//        timer = &LED_G.timer;
//        break;
//    case hal_ledB:
//        timer = &LED_B.timer;
//        break;
//    default:
//        break;
//    }
//
//    if (timer)
//    {
//        for (i = 0; i < blinkCount; ++i)
//        {
//            hal_timerSetCompare(timer, 2000);
//            hal_delay_ms(onTime);
//            hal_timerSetCompare(timer, 0);
//            hal_delay_ms(offTime);
//        }
//    }
//}
//
//
//uint16_t hal_getHardwareVersion()
//{
//    return 0;
//}
//
//
//
//uint32_t hal_getSerialNumber()
//{
//    enum
//    {
//        halfWordShift = 16
//    };
//
//    uint32_t serialNumber    = 0;
//    uint16_t serialHighBytes = 0;
//    uint16_t serialLowBytes  = 0;
//    uint16_t address         = gunbox_flashAddr_serialNumber;
//
//    // TODO: remove!!
//    //if( EE_ReadVariable(address, &serialLowBytes) != 0 )
//    //{
//    //    // SN 10001
//    //    uint16_t serialLow  = 0x2711;
//    //    uint16_t serialHigh = 0x0000;
//    //
//    //    EE_WriteVariable(address, serialLow);
//    //    EE_WriteVariable(address + 1, serialHigh);
//    //}
//
//    // stored as little endian
//    if( EE_ReadVariable(address, &serialLowBytes) == 0 )
//    {
//        ++address;
//        if( EE_ReadVariable(address, &serialHighBytes) == 0 )
//        {
//            serialNumber = (serialHighBytes << halfWordShift) | serialLowBytes;
//        }
//    }
//
//    return serialNumber;
//}
//
//
//
//void hal_getKey(uint8_t* key, uint8_t maxSize)
//{
//    enum
//    {
//        keySize      = 32,
//        bytesPerRead =  2,
//    };
//
//    uint16_t keyChunk     = 0;
//    uint16_t address      = gunbox_flashAddr_key;
//    uint8_t  keyPosition  = keySize - bytesPerRead;
//    uint8_t  i;
//
//    if( maxSize < keySize )
//    {
//        return;
//    }
//
//    // TODO: remove writing test and replace with return;
//    //if( EE_ReadVariable(address, &keyChunk) != 0 )
//    //{
//    //    // write secret key
//    //    // encryption key for test server
//    //    uint8_t secretKey[keySize] = {
//    //                                   0xD4, 0xC5, 0x8D, 0x02, 0xFE, 0xA1, 0x0A, 0x2F,
//    //                                   0x9D, 0x87, 0x5E, 0xC5, 0x7A, 0x25, 0xD4, 0xDE,
//    //                                   0x88, 0x3E, 0x95, 0x99, 0xBA, 0xE5, 0xFB, 0x76,
//    //                                   0x20, 0x91, 0x1D, 0xB9, 0x6C, 0x78, 0x5D, 0x60,
//    //                                 };
//    //    uint16_t keyU16      = 0;
//    //    uint8_t  keyBlocks   = keySize / bytesPerRead;
//    //
//    //    for( i = 0; i < keyBlocks; ++i )
//    //    {
//    //        vpEndian_readBig16(&keyU16, secretKey + keyPosition);
//    //        EE_WriteVariable(address, keyU16);
//    //
//    //        keyPosition -= bytesPerRead;
//    //        ++address;
//    //    }
//    //
//    //    address = gunbox_flashAddr_key;
//    //
//    //    keyPosition  = keySize - bytesPerRead;
//    //}
//
//    if( EE_ReadVariable(address, &keyChunk) == 0 )
//    {
//        uint8_t keyBlocks = keySize / bytesPerRead;
//
//        vpEndian_writeBig16(key + keyPosition, keyChunk);
//
//        keyPosition -= bytesPerRead;
//        ++address;
//        for( i = 1; i < keyBlocks; ++i )
//        {
//            // stored as little endian
//            EE_ReadVariable(address, &keyChunk);
//            vpEndian_writeBig16(key + keyPosition, keyChunk);
//
//            keyPosition -= bytesPerRead;
//            ++address;
//        }
//    }
//}
//
//
//
//void hal_ivPrng(uint8_t* IV, uint8_t size)
//{
//    // Fibonacci linear feedback shift register implementation
//
//    uint16_t adcValue;
//    uint16_t lfsr;
//    uint8_t  spins;
//    uint8_t  bit;
//    uint8_t  i;
//    uint8_t  j;
//
//    // generate random number from batt voltage lsb
//    lfsr = 0;
//    for( i = 0; i < 16; ++i )
//    {
//
//        adcValue  = hal_readAdc1SingleSample(adcChannel_powerLevel);
//        lfsr     |= (adcValue & 0x01) << i;
//    }
//
//    spins = halMilliseconds & 0xFF;
//
//    for( i = 0; i < size; ++i )
//    {
//        // run spins through LFSR to get different spin for each IV block
//        bit   = ((spins >> 1) ^ (spins >> 4) ) & 1;
//        spins =  (spins >> 1) | (bit << 7);
//
//        // calculate the pseudo-random number for the IV block
//        for( j = 0; j < spins; ++j )
//        {
//            /* taps: 16 13 10 9; feedback polynomial: x^16 + x^13 + x^10 + x^9 + 1 */
//            bit  = ((lfsr >> 0) ^ (lfsr >> 3) ^ (lfsr >> 6) ^ (lfsr >> 7) ) & 1;
//            lfsr =  (lfsr >> 1) | (bit << 15);
//        }
//
//        IV[i] = lfsr & 0xFF;
//    }
//
//    return;
//}
//


// eof
