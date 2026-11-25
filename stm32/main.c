/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file           : main.c
  * @brief          : 9주차 (마지막) — 가변저항 + HC-06 블루투스 + I2C LCD
  *
  *  ★ 바꿀 곳은 딱 하나 :  아래 TEAM_NAME  ("TEAM03" → 우리 조 번호)
  *
  *  하는 일
  *   - 처음 켤 때 HC-06 이름을 TEAM_NAME 으로 바꾼다 (AT 명령)
  *   - 1초마다 가변저항 값을  LCD 윗줄 + USB 창 + 블루투스 창 으로 보낸다
  *   - 노트북에서 한 줄(Enter)을 보내면 LCD 아랫줄에 띄운다  (BT : / USB: 이름표)
  ******************************************************************************
  */
/* USER CODE END Header */
/* Includes ------------------------------------------------------------------*/
#include "main.h"

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */
#include <stdio.h>
#include <string.h>
/* USER CODE END Includes */

/* Private typedef -----------------------------------------------------------*/
/* USER CODE BEGIN PTD */

/* USER CODE END PTD */

/* Private define ------------------------------------------------------------*/
/* USER CODE BEGIN PD */
#define TEAM_NAME   "TEAM03"        // ★ 우리 조 이름 — 조마다 다르게 (영어 · 숫자)
#define SET_NAME    1               //   1 = 켤 때 HC-06 이름 바꾸기,  이름 바뀐 것 확인하면 0 으로
#define LCD_ADDR    (0x27 << 1)     //   LCD 주소 — 지난주 스캔이 0x3F 였으면 (0x3F << 1)
/* USER CODE END PD */

/* Private macro -------------------------------------------------------------*/
/* USER CODE BEGIN PM */

/* USER CODE END PM */

/* Private variables ---------------------------------------------------------*/
ADC_HandleTypeDef hadc1;

I2C_HandleTypeDef hi2c1;

UART_HandleTypeDef huart1;
UART_HandleTypeDef huart2;

/* USER CODE BEGIN PV */
uint8_t rx1, rx2;                   // 블루투스(USART1) · USB(USART2) 로 받은 글자 하나
char    line_buf[13];               // 바구니 — LCD 아랫줄 16칸 = 이름표 4칸 + 글자 12칸
uint8_t line_len = 0;               // 담은 글자 수
volatile uint8_t line_ready = 0;    // 깃발 — 1 이면 한 줄 완성
volatile uint8_t line_from  = 0;    // 누가 보냈나 — 1 = USB,  2 = 블루투스
/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
void SystemClock_Config(void);
static void MX_GPIO_Init(void);
static void MX_USART2_UART_Init(void);
static void MX_ADC1_Init(void);
static void MX_USART1_UART_Init(void);
static void MX_I2C1_Init(void);
/* USER CODE BEGIN PFP */

/* USER CODE END PFP */

/* Private user code ---------------------------------------------------------*/
/* USER CODE BEGIN 0 */
/* ---------- printf → USB 창 + 블루투스 창 둘 다 ---------- */
int __io_putchar(int ch)
{
    HAL_UART_Transmit(&huart2, (uint8_t *)&ch, 1, HAL_MAX_DELAY);   // USB (STLink)
    HAL_UART_Transmit(&huart1, (uint8_t *)&ch, 1, HAL_MAX_DELAY);   // 블루투스 (HC-06)
    return ch;
}

/* ---------- 가변저항 : 8번 읽어서 평균 ---------- */
uint16_t adc_read_avg(void)
{
    uint32_t sum = 0;
    for (int i = 0; i < 8; i++) {
        HAL_ADC_Start(&hadc1);
        HAL_ADC_PollForConversion(&hadc1, 10);
        sum += HAL_ADC_GetValue(&hadc1);
        HAL_ADC_Stop(&hadc1);
    }
    return sum / 8;
}

/* ---------- HC-06 AT 명령 : 보내고, 답을 받아서 USB 창에 ---------- */
void at_send(const char *cmd)
{
    char resp[24] = {0};
    HAL_UART_Transmit(&huart1, (uint8_t *)cmd, strlen(cmd), 100);
    HAL_UART_Receive(&huart1, (uint8_t *)resp, sizeof(resp) - 1, 1500);
    char line[64];
    sprintf(line, "%s  ->  %s\r\n", cmd, resp);
    HAL_UART_Transmit(&huart2, (uint8_t *)line, strlen(line), 100);   // USB 창에만
}

/* ---------- I2C LCD1602 (뒷판 PCF8574) ---------- */
void lcd_send_cmd(char cmd)                 // 명령 (RS = 0)
{
    uint8_t dh = cmd & 0xF0;
    uint8_t dl = (cmd << 4) & 0xF0;
    uint8_t d[4] = { dh | 0x0C, dh | 0x08, dl | 0x0C, dl | 0x08 };
    HAL_I2C_Master_Transmit(&hi2c1, LCD_ADDR, d, 4, 100);
}

void lcd_send_data(char ch)                 // 글자 (RS = 1)
{
    uint8_t dh = ch & 0xF0;
    uint8_t dl = (ch << 4) & 0xF0;
    uint8_t d[4] = { dh | 0x0D, dh | 0x09, dl | 0x0D, dl | 0x09 };
    HAL_I2C_Master_Transmit(&hi2c1, LCD_ADDR, d, 4, 100);
}

static void lcd_half(uint8_t nib)           // 깨울 때만 쓰는 반쪽
{
    uint8_t d[2] = { nib | 0x0C, nib | 0x08 };
    HAL_I2C_Master_Transmit(&hi2c1, LCD_ADDR, d, 2, 100);
}

void lcd_init(void)
{
    HAL_Delay(50);
    lcd_half(0x30);  HAL_Delay(5);
    lcd_half(0x30);  HAL_Delay(1);
    lcd_half(0x30);  HAL_Delay(1);
    lcd_half(0x20);  HAL_Delay(1);          // 4비트 모드
    lcd_send_cmd(0x28);  HAL_Delay(1);      // 2줄, 5x8
    lcd_send_cmd(0x08);  HAL_Delay(1);      // 화면 끄기
    lcd_send_cmd(0x01);  HAL_Delay(2);      // 지우기
    lcd_send_cmd(0x06);  HAL_Delay(1);      // 쓰면 오른쪽으로
    lcd_send_cmd(0x0C);  HAL_Delay(1);      // 화면 켜기, 커서 숨김
}

void lcd_put_cur(int row, int col)
{
    lcd_send_cmd((row == 0 ? 0x80 : 0xC0) | col);
}

void lcd_send_string(const char *str)
{
    while (*str) lcd_send_data(*str++);
}

/* ---------- 받은 글자를 바구니에 — Enter 가 오면 깃발 ---------- */
void collect(uint8_t c, uint8_t from)
{
    if (c == '\r' || c == '\n') {
        if (line_len > 0) {
            line_buf[line_len] = '\0';
            line_from  = from;
            line_ready = 1;
        }
    } else if (!line_ready && line_len < 12) {
        line_buf[line_len++] = c;
    }
}
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
  MX_USART2_UART_Init();
  MX_ADC1_Init();
  MX_USART1_UART_Init();
  MX_I2C1_Init();
  /* USER CODE BEGIN 2 */
  lcd_init();
  lcd_put_cur(0, 0);
  lcd_send_string(TEAM_NAME);
  lcd_put_cur(1, 0);
  lcd_send_string("BT READY");

#if SET_NAME
  HAL_Delay(1000);                          // HC-06 이 켜질 때까지
  at_send("AT");                            // 답 : OK
  at_send("AT+NAME" TEAM_NAME);             // 답 : OKsetname
#endif

  HAL_UART_Receive_IT(&huart2, &rx2, 1);    // USB 로 오는 글자 받기 시작
  HAL_UART_Receive_IT(&huart1, &rx1, 1);    // 블루투스로 오는 글자 받기 시작
  printf("%s start\r\n", TEAM_NAME);

  uint32_t last = 0;                        // 마지막으로 값을 보낸 시각
  /* USER CODE END 2 */

  /* Infinite loop */
  /* USER CODE BEGIN WHILE */
  while (1)
  {
    /* USER CODE END WHILE */

    /* USER CODE BEGIN 3 */
    /* ① 1초마다 : 가변저항 값 → LCD 윗줄 + 노트북 */
    if (HAL_GetTick() - last >= 1000) {
        last = HAL_GetTick();
        uint16_t adc = adc_read_avg();
        int mv = (uint32_t)adc * 3300 / 4096;

        char line[20];
        sprintf(line, "ADC:%4d %d.%03dV", adc, mv / 1000, mv % 1000);
        lcd_put_cur(0, 0);
        lcd_send_string(line);

        printf("adc = %4d  mv = %4d\r\n", adc, mv);
    }

    /* ② 노트북에서 한 줄이 왔으면 : LCD 아랫줄 */
    if (line_ready) {
        const char *who = (line_from == 2) ? "BT " : "USB";
        char row[20];
        sprintf(row, "%s:%-12s", who, line_buf);
        lcd_put_cur(1, 0);
        lcd_send_string(row);
        printf(">>> LCD <- '%s' from %s\r\n", line_buf, who);
        line_len = 0;
        line_ready = 0;
    }
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
  RCC_PeriphCLKInitTypeDef PeriphClkInit = {0};

  /** Initializes the RCC Oscillators according to the specified parameters
  * in the RCC_OscInitTypeDef structure.
  */
  RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSI;
  RCC_OscInitStruct.HSIState = RCC_HSI_ON;
  RCC_OscInitStruct.HSICalibrationValue = RCC_HSICALIBRATION_DEFAULT;
  RCC_OscInitStruct.PLL.PLLState = RCC_PLL_ON;
  RCC_OscInitStruct.PLL.PLLSource = RCC_PLLSOURCE_HSI_DIV2;
  RCC_OscInitStruct.PLL.PLLMUL = RCC_PLL_MUL16;
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
  PeriphClkInit.PeriphClockSelection = RCC_PERIPHCLK_ADC;
  PeriphClkInit.AdcClockSelection = RCC_ADCPCLK2_DIV6;
  if (HAL_RCCEx_PeriphCLKConfig(&PeriphClkInit) != HAL_OK)
  {
    Error_Handler();
  }
}

/**
  * @brief ADC1 Initialization Function
  * @param None
  * @retval None
  */
static void MX_ADC1_Init(void)
{

  /* USER CODE BEGIN ADC1_Init 0 */

  /* USER CODE END ADC1_Init 0 */

  ADC_ChannelConfTypeDef sConfig = {0};

  /* USER CODE BEGIN ADC1_Init 1 */

  /* USER CODE END ADC1_Init 1 */

  /** Common config
  */
  hadc1.Instance = ADC1;
  hadc1.Init.ScanConvMode = ADC_SCAN_DISABLE;
  hadc1.Init.ContinuousConvMode = DISABLE;
  hadc1.Init.DiscontinuousConvMode = DISABLE;
  hadc1.Init.ExternalTrigConv = ADC_SOFTWARE_START;
  hadc1.Init.DataAlign = ADC_DATAALIGN_RIGHT;
  hadc1.Init.NbrOfConversion = 1;
  if (HAL_ADC_Init(&hadc1) != HAL_OK)
  {
    Error_Handler();
  }

  /** Configure Regular Channel
  */
  sConfig.Channel = ADC_CHANNEL_4;
  sConfig.Rank = ADC_REGULAR_RANK_1;
  sConfig.SamplingTime = ADC_SAMPLETIME_55CYCLES_5;
  if (HAL_ADC_ConfigChannel(&hadc1, &sConfig) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN ADC1_Init 2 */

  /* USER CODE END ADC1_Init 2 */

}

/**
  * @brief I2C1 Initialization Function
  * @param None
  * @retval None
  */
static void MX_I2C1_Init(void)
{

  /* USER CODE BEGIN I2C1_Init 0 */

  /* USER CODE END I2C1_Init 0 */

  /* USER CODE BEGIN I2C1_Init 1 */

  /* USER CODE END I2C1_Init 1 */
  hi2c1.Instance = I2C1;
  hi2c1.Init.ClockSpeed = 100000;
  hi2c1.Init.DutyCycle = I2C_DUTYCYCLE_2;
  hi2c1.Init.OwnAddress1 = 0;
  hi2c1.Init.AddressingMode = I2C_ADDRESSINGMODE_7BIT;
  hi2c1.Init.DualAddressMode = I2C_DUALADDRESS_DISABLE;
  hi2c1.Init.OwnAddress2 = 0;
  hi2c1.Init.GeneralCallMode = I2C_GENERALCALL_DISABLE;
  hi2c1.Init.NoStretchMode = I2C_NOSTRETCH_DISABLE;
  if (HAL_I2C_Init(&hi2c1) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN I2C1_Init 2 */

  /* USER CODE END I2C1_Init 2 */

}

/**
  * @brief USART1 Initialization Function
  * @param None
  * @retval None
  */
static void MX_USART1_UART_Init(void)
{

  /* USER CODE BEGIN USART1_Init 0 */

  /* USER CODE END USART1_Init 0 */

  /* USER CODE BEGIN USART1_Init 1 */

  /* USER CODE END USART1_Init 1 */
  huart1.Instance = USART1;
  huart1.Init.BaudRate = 9600;
  huart1.Init.WordLength = UART_WORDLENGTH_8B;
  huart1.Init.StopBits = UART_STOPBITS_1;
  huart1.Init.Parity = UART_PARITY_NONE;
  huart1.Init.Mode = UART_MODE_TX_RX;
  huart1.Init.HwFlowCtl = UART_HWCONTROL_NONE;
  huart1.Init.OverSampling = UART_OVERSAMPLING_16;
  if (HAL_UART_Init(&huart1) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN USART1_Init 2 */

  /* USER CODE END USART1_Init 2 */

}

/**
  * @brief USART2 Initialization Function
  * @param None
  * @retval None
  */
static void MX_USART2_UART_Init(void)
{

  /* USER CODE BEGIN USART2_Init 0 */

  /* USER CODE END USART2_Init 0 */

  /* USER CODE BEGIN USART2_Init 1 */

  /* USER CODE END USART2_Init 1 */
  huart2.Instance = USART2;
  huart2.Init.BaudRate = 115200;
  huart2.Init.WordLength = UART_WORDLENGTH_8B;
  huart2.Init.StopBits = UART_STOPBITS_1;
  huart2.Init.Parity = UART_PARITY_NONE;
  huart2.Init.Mode = UART_MODE_TX_RX;
  huart2.Init.HwFlowCtl = UART_HWCONTROL_NONE;
  huart2.Init.OverSampling = UART_OVERSAMPLING_16;
  if (HAL_UART_Init(&huart2) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN USART2_Init 2 */

  /* USER CODE END USART2_Init 2 */

}

/**
  * @brief GPIO Initialization Function
  * @param None
  * @retval None
  */
static void MX_GPIO_Init(void)
{
  GPIO_InitTypeDef GPIO_InitStruct = {0};
  /* USER CODE BEGIN MX_GPIO_Init_1 */

  /* USER CODE END MX_GPIO_Init_1 */

  /* GPIO Ports Clock Enable */
  __HAL_RCC_GPIOC_CLK_ENABLE();
  __HAL_RCC_GPIOD_CLK_ENABLE();
  __HAL_RCC_GPIOA_CLK_ENABLE();
  __HAL_RCC_GPIOB_CLK_ENABLE();

  /*Configure GPIO pin Output Level */
  HAL_GPIO_WritePin(LD2_GPIO_Port, LD2_Pin, GPIO_PIN_RESET);

  /*Configure GPIO pin : B1_Pin */
  GPIO_InitStruct.Pin = B1_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_IT_RISING;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  HAL_GPIO_Init(B1_GPIO_Port, &GPIO_InitStruct);

  /*Configure GPIO pin : LD2_Pin */
  GPIO_InitStruct.Pin = LD2_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
  HAL_GPIO_Init(LD2_GPIO_Port, &GPIO_InitStruct);


  /* USER CODE BEGIN MX_GPIO_Init_2 */

  /* USER CODE END MX_GPIO_Init_2 */
}

/* USER CODE BEGIN 4 */
/* ---------- 글자 하나가 들어올 때마다 (인터럽트) ---------- */
void HAL_UART_RxCpltCallback(UART_HandleTypeDef *huart)
{
    if (huart->Instance == USART2) {        // USB
        collect(rx2, 1);
        HAL_UART_Receive_IT(&huart2, &rx2, 1);
    }
    if (huart->Instance == USART1) {        // 블루투스
        collect(rx1, 2);
        HAL_UART_Receive_IT(&huart1, &rx1, 1);
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

#ifdef  USE_FULL_ASSERT
/**
  * @brief  Reports the name of the source file and the line number
  *         where the assert_param() failed.
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
