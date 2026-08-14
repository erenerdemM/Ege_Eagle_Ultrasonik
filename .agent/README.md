# EAGLEULTRASONİK — Code Audit & Architecture Reports Directory

Bu dizin, **Lead Embedded Systems Engineer** ve **Orchestrator** rolü kapsamında `C:\Users\ern0e\EAGLEULTRASONiK` repository'sinin tamamına uygulanan profesyonel gömülü sistemler kod denetimi (code audit) çıktılarını içerir.

> **ÖNEMLİ İLKE:** Denetim sürecinde mevcut kaynak kodlarında (`.c`, `.h`, `.ino`, `.ioc`, `.ld`) hiçbir değişiklik yapılmamıştır. Tüm analizler ve tespit edilen bulgular bu raporlama alanında dökümante edilmiştir.

---

## 📁 Rapor Dizini Yapısı

```
.agent/
├── reports/
│   ├── repository-overview.md  # Repository genel yapısı, teknoloji haritası ve bağımlılıklar
│   ├── stm32-analysis.md       # STM32G474RE firmware detaylı incelemesi ve donanım analizi
│   ├── esp32-analysis.md       # ESP32-S3 Master firmware detaylı incelemesi
│   ├── protocol-analysis.md    # Multi-drop UART otobüsü haberleşme ve paket matrisi
│   ├── algorithm-analysis.md   # Triyak PWM, PT100 ADC, röle ve zamanlayıcı algoritmaları
│   ├── bug-report.md           # Önceliklendirilmiş hata ve açıklık listesi (CRITICAL -> INFO)
│   └── architecture.md         # Sistem mimarisi, düğüm rolleri, state machine ve veri akışı
│
├── findings/
│   └── findings.json           # Otomatik araçlar ve entegrasyonlar için yapılandırılmış JSON bulguları
│
└── README.md                   # Bu rehber dosyası
```

---

## 📄 Rapor Özetleri

1. **[Repository Overview](reports/repository-overview.md):** Klasör yapısı, bağımlılıklar, HAL/FreeRTOS/Arduino kullanım durumları ve kullanılmayan miras dosyalar.
2. **[Architecture](reports/architecture.md):** ESP32-S3 Master ve STM32 Slave düğüm sorumluluk dağılımı, multi-drop haberleşme mimarisi ve durum makineleri.
3. **[STM32 Analysis](reports/stm32-analysis.md):** Clock, GPIO, ADC, PWM, TIM15, USART3, LPUART1, OPAMP3 ve Flash override yapılarının kod seviyesinde kanıtlı incelemesi.
4. **[ESP32 Analysis](reports/esp32-analysis.md):** NVS reçete yönetimi, Nextion HMI senkronizasyonu, `esp_timer` 100Hz zero-cross simülatörü ve 3000ms bağlantı watchdog'u.
5. **[Protocol Analysis](reports/protocol-analysis.md):** ASCII tabanlı `T<ID>:` adresli komut ve `STAT,...` telemetri paket matrisi, byte tipleri ve kırpma (clamping) kuralları.
6. **[Algorithm Analysis](reports/algorithm-analysis.md):** Triyak faz-açısı soft-start formülleri, PT100 doğrusal dönüştürme ve pencereleme, röle histerezisi (±1.0°C) ve driftsiz geri sayım.
7. **[Bug Report](reports/bug-report.md):** Severity dereceli (CRITICAL, HIGH, MEDIUM, LOW, INFO) 10 adet tespit edilen problem, kök nedenleri, potansiyel etkileri ve önerilen düzeltme sırası.
