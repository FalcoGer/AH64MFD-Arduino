#pragma once

#include <Arduino.h>

class AnalogAxis
{
  private:
    uint8_t m_analogPin;

    uint16_t m_calibrated {};
    uint16_t m_raw {};

  public:
    // clang-format off
    static const uint16_t GAMEPAD_ANALOG_MAX = 0xFFFF;
    static const uint16_t ANALOG_MAX_VALUE = 0b1111111111;    // 10 bit max value = 1023
    // clang-format on

    AnalogAxis(const AnalogAxis&)                     = delete;
    AnalogAxis(AnalogAxis&&)                          = delete;
    auto operator= (const AnalogAxis&) -> AnalogAxis& = delete;
    auto operator= (AnalogAxis&&) -> AnalogAxis&      = delete;
    ~AnalogAxis()                                     = default;

    // NOLINTNEXTLINE(bugprone-easily-swappable-parameters)
    explicit AnalogAxis(const uint8_t PIN)
            : m_analogPin {PIN}
    {
        pinMode(PIN, INPUT);
    }

    [[nodiscard]]
    auto get() const noexcept -> uint16_t
    {
        return m_calibrated;
    }

    [[nodiscard]]
    auto getRaw() const noexcept -> uint16_t
    {
        return m_raw;
    }

    void read() noexcept
    {
        m_raw = analogRead(m_analogPin);

        if (m_raw < 0)
        {
            m_calibrated = 0;
            return;
        }

        if (m_raw > ANALOG_MAX_VALUE)
        {
            m_calibrated = GAMEPAD_ANALOG_MAX;
            return;
        }

        // 0000 0011 1111 1111  // max raw value is 10 bits
        // Map 10-bit range [0, 1023] to 16-bit range [-32768, 32767]
        // Map 10-bit range [0x000, 0x3FF] to 16-bit range [0x8000, 0x7FFF]
        static const auto INT16_MIN_VAL = static_cast<int16_t>(0x8000);
        m_calibrated = static_cast<int16_t>((static_cast<uint32_t>(m_raw) * GAMEPAD_ANALOG_MAX / ANALOG_MAX_VALUE) - INT16_MIN_VAL);
    }
};
