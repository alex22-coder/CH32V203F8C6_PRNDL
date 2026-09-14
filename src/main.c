/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file           : main.c
  * @brief          : Main program body for CH32V203C8T6
  *                   HSE 8 MHz direct (no PLL), CAN = 500 kbps
  ******************************************************************************
  */
/* USER CODE END Header */

#include "ch32v20x.h"
#include "debug.h"

#define SUCCESS 1 

/* USER CODE BEGIN PD */

#if     defined(MURANO_Z50)
    #define TARGET_ID   0x255
    #define DLC_        8
    #define MODIFY_BYTE 5
#elif   defined(JUKE)
    #define TARGET_ID   0x421
    #define DLC_        3
    #define MODIFY_BYTE 0
#elif   defined(NOUT_3minus)
    #define TARGET_ID   0x255
    #define DLC_        8
    #define MODIFY_BYTE 5
#elif   defined(MURANO_Z50_PRND2_3minus)
    #define TARGET_ID   0x255
    #define DLC_        8
    #define MODIFY_BYTE 5
#endif

/* USER CODE END PD */

/* Private variables ---------------------------------------------------------*/
volatile uint8_t  last_sent_byte = 0;
volatile uint32_t millis_count   = 0;   /* для ESP-режима */

/* Private function prototypes -----------------------------------------------*/
static void SetClockTo8MHzHSE(void);
static void App_GPIO_Init(void);
static void App_CAN_Init(void);
static void App_TIM2_Init(void);
static void CAN_SendFrame(uint32_t id, uint8_t *data, uint8_t len);
static inline uint8_t count_bits(uint8_t byte);

void TIM2_IRQHandler(void) __attribute__((interrupt("WCH-Interrupt-fast")));

/* ============================================================================
 * Принудительная настройка тактирования на чистые 8 МГц от HSE
 * ==========================================================================*/
static void SetClockTo8MHzHSE(void)
{
    /* Включаем внешний кварц */
    RCC_HSEConfig(RCC_HSE_ON);
    if (RCC_WaitForHSEStartUp() != SUCCESS) {
        while (1);   /* кварц не запустился */
    }

    /* Делители шин AHB / APB1 / APB2 — без деления */
    RCC_HCLKConfig(RCC_SYSCLK_Div1);
    RCC_PCLK1Config(RCC_HCLK_Div1);
    RCC_PCLK2Config(RCC_HCLK_Div1);

    /* Переключаем SYSCLK на HSE напрямую */
    RCC_SYSCLKConfig(RCC_SYSCLKSource_HSE);

    /* Ждём подтверждения переключения */
    while (RCC_GetSYSCLKSource() != 0x04);

    /* Обновляем глобальную переменную частоты */
    SystemCoreClockUpdate();
}

/* ============================================================================
 * Точка входа
 * ==========================================================================*/
int main(void)
{
    /* Задержки WCH (настраивает SysTick под текущую частоту) */
    Delay_Init();

    /* Переключаем чип на 8 МГц от кварца */
    SetClockTo8MHzHSE();

    /* Заново инициализируем Delay, чтобы SysTick пересчитался под 8 МГц */
    Delay_Init();

    /* Группа приоритетов прерываний */
    NVIC_PriorityGroupConfig(NVIC_PriorityGroup_1);

    /* Периферия */
    App_GPIO_Init();
    App_CAN_Init();
    App_TIM2_Init();

    while (1)
    {
#ifdef ESP
        static uint32_t last_send = 0;

        uint8_t data1[8] = {0xff, 0xff, 0xff, 0xff, 0x06, 0x00, 0x00, 0x00};
        uint8_t data2[7] = {0x00, 0x20, 0x00, 0x26, 0x00, 0xe7, 0x0f};
        uint8_t data3[5] = {0xff, 0x0b, 0xc0, 0x00, 0x00};

        if (millis_count - last_send >= 10)
        {
            last_send = millis_count;

            CAN_SendFrame(0x174, data1, 8);
            CAN_SendFrame(0x176, data2, 7);
            CAN_SendFrame(0x177, data3, 5);
        }
#endif
    }
}

/* ============================================================================
 * GPIO
 * ==========================================================================*/
static void App_GPIO_Init(void)
{
    GPIO_InitTypeDef GPIO_InitStructure = {0};

    RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOA |
                           RCC_APB2Periph_GPIOB, ENABLE);

    /* PA2..PA6 — входы с подтяжкой вниз */
    GPIO_InitStructure.GPIO_Pin  = GPIO_Pin_2 | GPIO_Pin_3 |
                                   GPIO_Pin_4 | GPIO_Pin_5 | GPIO_Pin_6;
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_IPD;
    GPIO_Init(GPIOA, &GPIO_InitStructure);

#if defined(MURANO_Z50_PRND2_3minus) || defined(NOUT_3minus)
    /* PA7 — вход с подтяжкой вверх */
    GPIO_InitStructure.GPIO_Pin  = GPIO_Pin_7;
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_IPU;
    GPIO_Init(GPIOA, &GPIO_InitStructure);
#endif

    /* PB0 — «плавающий» вход */
    GPIO_InitStructure.GPIO_Pin  = GPIO_Pin_0;
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_IN_FLOATING;
    GPIO_Init(GPIOB, &GPIO_InitStructure);
}

/* ============================================================================
 * CAN — 500 кбит/с при APB1 = 8 МГц
 *
 *   1 (SYNC) + 13 (BS1) + 2 (BS2) = 16 TQ
 *   8 000 000 / (1 × 16) = 500 000
 *   Точка выборки: (1 + 13) / 16 = 87.5 %
 * ==========================================================================*/
static void App_CAN_Init(void)
{
    GPIO_InitTypeDef        GPIO_InitStructure      = {0};
    CAN_InitTypeDef         CAN_InitStructure       = {0};
    CAN_FilterInitTypeDef   CAN_FilterInitStructure = {0};

    RCC_APB1PeriphClockCmd(RCC_APB1Periph_CAN1, ENABLE);
    RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOA, ENABLE);

    /* PA11 = CAN_RX (вход с подтяжкой вверх) */
    GPIO_InitStructure.GPIO_Pin  = GPIO_Pin_11;
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_IPU;
    GPIO_Init(GPIOA, &GPIO_InitStructure);

    /* PA12 = CAN_TX (AF push-pull) */
    GPIO_InitStructure.GPIO_Pin   = GPIO_Pin_12;
    GPIO_InitStructure.GPIO_Mode  = GPIO_Mode_AF_PP;
    GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
    GPIO_Init(GPIOA, &GPIO_InitStructure);

    /* Параметры CAN */
    CAN_InitStructure.CAN_TTCM = DISABLE;
    CAN_InitStructure.CAN_ABOM = ENABLE;
    CAN_InitStructure.CAN_AWUM = DISABLE;
    CAN_InitStructure.CAN_NART = DISABLE;
    CAN_InitStructure.CAN_RFLM = DISABLE;
    CAN_InitStructure.CAN_TXFP = DISABLE;
    CAN_InitStructure.CAN_Mode = CAN_Mode_Normal;

    /* Сетка 16 TQ под 8 МГц */
    CAN_InitStructure.CAN_SJW       = CAN_SJW_1tq;
    CAN_InitStructure.CAN_BS1       = CAN_BS1_13tq;
    CAN_InitStructure.CAN_BS2       = CAN_BS2_2tq;
    CAN_InitStructure.CAN_Prescaler = 1;

    CAN_Init(CAN1, &CAN_InitStructure);

    /* Фильтр: пропускаем всё */
    CAN_FilterInitStructure.CAN_FilterNumber          = 0;
    CAN_FilterInitStructure.CAN_FilterMode            = CAN_FilterMode_IdMask;
    CAN_FilterInitStructure.CAN_FilterScale           = CAN_FilterScale_32bit;
    CAN_FilterInitStructure.CAN_FilterIdHigh          = 0x0000;
    CAN_FilterInitStructure.CAN_FilterIdLow           = 0x0000;
    CAN_FilterInitStructure.CAN_FilterMaskIdHigh      = 0x0000;
    CAN_FilterInitStructure.CAN_FilterMaskIdLow       = 0x0000;
    CAN_FilterInitStructure.CAN_FilterFIFOAssignment  = CAN_Filter_FIFO0;
    CAN_FilterInitStructure.CAN_FilterActivation      = ENABLE;
    CAN_FilterInit(&CAN_FilterInitStructure);
}

/* ============================================================================
 * TIM2 — 10 мс (или 50 мс для JUKE) при APB1 = 8 МГц
 *
 *   Prescaler = 8000 - 1 делит 8 МГц до 1 кГц (1 тик = 1 мс)
 * ==========================================================================*/
static void App_TIM2_Init(void)
{
    TIM_TimeBaseInitTypeDef TIM_TimeBaseInitStructure = {0};
    NVIC_InitTypeDef        NVIC_InitStructure        = {0};

    RCC_APB1PeriphClockCmd(RCC_APB1Periph_TIM2, ENABLE);

    TIM_TimeBaseInitStructure.TIM_Prescaler     = 8000 - 1;
    TIM_TimeBaseInitStructure.TIM_CounterMode   = TIM_CounterMode_Up;

#if defined(JUKE)
    TIM_TimeBaseInitStructure.TIM_Period        = 50 - 1;
#else
    TIM_TimeBaseInitStructure.TIM_Period        = 10 - 1;
#endif

    TIM_TimeBaseInitStructure.TIM_ClockDivision = TIM_CKD_DIV1;
    TIM_TimeBaseInit(TIM2, &TIM_TimeBaseInitStructure);

    TIM_ITConfig(TIM2, TIM_IT_Update, ENABLE);
    TIM_Cmd(TIM2, ENABLE);

    NVIC_InitStructure.NVIC_IRQChannel                   = TIM2_IRQn;
    NVIC_InitStructure.NVIC_IRQChannelPreemptionPriority = 1;
    NVIC_InitStructure.NVIC_IRQChannelSubPriority        = 1;
    NVIC_InitStructure.NVIC_IRQChannelCmd                = ENABLE;
    NVIC_Init(&NVIC_InitStructure);
}

/* ============================================================================
 * Отправка одного CAN-кадра
 * ==========================================================================*/
static void CAN_SendFrame(uint32_t id, uint8_t *data, uint8_t len)
{
    CanTxMsg TxMessage;
    uint8_t  i;

    TxMessage.StdId = id;
    TxMessage.ExtId = 0;
    TxMessage.IDE   = CAN_Id_Standard;
    TxMessage.RTR   = CAN_RTR_Data;
    TxMessage.DLC   = len;

    for (i = 0; i < len && i < 8; i++) {
        TxMessage.Data[i] = data[i];
    }

    (void)CAN_Transmit(CAN1, &TxMessage);
}

/* ============================================================================
 * Подсчёт установленных битов
 * ==========================================================================*/
static inline uint8_t count_bits(uint8_t byte)
{
    return (uint8_t)__builtin_popcount(byte);
}

/* ============================================================================
 * Обработчик TIM2 — логика состояний КПП
 * ==========================================================================*/
void TIM2_IRQHandler(void)
{
    if (TIM_GetITStatus(TIM2, TIM_IT_Update) == RESET)
        return;

    TIM_ClearITPendingBit(TIM2, TIM_IT_Update);

#if defined(JUKE)
    millis_count += 50;
#else
    millis_count += 10;
#endif

    uint8_t data[DLC_] = {0};
    uint8_t current_byte = 0;

#if defined(MURANO_Z50)
    current_byte |= GPIO_ReadInputDataBit(GPIOA, GPIO_Pin_2) << 0;
    current_byte |= GPIO_ReadInputDataBit(GPIOA, GPIO_Pin_3) << 1;
    current_byte |= GPIO_ReadInputDataBit(GPIOA, GPIO_Pin_4) << 2;
    current_byte |= GPIO_ReadInputDataBit(GPIOA, GPIO_Pin_5) << 3;

    data[4] = 0;

    if (GPIO_ReadInputDataBit(GPIOA, GPIO_Pin_6)) {
        current_byte = 0x08;
        data[4] = 0x09;
    }

    if (current_byte != 0 && count_bits(current_byte) == 1) {
        last_sent_byte = current_byte;
    }

#elif defined(JUKE)
    current_byte |= GPIO_ReadInputDataBit(GPIOA, GPIO_Pin_2) << 3;  /* 0x08 P */
    current_byte |= GPIO_ReadInputDataBit(GPIOA, GPIO_Pin_3) << 4;  /* 0x10 R */
    current_byte |= GPIO_ReadInputDataBit(GPIOA, GPIO_Pin_5) << 5;  /* 0x20 D */

#if defined(JUKE_Ds)
    if (GPIO_ReadInputDataBit(GPIOA, GPIO_Pin_6)) current_byte = 0x21;
#else
    if (GPIO_ReadInputDataBit(GPIOA, GPIO_Pin_6)) current_byte = 0x30;
#endif
    if (GPIO_ReadInputDataBit(GPIOA, GPIO_Pin_4)) current_byte = 0x18;

    if ((current_byte != 0 && count_bits(current_byte) == 1) ||
        (current_byte == 0x18) ||
        (current_byte == 0x30) ||
        (current_byte == 0x21)) {
        last_sent_byte = current_byte;
    }

#elif defined(MURANO_Z50_PRND2_3minus)
    uint8_t input_data =
        (GPIO_ReadInputDataBit(GPIOA, GPIO_Pin_2) ? 0x01 : 0) |
        (GPIO_ReadInputDataBit(GPIOA, GPIO_Pin_3) ? 0x02 : 0) |
        (GPIO_ReadInputDataBit(GPIOA, GPIO_Pin_4) ? 0x04 : 0) |
        (GPIO_ReadInputDataBit(GPIOA, GPIO_Pin_5) ? 0x08 : 0);

    if (GPIO_ReadInputDataBit(GPIOA, GPIO_Pin_6) &&
        !GPIO_ReadInputDataBit(GPIOA, GPIO_Pin_7)) {
        input_data = 0x08;
        data[4]    = 0x09;
    }
    else if (!GPIO_ReadInputDataBit(GPIOA, GPIO_Pin_7) &&
              GPIO_ReadInputDataBit(GPIOA, GPIO_Pin_5)) {
        input_data = 0x08;
        data[4]    = 0x0A;
    }

    if ((input_data != 0 && count_bits(input_data) == 1) ||
        (input_data == 0x08)) {
        last_sent_byte = input_data;
    }

    if (!GPIO_ReadInputDataBit(GPIOA, GPIO_Pin_6) &&
         GPIO_ReadInputDataBit(GPIOA, GPIO_Pin_7)) {
        data[4] = 0;
    }

#elif defined(NOUT_3minus)
    uint8_t input_data =
        (GPIO_ReadInputDataBit(GPIOA, GPIO_Pin_2) ? 0x01 : 0) |
        (GPIO_ReadInputDataBit(GPIOA, GPIO_Pin_3) ? 0x02 : 0) |
        (GPIO_ReadInputDataBit(GPIOA, GPIO_Pin_4) ? 0x04 : 0) |
        (GPIO_ReadInputDataBit(GPIOA, GPIO_Pin_5) ? 0x08 : 0);

    data[4] = 0;

    if (GPIO_ReadInputDataBit(GPIOA, GPIO_Pin_6)) {
        input_data = 0x08;
        data[4]    = 0x09;
    }
    else if (!GPIO_ReadInputDataBit(GPIOA, GPIO_Pin_7) &&
              GPIO_ReadInputDataBit(GPIOA, GPIO_Pin_5)) {
        input_data = 0x08;
        data[4]    = 0x0A;
    }

    if (input_data != 0 && count_bits(input_data) == 1) {
        last_sent_byte = input_data;
    }
#endif

    data[MODIFY_BYTE] = last_sent_byte;
    CAN_SendFrame(TARGET_ID, data, DLC_);
}

/* ============================================================================
 * Обработчик ошибок
 * ==========================================================================*/
void Error_Handler(void)
{
    NVIC_SystemReset();
    while (1) { }
}