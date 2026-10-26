// Phase 0: prove the toolchain. Prints a counter once a second and blinks the
// on-board LED. Pass: the serial monitor shows these lines.
#include <Arduino.h>

#include "config.h"

void setup() {
  Serial.begin(SERIAL_BAUD);
  pinMode(LED_PIN, OUTPUT);
  delay(500);
  Serial.println();
  Serial.println("cat-buttons phase0: hello from the ESP32");
  Serial.printf("chip: %s rev %d, %d cores, flash %u MB\n", ESP.getChipModel(), ESP.getChipRevision(),
                ESP.getChipCores(), (unsigned)(ESP.getFlashChipSize() / (1024 * 1024)));
  Serial.printf("touch pin for pad %s: GPIO%u, first touchRead = %lu\n", PADS[0].name, PADS[0].pin,
                (unsigned long)touchRead(PADS[0].pin));
}

void loop() {
  static uint32_t n = 0;
  digitalWrite(LED_PIN, n & 1);
  Serial.printf("tick %lu  touchRead(GPIO%u)=%lu\n", (unsigned long)n++, PADS[0].pin,
                (unsigned long)touchRead(PADS[0].pin));
  delay(1000);
}
