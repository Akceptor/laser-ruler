#include "ruler_core.h"
#include <cassert>
#include <cstdio>
#ifdef PIO_UNIT_TESTING
#include <unity.h>
#undef assert
#define assert(condition) TEST_ASSERT_TRUE(condition)
void setUp() {}
void tearDown() {}
#endif

using namespace ruler;

static Packet packet(uint8_t fn, uint8_t status = 1, uint16_t distance = 1234) {
    return Packet{fn, status, distance};
}

static void test_protocol() {
    uint8_t cmd[8];
    makeCommand(0x88, cmd);
    const uint8_t expected[] = {0x55, 0xAA, 0x88, 0xFF, 0xFF, 0xFF, 0xFF, 0x84};
    for (unsigned i = 0; i < 8; ++i) assert(cmd[i] == expected[i]);
    makeCommand(0x89, cmd); assert(cmd[7] == 0x85);
    makeCommand(0x8E, cmd); assert(cmd[7] == 0x8A);

    Parser parser;
    Packet out{};
    // Valid response: 123.4 m. Response checksum includes both header bytes.
    const uint8_t reply[] = {0x55, 0xAA, 0x88, 1, 0xFF, 4, 0xD2, 0x5D};
    assert(!parser.feed(0x00, 0, out));
    assert(!parser.feed(0x55, 1, out));
    for (unsigned i = 0; i < 8; ++i)
        assert(parser.feed(reply[i], 10 + i, out) == (i == 7));
    assert(out.function == 0x88 && out.status == 1 && out.decimetres == 1234);
    // Corruption must not prevent decoding the following frame.
    for (unsigned i = 0; i < 8; ++i)
        assert(!parser.feed(i == 7 ? 0 : reply[i], 30 + i, out));
    for (unsigned i = 0; i < 8; ++i)
        assert(parser.feed(reply[i], 50 + i, out) == (i == 7));
    // A stalled partial packet cannot poison a later response.
    parser.feed(0x55, 70, out); parser.feed(0xAA, 71, out);
    for (unsigned i = 0; i < 8; ++i)
        assert(parser.feed(reply[i], 300 + i, out) == (i == 7));
}

static void test_button() {
    Button b;
    assert(b.update(false, 0) == Gesture::None);
    b.update(false, 31);
    b.update(true, 100); b.update(false, 110); // Bounce.
    b.update(true, 120); b.update(true, 151);
    b.update(false, 200);
    assert(b.update(false, 231) == Gesture::Short);
    b.update(true, 300); b.update(true, 331);
    assert(b.update(true, 1131) == Gesture::Long);
    assert(b.update(true, 1200) == Gesture::None);
    b.update(false, 1300);
    assert(b.update(false, 1331) == Gesture::None);
    // A button held at startup must first be released, without an action.
    Button held;
    held.update(true, 0); held.update(true, 31);
    assert(held.update(true, 1000) == Gesture::None);
    held.update(false, 1100);
    assert(held.update(false, 1131) == Gesture::None);
    // Debounce and press duration remain valid across millis() rollover.
    Button wrap;
    wrap.update(false, 0xFFFFFF00U); wrap.update(false, 0xFFFFFF20U);
    wrap.update(true, 0xFFFFFF30U); wrap.update(true, 0xFFFFFF50U);
    assert(wrap.update(true, 0x00000300U) == Gesture::Long);
}

static void test_measurements() {
    Controller c;
    assert(c.gesture(Gesture::Short, 100) == 0x88);
    assert(c.gesture(Gesture::Short, 200) == 0); // One outstanding request.
    c.receive(packet(0x89), 201); assert(!c.hasDistance()); // Wrong mode.
    c.receive(packet(0x88), 250);
    assert(c.hasDistance() && c.distance() == 1234 && c.state() == State::Idle);
    c.gesture(Gesture::Short, 300);
    assert(!c.hasDistance()); // Never show an old result as current.
    c.receive(packet(0x88, 0), 350);
    assert(c.status() == Status::NoReturn && !c.hasDistance());
    c.gesture(Gesture::Short, 400); c.receive(packet(0x88, 1, 0xFFFF), 450);
    assert(c.status() == Status::OutOfRange && !c.hasDistance());
    c.gesture(Gesture::Short, 500); c.tick(3500);
    assert(c.status() == Status::Timeout && c.state() != State::Idle);
    c.receive(packet(0x88), 3501); assert(!c.hasDistance()); // Late response.
    // No request IDs exist: do not accept another request after a single timeout.
    assert(c.gesture(Gesture::Short, 3600) == 0);
    c.receive(packet(0x88), 3601); assert(!c.hasDistance());
    assert(c.gesture(Gesture::Long, 3700) == 0);
}

static void test_continuous() {
    Controller c;
    assert(c.gesture(Gesture::Long, 100) == 0x89);
    assert(c.state() == State::Live);
    c.receive(packet(0x89), 200); assert(c.hasDistance());
    c.tick(3200); assert(!c.hasDistance() && c.status() == Status::Timeout);
    c.receive(packet(0x89), 3300); assert(c.hasDistance()); // Recover.
    assert(c.gesture(Gesture::Long, 3400) == 0x8E);
    c.receive(packet(0x89), 3450); assert(c.state() == State::Stopping);
    c.receive(packet(0x8E), 3500); assert(c.state() == State::Idle);
    assert(!c.hasDistance());
    c.gesture(Gesture::Long, 4000); c.gesture(Gesture::Long, 4100);
    c.tick(7100);
    assert(c.state() == State::Live && c.status() == Status::StopFailed);
    assert(c.gesture(Gesture::Long, 7200) == 0x8E); // Retry stop.
    c.receive(packet(0x8E, 0), 7250);
    assert(c.state() == State::Live && c.status() == Status::StopFailed);
    Controller wrap;
    wrap.gesture(Gesture::Short, 0xFFFFFF00U);
    wrap.tick(0x00000AB8U);
    assert(wrap.status() == Status::Timeout);
}

static void test_late_stop_ack() {
    Controller c;
    c.gesture(Gesture::Long, 0);
    c.gesture(Gesture::Long, 100);
    c.tick(3100);
    assert(c.status() == Status::StopFailed);
    c.receive(packet(0x89), 3101);
    assert(c.hasDistance());
    c.receive(packet(0x8E), 3102);
    assert(c.state() == State::Idle && c.status() == Status::Ready);
    assert(!c.hasDistance());
}

int main() {
#ifdef PIO_UNIT_TESTING
    UNITY_BEGIN();
    RUN_TEST(test_protocol);
    RUN_TEST(test_button);
    RUN_TEST(test_measurements);
    RUN_TEST(test_continuous);
    RUN_TEST(test_late_stop_ack);
    return UNITY_END();
#else
    test_protocol(); test_button(); test_measurements(); test_continuous(); test_late_stop_ack();
    std::puts("All protocol, button, and controller tests passed.");
#endif
}
