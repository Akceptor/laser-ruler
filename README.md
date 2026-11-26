# Laser ruler

Outdoor handheld distance meter using an LR1200E2 rangefinder, an ESP32-C5-H4 development board, and an SSD1306 I²C OLED. Use the existing **BOOT button**; no external measurement button is needed.

"The rangefinder here is the LR1200E2 from ERDILRF. They make 59 laser rangefinder modules, from a tiny 9-metre sensor up to 1535 nm eye-safe modules that reach several kilometres, for civil drones, robots, surveying gear, handheld instruments and industrial sensing. Prices are listed on [their site](http://erdilrf.com), and the Python and ROS 2 drivers are free."

## Wiring

See **[the wiring guide](docs/wiring.md)** for connections, power, and the checks required before connecting the rangefinder.

## Controls

- Short BOOT press: request one measurement and hold the result.
- Hold BOOT for 800 ms: toggle continuous measurements. Release after the action;
  a long press does not also trigger a single measurement.
- OLED: distance in metres, measuring status, and an explicit failed-measurement indication.
- Start with USB power.

Short presses are ignored while a measurement is pending or continuous mode is
running. A stop command waits for acknowledgement; if it fails, hold BOOT again
to retry. A missing sensor response times out after 3 seconds. Failed, out-of-range,
or timed-out measurements clear the displayed distance. Live mode can recover
when valid responses resume. After a **single-shot timeout**, power-cycle both
the sensor and ESP board before measuring again: the protocol has no request IDs,
so a delayed reply could otherwise be mistaken for a fresh result. A board RESET
alone does not restart the sensor. The displayed decimal is the sensor's 0.1 m reporting
resolution, not its accuracy.

## PlatformIO setup

Install [PlatformIO](https://platformio.org/install/ide?install=vscode), open this
folder, and build the `esp32c5` environment. The project pins the
[pioarduino platform](https://github.com/pioarduino/platform-espressif32/releases/tag/55.03.313)
for Arduino ESP32-C5 support. The board profile uses **4 MB flash and no PSRAM**.
Verify this against your board before uploading.

Edit [`include/config.h`](include/config.h) to match your hardware:

| Setting | Default |
| --- | --- |
| OLED SDA / SCL | GPIO0 / GPIO1 |
| Rangefinder RX / TX on ESP | GPIO4 / GPIO5 |
| BOOT | GPIO28, active LOW |
| OLED size | 128×64; set height to 32 for 128×32 |
| OLED address | Auto-detect 0x3C, then 0x3D |
| Software enable GPIO | Disabled (`-1`); SW-SHOT tied to sensor VCC (HIGH) |

<img width="704" height="231" alt="image" src="https://github.com/user-attachments/assets/326474e5-0283-4406-bcc2-81a2621c44d4" />


The owner relayed factory confirmation for the LR1200E2: UART continuous mode
requires SW-SHOT HIGH, tied to VCC. This supersedes the contradictory grounding
instruction in the manual's test procedure. No enable GPIO is driven by default.
Before using optional GPIO control, remove the VCC jumper and verify the enable
input's voltage requirements; the ESP32 GPIO must not be connected to a 5 V rail.

From a terminal with PlatformIO installed:

```sh
pio run -e esp32c5
pio run -e esp32c5 -t upload --upload-port /path/to/serial-port
pio device monitor --port /path/to/serial-port --baud 115200
```

The LR1200E2 is a long-distance sensor: its published range is 4–1,200 m, with ±1 m accuracy at 6–500 m under the manufacturer's target conditions. It is not a centimetre-precision ruler. Its 905 nm beam provides no visible aiming dot. See the [manufacturer's specifications](https://erdilrf.com/models/lr1200e2/).
