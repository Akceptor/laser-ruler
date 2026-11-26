#include <Arduino.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include "config.h"
#include "ruler_core.h"

namespace {
HardwareSerial rangeSerial(1);
Adafruit_SSD1306 display(config::kDisplayWidth, config::kDisplayHeight, &Wire, -1);
ruler::Parser parser;
ruler::Button button;
ruler::Controller controller;
bool displayReady = false;
uint32_t lastDisplay = 0;

const char* statusText(ruler::Status status) {
    switch (status) {
        case ruler::Status::Ready: return "Ready";
        case ruler::Status::Measuring: return "Measuring...";
        case ruler::Status::Ok: return "Measured";
        case ruler::Status::NoReturn: return "No return";
        case ruler::Status::OutOfRange: return "Out of range";
        case ruler::Status::Timeout: return "Sensor timeout";
        case ruler::Status::Stopping: return "Stopping...";
        case ruler::Status::StopFailed: return "Stop failed: hold BOOT";
    }
    return "Unknown";
}

const char* modeText() {
    if (controller.state() == ruler::State::Fault) return "FAULT";
    if (controller.state() == ruler::State::Live) return "LIVE";
    if (controller.state() == ruler::State::Stopping) return "STOP";
    return "SINGLE";
}

const char* currentStatusText() {
    if (controller.state() == ruler::State::Fault) return "Timeout: cycle power";
    return statusText(controller.status());
}

bool responds(uint8_t address) {
    Wire.beginTransmission(address);
    return Wire.endTransmission() == 0;
}

void initDisplay() {
    Wire.begin(config::kSda, config::kScl);
    Wire.setClock(100000);
    Wire.setTimeOut(20);
    uint8_t address = config::kDisplayAddress;
    if (!address) {
        if (responds(0x3C)) address = 0x3C;
        else if (responds(0x3D)) address = 0x3D;
    }
    if (!address || !responds(address)) {
        Serial.println("OLED not found at configured address (auto: 0x3C/0x3D)");
        return;
    }
    // Wire has already been configured with explicit pins. No reset pin.
    displayReady = display.begin(SSD1306_SWITCHCAPVCC, address, false, false);
    if (!displayReady) {
        Serial.println("OLED allocation/init failed; serial output remains available");
        return;
    }
    display.clearDisplay();
    display.setTextColor(SSD1306_WHITE);
    display.setTextWrap(false);
    Serial.printf("OLED at 0x%02X (%ux%u)\n", address,
                  config::kDisplayWidth, config::kDisplayHeight);
}

void sendCommand(uint8_t function) {
    // Discard bytes from a previous transaction before starting a new one.
    while (rangeSerial.available()) rangeSerial.read();
    parser.reset();
    uint8_t bytes[8];
    ruler::makeCommand(function, bytes);
    rangeSerial.write(bytes, sizeof(bytes));
    Serial.printf("TX function 0x%02X:", function);
    for (uint8_t b : bytes) Serial.printf(" %02X", b);
    Serial.println();
}

void render() {
    if (!displayReady) return;
    display.clearDisplay();
    display.setTextSize(1);
    display.setCursor(0, 0);
    display.print("LR1200E2 "); display.print(modeText());
    display.setTextSize(config::kDisplayHeight == 64 ? 3 : 2);
    display.setCursor(0, config::kDisplayHeight == 64 ? 17 : 8);
    if (controller.hasDistance()) display.print(controller.distance() / 10.0f, 1);
    else display.print("---");
    display.setTextSize(1);
    display.setCursor(120, config::kDisplayHeight == 64 ? 31 : 16);
    display.print("m");
    display.setCursor(0, config::kDisplayHeight == 64 ? 45 : 24);
    display.print(currentStatusText());
    if (config::kDisplayHeight == 64) {
        display.setCursor(0, 56);
        display.print("BOOT: tap / hold");
    }
    display.display();
}

void reportChanges() {
    static ruler::Status previousStatus = ruler::Status::Ready;
    static ruler::State previousState = ruler::State::Idle;
    static uint16_t previousDistance = 0;
    if (previousStatus == controller.status() && previousState == controller.state() &&
        previousDistance == controller.distance()) return;
    previousStatus = controller.status(); previousState = controller.state();
    previousDistance = controller.distance();
    Serial.printf("%s: %s", modeText(), currentStatusText());
    if (controller.hasDistance()) Serial.printf(" %.1f m", controller.distance() / 10.0f);
    Serial.println();
}
} // namespace

void setup() {
    Serial.begin(115200);
    pinMode(config::kBoot, INPUT_PULLUP);
    button.update(digitalRead(config::kBoot) == LOW, millis());
    if (config::kEnablePin >= 0) {
        digitalWrite(config::kEnablePin, config::kEnableActiveHigh ? HIGH : LOW);
        pinMode(config::kEnablePin, OUTPUT);
    }
    rangeSerial.begin(115200, SERIAL_8N1, config::kRangeRx, config::kRangeTx);
    delay(250); // Sensor startup <=200 ms; no boot-time measurement request.
    Serial.println("Laser ruler: tap BOOT to measure; hold 800 ms to toggle live mode");
    if (config::kEnablePin < 0)
        Serial.println("Enable wiring: SW-SHOT tied to sensor VCC; no enable GPIO driven");
    else
        Serial.printf("Enable GPIO %d: active %s\n", config::kEnablePin,
                      config::kEnableActiveHigh ? "HIGH" : "LOW");
    initDisplay();
    render();
}

void loop() {
    // Service UART before input; drain at most 256 bytes to keep input responsive.
    bool sawRx = false;
    for (unsigned i = 0; i < 256 && rangeSerial.available(); ++i) {
        const uint8_t byte = static_cast<uint8_t>(rangeSerial.read());
        if (!sawRx) {
            Serial.print("RX:");
            sawRx = true;
        }
        Serial.printf(" %02X", byte);
        ruler::Packet packet{};
        if (parser.feed(byte, millis(), packet)) {
            Serial.println();
            sawRx = false;
            if (packet.function == 0x80)
                Serial.printf("Sensor self-test status: %u\n", packet.status);
            controller.receive(packet, millis());
        }
    }
    if (sawRx) Serial.println();
    const uint32_t now = millis();
    controller.tick(now);
    const auto gesture = button.update(digitalRead(config::kBoot) == LOW, now);
    const uint8_t command = controller.gesture(gesture, now);
    if (command) sendCommand(command);
    reportChanges();
    if (static_cast<uint32_t>(now - lastDisplay) >= config::kDisplayIntervalMs) {
        lastDisplay = now;
        render();
    }
    delay(1);
}
