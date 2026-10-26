# Cat Button Board

A talking button board for a cat. The cat touches a raised puck and the board speaks a recorded word. The pads use the ESP32's capacitive touch sensing, so a light paw touch is enough and nothing has to be pressed.

No soldering is required. It was inspired by the r/PetsWithButtons post "Proof of concept: button using a capacitative captor (easier to activate for cats)".

**Status:** six pads work on the bench, with placeholder words. Testing with an actual cat is next.

## What it's made of

- An original ESP32 dev board (ESP-WROOM-32). Not a C3 or C6: those have no touch sensing.
- A MAX98357A I2S amplifier and a small 4Ω speaker.
- Six touch pads, each a conductive disc about 60 mm across, for example copper foil tape.
- 5V USB power.

The parts list, wiring and what each pad needs are in [docs/HARDWARE.md](docs/HARDWARE.md).

## Wiring

| Button | ESP32 pin | | Amp | ESP32 pin |
|---|---|---|---|---|
| 1 Twine | D4 | | BCLK | D26 |
| 2 Treat | D13 | | LRC | D25 |
| 3 Cat dancer | D14 | | DIN | D22 |
| 4 Hoops | D27 | | V | VIN |
| 5 Snuggles | D32 | | G | GND |
| 6 Catnips | D33 | | | |

Each pad is one wire. Keep pad leads under 30 cm and about 1 cm from each other and from the amp wires.

## Building and flashing

Install PlatformIO, then build, upload the word clips and flash from the project folder:

```
uv tool install --python 3.13 platformio
tools/make_clips.sh --say
pio run -t uploadfs
pio run -t upload
```

`--say` makes placeholder words in the Mac's voice. Plug the pads in before powering the board, and keep hands clear for 3 seconds while it calibrates.

[docs/BUILD.md](docs/BUILD.md) covers every command, the serial tools and the partition table.

## Recording your own words

Record each word, for example with Voice Memos, and save it in `clips/` named after its button: `treat.m4a`, `hoops.m4a` and so on. Then run `tools/make_clips.sh` and `pio run -t uploadfs`. The script trims silence and evens out loudness.

To rename, add or remove buttons, edit the pad table in [include/config.h](include/config.h). Up to six fit on the ESP32's touch pins.

## Tuning the touch

```
uv run tools/plot_touch.py
```

The plotter graphs one pad live and prints a summary after each touch: how deep it went and whether it triggered. In its terminal, `d`/`D` shorten or lengthen the time a touch must last, and `t`/`T` move the touch threshold, without reflashing. Every tuning value lives in [include/config.h](include/config.h).

## Docs

- [docs/HARDWARE.md](docs/HARDWARE.md): parts, wiring, power and what each pad needs.
- [docs/FIRMWARE_SPEC.md](docs/FIRMWARE_SPEC.md): behavior, build phases and pass criteria.
- [docs/BUILD.md](docs/BUILD.md): toolchain and commands.
- [docs/DECISIONS.md](docs/DECISIONS.md): why it's built this way and what was ruled out.

## License

MIT. See [LICENSE](LICENSE).
