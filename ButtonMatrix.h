#pragma once

#include <Arduino.h>

#include "Array.h"

class ButtonMatrix
{
  public:
    // clang-format off
    enum class EButtons : uint8_t
    {
        TOP1, TOP2, TOP3, TOP4, TOP5, TOP6, RIGHT1, RIHGT2,
        RIGHT3, RIGHT4, RIGHT5, RIGHT6, BOTTOM1, BOTTOM2, BOTTOM3, BOTTOM4,
        BOTTOM5, BOTTOM6, LEFT1, LEFT2, LEFT3, LEFT4, LEFT5, LEFT6,
        FAV, VID, COM, AC, TSD, WPN, FCR, NOT_CONNECTED,
        SIZE_ // NOLINT(readability-identifier-naming)
    };
    // clang-format on

  private:
    uint32_t                       m_states {};

    static const uint8_t           PIN_MUX_INHIBIT = 6;     // D6 goes to 4051 Inhibit (NOT_ENABLE) input
    static const uint8_t           PIN_MUX_COMMON  = 10;    // D10 goes to 4051 X/Common input/output.
    static const Array<uint8_t, 3> MUX_ADDR_PINS;
    static const Array<uint8_t, 4> BTN_MATRIX_PINS;

    static const uint8_t           NUMBER_OF_MUX_ADDRESSES = 1U << 3U;
    static const uint16_t          MUX_SETTLE_DELAY        = 1;    // Datasheet timing is in ns.
    static const uint16_t          READ_DELAY              = 1;

    static void                    SetMuxAddr(const uint8_t ADDR) noexcept
    {
        digitalWrite(PIN_MUX_INHIBIT, HIGH);

        uint8_t idx = 0;
        for (const auto PIN : MUX_ADDR_PINS)
        {
            const bool STATE = (ADDR & (0b001U << idx++)) != 0;
            digitalWrite(PIN, static_cast<uint8_t>(STATE));
        }

        digitalWrite(PIN_MUX_INHIBIT, LOW);

        delay(MUX_SETTLE_DELAY);
    }

    void setBit(const uint8_t BIT, const bool VALUE) noexcept
    {
        if (VALUE)
        {
            m_states |= (1U << BIT);
        }
        else
        {
            m_states &= ~(1U << BIT);
        }
    }

  public:
    static const uint8_t NUMBER_OF_BUTTONS = static_cast<uint8_t>(EButtons::SIZE_);
    ButtonMatrix()
    {
        pinMode(PIN_MUX_INHIBIT, OUTPUT);
        pinMode(PIN_MUX_COMMON, INPUT);
        for (const auto PIN : MUX_ADDR_PINS)
        {
            pinMode(PIN, OUTPUT);
        }

        for (const auto PIN : BTN_MATRIX_PINS)
        {
            pinMode(PIN, OUTPUT);
            digitalWrite(PIN, LOW);
        }
    }

    enum class EButtonStates : uint8_t
    {
        OPEN,
        CLOSED
    };

    [[nodiscard]]
    auto get() const noexcept -> uint32_t
    {
        return m_states;
    }

    [[nodiscard]]
    auto get(const EButtons BUTTON) const noexcept -> EButtonStates
    {
        return ((1U << static_cast<uint8_t>(BUTTON)) & m_states) != 0U ? EButtonStates::CLOSED : EButtonStates::OPEN;
    }

    void read() noexcept
    {
        // walk backwards because wireing has logically first buttons on last address
        for (uint8_t muxAddr = 0; muxAddr < NUMBER_OF_MUX_ADDRESSES; muxAddr++)
        {
            SetMuxAddr(muxAddr);
            uint8_t btnRowIdx = 0;
            for (const uint8_t BTN : BTN_MATRIX_PINS)
            {
                // Set the corresponding button pin to high and then check the MUX common output if the signal arrives.
                digitalWrite(BTN, HIGH);
                delay(READ_DELAY);
                const bool STATE = (digitalRead(PIN_MUX_COMMON) != LOW);
                digitalWrite(BTN, LOW);

                const uint8_t BTN_IDX_LOGICAL = static_cast<uint8_t>((NUMBER_OF_MUX_ADDRESSES - 1 - muxAddr))
                                                | static_cast<uint8_t>(btnRowIdx << MUX_ADDR_PINS.size());
                btnRowIdx++;

                setBit(BTN_IDX_LOGICAL, STATE);
            }
        }
    }

    [[nodiscard]]
    static auto GetButtonName(const uint8_t LOGICAL_BTN_IDX) noexcept -> const char*
    {
        switch (static_cast<EButtons>(LOGICAL_BTN_IDX))
        {
            case EButtons::TOP1:          return "T1";
            case EButtons::TOP2:          return "T2";
            case EButtons::TOP3:          return "T3";
            case EButtons::TOP4:          return "T4";
            case EButtons::TOP5:          return "T5";
            case EButtons::TOP6:          return "T6";
            case EButtons::RIGHT1:        return "R1";
            case EButtons::RIHGT2:        return "R2";
            case EButtons::RIGHT3:        return "R3";
            case EButtons::RIGHT4:        return "R4";
            case EButtons::RIGHT5:        return "R5";
            case EButtons::RIGHT6:        return "R6";
            case EButtons::BOTTOM1:       return "M/B1";
            case EButtons::BOTTOM2:       return "B2";
            case EButtons::BOTTOM3:       return "B3";
            case EButtons::BOTTOM4:       return "B4";
            case EButtons::BOTTOM5:       return "B5";
            case EButtons::BOTTOM6:       return "B6";
            case EButtons::LEFT1:         return "L1";
            case EButtons::LEFT2:         return "L2";
            case EButtons::LEFT3:         return "L3";
            case EButtons::LEFT4:         return "L4";
            case EButtons::LEFT5:         return "L5";
            case EButtons::LEFT6:         return "L6";
            case EButtons::FAV:           return "FAV";
            case EButtons::VID:           return "VID";
            case EButtons::COM:           return "COM";
            case EButtons::AC:            return "A/C";
            case EButtons::TSD:           return "TSD";
            case EButtons::WPN:           return "WPN";
            case EButtons::FCR:           return "FCR";
            case EButtons::NOT_CONNECTED: return "Not Connected";
            default:                      return "INVALID";
        }
    }
};

inline const Array<uint8_t, 3> ButtonMatrix::MUX_ADDR_PINS = {
  static_cast<uint8_t>(9),    // D9 goes to 4051 A input
  static_cast<uint8_t>(8),    // D8 goes to 4051 B input
  static_cast<uint8_t>(7)     // D7 goes to 4051 C input
};

inline const Array<uint8_t, 4> ButtonMatrix::BTN_MATRIX_PINS = {
  static_cast<uint8_t>(2),    // D2 connects to First row of button matrix (T1-T6, R1-R2)
  static_cast<uint8_t>(3),    // D3 connects to Second row of button matrix (R3-R6, B1-B4)
  static_cast<uint8_t>(4),    // D4 connects to Third row of button matrix (B5-B6, L1-L6)
  static_cast<uint8_t>(5)     // D5 connects to Fourth row of button matrix
                              // (Fav, Vid, Com, AC, TSD, WPN, FCR, notConnected)
};
