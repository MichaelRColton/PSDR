/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * File Name          : freertos.c
  * Description        : Code for freertos applications
  ******************************************************************************
  * This notice applies to any and all portions of this file
  * that are not between comment pairs USER CODE BEGIN and
  * USER CODE END. Other portions of this file, whether 
  * inserted by the user or by software development tools
  * are owned by their respective copyright owners.
  *
  * Copyright (c) 2018 STMicroelectronics International N.V. 
  * All rights reserved.
  *
  * Redistribution and use in source and binary forms, with or without 
  * modification, are permitted, provided that the following conditions are met:
  *
  * 1. Redistribution of source code must retain the above copyright notice, 
  *    this list of conditions and the following disclaimer.
  * 2. Redistributions in binary form must reproduce the above copyright notice,
  *    this list of conditions and the following disclaimer in the documentation
  *    and/or other materials provided with the distribution.
  * 3. Neither the name of STMicroelectronics nor the names of other 
  *    contributors to this software may be used to endorse or promote products 
  *    derived from this software without specific written permission.
  * 4. This software, including modifications and/or derivative works of this 
  *    software, must execute solely and exclusively on microcontroller or
  *    microprocessor devices manufactured by or for STMicroelectronics.
  * 5. Redistribution and use of this software other than as permitted under 
  *    this license is void and will automatically terminate your rights under 
  *    this license. 
  *
  * THIS SOFTWARE IS PROVIDED BY STMICROELECTRONICS AND CONTRIBUTORS "AS IS" 
  * AND ANY EXPRESS, IMPLIED OR STATUTORY WARRANTIES, INCLUDING, BUT NOT 
  * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY, FITNESS FOR A 
  * PARTICULAR PURPOSE AND NON-INFRINGEMENT OF THIRD PARTY INTELLECTUAL PROPERTY
  * RIGHTS ARE DISCLAIMED TO THE FULLEST EXTENT PERMITTED BY LAW. IN NO EVENT 
  * SHALL STMICROELECTRONICS OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT,
  * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
  * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, 
  * OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF 
  * LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING 
  * NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE,
  * EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
  *
  ******************************************************************************
  */
/* USER CODE END Header */

/* Includes ------------------------------------------------------------------*/
#include "FreeRTOS.h"
#include "task.h"
#include "main.h"
#include "cmsis_os.h"

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */     
#include "Adafruit_ILI9340.h"
#include "Adafruit_GFX.h"
#include "drawing.h"
#include "images.h"
#include "queue.h"
/* USER CODE END Includes */

/* Private typedef -----------------------------------------------------------*/
/* USER CODE BEGIN PTD */

/* USER CODE END PTD */

/* Private define ------------------------------------------------------------*/
/* USER CODE BEGIN PD */

/* USER CODE END PD */

/* Private macro -------------------------------------------------------------*/
/* USER CODE BEGIN PM */

/* USER CODE END PM */

/* Private variables ---------------------------------------------------------*/
/* USER CODE BEGIN Variables */
QueueHandle_t displayQueue;
/* USER CODE END Variables */
osThreadId defaultTaskHandle;
osThreadId displayManagerHandle;

/* Private function prototypes -----------------------------------------------*/
/* USER CODE BEGIN FunctionPrototypes */
   
/* USER CODE END FunctionPrototypes */

void StartDefaultTask(void const * argument);
void StartDisplayManager(void const * argument);

void MX_FREERTOS_Init(void); /* (MISRA C 2004 rule 8.1) */

/**
  * @brief  FreeRTOS initialization
  * @param  None
  * @retval None
  */
void MX_FREERTOS_Init(void) {
  /* USER CODE BEGIN Init */
       
  /* USER CODE END Init */

  /* USER CODE BEGIN RTOS_MUTEX */
  /* add mutexes, ... */
  /* USER CODE END RTOS_MUTEX */

  /* USER CODE BEGIN RTOS_SEMAPHORES */
  /* add semaphores, ... */
  /* USER CODE END RTOS_SEMAPHORES */

  /* USER CODE BEGIN RTOS_TIMERS */
  /* start timers, add new ones, ... */
  /* USER CODE END RTOS_TIMERS */

  /* Create the thread(s) */
  /* definition and creation of defaultTask */
  osThreadDef(defaultTask, StartDefaultTask, osPriorityNormal, 0, 128);
  defaultTaskHandle = osThreadCreate(osThread(defaultTask), NULL);

  /* definition and creation of displayManager */
  osThreadDef(displayManager, StartDisplayManager, osPriorityNormal, 0, 128);
  displayManagerHandle = osThreadCreate(osThread(displayManager), NULL);

  /* USER CODE BEGIN RTOS_THREADS */
  /* add threads, ... */
  /* USER CODE END RTOS_THREADS */

  /* USER CODE BEGIN RTOS_QUEUES */
  /* add queues, ... */
//  displayQueue = xQueueCreate(16, sizeof(int));



  /* USER CODE END RTOS_QUEUES */
}

/* USER CODE BEGIN Header_StartDefaultTask */
/**
  * @brief  Function implementing the defaultTask thread.
  * @param  argument: Not used 
  * @retval None
  */
/* USER CODE END Header_StartDefaultTask */
void StartDefaultTask(void const * argument)
{

  /* USER CODE BEGIN StartDefaultTask */
  while(1)
    {
      HAL_GPIO_WritePin(GPIOE, GPIO_PIN_15, 1);
      //xQueueSendToBack(displayQueue, 0, 10);
      osDelay(500);
      HAL_GPIO_WritePin(GPIOE, GPIO_PIN_15, 0);
      //xQueueSendToBack(displayQueue, 1, 10);
      osDelay(500);
    }
  /* Infinite loop */
  for(;;)
  {
    osDelay(1);
  }
  /* USER CODE END StartDefaultTask */
}

/* USER CODE BEGIN Header_StartDisplayManager */
/**
* @brief Function implementing the displayManager thread.
* @param argument: Not used
* @retval None
*/
/* USER CODE END Header_StartDisplayManager */
void StartDisplayManager(void const * argument)
{
  /* USER CODE BEGIN StartDisplayManager */

  Adafruit_ILI9340_begin();
  //*****Hard earned knowledge:
  //The hardware seems to draw the picture to the display in blocks, these can
  //roll so sometimes a full screen fills in 10-12/400fps (rotation(0) while
  //sometimes it's about 1.5x that long.
  //At setRotation(0), the tears are vertical (on the PSDR), time to fill a
  //frame with one color appears to be 10-12 frames at 400fps at setRotation(1)
  //(the one that makes the most sense on the PSDR) fill is at 16-17/400fps

  //Well the PSDR doesn't know how fast the display is going, it just blasts out
  //the pixels as the same rate (~30fps for full screen) when updating the
  //waterfall (100x240, it looks like it gets about 90fps. Not sure what the
  //rate on the LCD is, but it's fast (I need to make an optical measuring
  //device for the oscilloscope so I can measure better.
  //Even when doing the waterfall, there is a visual tear (vertical for
  //rotation(0) and diagonal for rotation(1). I think I just like rotation(0)
  //better, but it means rotating all the assets (maybe not a big deal?)
  //and either changing the coordinate system in the library, or in the
  //developer's head (I think I'll do the latter)
  Adafruit_ILI9340_setRotation(0);

  Adafruit_ILI9340_fillScreen(ILI9340_BLACK);

  while(1)
    {
      int messageVal = 0;
      //xQueueReceive(displayQueue, &messageVal, portMAX_DELAY);
      //Adafruit_ILI9340_fillRect(0,0,240,100, messageVal = 1 ? ILI9340_BLACK : ILI9340_WHITE);
      //HAL_GPIO_WritePin(GPIOE, GPIO_PIN_12, messageVal);
      Adafruit_ILI9340_fillRect(0,0,240,100, ILI9340_WHITE);
      HAL_GPIO_WritePin(GPIOE, GPIO_PIN_12, 1);


      Adafruit_ILI9340_fillRect(0,0,240,100, ILI9340_BLACK);
      //Adafruit_ILI9340_fillScreen(ILI9340_WHITE);
      HAL_GPIO_WritePin(GPIOE, GPIO_PIN_12, 0);
    }


  Adafruit_GFX_drawColorBitmap(100, 80, imagePsdrLogo.imageData, imagePsdrLogo.width, imagePsdrLogo.height, 0xFFFF);

  osDelay(1500);

  while(1)
    {
      //Adafruit_GFX_fillScreen(ILI9340_BLACK);
      Adafruit_ILI9340_fillScreen(ILI9340_BLUE);
      //Adafruit_GFX_drawColorBitmap(100, 100, bitmapThree, 15, 19, 0xFFFF);
      //Adafruit_GFX_fillScreen(ILI9340_RED);
      osDelay(300);

      //osDelay(400);
      //Adafruit_GFX_fillScreen(ILI9340_BLUE);
      Adafruit_ILI9340_fillScreen(ILI9340_BLACK);
  //    Adafruit_GFX_drawColorBitmap(100, 100, (uint8_t*)bitmapThreeFlip, 15, 19, 0xFFFF);

     osDelay(300);
      for(int j = 0; j < 12; j++)
        {
          for(int i = 0; i < 8/*21*/; i++)
            {
              Adafruit_GFX_drawColorBitmap(i * 15, j * 19, imageThree.imageData, imageThree.width, imageThree.height, 0xFFFF);
            }
        }

      osDelay(300);
      for(int j = 0; j < 12; j++)
        {
          for(int i = 0; i < 8/*21*/; i++)
            {
              Adafruit_GFX_drawColorBitmap(i * 15, j * 19, imageSeven.imageData, imageSeven.width, imageSeven.height, 0xFFFF);
            }
        }
      osDelay(300);

      for(int i = 0; i < 10000; i++)
        {
          drawNumber('0' + (i/1000 % 10), 200, 150, 0xFFFF);
          drawNumber('0' + (i/100 % 10), 215, 150, 0xFFFF);
          //drawNumber('0' + (i/10 % 10), 230, 150, 0xFFFF);
          drawNumber('0' + (i % 10), 245, 150, 0xFFFF);
        }

    }
  /* Infinite loop */
  for(;;)
  {
    osDelay(1);
  }
  /* USER CODE END StartDisplayManager */
}

/* Private application code --------------------------------------------------*/
/* USER CODE BEGIN Application */
     
/* USER CODE END Application */

/************************ (C) COPYRIGHT STMicroelectronics *****END OF FILE****/
