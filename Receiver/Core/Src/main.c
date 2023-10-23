/**
******************************************************************************
* @file           : Receiver Code for EEE3095S
* @brief          : Light-of-Things CS Project
******************************************************************************
* @attention
* Group Members:
* -> Tumi Mokoka (MKKBOI005)
* -> Matome Mbowene (MBWMAT002)
* -> Cassandra Wallace (WLLCAS004)
*
* Copyright (c) 2023 STMicroelectronics.
* All rights reserved.
*
* This software is licensed under terms that can be found in the LICENSE file
* in the root directory of this software component.
* If no LICENSE file comes with this software, it is provided AS-IS.
******************************************************************************
*/

/* Includes ------------------------------------------------------------------*/
#include "main.h"

/* Private includes ----------------------------------------------------------*/
#include <stdio.h>
#include "stm32f0xx.h"
#include <lcd_stm32f0.c>

/* Private typedef -----------------------------------------------------------*/

/* Private define ------------------------------------------------------------*/

/* Private macro -------------------------------------------------------------*/

/* Private variables ---------------------------------------------------------*/
ADC_HandleTypeDef hadc;
TIM_HandleTypeDef htim3;

uint32_t prev_millis = 0;
uint32_t curr_millis = 0;
uint32_t delay_t = 500; // Initialise delay to 500ms

// 16-bit for efficiency, because we know that the maximum ADC value that
// the Transmitter could send to the Receiver is 4095.
uint16_t adc_val;

// Counter for Received Data Messages
uint16_t received_counter = 0;

/* Private function prototypes -----------------------------------------------*/
void SystemClock_Config(void);
static void MX_GPIO_Init(void);
static void MX_ADC_Init(void);
static void MX_TIM3_Init(void);

void receiveMessage(void);
void decodeMessage(uint8_t message_type, uint16_t received_data, uint8_t received_parity);
uint8_t calculateParity(uint16_t data);
void displaySuccess(void);
void displayError(void);

/**
  * @brief  The application entry point.
  * @retval int
  */
int main(void)
{
  /* MCU Configuration--------------------------------------------------------*/

  /* Reset of all peripherals, Initializes the Flash interface and the Systick. */
  HAL_Init();

  /* Configure the system clock */
  SystemClock_Config();

  /* Initialize all configured peripherals */
  MX_GPIO_Init();
  MX_ADC_Init();
  MX_TIM3_Init();

  init_LCD();

  // PWM setup
  HAL_TIM_PWM_Start(&htim3, TIM_CHANNEL_3); // Start PWM on TIM3 Channel 3

  lcd_putstring("Listening for message...");

  /* Infinite loop */
  while (1)
  {
	  // Listen to the GPIO PB7 (GPIO_IDR_7) for incoming data,
	  // i.e. the SOT bit (LED HIGH)
	  if (LL_GPIO_IsInputPinSet(GPIOB, LL_GPIO_PIN_7)) {
		  receiveMessage();
	  }

	  lcd_command(CLEAR);
	  lcd_putstring("Listening for message...");
  }
}

/**
  * @brief System Clock Configuration
  * @retval None
  */
void SystemClock_Config(void)
{
  LL_FLASH_SetLatency(LL_FLASH_LATENCY_0);
  while(LL_FLASH_GetLatency() != LL_FLASH_LATENCY_0)
  {
  }
  LL_RCC_HSI_Enable();

   /* Wait till HSI is ready */
  while(LL_RCC_HSI_IsReady() != 1)
  {

  }
  LL_RCC_HSI_SetCalibTrimming(16);
  LL_RCC_HSI14_Enable();

   /* Wait till HSI14 is ready */
  while(LL_RCC_HSI14_IsReady() != 1)
  {

  }
  LL_RCC_HSI14_SetCalibTrimming(16);
  LL_RCC_SetAHBPrescaler(LL_RCC_SYSCLK_DIV_1);
  LL_RCC_SetAPB1Prescaler(LL_RCC_APB1_DIV_1);
  LL_RCC_SetSysClkSource(LL_RCC_SYS_CLKSOURCE_HSI);

   /* Wait till System clock is ready */
  while(LL_RCC_GetSysClkSource() != LL_RCC_SYS_CLKSOURCE_STATUS_HSI)
  {

  }
  LL_SetSystemCoreClock(8000000);

   /* Update the time base */
  if (HAL_InitTick (TICK_INT_PRIORITY) != HAL_OK)
  {
    Error_Handler();
  }
  LL_RCC_HSI14_EnableADCControl();
}

/**
  * @brief ADC Initialization Function
  * @param None
  * @retval None
  */
static void MX_ADC_Init(void)
{
  ADC_ChannelConfTypeDef sConfig = {0};

  /** Configure the global features of the ADC (Clock, Resolution, Data Alignment and number of conversion)
  */
  hadc.Instance = ADC1;
  hadc.Init.ClockPrescaler = ADC_CLOCK_ASYNC_DIV1;
  hadc.Init.Resolution = ADC_RESOLUTION_12B;
  hadc.Init.DataAlign = ADC_DATAALIGN_RIGHT;
  hadc.Init.ScanConvMode = ADC_SCAN_DIRECTION_FORWARD;
  hadc.Init.EOCSelection = ADC_EOC_SINGLE_CONV;
  hadc.Init.LowPowerAutoWait = DISABLE;
  hadc.Init.LowPowerAutoPowerOff = DISABLE;
  hadc.Init.ContinuousConvMode = DISABLE;
  hadc.Init.DiscontinuousConvMode = DISABLE;
  hadc.Init.ExternalTrigConv = ADC_SOFTWARE_START;
  hadc.Init.ExternalTrigConvEdge = ADC_EXTERNALTRIGCONVEDGE_NONE;
  hadc.Init.DMAContinuousRequests = DISABLE;
  hadc.Init.Overrun = ADC_OVR_DATA_PRESERVED;
  if (HAL_ADC_Init(&hadc) != HAL_OK)
  {
    Error_Handler();
  }

  /** Configure for the selected ADC regular channel to be converted.
  */
  sConfig.Channel = ADC_CHANNEL_6;
  sConfig.Rank = ADC_RANK_CHANNEL_NUMBER;
  sConfig.SamplingTime = ADC_SAMPLETIME_1CYCLE_5;
  if (HAL_ADC_ConfigChannel(&hadc, &sConfig) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN ADC_Init 2 */
  ADC1->CR |= ADC_CR_ADCAL;
  while(ADC1->CR & ADC_CR_ADCAL);			// Calibrate the ADC
  ADC1->CR |= (1 << 0);						// Enable ADC
  while((ADC1->ISR & (1 << 0)) == 0);		// Wait for ADC ready
  /* USER CODE END ADC_Init 2 */
}

/**
  * @brief TIM3 Initialization Function
  * @param None
  * @retval None
  */
static void MX_TIM3_Init(void)
{
  TIM_ClockConfigTypeDef sClockSourceConfig = {0};
  TIM_MasterConfigTypeDef sMasterConfig = {0};
  TIM_OC_InitTypeDef sConfigOC = {0};

  htim3.Instance = TIM3;
  htim3.Init.Prescaler = 0;
  htim3.Init.CounterMode = TIM_COUNTERMODE_UP;
  htim3.Init.Period = 47999;
  htim3.Init.ClockDivision = TIM_CLOCKDIVISION_DIV1;
  htim3.Init.AutoReloadPreload = TIM_AUTORELOAD_PRELOAD_DISABLE;
  if (HAL_TIM_Base_Init(&htim3) != HAL_OK)
  {
    Error_Handler();
  }
  sClockSourceConfig.ClockSource = TIM_CLOCKSOURCE_INTERNAL;
  if (HAL_TIM_ConfigClockSource(&htim3, &sClockSourceConfig) != HAL_OK)
  {
    Error_Handler();
  }
  if (HAL_TIM_PWM_Init(&htim3) != HAL_OK)
  {
    Error_Handler();
  }
  sMasterConfig.MasterOutputTrigger = TIM_TRGO_RESET;
  sMasterConfig.MasterSlaveMode = TIM_MASTERSLAVEMODE_DISABLE;
  if (HAL_TIMEx_MasterConfigSynchronization(&htim3, &sMasterConfig) != HAL_OK)
  {
    Error_Handler();
  }
  sConfigOC.OCMode = TIM_OCMODE_PWM1;
  sConfigOC.Pulse = 0;
  sConfigOC.OCPolarity = TIM_OCPOLARITY_HIGH;
  sConfigOC.OCFastMode = TIM_OCFAST_DISABLE;
  if (HAL_TIM_PWM_ConfigChannel(&htim3, &sConfigOC, TIM_CHANNEL_3) != HAL_OK)
  {
    Error_Handler();
  }

  HAL_TIM_MspPostInit(&htim3);
}

/**
  * @brief GPIO Initialization Function
  * @param None
  * @retval None
  */
static void MX_GPIO_Init(void)
{
  //LL_EXTI_InitTypeDef EXTI_InitStruct = {0};
  LL_GPIO_InitTypeDef GPIO_InitStruct = {0};

  /* GPIO Ports Clock Enable */
  LL_AHB1_GRP1_EnableClock(LL_AHB1_GRP1_PERIPH_GPIOF);
  LL_AHB1_GRP1_EnableClock(LL_AHB1_GRP1_PERIPH_GPIOA);
  LL_AHB1_GRP1_EnableClock(LL_AHB1_GRP1_PERIPH_GPIOB);

  /**/
  LL_GPIO_ResetOutputPin(LED7_GPIO_Port, LED7_Pin);

//  /**/
//  LL_SYSCFG_SetEXTISource(LL_SYSCFG_EXTI_PORTA, LL_SYSCFG_EXTI_LINE0);
//
//  /**/
//  LL_GPIO_SetPinPull(Button0_GPIO_Port, Button0_Pin, LL_GPIO_PULL_UP);
//  LL_GPIO_SetPinPull(GPIOA, LL_GPIO_PIN_1, LL_GPIO_PULL_UP); //@
//
//  /**/
//  LL_GPIO_SetPinMode(Button0_GPIO_Port, Button0_Pin, LL_GPIO_MODE_INPUT);
//  LL_GPIO_SetPinMode(GPIOA, LL_GPIO_PIN_1, LL_GPIO_MODE_INPUT); //@
//
//  /**/
//  EXTI_InitStruct.Line_0_31 = LL_EXTI_LINE_0;
//  EXTI_InitStruct.LineCommand = ENABLE;
//  EXTI_InitStruct.Mode = LL_EXTI_MODE_IT;
//  EXTI_InitStruct.Trigger = LL_EXTI_TRIGGER_RISING;
//  LL_EXTI_Init(&EXTI_InitStruct);
//
//  /**/
//  GPIO_InitStruct.Pin = LED7_Pin;
//  GPIO_InitStruct.Mode = LL_GPIO_MODE_OUTPUT;
//  GPIO_InitStruct.Speed = LL_GPIO_SPEED_FREQ_LOW;
//  GPIO_InitStruct.OutputType = LL_GPIO_OUTPUT_PUSHPULL;
//  GPIO_InitStruct.Pull = LL_GPIO_PULL_NO;
//  LL_GPIO_Init(LED7_GPIO_Port, &GPIO_InitStruct);
//
//  /* USER CODE BEGIN MX_GPIO_Init_2 */
//  HAL_NVIC_SetPriority(EXTI0_1_IRQn, 0, 0);
//  HAL_NVIC_EnableIRQ(EXTI0_1_IRQn);
//  /* USER CODE END MX_GPIO_Init_2 */

  // Configure PB7 (GPIO_IDR_7) as input for receiving data
  GPIO_InitStruct.Pin = GPIO_PIN_7;
  GPIO_InitStruct.Mode = LL_GPIO_MODE_INPUT;
  GPIO_InitStruct.Pull = LL_GPIO_PULL_NO;
  LL_GPIO_Init(GPIOB, &GPIO_InitStruct);

  // Configure PB0 (LED0) as an output for indicating success or error
  GPIO_InitStruct.Pin = LL_GPIO_PIN_0;
  GPIO_InitStruct.Mode = LL_GPIO_MODE_OUTPUT;
  GPIO_InitStruct.Speed = LL_GPIO_SPEED_FREQ_LOW;
  GPIO_InitStruct.OutputType = LL_GPIO_OUTPUT_PUSHPULL;
  LL_GPIO_Init(GPIOB, &GPIO_InitStruct);
}

void receiveMessage(void) {

	// If this function is invoked, it means that the SOT of a message was found
	delay(200000);

	// Message Identifier
	// If pin is low next -> Data Message
	// If pin is high next -> Checkpoint Message
	uint8_t message_type = LL_GPIO_IsInputPinSet(GPIOB, LL_GPIO_PIN_7);
	delay(200000);

	uint16_t received_data = 0;

	// Receive the data bits in Little Endian format (MSB first)
	for (int i = 0; i < 16; i++) {

		if (LL_GPIO_IsInputPinSet(GPIOB, LL_GPIO_PIN_7)) {
			received_data |= (1 << i);  // Set the bit to 1 if received
		}

		// Wait for 500ms for each bit
		delay(100000);
	}

	// At this point, received_data should hold the Data/Checkpoint Payload

	// Receive the parity bit
	uint8_t received_parity = LL_GPIO_IsInputPinSet(GPIOB, LL_GPIO_PIN_7);
	delay(100000);

	// Receive the stop bit (EOT)

	// Message Successfully Received
	decodeMessage(message_type, received_data, received_parity);
}

void decodeMessage(uint8_t message_type, uint16_t received_data, uint8_t received_parity) {

    // Calculate the expected parity for the received data
    uint8_t expected_parity = calculateParity(received_data);

    // Check if the received data has the correct parity
    if (received_parity == expected_parity) {
        // If parity is correct, it's a valid message

        // Check if it's a Checkpoint Message or a Data Message
        if (message_type == 1) {

        	lcd_command(CLEAR);

            // Checkpoint Message
            if (received_data == received_counter) {
                // Counter matches the expected value
            	lcd_putstring("Checkpoint Successful!");

            	lcd_command(LINE_TWO);
            	char lcd_message[20];
            	sprintf(lcd_message, "All OK %d->%d", received_counter, received_data);
            	lcd_putstring(lcd_message);

                displaySuccess();

            } else {
                // Counter does not match the expected value
            	lcd_putstring("Counter Mismatch!");

            	lcd_command(LINE_TWO);
				char lcd_message[20];
				sprintf(lcd_message, "Updating %d->%d", received_counter, received_data);
				lcd_putstring(lcd_message);

				received_counter = received_data;

                displayError();
            }

        } else if (message_type == 0) {

            // Data Message
            // Display the received ADC value on the LCD
            char lcd_message[20];
            sprintf(lcd_message, "Received ADC: %d", received_data);
            lcd_command(CLEAR);
            lcd_putstring(lcd_message);

            // Successful receive
            displaySuccess();
            received_counter++;
        }

    } else {
        // Parity error - corruption has occured
    	// Display error on LED0
        displayError();
    }
}

/**
 * @brief   Calculate the parity bit for a 16-bit data value.
 *
 * This function calculates the parity bit for a 16-bit data value by
 * performing an XOR operation on all the individual bits. Parity is used
 * to check for data integrity and to ensure that the number of set bits in
 * the data is even or odd.
 *
 * @param   data The 16-bit data for which to calculate the parity.
 * @return  The calculated parity bit (0 or 1) representing data integrity.
 */
uint8_t calculateParity(uint16_t data) {
    uint8_t parity = 0;
    for (int i = 0; i < 16; i++) {
    	// XOR each bit to calculate the parity
        parity ^= (data >> i) & 0x01;
    }
    return parity;
}

/**
 * @brief   Display success by turning on LED0 (PB0) for 2 seconds.
 */
void displaySuccess(void) {
    LL_GPIO_SetOutputPin(GPIOB, LL_GPIO_PIN_0);
    delay(200000);  // Turn on LED0 for 2 seconds
    LL_GPIO_ResetOutputPin(GPIOB, LL_GPIO_PIN_0);
}

/**
 * @brief   Display error by flashing LED0 (PB0) for 2 seconds with 100ms intervals.
 */
void displayError(void) {
    for (int i = 0; i < 20; i++) {
        HAL_GPIO_TogglePin(GPIOB, LL_GPIO_PIN_0);
        delay(10000);
    }
    LL_GPIO_ResetOutputPin(GPIOB, LL_GPIO_PIN_0);
}

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

#ifdef  USE_FULL_ASSERT
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
