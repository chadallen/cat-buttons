# Decisions

## Capacitive touch over mechanical or force sensing
- **Why:** the inspiration build, the r/PetsWithButtons post "Proof of concept: button using a capacitative captor (easier to activate for cats)", was for a cat that couldn't trigger commercial press buttons and clawed them instead. Capacitive needs no force, just a light paw touch through a thin cover.
- **Considered:**
  - Low-force mechanical switches (light keyboard switches, lever microswitches).
  - FSRs (Interlink 402/406).
  - A hybrid of capacitive plus FSR.
- **Rejected for v0:** mechanical still needs a downward press, which a tapping or swatting cat won't give. An FSR is the fallback if phase 1 shows paws don't read reliably through the cover.
- **Known risks, which the firmware has to handle:**
  - Paws and fur conduct worse than fingers.
  - Humidity drift.
  - False triggers from a cat lying near or across the board.

## ESP32 native touch, not an MPR121
- About 6 clean touch pins are enough for 4–6 buttons, with no extra board.
- If the build outgrows that, or native readings prove too noisy: add an MPR121 (12 channels, better filtering) or move to an ESP32-S3 (14 touch pins).

## Raised pucks on a foam mat
- Copied from the inspiration build. A raised puck reads as a distinct target, and it lifts the pad above a cat resting nearby.
- The pad sits 5 mm in from the puck edge for the same reason.
- A 70 mm puck is well above the "at least nickel-sized" floor for the target.

## Copper foil, not a washer, for v0
- The Reddit build glued an aluminum washer to a 3D-printed cylinder.
- Foil can be cut to any size and takes a soldered or taped lead, which suits foam board.
- Revisit a washer or a printed puck in v1.

## Vinyl cover, not paper
- The Reddit build used paper, which a clawing cat will shred.
- Laminated vinyl under 0.5 mm survives claws and still senses through.

## Six pads, not four
- The builder chose six words: twine, treat, cat dancer, hoops, snuggles, catnips.
- Six is every ESP32 touch pin that isn't a boot-strapping pin, so a seventh needs an MPR121 or an ESP32-S3 (see above).
- A 2×3 grid at 80 mm spacing still fits one 12" mat tile.

## Phase 1 feedback is the on-board LED and the serial log, not a piezo
- The spare piezos would need their own wiring and an active-vs-passive check. The LED on GPIO2 and the `# TRIGGER` lines give the same signal with no parts, and the log is what the pass criteria count anyway.
- GPIO23 is free again.

## ESP_I2S, not ESP8266Audio
- ESP_I2S ships with the arduino-esp32 3.x core. The clips are plain 16 kHz PCM WAV, so no decoder is needed, which is most of what ESP8266Audio adds.
- The player sends stereo with each sample in both slots. The amp plays (L+R)/2, and this sidesteps the core's 16-bit mono workaround for the original ESP32.

## Clips in flash, not on an SD card
- 4 MB of flash holds dozens of short 16 kHz clips, which means fewer parts than a DFPlayer and SD card.

## Wall power, not battery
- Capacitive sensing is more stable with a grounded, plugged-in system, and power banks auto-shutoff at low draw.
