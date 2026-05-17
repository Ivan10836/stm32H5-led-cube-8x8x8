#include "main.h"
#include "stm32h5xx_hal_gpio.h"
#include "stm32h5xx_hal_tim.h"
#include <stdint.h>
#include <stdbool.h>
#include "arm_math.h"
#include "arm_const_structs.h"

static TIM_HandleTypeDef *Tim1;
static uint32_t Channel1;

static TIM_HandleTypeDef *Tim2;
static uint32_t Channel2;

static bool DMAUseBuff1 = false;
static bool TranslateCompleat = false;
static bool FrameCompleat = false;

#define DataToTranslateLenght (24 * 8 * 8 * 4 + 50)

static uint8_t Buff1Tim1[DataToTranslateLenght] __ALIGNED(32);
static uint8_t Buff1Tim2[DataToTranslateLenght] __ALIGNED(32);
static uint8_t Buff2Tim1[DataToTranslateLenght] __ALIGNED(32);
static uint8_t Buff2Tim2[DataToTranslateLenght] __ALIGNED(32);

#define PWM_0 93 // 0
#define PWM_1 218 // 1


static volatile uint32_t frame_time_ms = 0; 
static uint32_t start_time = 0;

static void Upload();
static void DecodeRGBFromBuffer(uint8_t *buff, uint32_t indexInBuff, uint8_t *red, uint8_t *green, uint8_t *blue);

void SetColor(uint32_t index, uint8_t red, uint8_t green, uint8_t blue);
void Frame();
uint32_t GetIndex(uint8_t x, uint8_t y, uint8_t z);
void GetColorThisFrame(uint32_t index, uint8_t *red, uint8_t *green, uint8_t *blue);
void GetColorLastFrame(uint32_t index, uint8_t *red, uint8_t *green, uint8_t *blue);

//93 - 0/*  */
//218 - 1

static void Loop(){
    while(1){
        if(TranslateCompleat && FrameCompleat){

            frame_time_ms = HAL_GetTick() - start_time;
            start_time = HAL_GetTick();

            Upload();
        }
    }
}

static void Upload(){
    TranslateCompleat = false;
    FrameCompleat = false;
    DMAUseBuff1 = !DMAUseBuff1;
    if (DMAUseBuff1) {
        HAL_TIM_PWM_Start_DMA(Tim1, Channel1, (uint32_t *)Buff1Tim1, DataToTranslateLenght);
        HAL_TIM_PWM_Start_DMA(Tim2, Channel2, (uint32_t *)Buff1Tim2, DataToTranslateLenght);
    } else {
        HAL_TIM_PWM_Start_DMA(Tim1, Channel1, (uint32_t *)Buff2Tim1, DataToTranslateLenght);
        HAL_TIM_PWM_Start_DMA(Tim2, Channel2, (uint32_t *)Buff2Tim2, DataToTranslateLenght);
    }
    Frame();

    if(TranslateCompleat) HAL_GPIO_WritePin(Led_GPIO_Port, Led_Pin, GPIO_PIN_SET);  
    else HAL_GPIO_WritePin(Led_GPIO_Port, Led_Pin, GPIO_PIN_RESET);

    FrameCompleat = true;
}

static void DecodeRGBFromBuffer(uint8_t *buff, uint32_t indexInBuff, uint8_t *red, uint8_t *green, uint8_t *blue) {
    uint32_t offset = indexInBuff * 24;
    uint32_t color = 0;

    for (int i = 0; i < 24; i++) {
        color <<= 1;
        // Если значение в буфере ближе к PWM_1 (218), считаем это единицей
        // Используем порог (93 + 218) / 2 = ~155 для надежности
        if (buff[offset + i] > 155) {
            color |= 1;
        }
    }

    *green = (uint8_t)((color >> 16) & 0xFF);
    *red   = (uint8_t)((color >> 8) & 0xFF);
    *blue  = (uint8_t)(color & 0xFF);
}

void WS2812B_Init(TIM_HandleTypeDef *htim1, uint32_t channel1, TIM_HandleTypeDef *htim2, uint32_t channel2)
{
    Tim1 = htim1;
    Channel1 = channel1;
    Tim2 = htim2;
    Channel2 = channel2;

    __HAL_TIM_MOE_ENABLE(Tim1);
    __HAL_TIM_MOE_ENABLE(Tim2);

    for(uint32_t i = 0; i < DataToTranslateLenght; i++)
    {
        uint8_t val = (i >= 8 * 8 * 4 * 24) ? 0 : 93;
        Buff1Tim1[i] = val;
        Buff1Tim2[i] = val;
        Buff2Tim1[i] = val;
        Buff2Tim2[i] = val;
    }

    Upload();
    Loop();
}

uint32_t GetIndex(uint8_t x, uint8_t y, uint8_t z) {
    uint8_t x_final = x;
    uint8_t z_final = z;

    // 1. Зеркалим слой по X (если слой 1, 3, 5, 7)
    if (y % 2 != 0) {
        x_final = 7 - x;
    }

    // 2. Логика зигзага по оси Z (зависит от x_final)
    if (x_final % 2 != 0) {
        z_final = 7 - z;
    }

    // 3. Итоговый расчет
    // y * 64 (слой) + x_final * 8 (теперь x определяет "ряд") + z_final
    return (y * 64) + (x_final * 8) + z_final;
}

void SetColor(uint32_t index, uint8_t red, uint8_t green, uint8_t blue)
{
    uint8_t *buff = NULL;
    uint32_t indexInBuff = 0;

    if (index < 256) {
        indexInBuff = index; 
        buff = (DMAUseBuff1) ? Buff2Tim1 : Buff1Tim1;
    } else {
        indexInBuff = index - 256;
        buff = (DMAUseBuff1) ? Buff2Tim2 : Buff1Tim2;
    }

    if (buff == NULL) return;

    uint32_t offset = indexInBuff * 24;

    uint32_t color = ((uint32_t)green << 16) | ((uint32_t)red << 8) | blue;

    for (int i = 23; i >= 0; i--) {
        if (color & (1 << i)) {
            buff[offset++] = PWM_1;
        } else {
            buff[offset++] = PWM_0;
        }
    }
}

void GetColorThisFrame(uint32_t index, uint8_t *red, uint8_t *green, uint8_t *blue)
{
    uint8_t *buff = NULL;
    uint32_t indexInBuff = 0;

    // "This Frame" — это буфер, в который мы СЕЙЧАС пишем (противоположный тому, что шлет DMA)
    if (index < 256) {
        indexInBuff = index;
        buff = (DMAUseBuff1) ? Buff2Tim1 : Buff1Tim1;
    } else {
        indexInBuff = index - 256;
        buff = (DMAUseBuff1) ? Buff2Tim2 : Buff1Tim2;
    }

    if (buff != NULL) {
        DecodeRGBFromBuffer(buff, indexInBuff, red, green, blue);
    }
}

void GetColorLastFrame(uint32_t index, uint8_t *red, uint8_t *green, uint8_t *blue)
{
    uint8_t *buff = NULL;
    uint32_t indexInBuff = 0;

    // "Last Frame" — это буфер, который СЕЙЧАС читает DMA
    if (index < 256) {
        indexInBuff = index;
        buff = (DMAUseBuff1) ? Buff1Tim1 : Buff2Tim1;
    } else {
        indexInBuff = index - 256;
        buff = (DMAUseBuff1) ? Buff1Tim2 : Buff2Tim2;
    }

    if (buff != NULL) {
        DecodeRGBFromBuffer(buff, indexInBuff, red, green, blue);
    }
}

//interupt
void HAL_TIM_PWM_PulseFinishedCallback(TIM_HandleTypeDef *htim) {
    if (htim->Instance == Tim2->Instance) {
        HAL_TIM_PWM_Stop_DMA(Tim1, Channel1);
        HAL_TIM_PWM_Stop_DMA(Tim2, Channel2);
        
        TranslateCompleat = true;
    }
}
int32_t GetMicroValueWithStartOfset(uint32_t timeIndex);
int32_t GetCleanMicroValue(uint32_t timeIndex);
uint32_t GetMicroValueCentred ();
float32_t* GetMicrophoneSpectrum(void);
#include "frame.h"