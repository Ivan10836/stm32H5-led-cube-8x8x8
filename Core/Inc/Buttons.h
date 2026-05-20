
#include "main.h"
#include <stdbool.h>

/* --- Настройки --- */
#define DEBOUNCE_MS 30
#define SMOOTH_SPEED 5  // Скорость доводки (чем выше, тем быстрее меняется значение)

/* --- Перечисления --- */
typedef enum {
    BTN_LEFT, BTN_RIGHT,
    BTN_FRONT, BTN_BACK,
    BTN_UP, BTN_DOWN,
    BTN_COUNT
} ButtonID;

typedef enum {
    AXIS_X, // Forward / Back (FRONT / BACK)
    AXIS_Y, // Up / Down (UP / DOWN)
    AXIS_Z  // Right / Left (RIGHT / LEFT)
} AxisID;

/* --- Структуры --- */
typedef struct {
    GPIO_TypeDef* port;
    uint16_t pin;
    bool current_state;
    bool last_state;
    bool raw_phys_state;
    uint32_t last_tick;
} Button_t;

/* --- Внутренние переменные --- */
static Button_t _btns[BTN_COUNT];
static int16_t _axis_smooth[3] = {0, 0, 0}; // Хранит значения от -128 до 128

/* --- Инициализация --- */
static void Input_Init(void) {
    _btns[BTN_LEFT]  = (Button_t){LEFT_GPIO_Port, LEFT_Pin};
    _btns[BTN_RIGHT] = (Button_t){RIGHT_GPIO_Port, RIGHT_Pin};
    _btns[BTN_FRONT] = (Button_t){FRONT_GPIO_Port, FRONT_Pin};
    _btns[BTN_BACK]  = (Button_t){BACK_GPIO_Port, BACK_Pin};
    _btns[BTN_UP]    = (Button_t){UP_GPIO_Port, UP_Pin};
    _btns[BTN_DOWN]  = (Button_t){DOWN_GPIO_Port, DOWN_Pin};
}

/* --- Логика обновления --- */
static void Input_Update(void) {
    uint32_t now = HAL_GetTick();

    // 1. Опрос кнопок с антидребезгом
    for (int i = 0; i < BTN_COUNT; i++) {
        _btns[i].last_state = _btns[i].current_state;
        bool phys = (HAL_GPIO_ReadPin(_btns[i].port, _btns[i].pin) == GPIO_PIN_RESET);

        if (phys != _btns[i].raw_phys_state) {
            _btns[i].last_tick = now;
            _btns[i].raw_phys_state = phys;
        }

        if ((now - _btns[i].last_tick) > DEBOUNCE_MS) {
            _btns[i].current_state = _btns[i].raw_phys_state;
        }
    }

    // 2. Расчет сглаженных значений (для GetAxisRaw)
    int16_t targets[3] = {0, 0, 0};
    
    // X (Forward/Back)
    if (_btns[BTN_FRONT].current_state) targets[AXIS_X] = 128;
    else if (_btns[BTN_BACK].current_state) targets[AXIS_X] = -128;

    // Y (Up/Down)
    if (_btns[BTN_UP].current_state) targets[AXIS_Y] = 128;
    else if (_btns[BTN_DOWN].current_state) targets[AXIS_Y] = -128;

    // Z (Right/Left)
    if (_btns[BTN_RIGHT].current_state) targets[AXIS_Z] = 128;
    else if (_btns[BTN_LEFT].current_state) targets[AXIS_Z] = -128;

    // Простейшая линейная интерполяция на целых числах
    for (int a = 0; a < 3; a++) {
        if (_axis_smooth[a] < targets[a]) _axis_smooth[a] += SMOOTH_SPEED;
        if (_axis_smooth[a] > targets[a]) _axis_smooth[a] -= SMOOTH_SPEED;
        
        // "Доводка" чтобы не проскочить цель из-за шага SMOOTH_SPEED
        if (abs(targets[a] - _axis_smooth[a]) < SMOOTH_SPEED) {
            _axis_smooth[a] = targets[a];
        }
    }
}

/* --- Интерфейс пользователя --- */

// Удержание: true пока зажата
static bool GetButton(ButtonID id) {
    return _btns[id].current_state;
}

// Нажатие: true только в первом кадре
static bool GetButtonDown(ButtonID id) {
    return (_btns[id].current_state && !_btns[id].last_state);
}

// Дискретная ось: -1, 0, 1 (как ты просил)
static int8_t GetAxis(AxisID axis) {
    if (axis == AXIS_X) {
        if (_btns[BTN_FRONT].current_state) return 1;
        if (_btns[BTN_BACK].current_state) return -1;
    } else if (axis == AXIS_Y) {
        if (_btns[BTN_UP].current_state) return 1;
        if (_btns[BTN_DOWN].current_state) return -1;
    } else if (axis == AXIS_Z) {
        if (_btns[BTN_RIGHT].current_state) return 1;
        if (_btns[BTN_LEFT].current_state) return -1;
    }
    return 0;
}

// Сглаженная ось: плавно от -128 до 128
static int16_t GetAxisRaw(AxisID axis) {
    return _axis_smooth[axis];
}
