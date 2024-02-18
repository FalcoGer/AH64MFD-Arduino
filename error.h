#pragma once

#include <Arduino.h>

[[noreturn]]
void error(const char* const /*ptr_msg*/);

[[noreturn]]
inline void error(const char* const ptr_msg, const bool SERIAL_OUTPUT)
{
    while (true)
    {
        // clang-format off
        const uint16_t ONE_SECOND = 1000;
        // clang-format on
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
    }
}
