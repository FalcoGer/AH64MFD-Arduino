#pragma once

#include <Arduino.h>

[[noreturn]]
void error(const char* const /*ptr_msg*/);

[[noreturn]]
inline void error(const char* const ptr_msg, const bool SERIAL_OUTPUT)
{
    pinMode(LED_BUILTIN_RX, OUTPUT);

    // clang-format off
    const uint16_t ONE_SECOND = 1000;
    const uint16_t TWO_HUNDRED_MS = 200;
    // clang-format on

    while (true)
    {
        delay(ONE_SECOND);
        if (SERIAL_OUTPUT)
        {
            const char* const ptr_divider = "=================================\n";
            Serial.write(ptr_divider);
            Serial.write("FATAL ERROR\n");
            Serial.write(ptr_msg);
            Serial.write("\n");
            Serial.write(ptr_divider);
        }

        digitalWrite(LED_BUILTIN_RX, static_cast<uint8_t>(HIGH));
        delay(TWO_HUNDRED_MS);
        digitalWrite(LED_BUILTIN_RX, static_cast<uint8_t>(LOW));
        delay(TWO_HUNDRED_MS);

        digitalWrite(LED_BUILTIN_RX, static_cast<uint8_t>(HIGH));
        delay(TWO_HUNDRED_MS);
        digitalWrite(LED_BUILTIN_RX, static_cast<uint8_t>(LOW));

    }
}
