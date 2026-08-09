agents:
  - name: STM32_Uzmani
    description: "Sadece STM32 dizininde çalışır, donanım kesmeleri ve PWM ayarlarından sorumludur."
    tools:
      - scope: "STM32/"

  - name: ESP_Ekran_Haberlesmeci
    description: "Sadece esp32 ve Ekran dizinlerinde çalışır, UART haberleşmesini ve HMI güncellemelerini yönetir."
    tools:
      - scope: "esp32/"
      - scope: "Ekran/"
