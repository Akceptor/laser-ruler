# Firmware implementation plan

Implement the controls and wiring approved in the conversation using PlatformIO,
Arduino, and a pinned pioarduino platform with ESP32-C5 support.

1. Add host tests in `test/test_core/test_main.cpp` for wire checksums, fragmented
   packets, corrupt-frame recovery, failed/out-of-range measurements, button
   debounce/long presses, command timeouts, and stop acknowledgements.
2. Implement dependency-free protocol and interaction logic in
   `include/ruler_core.h`; run the host tests with a C++ compiler and PlatformIO.
3. Add editable board/display configuration in `include/config.h`, and the Arduino
   UART/OLED integration in `src/main.cpp`. Use GPIO0/1 for I²C, GPIO4/5 for UART,
   BOOT GPIO28, and no software enable pin by default (SW-SHOT tied to sensor VCC).
4. Compile `pio run -e esp32c5`. Review command transitions, stale measurement
   handling, 32-bit clock rollover, OLED failure behavior, and RAM/flash use.
5. Update README and wiring documentation with setup, configuration, upload,
   controls, tests, and hardware limitations. Commit and push after verification.

No hardware flashing is part of this implementation. The board model remains a
hardware check. The owner relayed factory confirmation that SW-SHOT must be tied
HIGH to sensor VCC for UART continuous mode, superseding the manual's conflicting
test procedure. Software enable control remains disabled by default.

## Verification completed

- PlatformIO `esp32c5` build passed using pioarduino 55.03.313 / Arduino 3.3.13.
- PlatformIO `native` passed all five test groups.
- The compiler-only tests passed with address and undefined-behavior sanitizers.
- Review findings have regression coverage: single-shot timeouts latch a fault
  until a complete power cycle, and late stop acknowledgements clear stale results.
- Hardware upload and electrical/optical testing remain outstanding.
