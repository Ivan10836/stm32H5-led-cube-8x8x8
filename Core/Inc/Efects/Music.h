void MusicFrame(){
        // ==================== SETTINGS ====================
    static const uint8_t SHIFT_SPEED = 4;      // Скорость движения назад
    static const float FADE_COEFF = 0.85f;     // Затухание яркости
    static const float GRAVITY_STEP = 0.6f;    // Смещение вниз за один сдвиг (0.1 - медленно, 1.0 - на целый диод)
    
    uint16_t bins[] = {2, 5, 15, 30, 40, 80, 160, 300, 450};
    float32_t Bias[] = {2.0f, 0.9f, 0.7f, 0.55f, 1.2f, 2.1f, 2.1f, 1.3f}; 
    float32_t GLOBAL_GAIN = 0.6f;
    // ==================================================

    static uint8_t shift_divider = 0;
    shift_divider++;

    // --- 1. АНАЛОГОВЫЙ СДВИГ (ЗАТУХАНИЕ + ПЛАВНОЕ ПАДЕНИЕ) ---
    bool is_shift_frame = (shift_divider >= SHIFT_SPEED);
    if (is_shift_frame) shift_divider = 0;

    for (uint8_t z = 7; z > 0; z--) {
        for (uint8_t x = 0; x < 8; x++) {
            for (uint8_t y = 0; y < 8; y++) {
                uint8_t r = 0, g = 0, b = 0;
                
                if (is_shift_frame) {
                    // Читаем два соседних пикселя по вертикали для смешивания
                    uint8_t rCurr, gCurr, bCurr;
                    uint8_t rAbove = 0, gAbove = 0, bAbove = 0;

                    GetColorLastFrame(GetIndex(x, y, z - 1), &rCurr, &gCurr, &bCurr);
                    if (y < 7) {
                        GetColorLastFrame(GetIndex(x, y + 1, z - 1), &rAbove, &gAbove, &bAbove);
                    }

                    // Линейная интерполяция (смешиваем текущий и верхний)
                    // Чем больше GRAVITY_STEP, тем больше берем от верхнего пикселя
                    r = (uint8_t)((rCurr * (1.0f - GRAVITY_STEP) + rAbove * GRAVITY_STEP) * FADE_COEFF);
                    g = (uint8_t)((gCurr * (1.0f - GRAVITY_STEP) + gAbove * GRAVITY_STEP) * FADE_COEFF);
                    b = (uint8_t)((bCurr * (1.0f - GRAVITY_STEP) + bAbove * GRAVITY_STEP) * FADE_COEFF);
                } else {
                    // Копируем слой для стабильности кадра
                    GetColorLastFrame(GetIndex(x, y, z), &r, &g, &b);
                }
                SetColor(GetIndex(x, y, z), r, g, b);
            }
        }
    }

    // --- 2. НОВЫЙ СПЕКТР (Z=0) ---
    float32_t* spectrum = GetMicrophoneSpectrum();

    for (uint8_t x = 0; x < 8; x++) {
        float32_t sum = 0;
        uint16_t start_index = bins[x];
        uint16_t end_index = bins[x+1];

        for (uint16_t j = start_index; j < end_index; j++) {
            sum += spectrum[j];
        }
        
        float32_t avg = sum / (float32_t)(end_index - start_index);
        float32_t boost = 0.1f + (x * 0.2f); 
        float32_t exact_height = (avg * GLOBAL_GAIN * boost - 1.0f) * Bias[x]; 

        if (exact_height > 8.0f) exact_height = 8.0f;
        if (exact_height < 0.0f) exact_height = 0.0f;

        // Палитра (градиент)
        uint8_t baseR = (x < 4) ? 100 : 0;
        uint8_t baseG = (x > 1 && x < 6) ? 100 : 0;
        uint8_t baseB = (x >= 4) ? 100 : 0;

        for (uint8_t y = 0; y < 8; y++) {
            float32_t diff = exact_height - (float32_t)y;
            float32_t factor = (diff >= 1.0f) ? 1.0f : (diff > 0.0f ? diff : 0.0f);

            SetColor(GetIndex(x, y, 0), 
                     (uint8_t)(baseR * factor), 
                     (uint8_t)(baseG * factor), 
                     (uint8_t)(baseB * factor));
        }
    }
}