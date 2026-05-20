void BallFrame(){
    uint32_t tick = HAL_GetTick();
    float t = tick / 1000.0f;
    
    // Предварительный расчет общих параметров (выносим из циклов)
    float t_slow = t * 0.3f;
    float t_med  = t * 1.2f;
    float t_fast = t * 2.5f;

    // 1. СТАБИЛЬНОЕ ЗАТУХАНИЕ
    // Фиксируем коэффициент, чтобы избежать мерцания яркости всего куба
    for(uint32_t i = 0; i < 512; i++) {
        uint8_t r, g, b;
        GetColorLastFrame(i, &r, &g, &b);

        // Умножаем на целые числа и делим (быстрее и стабильнее для МК)
        r = (r > 2) ? (uint8_t)((r * 230) >> 8) : 2; // ~0.9 затухание
        g = (g > 1) ? (uint8_t)((g * 225) >> 8) : 1;
        b = (b > 3) ? (uint8_t)((b * 235) >> 8) : 3;

        SetColor(i, r, g, b);
    }

    // 2. ГЛАДКИЙ РАДИУС
    // Убираем "рваный" ритм, оставляем один чистый синус
    float radius = 2.0f + (sinf(t_med) + 1.0f) * 2.0f; 

    // Центр движется по более простой траектории
    float cx = 3.5f + 2.0f * sinf(t_slow);
    float cy = 3.5f + 2.0f * cosf(t_slow * 1.1f);
    float cz = 3.5f;

    // Предварительно считаем цвета для текущего кадра
    uint8_t r_target = (uint8_t)((sinf(t_slow * 1.5f) + 1.0f) * 30.0f);
    uint8_t g_target = (uint8_t)((cosf(t_med * 0.8f) + 1.0f) * 35.0f);
    uint8_t b_target = (uint8_t)((sinf(t_fast * 0.2f) + 1.0f) * 50.0f);

    // 3. ЦИКЛ ОТРИСОВКИ
    for (uint8_t y = 0; y < 8; y++) {
        for (uint8_t x = 0; x < 8; x++) {
            for (uint8_t z = 0; z < 8; z++) {
                float dx = x - cx;
                float dy = y - cy;
                float dz = z - cz;
                
                // Используем квадрат расстояния для сравнения (экономим sqrtf)
                float distSq = dx*dx + dy*dy + dz*dz;
                float rSq = radius * radius;
                float innerRSq = (radius - 1.2f) * (radius - 1.2f);

                if (distSq < rSq && distSq > innerRSq) {
                    // Только для тех, кто попал в оболочку, считаем шум и интенсивность
                    float noise = sinf(x * 1.5f + y * 1.2f + z * 1.7f + t_fast) * 0.5f + 0.5f;
                    
                    uint8_t oldR, oldG, oldB;
                    GetColorThisFrame(GetIndex(x,y,z), &oldR, &oldG, &oldB);

                    // Добавляем цвет аккуратно
                    SetColor(GetIndex(x, y, z), 
                             (uint8_t)fminf(120, oldR + (r_target * noise)), 
                             (uint8_t)fminf(120, oldG + (g_target * noise)), 
                             (uint8_t)fminf(120, oldB + (b_target * noise)));
                }
            }
        }
    }
}