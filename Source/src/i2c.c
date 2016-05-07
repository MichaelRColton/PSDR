#include <i2c.h>




HAL_I2C_MspInit(I2C_HandleTypeDef hi2c)
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


clearStuckBusyFlag()
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




#define NUM_REGS_MAX 350
#define SI5338_ADDRESS 0x70
#define LOCK_MASK 0x15
#define LOS_MASK 0x04



//typedef struct Reg_Data{
//   unsigned unsigned char Reg_Addr;
//   unsigned unsigned char Reg_Val;
//   unsigned unsigned char Reg_Mask;
//} Reg_Data;

//Reg_Data const code Reg_Store[NUM_REGS_MAX] = {
unsigned char Reg_Store[][3] = {
    {  0,0x00,0x00},
    {  1,0x00,0x00},
    {  2,0x00,0x00},
    {  3,0x00,0x00},
    {  4,0x00,0x00},
    {  5,0x00,0x00},
    {  6,0x08,0x1D},
    {  7,0x00,0x00},
    {  8,0x70,0x00},
    {  9,0x0F,0x00},
    { 10,0x00,0x00},
    { 11,0x00,0x00},
    { 12,0x00,0x00},
    { 13,0x00,0x00},
    { 14,0x00,0x00},
    { 15,0x00,0x00},
    { 16,0x00,0x00},
    { 17,0x00,0x00},
    { 18,0x00,0x00},
    { 19,0x00,0x00},
    { 20,0x00,0x00},
    { 21,0x00,0x00},
    { 22,0x00,0x00},
    { 23,0x00,0x00},
    { 24,0x00,0x00},
    { 25,0x00,0x00},
    { 26,0x00,0x00},
    { 27,0x70,0x80},
    { 28,0x16,0xFF},
    { 29,0x90,0xFF},
    { 30,0xB0,0xFF},
    { 31,0xC0,0xFF},
    { 32,0xC0,0xFF},
    { 33,0xCC,0xFF},
    { 34,0xE3,0xFF},
    { 35,0x00,0xFF},
    { 36,0x06,0x1F},
    { 37,0x06,0x1F},
    { 38,0x06,0x1F},
    { 39,0x00,0x1F},
    { 40,0x63,0xFF},
    { 41,0x0C,0x7F},
    { 42,0x23,0x3F},
    { 43,0x00,0x00},
    { 44,0x00,0x00},
    { 45,0x00,0xFF},
    { 46,0x00,0xFF},
    { 47,0x14,0x3F},
    { 48,0x2B,0xFF},
    { 49,0x00,0xFF},
    { 50,0xC2,0xFF},
    { 51,0x27,0xFF},
    { 52,0x10,0xFF},
    { 53,0xD9,0xFF},
    { 54,0x02,0xFF},
    { 55,0x84,0xFF},
    { 56,0x01,0xFF},
    { 57,0x00,0xFF},
    { 58,0x00,0xFF},
    { 59,0xB7,0xFF},
    { 60,0x00,0xFF},
    { 61,0x00,0xFF},
    { 62,0x00,0x3F},
    { 63,0x10,0xFF},
    { 64,0x00,0xFF},
    { 65,0x00,0xFF},
    { 66,0x00,0xFF},
    { 67,0x00,0xFF},
    { 68,0x00,0xFF},
    { 69,0x00,0xFF},
    { 70,0x01,0xFF},
    { 71,0x00,0xFF},
    { 72,0x00,0xFF},
    { 73,0x00,0x3F},
    { 74,0x10,0xFF},
    { 75,0x38,0xFF},
    { 76,0xC3,0xFF},
    { 77,0x20,0xFF},
    { 78,0x00,0xFF},
    { 79,0x00,0xFF},
    { 80,0x00,0xFF},
    { 81,0x09,0xFF},
    { 82,0x00,0xFF},
    { 83,0x00,0xFF},
    { 84,0x00,0x3F},
    { 85,0x10,0xFF},
    { 86,0x00,0xFF},
    { 87,0x00,0xFF},
    { 88,0x00,0xFF},
    { 89,0x00,0xFF},
    { 90,0x00,0xFF},
    { 91,0x00,0xFF},
    { 92,0x00,0xFF},
    { 93,0x00,0xFF},
    { 94,0x00,0xFF},
    { 95,0x00,0x3F},
    { 96,0x10,0x00},
    { 97,0x9D,0xFF},
    { 98,0x34,0xFF},
    { 99,0x1C,0xFF},
    {100,0x00,0xFF},
    {101,0x00,0xFF},
    {102,0x00,0xFF},
    {103,0x0D,0xFF},
    {104,0x00,0xFF},
    {105,0x00,0xFF},
    {106,0x80,0xBF},
    {107,0x00,0xFF},
    {108,0x00,0xFF},
    {109,0x00,0xFF},
    {110,0x40,0xFF},
    {111,0x00,0xFF},
    {112,0x00,0xFF},
    {113,0x00,0xFF},
    {114,0x40,0xFF},
    {115,0x00,0xFF},
    {116,0x80,0xFF},
    {117,0x00,0xFF},
    {118,0x40,0xFF},
    {119,0x00,0xFF},
    {120,0x00,0xFF},
    {121,0x00,0xFF},
    {122,0x40,0xFF},
    {123,0x00,0xFF},
    {124,0x00,0xFF},
    {125,0x00,0xFF},
    {126,0x00,0xFF},
    {127,0x00,0xFF},
    {128,0x00,0xFF},
    {129,0x00,0x0F},
    {130,0x00,0x0F},
    {131,0x00,0xFF},
    {132,0x00,0xFF},
    {133,0x00,0xFF},
    {134,0x00,0xFF},
    {135,0x00,0xFF},
    {136,0x00,0xFF},
    {137,0x00,0xFF},
    {138,0x00,0xFF},
    {139,0x00,0xFF},
    {140,0x00,0xFF},
    {141,0x00,0xFF},
    {142,0x00,0xFF},
    {143,0x00,0xFF},
    {144,0x00,0xFF},
    {145,0x00,0x00},
    {146,0xFF,0x00},
    {147,0x00,0x00},
    {148,0x00,0x00},
    {149,0x00,0x00},
    {150,0x00,0x00},
    {151,0x00,0x00},
    {152,0x00,0xFF},
    {153,0x00,0xFF},
    {154,0x00,0xFF},
    {155,0x00,0xFF},
    {156,0x00,0xFF},
    {157,0x00,0xFF},
    {158,0x00,0x0F},
    {159,0x00,0x0F},
    {160,0x00,0xFF},
    {161,0x00,0xFF},
    {162,0x00,0xFF},
    {163,0x00,0xFF},
    {164,0x00,0xFF},
    {165,0x00,0xFF},
    {166,0x00,0xFF},
    {167,0x00,0xFF},
    {168,0x00,0xFF},
    {169,0x00,0xFF},
    {170,0x00,0xFF},
    {171,0x00,0xFF},
    {172,0x00,0xFF},
    {173,0x00,0xFF},
    {174,0x00,0xFF},
    {175,0x00,0xFF},
    {176,0x00,0xFF},
    {177,0x00,0xFF},
    {178,0x00,0xFF},
    {179,0x00,0xFF},
    {180,0x00,0xFF},
    {181,0x00,0x0F},
    {182,0x00,0xFF},
    {183,0x00,0xFF},
    {184,0x00,0xFF},
    {185,0x00,0xFF},
    {186,0x00,0xFF},
    {187,0x00,0xFF},
    {188,0x00,0xFF},
    {189,0x00,0xFF},
    {190,0x00,0xFF},
    {191,0x00,0xFF},
    {192,0x00,0xFF},
    {193,0x00,0xFF},
    {194,0x00,0xFF},
    {195,0x00,0xFF},
    {196,0x00,0xFF},
    {197,0x00,0xFF},
    {198,0x00,0xFF},
    {199,0x00,0xFF},
    {200,0x00,0xFF},
    {201,0x00,0xFF},
    {202,0x00,0xFF},
    {203,0x00,0x0F},
    {204,0x00,0xFF},
    {205,0x00,0xFF},
    {206,0x00,0xFF},
    {207,0x00,0xFF},
    {208,0x00,0xFF},
    {209,0x00,0xFF},
    {210,0x00,0xFF},
    {211,0x00,0xFF},
    {212,0x00,0xFF},
    {213,0x00,0xFF},
    {214,0x00,0xFF},
    {215,0x00,0xFF},
    {216,0x00,0xFF},
    {217,0x00,0xFF},
    {218,0x00,0x00},
    {219,0x00,0x00},
    {220,0x00,0x00},
    {221,0x0D,0x00},
    {222,0x00,0x00},
    {223,0x00,0x00},
    {224,0xF4,0x00},
    {225,0xF0,0x00},
    {226,0x00,0x00},
    {227,0x00,0x00},
    {228,0x00,0x00},
    {229,0x00,0x00},
    {231,0x00,0x00},
    {232,0x00,0x00},
    {233,0x00,0x00},
    {234,0x00,0x00},
    {235,0x00,0x00},
    {236,0x00,0x00},
    {237,0x00,0x00},
    {238,0x14,0x00},
    {239,0x00,0x00},
    {240,0x00,0x00},
    {242,0x00,0x02},
    {243,0xF0,0x00},
    {244,0x00,0x00},
    {245,0x00,0x00},
    {247,0x00,0x00},
    {248,0x00,0x00},
    {249,0xA8,0x00},
    {250,0x00,0x00},
    {251,0x84,0x00},
    {252,0x00,0x00},
    {253,0x00,0x00},
    {254,0x00,0x00},
    {255, 1, 0xFF}, // set page bit to 1
    {  0,0x00,0x00},
    {  1,0x00,0x00},
    {  2,0x00,0x00},
    {  3,0x00,0x00},
    {  4,0x00,0x00},
    {  5,0x00,0x00},
    {  6,0x00,0x00},
    {  7,0x00,0x00},
    {  8,0x00,0x00},
    {  9,0x00,0x00},
    { 10,0x00,0x00},
    { 11,0x00,0x00},
    { 12,0x00,0x00},
    { 13,0x00,0x00},
    { 14,0x00,0x00},
    { 15,0x00,0x00},
    { 16,0x00,0x00},
    { 17,0x01,0x00},
    { 18,0x00,0x00},
    { 19,0x00,0x00},
    { 20,0x90,0x00},
    { 21,0x31,0x00},
    { 22,0x00,0x00},
    { 23,0x00,0x00},
    { 24,0x01,0x00},
    { 25,0x00,0x00},
    { 26,0x00,0x00},
    { 27,0x00,0x00},
    { 28,0x00,0x00},
    { 29,0x00,0x00},
    { 30,0x00,0x00},
    { 31,0x00,0xFF},
    { 32,0x00,0xFF},
    { 33,0x01,0xFF},
    { 34,0x00,0xFF},
    { 35,0x00,0xFF},
    { 36,0x90,0xFF},
    { 37,0x31,0xFF},
    { 38,0x00,0xFF},
    { 39,0x00,0xFF},
    { 40,0x01,0xFF},
    { 41,0x00,0xFF},
    { 42,0x00,0xFF},
    { 43,0x00,0x0F},
    { 44,0x00,0x00},
    { 45,0x00,0x00},
    { 46,0x00,0x00},
    { 47,0x00,0xFF},
    { 48,0x00,0xFF},
    { 49,0x01,0xFF},
    { 50,0x00,0xFF},
    { 51,0x00,0xFF},
    { 52,0x90,0xFF},
    { 53,0x31,0xFF},
    { 54,0x00,0xFF},
    { 55,0x00,0xFF},
    { 56,0x01,0xFF},
    { 57,0x00,0xFF},
    { 58,0x00,0xFF},
    { 59,0x00,0x0F},
    { 60,0x00,0x00},
    { 61,0x00,0x00},
    { 62,0x00,0x00},
    { 63,0x00,0xFF},
    { 64,0x00,0xFF},
    { 65,0x01,0xFF},
    { 66,0x00,0xFF},
    { 67,0x00,0xFF},
    { 68,0x90,0xFF},
    { 69,0x31,0xFF},
    { 70,0x00,0xFF},
    { 71,0x00,0xFF},
    { 72,0x01,0xFF},
    { 73,0x00,0xFF},
    { 74,0x00,0xFF},
    { 75,0x00,0x0F},
    { 76,0x00,0x00},
    { 77,0x00,0x00},
    { 78,0x00,0x00},
    { 79,0x00,0xFF},
    { 80,0x00,0xFF},
    { 81,0x00,0xFF},
    { 82,0x00,0xFF},
    { 83,0x00,0xFF},
    { 84,0x90,0xFF},
    { 85,0x31,0xFF},
    { 86,0x00,0xFF},
    { 87,0x00,0xFF},
    { 88,0x01,0xFF},
    { 89,0x00,0xFF},
    { 90,0x00,0xFF},
    { 91,0x00,0x0F},
    { 92,0x00,0x00},
    { 93,0x00,0x00},
    { 94,0x00,0x00},
    {255, 0, 0xFF} };



unsigned char hello = 1;

void write_Si5338(unsigned char address, unsigned char value)
{
  unsigned char txBuf[2];
  txBuf[0] = address;
  txBuf[1] = value;

  HAL_I2C_Master_Transmit(&handleI2C, 0xE0, &txBuf, 2, 100);
  //HAL_I2C_Mem_Write(&handleI2C, 0xE0, address, 1, value, 1, 100);

//  Wire.beginTransmission(SI5338_ADDRESS);
//  Wire.write(address);
//  //delay(10);
//  Wire.write(value);
//  Wire.endTransmission();
//  delay(10);
//  Serial.print(address, DEC);
//  Serial.print(" w ");
//  Serial.print(value, DEC);
//  Serial.print("\r\n");
}

#define MULTISYNTH0 53
void writeMultiSynthParameters(int multisynth, unsigned long a, unsigned long b, unsigned long c)
{
  unsigned long long msx_p1 = ((a * c + b) * 128) / c - 512;
  unsigned long long msx_p2 = (b * 128) % c;
  unsigned long long msx_p3 = c;

  write_Si5338(multisynth,     ( msx_p1 & 0xFF));
  write_Si5338(multisynth + 1, ( msx_p1 >> 8) & 0xFF);
  write_Si5338(multisynth + 2, (( msx_p1 >> 16) & 0b00000011) + (( msx_p2 << 2 ) & 0b11111100));
  write_Si5338(multisynth + 3, ( msx_p2 >> 6 ) & 0xFF);
  write_Si5338(multisynth + 4, ( msx_p2 >> 14 ) & 0xFF);
  write_Si5338(multisynth + 5, ( msx_p2 >> 22 ) & 0xFF);
  write_Si5338(multisynth + 6, ( msx_p3 & 0xFF));
  write_Si5338(multisynth + 7, ( msx_p3 >> 8) & 0xFF);
  write_Si5338(multisynth + 8, ( msx_p3 >> 16) & 0xFF);
  write_Si5338(multisynth + 9, ( msx_p3 >> 24) & 0b00111111);
}

unsigned char read_Si5338(unsigned char address, int increment)
{
  unsigned char retVal = 0;

  HAL_I2C_Master_Transmit(&handleI2C, 0xE0, &address, 1, 100);

  HAL_I2C_Master_Receive(&handleI2C, 0xE0, &retVal, 1, 100);


//  Wire.beginTransmission(SI5338_ADDRESS);
//  //Wire.write(0x00);
//  Wire.write(address);
//  Wire.endTransmission();
//
//  //delay(2);
//  Wire.beginTransmission(SI5338_ADDRESS);
//  Wire.requestFrom(SI5338_ADDRESS,1);
//
//  while(Wire.available())
//  {
//    retVal = Wire.read();
//    delay(10);
//    Serial.print(address, DEC);
//    Serial.print(" r ");
//    Serial.print(retVal, DEC);
//    Serial.print(" - ");
//    Serial.print(Reg_Store[increment][1], DEC);
//    Serial.print("\r\n");
//  }

  return retVal;
}

//add, val, mask
void sendRegToSi5338(int increment)
{
  unsigned char addr = Reg_Store[increment][0];
  unsigned char data = Reg_Store[increment][1];
  unsigned char mask = Reg_Store[increment][2];
//  unsigned char addr = Reg_Store[increment].Reg_Addr;
//  unsigned char data = Reg_Store[increment].Reg_Val;
//  unsigned char mask = Reg_Store[increment].Reg_Mask;

  // ignore registers with masks of 0x00
  if(mask != 0x00){
    if(mask == 0xFF){
      // do a regular I2C write to the register
      // at addr with the desired data value
      write_Si5338(addr, data);
    } else {
      // do a read-modify-write using I2C and
      // bit-wise operations
      // get the current value from the device at the
      // register located at addr
      unsigned char curr_val = read_Si5338(addr, increment);
      // clear the bits that are allowed to be
      // accessed in the current value of the register
      unsigned char clear_curr_val = curr_val & (~ mask);
      // clear the bits in the desired data that
      // are not allowed to be accessed
      unsigned char clear_new_val = data & mask;
      // combine the cleared values to get the new
      // value to write to the desired register
      unsigned char combined = clear_curr_val | clear_new_val;
      write_Si5338(addr, combined);
    }
  } else {
    //Serial.println("skip");
  }
}


void i2cSetup() {
  // put your setup code here, to run once:

  //pinMode(A3, INPUT);
  //digitalWrite(A3, LOW);


  //Serial.begin(115200);
  //Serial.write("\n\r\n\rHi Michael, you can do this\n\r");
  //Wire.begin();

//delay(50);

////This part is working correctly!
//  for(int i = 0; i < NUM_REGS_MAX; i++)
//  {
//    Serial.println(Reg_Store[i][1], HEX);
//  }

//Serial.println("and now this part");



//Serial.println("and now THIS part");
  //delay(2000);

  //write_Si5338(230, 1 << 4);
  //write_Si5338(241, 1 << 7);
write_Si5338(230, 0x10); //OEB_ALL = 1
write_Si5338(241, 0xE5); //DIS_LOL = 1

//delay(100);
  for(int i = 0; i < NUM_REGS_MAX; i++)
  {
    sendRegToSi5338(i);
  }
//for(int counter=0; counter<NUM_REGS_MAX; counter++){
//unsigned char curr = Reg_Store[counter];
//if(curr.Reg_Mask != 0x00) {
//  if(curr.Reg_Mask == 0xFF) {
//    // do a write transaction only
//    // since the mask is all ones
//    I2C_ByteWrite(curr.Reg_Addr, curr.Reg_Val);
//  } else {
//    //do a read-modify-write
//    curr_chip_val = I2C_ByteRead(curr.Reg_Addr);
//    clear_curr_val = curr_chip_val & ~curr.Reg_Mask;
//    clear_new_val = curr.Reg_Val & curr.Reg_Mask;
//    combined = clear_new_val | clear_curr_val;
//    I2C_ByteWrite(curr.Reg_Addr, combined);
//    }
//  }
//}

//  write_Si5338(49, 0 << 7);
//  write_Si5338(246, 1 << 1);

//  delay(30);

//  write_Si5338(241, 0 << 7);
//  //skipped some steps
//
//  write_Si5338(230, 0 << 4);

// check LOS alarm for the xtal input
// on IN1 and IN2 (and IN3 if necessary) -
// change this mask if using inputs on IN4, IN5, IN6
unsigned char reg = read_Si5338(218,1) & LOS_MASK;
while(reg != 0){
  reg = read_Si5338(218, 1) & LOS_MASK;
}
//delay(100);
write_Si5338(49, read_Si5338(49,1) & 0x7F); //FCAL_OVRD_EN = 0
write_Si5338(246, 2); //soft reset
write_Si5338(241, 0x65); //DIS_LOL = 0

// wait for Si5338 to be ready after calibration (ie, soft reset)
//delay(50);
//make sure the device locked by checking PLL_LOL and SYS_CAL
reg = read_Si5338(218,1) & LOCK_MASK;
while(reg != 0){
  reg = read_Si5338(218,1) & LOCK_MASK;
}
//delay(100);
//copy FCAL values
write_Si5338(45, read_Si5338(235,1));
write_Si5338(46, read_Si5338(236,1));
// clear bits 0 and 1 from 47 and
// combine with bits 0 and 1 from 237
reg = (read_Si5338(47,1) & 0xFC) | (read_Si5338(237,1) & 3);
write_Si5338(47, reg);
write_Si5338(49, read_Si5338(49,1) | 0x80); // FCAL_OVRD_EN = 1
write_Si5338(230, 0x00); // OEB_ALL = 0

//Serial.println("and that's it");

//  for(int i = 0; i < NUM_REGS_MAX; i++)
//  {
//    read_Si5338(i, i);
//
//  }

//while(1);



write_Si5338(31, 0xC1); //output driver off?

//Start us at a known frequency (in this case 14.06 which equals 7.03)
//TODO: Make this start at whatever the unit is supposed to power up at. Not hard coded
writeMultiSynthParameters(MULTISYNTH0, 201, 697, 703);
write_Si5338(31, 0xC0); //output driver off?
}


unsigned long long vcoFreq = 2840000000;
double correctionFactor = 1;
void setFreq(unsigned long frequency)
{
//void i2cLoop() {
  // put your main code here, to run repeatedly:
  //delay(400);
  //write_Si5338(31, 0xC1); //output driver off?

//long integer = 8;
//long neumerator = 0;
//do
//{
//
//  writeMultiSynthParameters(MULTISYNTH0, integer, neumerator, 10000UL);
//  //neumerator += 1000;
//  //if(neumerator > 9999)
//  //{
//  //  neumerator = 0;
//    integer++;
//
//  //}
//  if( integer >= 500) integer = 8;
//
//} while (true);
//double  desiredFrequency = 150.5;
//double  measuredFrequency = 150.48806;


//  correctionFactor = desiredFrequency/measuredFrequency;

//  frequency *= correctionFactor;

  frequency *= 2; //because the flipflops will devide it back down by 2

if(frequency < 5000000) return;
if(frequency > 350000000) return;



unsigned long integer = vcoFreq / frequency;

//if((integer % 2) != 0) integer++;



unsigned long remainder = vcoFreq % frequency;


//do
//{

  //writeMultiSynthParameters(MULTISYNTH0, integer, neumerator, 10000UL);
  //neumerator += 1000;
  //if(neumerator > 9999)
  //{
  //  neumerator = 0;
  //  integer++;

  //}
//  if( integer >= 500) integer = 8;

  //neumerator = (analogRead(A3) * 10); //578914 referal nu  801 435 7764 john thank you
  //integer = (analogRead(A2) /10) + 150;
//
//} while (true);


    //write_Si5338(31, 0xC0); //output driver off?

  //delay(400);



while(frequency > 100000) //This value should be fine tuned. I thought the numerator and denominator could be 30 bits, but it seems to be less than 24
  {
    remainder /= 2;
    frequency /= 2;
  }

    //write_Si5338(31, 0xC1); //output driver off?
    writeMultiSynthParameters(MULTISYNTH0, integer, remainder , frequency);
    //writeMultiSynthParameters(MULTISYNTH0, 232, 44960 , 122220);
  //writeMultiSynthParameters(MULTISYNTH0, integer, /*neumerator*/ 0 , 1000000UL);
    //write_Si5338(31, 0xC0); //output driver off?

}












