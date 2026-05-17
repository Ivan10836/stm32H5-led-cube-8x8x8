#include "main.h"
#include "stm32h5xx_hal_gpio.h"
#include "stm32h5xx_hal_tim.h"
#include <stdint.h>
#include <stdbool.h>
#include "arm_math.h"
#include "arm_const_structs.h"

#define ADC_BUF_LEN 10000
#define DMANow (ADC_BUF_LEN - (__HAL_DMA_GET_COUNTER(ADC->DMA_Handle) / 2))

static TIM_HandleTypeDef *ADCTim;
static ADC_HandleTypeDef *ADC;

static uint16_t adc_buffer[ADC_BUF_LEN] __ALIGNED(32);
static uint32_t offset;
int32_t GetMicroValueWithStartOfset(uint32_t timeIndex);
int32_t GetCleanMicroValue(uint32_t timeIndex);


void Microfone_Init(TIM_HandleTypeDef *htim, ADC_HandleTypeDef *hadc) {
    ADCTim = htim;
    ADC = hadc;

    HAL_ADCEx_Calibration_Start(hadc, ADC_SINGLE_ENDED);
    HAL_ADC_Start_DMA(hadc, (uint32_t*)adc_buffer, ADC_BUF_LEN);

    HAL_TIM_Base_Start(htim);

    HAL_Delay(1100);

    offset = 0;
    for (uint32_t i = 0; i < ADC_BUF_LEN; i++) {
        offset += adc_buffer[i];
    }
    offset /= ADC_BUF_LEN;
}

int32_t GetMicroValueWithStartOfset(uint32_t timeIndex) {
    uint32_t indexToRead = 0;
    uint32_t DMAPos = ADC_BUF_LEN - (__HAL_DMA_GET_COUNTER(ADC->DMA_Handle) / 2);
    if(DMAPos > ADC_BUF_LEN) 
    {
        return 0;
    }

    if(DMAPos < timeIndex){
        indexToRead = ADC_BUF_LEN - timeIndex + DMAPos;
    }else{
        indexToRead = DMAPos - timeIndex;
    }
    return (int32_t)(adc_buffer[indexToRead]) - offset;
}

uint32_t GetMicroValueRaw(uint32_t timeIndex) {
    uint32_t indexToRead = 0;
    uint32_t DMAPos = ADC_BUF_LEN - (__HAL_DMA_GET_COUNTER(ADC->DMA_Handle) / 1);
    if(DMAPos > ADC_BUF_LEN) 
    {
        return 0;
    }

    if(DMAPos < timeIndex){
        indexToRead = ADC_BUF_LEN - timeIndex + DMAPos;
    }else{
        indexToRead = DMAPos - timeIndex;
    }
    return (adc_buffer[indexToRead]);
}

static float filtered_val = 0;
static float last_raw = 0;

int32_t GetCleanMicroValue(uint32_t timeIndex) {
    uint32_t current_raw = GetMicroValueRaw(timeIndex);
    // Коэффициент 0.99 определяет скорость возврата. 
    // Чем меньше число (например 0.95), тем быстрее сигнал будет падать к нулю.
    filtered_val = 0.9 * (filtered_val + (float)current_raw - last_raw);
    last_raw = (float)current_raw;
    
    return (int32_t)filtered_val;
}

uint32_t GetMicroValueCentred (){
    uint32_t dmapos = DMANow;
    uint32_t maxValue = 0;
    uint32_t minValue = 0xFFFFFFFF;
    uint32_t scanPoint = 0;
    static uint32_t scanSize = 1000;

    if(dmapos < scanSize){
        scanPoint = ADC_BUF_LEN - (scanSize - dmapos);
    }else{
        scanPoint = dmapos - scanSize;
    }

    for(int i = 0; i < scanSize; i++){
        if(adc_buffer[scanPoint] > maxValue){
            maxValue = adc_buffer[scanPoint];
        }
        if(adc_buffer[scanPoint] < minValue){
            minValue = adc_buffer[scanPoint];
        }
        scanPoint++;
        if(scanPoint >= ADC_BUF_LEN - 1){
            scanPoint = 0;
        }
    }
    uint32_t amplitude = (maxValue - minValue);
    return amplitude;
}

#define FFT_SAMPLES 1024  // Оптимально для STM32

// Статические буферы, чтобы не нагружать стек
static float32_t fft_input[FFT_SAMPLES];
static float32_t fft_output[FFT_SAMPLES];
static float32_t magnitudes[FFT_SAMPLES / 2];
static arm_rfft_fast_instance_f32 fft_instance;
static bool fft_inited = false;

/**
 * Функция обновляет глобальный массив magnitudes и возвращает указатель на него.
 * magnitudes[0] - постоянная составляющая
 * magnitudes[1...511] - частоты от (Fs/1024) до (Fs/2)
 */
float32_t* GetMicrophoneSpectrum(void) {
    if (!fft_inited) {
        arm_rfft_fast_init_f32(&fft_instance, FFT_SAMPLES);
        fft_inited = true;
    }

    // Определяем текущую позицию DMA
    uint32_t dma_pos = ADC_BUF_LEN - (__HAL_DMA_GET_COUNTER(ADC->DMA_Handle) / 2);
    
    // Прямое копирование данных из буфера АЦП во вход FFT
    // Мы просто берем последние 1024 семпла
    for (uint32_t i = 0; i < FFT_SAMPLES; i++) {
        int32_t idx = (int32_t)dma_pos - (int32_t)FFT_SAMPLES + (int32_t)i;
        
        // Обработка кольцевого буфера
        if (idx < 0) idx += ADC_BUF_LEN;
        if (idx >= ADC_BUF_LEN) idx -= ADC_BUF_LEN;

        // Вычитаем offset вручную (быстрее, чем фильтр для этого случая)
        float32_t val = (float32_t)adc_buffer[idx] - (float32_t)offset;
        
        // Окно Ханна
        float32_t window = 0.5f * (1.0f - arm_cos_f32(2.0f * 3.14159f * i / (float32_t)(FFT_SAMPLES - 1)));
        fft_input[i] = (val / 2048.0f) * window;
    }

    arm_rfft_fast_f32(&fft_instance, fft_input, fft_output, 0);
    arm_cmplx_mag_f32(fft_output, magnitudes, FFT_SAMPLES / 2);

    return magnitudes;
}