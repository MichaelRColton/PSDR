#include <i2c.h>




void HAL_I2C_MspInit(I2C_HandleTypeDef *hi2c)
{
  //Ensure that appropriate GPIO and I2C hardware is powered up
  //um, they are elsewhere. Good enough for now.

  //handleI2C = &hi2c;

 GPIO_InitTypeDef gpioInitStructure;


 //Check for stuck SDA condition and attempt to clear it
//  gpioInitStructure.Pin = I2C_SDA.pin;
//  gpioInitStructure.Speed = GPIO_SPEED_FAST;
//  gpioInitStructure.Mode = GPIO_MODE_OUTPUT_OD;
//  gpioInitStructure.Pull = GPIO_PULLUP;
//  gpioInitStructure.Alternate = 0;
//  HAL_GPIO_Init (I2C_SDA.port, &gpioInitStructure);
//
//
//  gpioInitStructure.Pin = I2C_SCL.pin;
//  gpioInitStructure.Speed = GPIO_SPEED_FAST;
//  gpioInitStructure.Mode = GPIO_MODE_OUTPUT_OD;
//  gpioInitStructure.Pull = GPIO_PULLUP;
//  gpioInitStructure.Alternate = 0;
//  HAL_GPIO_Init (I2C_SCL.port, &gpioInitStructure);
//
//
//
//  //Attempt to clear stuck SDA state (caused by poorly timed reset, for example)
//  int retries = 0;
//  //while((!HAL_GPIO_ReadPin(I2C_SDA.port, I2C_SDA.pin)) && retries < 100)
//    while(retries < 10)
//    {
//      HAL_GPIO_WritePin(I2C_SCL.port, I2C_SCL.pin, 0);
//      HAL_GPIO_WritePin(I2C_SCL.port, I2C_SCL.pin, 1);
//      retries++;
//    }
//
//    HAL_GPIO_WritePin(I2C_SDA.port, I2C_SDA.pin, 1);
//    HAL_GPIO_WritePin(I2C_SCL.port, I2C_SCL.pin, 0);
//    HAL_GPIO_WritePin(I2C_SDA.port, I2C_SDA.pin, 0);


 //THESE HAVE TO BE SET UP IN THIS ORDER!! If they are flipped, the BUSY flag will get flipped and never clear and you'll have a bad day.
  gpioInitStructure.Pin = I2C_SCL.pin;
  gpioInitStructure.Speed = GPIO_SPEED_FAST;
  gpioInitStructure.Mode = GPIO_MODE_AF_OD;
  gpioInitStructure.Pull = GPIO_PULLUP;
  gpioInitStructure.Alternate = GPIO_AF4_I2C2;
  HAL_GPIO_Init (I2C_SCL.port, &gpioInitStructure);

 gpioInitStructure.Pin = I2C_SDA.pin;
  gpioInitStructure.Speed = GPIO_SPEED_FAST;
  gpioInitStructure.Mode = GPIO_MODE_AF_OD;
  gpioInitStructure.Pull = GPIO_PULLUP;
  gpioInitStructure.Alternate = GPIO_AF4_I2C2;
  HAL_GPIO_Init (I2C_SDA.port, &gpioInitStructure);



//  (*hi2c).Init.AddressingMode = I2C_ADDRESSINGMODE_7BIT;
//  (*hi2c).Init.ClockSpeed = I2C_
}


void clearStuckBusyFlag(void)
{
  //disable I2C peripheral
  //__I2C2_CLK_DISABLE();
  I2C2->CR1 &= 0b1111111111111110;

  //set pins as OD high,
  GPIO_InitTypeDef gpioInitStructure;
    gpioInitStructure.Pin = I2C_SDA.pin;
    gpioInitStructure.Speed = GPIO_SPEED_FAST;
    gpioInitStructure.Mode = GPIO_MODE_OUTPUT_OD;
    gpioInitStructure.Pull = GPIO_NOPULL;
    gpioInitStructure.Alternate = 0;
    HAL_GPIO_Init (I2C_SDA.port, &gpioInitStructure);

    gpioInitStructure.Pin = I2C_SCL.pin;
    gpioInitStructure.Speed = GPIO_SPEED_FAST;
    gpioInitStructure.Mode = GPIO_MODE_OUTPUT_OD;
    gpioInitStructure.Pull = GPIO_NOPULL;
    gpioInitStructure.Alternate = 0;
    HAL_GPIO_Init (I2C_SCL.port, &gpioInitStructure);

    HAL_GPIO_WritePin(I2C_SCL.port, I2C_SCL.pin, 1);
    HAL_GPIO_WritePin(I2C_SDA.port, I2C_SDA.pin, 1);

  //set them low (and check them)
    HAL_GPIO_WritePin(I2C_SCL.port, I2C_SCL.pin, 0);
    HAL_GPIO_WritePin(I2C_SDA.port, I2C_SDA.pin, 0);

  //set them high (and check them)
    HAL_GPIO_WritePin(I2C_SCL.port, I2C_SCL.pin, 1);
    HAL_GPIO_WritePin(I2C_SDA.port, I2C_SDA.pin, 1);

  //return pins to AF configuration
    gpioInitStructure.Pin = I2C_SDA.pin;
    gpioInitStructure.Speed = GPIO_SPEED_FAST;
    gpioInitStructure.Mode = GPIO_MODE_AF_OD;
    gpioInitStructure.Pull = GPIO_PULLUP;
    gpioInitStructure.Alternate = GPIO_AF4_I2C2;
    HAL_GPIO_Init (I2C_SDA.port, &gpioInitStructure);

    gpioInitStructure.Pin = I2C_SCL.pin;
    gpioInitStructure.Speed = GPIO_SPEED_FAST;
    gpioInitStructure.Mode = GPIO_MODE_AF_OD;
    gpioInitStructure.Pull = GPIO_PULLUP;
    gpioInitStructure.Alternate = GPIO_AF4_I2C2;
    HAL_GPIO_Init (I2C_SCL.port, &gpioInitStructure);

  //set the reset bit in I2Cx_CR1 register then clear it
    I2C2->CR1 |= 0b1000000000000000;
    I2C2->CR1 &= 0b0111111111111111;

  //turn I2C peripheral back on
    //__I2C2_CLK_ENABLE();
    I2C2->CR1 |= 0b0000000000000001;
}












