# Laser ruler

Outdoor handheld distance meter using an LR1200E2 rangefinder, an ESP32-C5-H4 development board, and an SSD1306 I²C OLED. Use the existing **BOOT button**; no external measurement button is needed.

## Wiring

See **[the wiring guide](docs/wiring.md)** for connections, power, and the checks required before connecting the rangefinder.

## Planned controls

- Short BOOT press: request one measurement and hold the result.
- Long BOOT press: toggle continuous measurements.
- OLED: distance in metres, measuring status, and an explicit failed-measurement indication.
- Start with USB power.

## Status

Documentation only; firmware is not included yet. Wiring is a proposed assignment, not hardware-tested. The exact ESP32-C5-H4 board manufacturer and revision have not been identified. GPIO numbers below refer to ESP32-C5 signals, not physical header positions.

The LR1200E2 is a long-distance sensor: its published range is 4–1,200 m, with ±1 m accuracy at 6–500 m under the manufacturer's target conditions. It is not a centimetre-precision ruler. Its 905 nm beam provides no visible aiming dot. See the [manufacturer's specifications](https://erdilrf.com/models/lr1200e2/).
