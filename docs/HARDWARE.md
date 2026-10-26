# Hardware

## Shopping list

| Item | Pick | ~Cost | Notes |
|---|---|---|---|
| ESP32 dev boards | ELEGOO 3-pack, ESP-WROOM-32, USB-C (Amazon B0D8T53CQ5) | $20 | Too wide for one breadboard: use F-F jumpers to the pins, or straddle two breadboards |
| Amp and speaker | NULLLAB MAX98357A I2S amp and 3W speaker kit (Amazon B0FSZQRWSF), **"MAX98357A" set, not NS4168** | $10 | Speaker plugs into the PH2.0 jack. Module docs: github.com/nulllaborg/i2s_audio_amplifier_module |
| Copper foil tape | 2", **conductive adhesive** | $10 | Conductive adhesive matters for the no-solder joint |
| Hookup wire / jumpers | F-F Dupont jumpers, 20 cm, plus 22 AWG stranded | $10 | A cut F-F jumper is the pad lead |

**Optional:** an ELEGOO MB102 breadboard power module, or any 5V USB breakout, to power the amp separately if playback browns out the ESP32.

## Wiring

```
Pads (one wire each, no ground return needed)
  1 TWINE      -> GPIO4  (D4)     4 HOOPS    -> GPIO27 (D27)
  2 TREAT      -> GPIO13 (D13)    5 SNUGGLES -> GPIO32 (D32)
  3 CAT DANCER -> GPIO14 (D14)    6 CATNIPS  -> GPIO33 (D33)

ESP32            NULLLAB MAX98357A
  GPIO26  ---->  BCLK
  GPIO25  ---->  LRC
  GPIO22  ---->  DIN
  VIN(5V) ---->  VIN
  GND     ---->  GND
                 OUT -> speaker (PH2.0 jack)
```

- **Finding the pad pins:** five are on the left edge. Counting up from the USB end: VIN, GND, D13, D12, D14, D27, D26, D25, D33, D32. D4 is on the right edge, fifth pin from the USB end. D12 sits between D13 and D14 and is a boot-strapping pin: a pad on it can stop the board booting.
- **Phase 1 feedback:** the board's on-board blue LED (GPIO2) lights while the pad reads touched. No extra parts.
- **Amp connector:** the NULLLAB module's 5-pin PH2.0 header is labeled G, V, BCLK, LRC, DIN. The supplied cable ends in female Dupont sockets; a male jumper in each reaches the breadboard. G goes to GND, V to VIN, and the three signals as in the diagram.
- **Speaker:** plugs into the 2-pin PH2.0 jack beside the blue screw terminal. The screw terminal is unused.
- **Amp GAIN and SD:** not on the connector. The amp runs at its fixed 9 dB gain and stays on; volume is set in software (`VOLUME` in `include/config.h`).
- **Leads:** keep each pad lead under 30 cm and about 1 cm from its neighbors, or the leads pick up touches themselves.

## Power

- Use a 5V/2A USB wall adapter into the ESP32's USB port. Budget about 250 mA for the ESP32 with WiFi on, and up to roughly 700 mA for the amp at full volume into 4Ω.
- **Brownout risk:** a loud clip can sag the board's 5V line and reset the ESP32. Fix in this order:
  1. Lower the software volume.
  2. Add the 470–1000 µF capacitor at the amp.
  3. Feed the amp 5V directly (MB102 module or a USB breakout), with a shared GND.
- **No battery for v0.** Capacitive readings drift more when the circuit floats, and power banks auto-shutoff at low draw.

## What each pad needs

How you build the pucks is up to you. The touch sensing needs:

- **A conductive pad about 60 mm across** on top of each puck.
- **A cover under 0.5 mm thick, if you use one.** Thicker covers shrink the touch signal. Recalibrate and retest after any cover change.
- **The lead joint off the touch surface,** for example down the side of the puck, so a paw never lands on it.
- **A clamped or soldered lead joint.** In testing, a wire held only by copper tape's adhesive read too weakly to trigger. Bare wire clamped between two layers of copper tape read as strongly as the bare wire itself. Solder is the other option.
- **About 80 mm between pad centers,** so one paw doesn't touch two pads. Six pads in a 2×3 grid take about 230 × 150 mm.
