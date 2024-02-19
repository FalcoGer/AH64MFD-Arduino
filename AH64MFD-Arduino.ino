#include <HID-Project.h>
#include <HID-Settings.h>

#include "AnalogAxis.h"
#include "Arduino.h"
#include "Array.h"
#include "ButtonMatrix.h"
#include "USBAPI.h"
#include "error.h"
#include "Gamepad.h"

// NOLINTBEGIN(cppcoreguidelines-avoid-non-const-global-variables) // Embedded platform, globals are OK.
// NOLINTBEGIN(cppcoreguidelines-pro-bounds-array-to-pointer-decay) // Requires the use of c functions like sprintf. Using static_cast everywhere is annoying
// NOLINTBEGIN(cppcoreguidelines-pro-type-vararg) // No alternatives to c-style function since no standard library

namespace
{
enum class EModes : uint8_t
{
    INIT_MODE,
    SERIAL_MODE,
    GAMEPAD_MODE,
    CALIBRATION_MODE
};

enum class EAxis : uint8_t
{
    BRT,
    VID,
    SIZE_    // NOLINT(readability-identifier-naming)
};

const size_t   BUFFER_SIZE                 = 128;
// NOLINTNEXTLINE(modernize-avoid-c-arrays) // must be a char array for sprintf and Serial.Write.
char           g_serialBuffer[BUFFER_SIZE] = {};
EModes         g_mode                      = EModes::INIT_MODE;
ButtonMatrix   g_buttonMatrix {};
const uint8_t  PIN_ANALOG_BRT   = PIN_A1;
const uint8_t  PIN_ANALOG_VID   = PIN_A0;

// clang-format off
const uint16_t ANALOG_MAX_VALUE = 0b1111111111;    // 10 bit max value = 1023
// clang-format on
AnalogAxis     g_brtAxis(PIN_ANALOG_BRT, 0U, ANALOG_MAX_VALUE);
AnalogAxis     g_vidAxis(PIN_ANALOG_VID, 0U, ANALOG_MAX_VALUE);
auto           g_axis           = Array<AnalogAxis* const, static_cast<size_t>(EAxis::SIZE_)>({&g_brtAxis, &g_vidAxis});
}    // namespace

// Entry point
void setup()
{
    const uint16_t RESET_DELAY = 250;
    delay(RESET_DELAY);

    g_buttonMatrix.read();

    // Check if FAV and FCR are pressed at the same time
    // If they are during startup, this enables serial mode for debugging

    g_mode = (g_buttonMatrix.get(ButtonMatrix::EButtons::FCR) == ButtonMatrix::EButtonStates::CLOSED
              && g_buttonMatrix.get(ButtonMatrix::EButtons::FAV) == ButtonMatrix::EButtonStates::CLOSED)
               ? EModes::SERIAL_MODE
               : EModes::GAMEPAD_MODE;

    if (g_mode == EModes::SERIAL_MODE)
    {
        // clang-format off
        static const uint32_t BAUD_RATE = 9600;
        // clang-format on
        Serial.begin(BAUD_RATE);
    }
    else if (g_mode == EModes::GAMEPAD_MODE)
    {
        Gamepad.begin();
        Gamepad.releaseAll();
    }
    else
    {
        // Empty
    }
}

// Reads an axis and handles the value according to g_mode.
void handleAxis()
{
    uint8_t axisIdx = 0;
    if (g_mode == EModes::GAMEPAD_MODE)
    {
        for (AnalogAxis* const ptr_axis : g_axis)
        {
            ptr_axis->read();
            if (static_cast<EAxis>(axisIdx) == EAxis::BRT)
            {
                Gamepad.rxAxis(static_cast<int16_t>(ptr_axis->get()));
            }
            else
            {
                Gamepad.ryAxis(static_cast<int16_t>(ptr_axis->get()));
            }
            axisIdx++;
        }
        return;
    }

    if (g_mode == EModes::CALIBRATION_MODE)
    {
        for (AnalogAxis* const ptr_axis : g_axis)
        {
            ptr_axis->calibrate();
        }
        return;
    }

    if (g_mode == EModes::SERIAL_MODE)
    {
        static Array<uint16_t, static_cast<size_t>(EAxis::SIZE_)> lastAxisValues;
        for (AnalogAxis* const ptr_axis : g_axis)
        {
            ptr_axis->read();

            if (ptr_axis->get() != lastAxisValues[axisIdx])
            {
                const double   PERCENT = 100.0 * (static_cast<double>(ptr_axis->get()) / AnalogAxis::GAMEPAD_ANALOG_MAX);
                const uint16_t WRITTEN = sprintf(
                  g_serialBuffer,
                  "%s Calibrated Value: %x / %x (%.2f %%)  |  RAW: %x\n",
                  (static_cast<EAxis>(axisIdx) == EAxis::BRT) ? "BRT" : "VID",
                  ptr_axis->get(),
                  AnalogAxis::GAMEPAD_ANALOG_MAX,
                  PERCENT,
                  ptr_axis->getRaw()
                );
                if (WRITTEN >= BUFFER_SIZE)
                {
                    error("Buffer overrun.", true);
                }
                Serial.write(g_serialBuffer);

                lastAxisValues[axisIdx] = ptr_axis->get();
            }
            axisIdx++;
        }
    }
    else
    {
        // Empty
    }
}

// Toggles calibration mode on/off
void toggleCalibrationMode()
{
    static EModes lastMode = g_mode;
    if (g_mode == EModes::SERIAL_MODE)
    {
        Serial.write("Entering calibration mode.\n");
    }
    if (g_mode != EModes::CALIBRATION_MODE)
    {
        lastMode = g_mode;
        g_mode   = EModes::CALIBRATION_MODE;

        for (AnalogAxis* const ptr_axis : g_axis)
        {
            ptr_axis->startCalibration();
        }
    }
    else
    {
        g_mode = lastMode;
        if (g_mode == EModes::SERIAL_MODE)
        {
            Serial.write("Leaving calibration mode.\n");
            uint8_t axisIdx = 0;
            for (const AnalogAxis* const ptr_axis : g_axis)
            {
                const uint16_t WRITTEN = sprintf(
                  g_serialBuffer,
                  "Cal data %s: %d / %d\n",
                  (static_cast<EAxis>(axisIdx) == EAxis::BRT ? "BRT" : "VID"),
                  ptr_axis->getCalibrationLow(),
                  ptr_axis->getCalibrationHigh()
                );
                if (WRITTEN >= BUFFER_SIZE)
                {
                    error("Buffer overrun.", true);
                }
                Serial.write(g_serialBuffer);
            }
        }
    }
}

// Outputs the button states to the serial console if they have changed since the last time.
void debugOutputButtonStates()
{
    static uint32_t lastStateDisplayed;
    const uint32_t  STATE = g_buttonMatrix.get();

    if (lastStateDisplayed == STATE)
    {
        return;    // nothing to display
    }

    Serial.print("State = ");
    Serial.println(STATE);

    for (uint8_t btnIdx {}; btnIdx < ButtonMatrix::NUMBER_OF_BUTTONS; btnIdx++)
    {
        // NOLINTBEGIN(hicpp-signed-bitwise) // not my fault that the macros suck.
        const bool STATE_OF_BUTTON      = bitRead(STATE, btnIdx);
        const bool LAST_STATE_OF_BUTTON = bitRead(lastStateDisplayed, btnIdx);
        // NOLINTEND(hicpp-signed-bitwise)

        if (STATE_OF_BUTTON != LAST_STATE_OF_BUTTON)
        {
            const uint16_t WRITTEN = sprintf(
              g_serialBuffer,
              "Button %d (%s) now %s\n",
              btnIdx,
              ButtonMatrix::GetButtonName(btnIdx),
              STATE_OF_BUTTON ? "pressed" : "released"
            );

            if (WRITTEN >= BUFFER_SIZE)
            {
                error("Buffer overrun.", true);
            }
            Serial.write(g_serialBuffer);
        }
    }
    lastStateDisplayed = STATE;
}

void loop()
{
    // Read buttons
    g_buttonMatrix.read();

    // when WPN and FAV buttons are pressed at the same time, enter calibration procedure
    auto isCalibrationButtonsPressed = []() -> bool
    {
        return g_buttonMatrix.get(ButtonMatrix::EButtons::FAV) == ButtonMatrix::EButtonStates::CLOSED
               && g_buttonMatrix.get(ButtonMatrix::EButtons::WPN) == ButtonMatrix::EButtonStates::CLOSED;
    };

    if (isCalibrationButtonsPressed())
    {
        // wait for the buttons to no longer be pressed
        while (isCalibrationButtonsPressed())
        {
            delay(1);    // delay first for debouncing
            g_buttonMatrix.read();
        }
        toggleCalibrationMode();
    }

    handleAxis();

    if (g_mode == EModes::GAMEPAD_MODE)
    {
        Gamepad.buttons(g_buttonMatrix.get());
        Gamepad.write();
    }
    else if (g_mode == EModes::SERIAL_MODE)
    {
        debugOutputButtonStates();
    }
    else
    {
        // Empty
    }
}

// NOLINTEND(cppcoreguidelines-pro-type-vararg)
// NOLINTEND(cppcoreguidelines-pro-bounds-array-to-pointer-decay)
// NOLINTEND(cppcoreguidelines-avoid-non-const-global-variables)
