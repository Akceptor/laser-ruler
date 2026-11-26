#pragma once

#include <cstddef>
#include <cstdint>
#include <cstring>

namespace ruler {

struct Packet {
    uint8_t function;
    uint8_t status;
    uint16_t decimetres;
};

inline void makeCommand(uint8_t function, uint8_t (&bytes)[8]) {
    bytes[0] = 0x55; bytes[1] = 0xAA; bytes[2] = function;
    for (unsigned i = 3; i < 7; ++i) bytes[i] = 0xFF;
    bytes[7] = static_cast<uint8_t>(function + 4U * 0xFFU);
}

// Sliding window: recover from noise, bad checksums, and partial transmissions.
class Parser {
public:
    void reset() { size_ = 0; }
    bool feed(uint8_t byte, uint32_t now, Packet& result) {
        if (size_ && static_cast<uint32_t>(now - lastByte_) > 100) reset();
        lastByte_ = now;
        bytes_[size_++] = byte;
        while (size_) {
            if (bytes_[0] == 0x55 && (size_ == 1 || bytes_[1] == 0xAA)) {
                if (size_ < 8) return false;
                uint8_t sum = 0;
                for (unsigned i = 0; i < 7; ++i) sum += bytes_[i];
                if (sum == bytes_[7]) {
                    result = Packet{bytes_[2], bytes_[3],
                        static_cast<uint16_t>((uint16_t(bytes_[5]) << 8) | bytes_[6])};
                    reset();
                    return true;
                }
            }
            --size_;
            std::memmove(bytes_, bytes_ + 1, size_);
        }
        return false;
    }
private:
    uint8_t bytes_[8]{};
    size_t size_ = 0;
    uint32_t lastByte_ = 0;
};

enum class Gesture { None, Short, Long };

class Button {
public:
    Gesture update(bool pressed, uint32_t now) {
        if (!initialized_) {
            initialized_ = true;
            raw_ = stable_ = pressed;
            changedAt_ = now;
            armed_ = !pressed;
        }
        if (pressed != raw_) { raw_ = pressed; changedAt_ = now; }
        if (raw_ != stable_ && static_cast<uint32_t>(now - changedAt_) >= 30) {
            stable_ = raw_;
            if (stable_) { pressedAt_ = now; longSent_ = false; }
            else {
                const bool shortPress = armed_ && !longSent_;
                armed_ = true;
                return shortPress ? Gesture::Short : Gesture::None;
            }
        }
        if (stable_ && raw_ && armed_ && !longSent_ &&
            static_cast<uint32_t>(now - pressedAt_) >= 800) {
            longSent_ = true;
            return Gesture::Long;
        }
        return Gesture::None;
    }
private:
    bool initialized_ = false, raw_ = false, stable_ = false;
    bool armed_ = false, longSent_ = false;
    uint32_t changedAt_ = 0, pressedAt_ = 0;
};

enum class State { Idle, Single, Live, Stopping, Fault };
enum class Status { Ready, Measuring, Ok, NoReturn, OutOfRange, Timeout, Stopping, StopFailed };

class Controller {
public:
    // Return the UART function to send, or zero if no command is needed.
    uint8_t gesture(Gesture gesture, uint32_t now) {
        if (gesture == Gesture::None || state_ == State::Single ||
            state_ == State::Stopping || state_ == State::Fault)
            return 0;
        if (gesture == Gesture::Short && state_ == State::Live) return 0;
        hasDistance_ = false;
        lastActivity_ = now;
        if (gesture == Gesture::Long && state_ == State::Live) {
            stopPending_ = true;
            state_ = State::Stopping; status_ = Status::Stopping; return 0x8E;
        }
        state_ = gesture == Gesture::Long ? State::Live : State::Single;
        status_ = Status::Measuring;
        return state_ == State::Live ? 0x89 : 0x88;
    }
    void receive(const Packet& p, uint32_t now) {
        if (stopPending_ && p.function == 0x8E &&
            (state_ == State::Stopping || state_ == State::Live)) {
            hasDistance_ = false;
            state_ = p.status == 1 ? State::Idle : State::Live;
            status_ = p.status == 1 ? Status::Ready : Status::StopFailed;
            if (p.status == 1) stopPending_ = false;
            lastActivity_ = now;
            return;
        }
        if (!((state_ == State::Single && p.function == 0x88) ||
              (state_ == State::Live && p.function == 0x89))) return;
        lastActivity_ = now;
        if (state_ == State::Single) state_ = State::Idle;
        hasDistance_ = false;
        if (p.status != 1) status_ = Status::NoReturn;
        else if (p.decimetres < 40 || p.decimetres > 12000) status_ = Status::OutOfRange;
        else {
            distance_ = p.decimetres; hasDistance_ = true; status_ = Status::Ok;
        }
    }
    void tick(uint32_t now) {
        if (state_ == State::Idle || state_ == State::Fault ||
            static_cast<uint32_t>(now - lastActivity_) < 3000) return;
        hasDistance_ = false;
        if (state_ == State::Stopping) {
            state_ = State::Live; status_ = Status::StopFailed; lastActivity_ = now;
        } else {
            status_ = Status::Timeout;
            // Without request IDs, a delayed single reply cannot be distinguished
            // from a reply to a new request. Require a sensor/host power cycle.
            if (state_ == State::Single) state_ = State::Fault;
        }
    }
    State state() const { return state_; }
    Status status() const { return status_; }
    bool hasDistance() const { return hasDistance_; }
    uint16_t distance() const { return distance_; }
private:
    State state_ = State::Idle;
    Status status_ = Status::Ready;
    bool hasDistance_ = false;
    bool stopPending_ = false;
    uint16_t distance_ = 0;
    uint32_t lastActivity_ = 0;
};

} // namespace ruler
