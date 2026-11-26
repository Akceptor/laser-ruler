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
| LR1200E2 pin 6, SW-SHOT | Tie to sensor VCC (pin 2) | HIGH enables UART continuous mode; factory confirmation relayed by owner |
| Built-in BOOT | Already on board | Reference DevKitC-1: GPIO28, pressed LOW; verify your board |

TX and RX cross: rangefinder TX → ESP RX, ESP TX → rangefinder RX. The firmware uses a hardware UART at **115200 baud, 8N1**, explicitly assigning RX=4 and TX=5. See `include/config.h` to change these assignments; they are not the board's default UART pins.

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
sensor VCC (same rail as pin 2) -- 6 SW-SHOT (HIGH)
```

The owner's [supplied LR1200E2 manual](https://files.myshopline.com/file/b2b/1728356751512/4cd955d641c1499ca01be0b5aa7677bb.pdf) gives the pin table on PDF page 3 (section 4.2) and the connector drawing on PDF page 4. Match that drawing to the module; do not infer pin 1 from wire colours or this logical diagram. For enable wiring, follow the factory clarification relayed by the owner below.

## Power and UART levels

Use USB to power the ESP board. Power the rangefinder from a regulated 5 V rail with at least 500 mA available for the sensor; this is a design allowance above its documented approximately 350 mA inrush. If using the board's USB-derived 5 V header, first confirm its schematic, output availability, and current capacity. A separate sensor supply is an alternative: join grounds, but do not join its positive rail to the USB-powered board's 5 V header.

Do not power the rangefinder from a GPIO. Keep power leads short; a suggested starting point is 100 nF ceramic plus 100 µF bulk capacitance across sensor VCC/GND near the connector, observing polarity.

The owner confirmed this unit's 3.3 V UART logic and subsequently relayed that the module's TXD/RXD interface works with both 3.3 V and 5 V logic, at **115200 baud, 8N1**. Use the 3.3 V UART connection shown for the ESP32-C5. The module's compatibility with 5 V logic does not make ESP32 GPIOs 5 V tolerant. This information is owner-supplied, not a measurement made during this project. The proposed sensor supply is regulated 5 V; tie SW-SHOT to that same sensor VCC rail, without an ESP GPIO attached.

## Rangefinder enable: factory clarification

The owner relayed factory confirmation specifically for the LR1200E2: **UART continuous mode requires SW-SHOT HIGH, tied to VCC**. Use this wiring for the project, including initial bring-up. Pin 6 connects to pin 2's supply rail; no ESP enable GPIO is needed.

This clarification resolves the supplied manual's conflicting instructions:

- PDF page 3, section 4.2: the pin table labels SW-SHOT active high and mentions active-low compatibility.
- PDF page 6, section 5.1, step 1, note 2: the operating procedure explicitly requires the Enable pin to be LOW, connected to GND.

The factory clarification supersedes the test-step grounding instruction and the earlier repository recommendation based on it. Do not ground SW-SHOT for this build. Do not infer automatic polarity detection from the active-low compatibility note.

Firmware defaults to no enable GPIO (`kEnablePin = -1`) and active-high polarity for optional software control (`kEnableActiveHigh = true`) in `include/config.h`. Before using GPIO6 or another GPIO for enable control, remove the VCC jumper, verify board availability and enable input voltage requirements, and add an interface circuit if needed. Never connect the ESP GPIO to the sensor's 5 V supply rail. Startup allows 250 ms before operation. SW-SHOT is an enable signal, not the user's measurement button; do not connect it to BOOT.

## OLED checks

Use a complete I²C display breakout, not a bare OLED panel. Verify its supply specification, resolution, and SDA/SCL labels. Keep I²C pull-ups at 3.3 V. If the breakout has no pull-ups, 4.7 kΩ from each bus line to 3V3 is a reasonable starting point for short wiring. Avoid adding unnecessary parallel pull-ups.

Scan for the actual I²C address during firmware bring-up; 0x3C and 0x3D are common possibilities. Configure the actual display height, usually 32 or 64 pixels. Adafruit's [SSD1306 guide](https://learn.adafruit.com/monochrome-oled-breakouts) provides breakout examples; its connector layouts and supply arrangements are not universal.

## BOOT control

No extra button wiring is required. Verify the BOOT GPIO against your board schematic; do not assume the GPIO0 mapping used on older ESP32 boards. On the reference DevKitC-1 it is GPIO28, active low.

Firmware debounces presses, triggers a single measurement on short release, and toggles continuous mode once after an 800 ms hold. A long press does not also issue a short-press measurement. BOOT must be released during power-up/reset to start the application normally; holding it while resetting requests the bootloader. Keep external peripherals off this pin.

## Bring-up order

1. Identify the board and verify GPIO assignments, BOOT, and power rails.
2. Connect only the OLED; confirm supply, I²C address, and resolution.
3. Confirm rangefinder connector orientation against PDF page 4; this unit's 3.3 V UART logic is owner-confirmed.
4. Connect sensor power and tie SW-SHOT to sensor VCC per the factory clarification; check supply voltage under load. Leave GPIO6 disconnected.
5. With power off, connect the compatible UART interface, then power up.
6. Upload the firmware using the README instructions, then test a stationary outdoor target beyond the minimum range. Verify single measurement, continuous mode, and explicit timeout/no-return handling. Never present an old result as a fresh successful measurement.

This guide has been checked against the linked documentation, but the assembled hardware has not been tested.
