# Wiring

## Board identification

This is a **proposed GPIO assignment** for an ESP32-C5 board. “C5-H4” alone does not identify its PCB layout. Check that your board exposes the listed signals and does not use them for other hardware. Follow printed GPIO labels, not header positions copied from another board.

The reference is Espressif's [ESP32-C5-DevKitC-1 guide](https://docs.espressif.com/projects/esp-dev-kits/en/latest/esp32c5/esp32-c5-devkitc-1/user_guide.html) and [v1.2 schematic](https://dl.espressif.com/dl/schematics/SCH_ESP32-C5-DevkitC-1_V1.2_20250211.pdf). Your H4 board may differ.

## Connections

Disconnect power while wiring. All modules share GND.

| Device signal | ESP32-C5 connection | Notes |
| --- | --- | --- |
| OLED GND | GND | Follow display labels; pin order varies |
| OLED VCC | 3V3 | Only for an OLED breakout rated for 3.3 V supply |
| OLED SDA | GPIO0 | Proposed I²C data |
| OLED SCL | GPIO1 | Proposed I²C clock |
| LR1200E2 pin 1, GND | GND | Common ground |
| LR1200E2 pin 2, VCC | Regulated 5 V supply | See power section |
| LR1200E2 pin 3, reserved | Unconnected | Do not use |
| LR1200E2 pin 4, TXD | GPIO4, UART RX | Direct connection; owner confirmed 3.3 V logic |
| LR1200E2 pin 5, RXD | GPIO5, UART TX | Direct connection; owner confirmed 3.3 V logic |
| LR1200E2 pin 6, SW-SHOT | GPIO6 | Active-high enable: HIGH = enabled, LOW = disabled |
| Built-in BOOT | Already on board | Reference DevKitC-1: GPIO28, pressed LOW; verify your board |

TX and RX cross: rangefinder TX → ESP RX, ESP TX → rangefinder RX. Use a hardware UART at **115200 baud, 8N1**, explicitly assigning RX=4 and TX=5. These are planned firmware settings, not instructions to use the board's default UART pins.

```text
ESP32-C5                         SSD1306 I²C breakout
3V3 ---------------------------- VCC (if 3.3 V compatible)
GND ---------------------------- GND
GPIO0 -------------------------- SDA
GPIO1 -------------------------- SCL

ESP32-C5                         LR1200E2
GND ---------------------------- 1 GND
regulated 5 V ------------------ 2 VCC
GPIO4 RX <---------------------- 4 TXD (3.3 V logic)
GPIO5 TX ----------------------> 5 RXD (3.3 V logic)
                                 3 reserved: leave open
GPIO6 -------------------------- 6 SW-SHOT (HIGH = enabled)
```

The [LR1200E2 integration documentation](https://erdilrf.com/docs/models/lr1200e2/) supplies the pin numbering. Confirm connector orientation against the drawing in the [module manual, section 5](https://erdilrf.com/datasheets/ERDI-LR1200E2-datasheet.pdf); do not infer pin 1 from wire colours or this logical diagram.

## Power and UART levels

Use USB to power the ESP board. Power the rangefinder from a regulated 5 V rail with at least 500 mA available for the sensor; this is a design allowance above its documented approximately 350 mA inrush. If using the board's USB-derived 5 V header, first confirm its schematic, output availability, and current capacity. A separate sensor supply is an alternative: join grounds, but do not join its positive rail to the USB-powered board's 5 V header.

Do not power the rangefinder from a GPIO. Keep power leads short; a suggested starting point is 100 nF ceramic plus 100 µF bulk capacitance across sensor VCC/GND near the connector, observing polarity.

The owner confirmed on 2026-11-26 that this rangefinder uses **3.3 V logic**. Connect its UART directly to the ESP32-C5 as shown; no level translator is needed for this unit. This is owner-supplied information, not a measurement made during this project. Supply voltage and logic voltage are separate: the proposed sensor supply remains regulated 5 V. Use a 3.3 V GPIO signal for SW-SHOT as well; do not tie it to the 5 V supply.

## Rangefinder enable

The owner supplied the manual wording: **Function Enable (active high)**, with a note about compatibility with active-low control requirements. This design follows the explicit active-high definition: connect pin 6 to GPIO6; drive HIGH (3.3 V) to enable and LOW to disable. The note does not establish automatic polarity detection or mean that both levels enable the same unit; treat active-low operation as an alternative that needs separate confirmation if used.

GPIO6 availability still depends on the exact board. Planned firmware will configure it as an output, enable the sensor, allow startup time, and request measurements over UART. SW-SHOT is an enable signal, not the user's measurement button; do not connect it to BOOT. An optional 10 kΩ pull-down from SW-SHOT to GND keeps enable LOW while the ESP pin is high impedance during reset. Check that it does not conflict with any existing pull-up on the supplied module.

## OLED checks

Use a complete I²C display breakout, not a bare OLED panel. Verify its supply specification, resolution, and SDA/SCL labels. Keep I²C pull-ups at 3.3 V. If the breakout has no pull-ups, 4.7 kΩ from each bus line to 3V3 is a reasonable starting point for short wiring. Avoid adding unnecessary parallel pull-ups.

Scan for the actual I²C address during firmware bring-up; 0x3C and 0x3D are common possibilities. Configure the actual display height, usually 32 or 64 pixels. Adafruit's [SSD1306 guide](https://learn.adafruit.com/monochrome-oled-breakouts) provides breakout examples; its connector layouts and supply arrangements are not universal.

## BOOT control

No extra button wiring is required. Verify the BOOT GPIO against your board schematic; do not assume the GPIO0 mapping used on older ESP32 boards. On the reference DevKitC-1 it is GPIO28, active low.

Planned firmware behavior: debounce presses, trigger a single measurement on short release, and toggle continuous mode once after a long press. A long press must not also issue a short-press measurement. BOOT must be released during power-up/reset to start the application normally; holding it while resetting requests the bootloader. Keep external peripherals off this pin.

## Bring-up order

1. Identify the board and verify GPIO assignments, BOOT, and power rails.
2. Connect only the OLED; confirm supply, I²C address, and resolution.
3. Confirm rangefinder connector orientation; this unit's 3.3 V logic and documented active-high enable are recorded above.
4. Connect sensor power and SW-SHOT to GPIO6; check voltage under load with enable HIGH.
5. With power off, connect the compatible UART interface, then power up.
6. Once firmware exists, test a stationary outdoor target beyond the minimum range. Verify single measurement, continuous mode, and explicit timeout/no-return handling. Never present an old result as a fresh successful measurement.

This guide has been checked against the linked documentation, but the assembled hardware has not been tested.
