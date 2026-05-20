#include "Efects/Ball.h"
#include "Efects/Snake.h"
#include "Efects/Music.h"
#include "Buttons.h"

// Координаты нашей точки
int8_t pointX = 3; 
int8_t pointY = 3;
int8_t pointZ = 3;
bool isFirst = true;

void Frame() {
    // Инициализация при первом запуске
    if(isFirst) {
        isFirst = false;
        Input_Init();
    }

    // 1. Обновляем состояние кнопок и инкремент осей
    Input_Update();

    // --- ПРОВЕРКА GetAxis (Дискретно: -1, 0, 1) ---
    // По Z (Право/Лево) и Y (Верх/Низ) точка будет ПРЫГАТЬ
    pointZ = 3 + GetAxis(AXIS_Z);
    pointY = 3 + GetAxis(AXIS_Y);

    // --- ПРОВЕРКА GetAxisRaw (Плавно: -128...128) ---
    // По X (Вперед/Назад) точка будет ПЛАВНО ПЛЫТЬ
    // Делим на 32, чтобы диапазон был от -4 до +4 относительно центра
    pointX = 3 + (GetAxisRaw(AXIS_X) / 32);

    // --- ПРОВЕРКА GetButtonDown и GetButton ---
    uint8_t r = 255, g = 255, b = 255;

    if (GetButton(BTN_DOWN)) {
        // Пока держим DOWN — точка синяя
        r = 0; g = 0; b = 255;
    }

    if (GetButtonDown(BTN_UP)) {
        // В момент нажатия UP — вспышка красным на один кадр
        r = 255; g = 0; b = 0;
    }

    // 2. Отрисовка
    // Полная очистка куба перед рисованием новой точки
    for(int i = 0; i < 512; i++) SetColor(i, 0, 0, 0);

    // Рисуем точку в новых координатах
    SetColor(GetIndex(pointX, pointY, pointZ), r, g, b);
}