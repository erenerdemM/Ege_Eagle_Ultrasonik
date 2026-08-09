# Proje Geliştirme Kuralları

1. **STM32 Geliştirme Kuralları**
   - STM32 dizinindeki kodlarda her zaman **HAL kütüphanelerini** kullanın.
   - **MISRA C** standartlarına uygun, bellek sızıntısı yaratmayan güvenli C kodu yazın.

2. **ESP32 Geliştirme Kuralları**
   - ESP32 dizininde **FreeRTOS** standartlarına uygun C++ kodları üretin.

3. **HIL Test Kuralları**
   - HIL testleri için (`test_hil_uart.py`) **pytest** standartlarını kullanın.
