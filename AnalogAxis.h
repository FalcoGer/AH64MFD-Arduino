#pragma once

#include <Arduino.h>

class AnalogAxis
{
  private:
    uint8_t  m_analogPin;
    uint16_t m_calibrationLow;
    uint16_t m_calibrationHigh;

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
    AnalogAxis(const uint8_t PIN, const uint16_t CALIBRATION_LOW, const uint16_t CALIBRATION_HIGH)
            : m_analogPin {PIN}, m_calibrationLow {CALIBRATION_LOW}, m_calibrationHigh {CALIBRATION_HIGH}
    {
        pinMode(PIN, INPUT);
    }

    void startCalibration() noexcept
    {
        const uint16_t VALUE = analogRead(m_analogPin);
        m_calibrationLow     = VALUE;
        m_calibrationHigh    = VALUE;
    }

    void calibrate() noexcept
    {
        const uint16_t VALUE = analogRead(m_analogPin);
        m_calibrationLow     = min(m_calibrationLow, VALUE);
        m_calibrationHigh    = max(m_calibrationHigh, VALUE);
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

    [[nodiscard]]
    auto getCalibrationLow() const noexcept -> uint16_t
    {
        return m_calibrationLow;
    }

    [[nodiscard]]
    auto getCalibrationHigh() const noexcept -> uint16_t
    {
        return m_calibrationHigh;
    }

    void read() noexcept
    {
        m_raw = analogRead(m_analogPin);

        if (m_calibrationLow == m_calibrationHigh)
        {
            m_calibrated = GAMEPAD_ANALOG_MAX;
        }

        if (m_raw < m_calibrationLow)
        {
            m_calibrated = 0;
        }

        if (m_raw > m_calibrationHigh)
        {
            m_calibrated = GAMEPAD_ANALOG_MAX;
        }

        // normalize value from value in [low .. high] to [0 .. GAMEPAD_ANALOG_MAX]
        // div by 2 because only need positive values.
        m_calibrated = static_cast<uint16_t>(
          ((static_cast<int32_t>(m_raw - m_calibrationLow) * GAMEPAD_ANALOG_MAX)
           / (m_calibrationHigh - m_calibrationLow))
          - ((static_cast<int32_t>(GAMEPAD_ANALOG_MAX) / 2) + 1)
        );
    }
};
