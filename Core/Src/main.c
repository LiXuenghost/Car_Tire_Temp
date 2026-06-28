/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file           : main.c
  * @brief          : Main program body
  ******************************************************************************
  * @attention
  *
  * Copyright (c) 2026 STMicroelectronics.
  * All rights reserved.
  *
  * This software is licensed under terms that can be found in the LICENSE file
  * in the root directory of this software component.
  * If no LICENSE file comes with this software, it is provided AS-IS.
  *
  ******************************************************************************
  */
/* USER CODE END Header */
/* Includes ------------------------------------------------------------------*/
#include "main.h"
#include "can.h"
#include "i2c.h"
#include "usart.h"
#include "gpio.h"

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */
#include <stdio.h>
#include "MLX90640_API.h"
/* USER CODE END Includes */

/* Private typedef -----------------------------------------------------------*/
/* USER CODE BEGIN PTD */
#ifdef __GNUC__
#define PUTCHAR_PROTOTYPE int __io_putchar(int ch)
#else
#define PUTCHAR_PROTOTYPE int fputc(int ch, FILE *f)
#endif /* __GNUC__ */

/* USER CODE END PTD */

/* Private define ------------------------------------------------------------*/
/* USER CODE BEGIN PD */

/* USER CODE END PD */

/* Private macro -------------------------------------------------------------*/
/* USER CODE BEGIN PM */

/* USER CODE END PM */

/* Private variables ---------------------------------------------------------*/

/* USER CODE BEGIN PV */

#define  FPS2HZ   0x02
#define  FPS4HZ   0x03
#define  FPS8HZ   0x04
#define  FPS16HZ  0x05
#define  FPS32HZ  0x06

#define  MLX90640_ADDR 0x33
#define	 RefreshRate FPS16HZ 
#define  TA_SHIFT 8 //Default shift for MLX90640 in open air

static uint16_t eeMLX90640[832];  
static float mlx90640To[768];
uint16_t frame[834];
float emissivity=0.95;
int status;

CAN_TxHeaderTypeDef   TxHeader; /* Header containing the information of the transmitted frame */
CAN_RxHeaderTypeDef   RxHeader; ; /* Header containing the information of the received frame */
uint8_t               TxData[8] = {0};  /* Buffer of the data to send */
uint8_t               RxData[8]; /* Buffer of the received data */
uint32_t              TxMailbox;  /* The number of the mail box that transmitted the Tx message */
CAN_RxHeaderTypeDef   RxHeaderFIFO0;   /* Header containing the information of the received frame */
uint8_t               RxDataFIFO0[8];  /* Buffer of the received data */
uint8_t               Mode;
/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
void SystemClock_Config(void);
/* USER CODE BEGIN PFP */

/* USER CODE END PFP */

/* Private user code ---------------------------------------------------------*/
/* USER CODE BEGIN 0 */

/* USER CODE END 0 */

/**
  * @brief  The application entry point.
  * @retval int
  */
int main(void)
{

  /* USER CODE BEGIN 1 */

  /* USER CODE END 1 */

  /* MCU Configuration--------------------------------------------------------*/

  /* Reset of all peripherals, Initializes the Flash interface and the Systick. */
  HAL_Init();

  /* USER CODE BEGIN Init */

  /* USER CODE END Init */

  /* Configure the system clock */
  SystemClock_Config();

  /* USER CODE BEGIN SysInit */

  /* USER CODE END SysInit */

  /* Initialize all configured peripherals */
  MX_GPIO_Init();
  MX_I2C1_Init();
  MX_USART1_UART_Init();
  MX_CAN_Init();
  /* USER CODE BEGIN 2 */

  TxHeader.StdId = 0x610;
  TxHeader.ExtId = 0x00;
  TxHeader.RTR = CAN_RTR_DATA;
  TxHeader.IDE = CAN_ID_STD;
  TxHeader.DLC = 8;
  TxHeader.TransmitGlobalTime = DISABLE;
  TxData[0] = 1;
  TxData[7] = 0xFF;
  
  HAL_Delay(50);
  MLX90640_SetRefreshRate(MLX90640_ADDR, RefreshRate);
	// MLX90640_SetChessMode(MLX90640_ADDR);
  MLX90640_SetInterleavedMode(MLX90640_ADDR);
	paramsMLX90640 mlx90640;
  status = MLX90640_DumpEE(MLX90640_ADDR, eeMLX90640);
  if (status != 0) printf("\r\nload system parameters error with code:%d\r\n",status);
  status = MLX90640_ExtractParameters(eeMLX90640, &mlx90640);
  if (status != 0) printf("\r\nParameter extraction failed with error code:%d\r\n",status);

  int lastSubpage = -1;
  int frameValid = 1;

  Mode = 3;
  /* USER CODE END 2 */

  /* Infinite loop */
  /* USER CODE BEGIN WHILE */
  while (1)
  {
    // HAL_GPIO_WritePin(GPIOA, GPIO_PIN_11, SET);
    // HAL_Delay(500);
    // HAL_GPIO_WritePin(GPIOA, GPIO_PIN_11, RESET);
    // HAL_Delay(500);
    // printf("Hello world\n");
    /* USER CODE END WHILE */

    /* USER CODE BEGIN 3 */
    status = MLX90640_GetFrameData(MLX90640_ADDR, frame);
    if (status < 0) {
        printf("GetFrame Error: %d\r\n", status);
        continue;
    }

    int curSubpage = frame[833];
    float Ta = MLX90640_GetTa(frame, &mlx90640);
    float vdd = MLX90640_GetVdd(frame, &mlx90640);
    float tr = Ta - TA_SHIFT;

    // 每次都呼叫 CalculateTo，讓兩個 subpage 分別填入各自的 384 個 pixel
    MLX90640_CalculateTo(frame, &mlx90640, emissivity, tr, mlx90640To);
    MLX90640_BadPixelsCorrection(&mlx90640.brokenPixels, mlx90640To, 1, &mlx90640);
    MLX90640_BadPixelsCorrection(&mlx90640.outlierPixels, mlx90640To, 1, &mlx90640);

    // 等到 subpage 0 和 1 都收到（一個完整循環），才印出完整畫面
    if (curSubpage == 1 && lastSubpage == 0)
    {
      frameValid = 1;
      for (int i = 0; i < 768; i++)
      {
        float temp = mlx90640To[i];

        // NaN 檢查
        if (isnan(temp))
        {
          frameValid = 0;
          printf("ERROR: NAN at pixel %d\r\n", i);
          break;
        }
      
        // 溫度範圍檢查
        if (temp < 1.0f || temp > 100.0f)
        {
          frameValid = 0;
          printf("ERROR: Invalid Temp %.2f at pixel %d\r\n", temp, i);
          break;
        }
      }

      if (frameValid)
      {
        // 計算每一個直行的平均
        uint8_t column_avg[32];
        for(int col = 0; col < 32; col++)
        {
          float sum = 0.0f;
        
          for(int row = 0; row < 24; row++)
          {
              int index = row * 32 + col;
              sum += mlx90640To[index];
          }
        
          float avg = sum / 24;
        
          column_avg[col] = (uint8_t)(avg * 2);
        }
        // printf("0\n");
        // for (int i = 0; i < 768; i++)
        // {
        //   if (i % 32 == 0 && i != 0)
        //     printf("\r\n");
        
        //   printf("%.2d ", (int)mlx90640To[i]);
        // }
        
        // printf("\n1\n");

        int frame_index = 0;
        

        if(Mode == 1) // Reduced Mode
        {
          TxHeader.StdId = 0x594;

          int number = 0;
          for(int col = 0 ; col < 32; col++)
          {
            float sum = 0.0f;
            for(int i = col; i < col + 4; i++)
              sum += column_avg[i]; 
            sum /= 4;
            col += 3;
            TxData[number] = (uint8_t)(sum);
            number++;
          }

          HAL_GPIO_WritePin(GPIOA, GPIO_PIN_11, SET);
            while(HAL_CAN_GetTxMailboxesFreeLevel(&hcan) == 0); /* Wait till a Tx mailbox is free. Using while loop instead of HAL_Delay() */
            if (HAL_CAN_AddTxMessage(&hcan, &TxHeader, TxData, &TxMailbox) != HAL_OK)
            {
              /* Transmission request Error */
              Error_Handler();
            }
            HAL_GPIO_WritePin(GPIOA, GPIO_PIN_11, RESET);
        }
        else if(Mode == 3) // Full Range
        {
          TxHeader.StdId = 0x694;
        
          // printf("subpage=%d\n", frame[833]);
          for(int i = 0; i < 768;)
          {  
            TxData[0] = frame_index;
            for(int j = 1; j < 8; j++)
            {
              if(i < 768)
                  TxData[j] = (uint8_t)(mlx90640To[i++] * 2);
              else
                  TxData[j] = 0;
            }
          
            HAL_GPIO_WritePin(GPIOA, GPIO_PIN_11, SET);
            while(HAL_CAN_GetTxMailboxesFreeLevel(&hcan) == 0); /* Wait till a Tx mailbox is free. Using while loop instead of HAL_Delay() */
            if (HAL_CAN_AddTxMessage(&hcan, &TxHeader, TxData, &TxMailbox) != HAL_OK)
            {
              /* Transmission request Error */
              Error_Handler();
            }
            HAL_GPIO_WritePin(GPIOA, GPIO_PIN_11, RESET);
            frame_index += 1;
          }
        }
        else if(Mode == 2) // Average Line
        {
          TxHeader.StdId = 0x598;
          frame_index = 0;
          for(int i = 0; i < 32;)
          {
              TxData[0] = frame_index;
          
              for(int j = 1; j < 8; j++)
              {
                if(i < 32)
                {
                    TxData[j] = (uint8_t)(column_avg[i++]);
                }
                else
                {
                    TxData[j] = 0;
                }
              }
            
              HAL_GPIO_WritePin(GPIOA, GPIO_PIN_11, SET);
              while(HAL_CAN_GetTxMailboxesFreeLevel(&hcan) == 0);
            
              if(HAL_CAN_AddTxMessage(&hcan, &TxHeader, TxData, &TxMailbox) != HAL_OK)
              {
                Error_Handler();
              }
              HAL_GPIO_WritePin(GPIOA, GPIO_PIN_11, RESET);
              frame_index += 1;
          }
        }
      }
    }
    lastSubpage = curSubpage;
  }
  /* USER CODE END 3 */
}

/**
  * @brief System Clock Configuration
  * @retval None
  */
void SystemClock_Config(void)
{
  RCC_OscInitTypeDef RCC_OscInitStruct = {0};
  RCC_ClkInitTypeDef RCC_ClkInitStruct = {0};

  /** Initializes the RCC Oscillators according to the specified parameters
  * in the RCC_OscInitTypeDef structure.
  */
  RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSE;
  RCC_OscInitStruct.HSEState = RCC_HSE_ON;
  RCC_OscInitStruct.HSEPredivValue = RCC_HSE_PREDIV_DIV2;
  RCC_OscInitStruct.HSIState = RCC_HSI_ON;
  RCC_OscInitStruct.PLL.PLLState = RCC_PLL_ON;
  RCC_OscInitStruct.PLL.PLLSource = RCC_PLLSOURCE_HSE;
  RCC_OscInitStruct.PLL.PLLMUL = RCC_PLL_MUL9;
  if (HAL_RCC_OscConfig(&RCC_OscInitStruct) != HAL_OK)
  {
    Error_Handler();
  }

  /** Initializes the CPU, AHB and APB buses clocks
  */
  RCC_ClkInitStruct.ClockType = RCC_CLOCKTYPE_HCLK|RCC_CLOCKTYPE_SYSCLK
                              |RCC_CLOCKTYPE_PCLK1|RCC_CLOCKTYPE_PCLK2;
  RCC_ClkInitStruct.SYSCLKSource = RCC_SYSCLKSOURCE_PLLCLK;
  RCC_ClkInitStruct.AHBCLKDivider = RCC_SYSCLK_DIV1;
  RCC_ClkInitStruct.APB1CLKDivider = RCC_HCLK_DIV2;
  RCC_ClkInitStruct.APB2CLKDivider = RCC_HCLK_DIV1;

  if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_2) != HAL_OK)
  {
    Error_Handler();
  }
}

/* USER CODE BEGIN 4 */
PUTCHAR_PROTOTYPE
{
  /* Place your implementation of fputc here */
  /* e.g. write a character to the USART1 and Loop until the end of transmission */
  HAL_UART_Transmit(&huart1, (uint8_t *)&ch, 1, 0xFFFF);

  return ch;
}

// Example for STM32 HAL
int _write(int file, char *ptr, int len) {
    HAL_UART_Transmit(&huart1, (uint8_t*)ptr, len, HAL_MAX_DELAY);
    return len;
}

void HAL_CAN_RxFifo0MsgPendingCallback(CAN_HandleTypeDef *CanHandle)
{
  
  /* Get RX message */
  if (HAL_CAN_GetRxMessage(CanHandle, CAN_RX_FIFO0, &RxHeader, RxData) != HAL_OK)
  {
    
    /* Reception Error */
    Error_Handler();
  }
  if (RxHeader.StdId == 0x590) //從0x590收資料
  {
    // printf("Get Data\n");
    Mode = RxData[0];  // Byte 1 = Accel (1%/LSB)
    // uint8_t brake = RxData[5];  // Byte 4 = Brake (1%/LSB)
    // Car_Brake = (brake) / 5;
    // Car_Acc = (accel) / 5;

    // printf("MODE: %d%%\r\n", Mode);
    // printf("Brake: %d%%\r\n", brake);
  }
}

/* USER CODE END 4 */

/**
  * @brief  This function is executed in case of error occurrence.
  * @retval None
  */
void Error_Handler(void)
{
  /* USER CODE BEGIN Error_Handler_Debug */
  /* User can add his own implementation to report the HAL error return state */
  __disable_irq();
  while (1)
  {
  }
  /* USER CODE END Error_Handler_Debug */
}
#ifdef USE_FULL_ASSERT
/**
  * @brief  Reports the name of the source file and the source line number
  *         where the assert_param error has occurred.
  * @param  file: pointer to the source file name
  * @param  line: assert_param error line source number
  * @retval None
  */
void assert_failed(uint8_t *file, uint32_t line)
{
  /* USER CODE BEGIN 6 */
  /* User can add his own implementation to report the file name and line number,
     ex: printf("Wrong parameters value: file %s on line %d\r\n", file, line) */
  /* USER CODE END 6 */
}
#endif /* USE_FULL_ASSERT */
