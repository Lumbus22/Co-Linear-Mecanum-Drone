// ESP32 DOIT DevKit V1 + Cyclone 35A ESC
// MCPWM control with Serial monitor status messages
// Signal pin: GPIO 18

#include <Arduino.h>
#include "driver/mcpwm.h"

#define ESC_PIN     18
#define ESC_MIN_US  1000   // min throttle
#define ESC_MAX_US  2000   // max throttle
#define ESC_FREQ_HZ 50     // 50 Hz, 20 ms frame

// Helper: write pulse width in microseconds
static inline void escWriteUs(int us) {
  mcpwm_set_duty_in_us(MCPWM_UNIT_0, MCPWM_TIMER_0, MCPWM_OPR_A, us);
}

void setup() {
  Serial.begin(115200);
  Serial.println("\n=== ESC Test Starting (ESP32 DevKit V1, MCPWM) ===");

  // Setup MCPWM
  mcpwm_gpio_init(MCPWM_UNIT_0, MCPWM0A, ESC_PIN);
  mcpwm_config_t cfg = {};
  cfg.frequency = ESC_FREQ_HZ;
  cfg.cmpr_a = 0;
  cfg.cmpr_b = 0;
  cfg.counter_mode = MCPWM_UP_COUNTER;
  cfg.duty_mode = MCPWM_DUTY_MODE_0;
  mcpwm_init(MCPWM_UNIT_0, MCPWM_TIMER_0, &cfg);

  // Arm ESC
  Serial.println("Arming ESC at minimum throttle (1000us) for 3s...");
  escWriteUs(ESC_MIN_US);
  delay(3000);
  Serial.println("ESC Armed! Beginning throttle sequence...");
}

void loop() {
  // Ramp up
  Serial.println("Ramping UP...");
  for (int us = ESC_MIN_US; us <= ESC_MAX_US; us += 10) {
    escWriteUs(us);
    if (us % 100 == 0) {   // print every 100us step
      Serial.printf("Throttle: %dus\n", us);
    }
    delay(20);
  }
  Serial.println("At MAX throttle (2000us)!");
  delay(1500);

  // Ramp down
  Serial.println("Ramping DOWN...");
  for (int us = ESC_MAX_US; us >= ESC_MIN_US; us -= 10) {
    escWriteUs(us);
    if (us % 100 == 0) {
      Serial.printf("Throttle: %dus\n", us);
    }
    delay(20);
  }
  Serial.println("Back at MIN throttle (1000us).");
  delay(1500);
}
