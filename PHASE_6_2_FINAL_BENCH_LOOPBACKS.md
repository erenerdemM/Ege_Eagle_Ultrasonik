# PHASE 6.2 FINAL BENCH LOOPBACKS

> [!CAUTION]
> Bu doküman **SADECE FİZİKSEL BENCH TEST** için gereken hardware loopback'lerini tanımlar. 
> Üretim kartı basıldığında bu loopback'lere ihtiyaç kalmayacak (veya X9C gibi kalıcı olanlar kalacaktır). 
> **FIRMWARE READBACK = IMPLEMENTED** (Firmware güncellemesi repository'de doğrulanmıştır).

Bench üzerindeki kapalı çevrim testleri gerçekleştirmek için 3 ana donanım loopback'i KESİNLİKLE takılı olmalıdır.

| Loopback ID | Kaynak Pin (Çıkış) | Seri Direnç (Koruma/Kısa Devre Önleme) | Direnç Terminal A | Direnç Terminal B | Geri Besleme Pini (Giriş) | Ölçüm / Doğrulama Noktası | Testin Amacı | Beklenen Sinyal Davranışı |
|:---|:---|:---|:---|:---|:---|:---|:---|:---|
| **LP-01** | STM32 PB15 (CN10-26) | R-HTR-FB (1kΩ) | PB15 | PA4 | STM32 PA4 (CN7-32) | PA4 Pini / COM11 `HEATER_FB` | Isıtıcı SSR röle çıkışının, firmware tarafından Timer süresince başarıyla üretilip üretilmediğinin ve Timer sıfırlandığında güvenle 0'a çekilip çekilmediğinin fiziksel olarak MCU tarafından okunması. | Timer `START` verildiğinde `HEATER_OUT=1` ve `HEATER_FB=1` olur. Timer `EXPIRE` olduğunda veya `STOP` verildiğinde her ikisi de `0` olur. |
| **LP-02** | STM32 PC6 (CN10-4) | R-TRC-FB (1kΩ) | PC6 | PA6 | STM32 PA6 (CN7-34) | PA6 Pini / COM11 `TRIAC_FB` | Triyak gate pulse çıkışının, zero-cross ile senkronize şekilde üretildiğinin ve sistem `IDLE` modundayken tamamen susturulduğunun fiziksel olarak MCU tarafından okunması. | `RUNNING` modunda PWM pulse üretilir (Osiloskopta 100µs pulse görülür, `TRIAC_OUT=1`, `TRIAC_FB=1` okur). `IDLE` modunda tamamen 0V seviyesinde kalır. |
| **LP-03** | ESP32 GPIO4 | R-ZC-SIM (1kΩ) | GPIO4 | PC7 | STM32 PC7 (CN5-2) | PC7 Pini / COM11 `fault` bit 4 | Gerçek 220V donanım yokken, AC şebekenin sıfır geçiş (zero-cross) sinyallerini ESP32 üzerinden 100Hz'lik bir kare dalga ile STM32'ye simüle etmek ve `EXTI9_5_IRQHandler`'ın bunu doğru işlediğini test etmek. | ESP32 başlatıldığında GPIO4 sürekli 100Hz sinyal üretir. STM32 bu sayede `FAULT_ZERO_CROSS_LOST` vermez ve triyakları ateşleyebilir. Kablo sökülürse sistem otomatik kapanır. |

> [!NOTE]
> Firmware, `PA4` ve `PA6` pinlerini artık doğrudan okumakta ve `BenchTest_Process()` içerisinde karşılaştırarak bir hata durumunda (Örn: Çıkış `1` verilmiş ama geri beslemeden `0` geliyorsa) COM11 UART terminalinde `DIAGNOSTIC: HEATER_FEEDBACK_MISMATCH` uyarısı üretmektedir. Bu üretim `SystemState` yapısını etkilemeden bench ortamındaki fiziksel kopuklukları veya firmware mantık hatalarını tespit etmeye yarar.
