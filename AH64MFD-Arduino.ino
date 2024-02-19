#include <Arduino.h>
#include <HID-Project.h>
#include <HID-Settings.h>

#include "AnalogAxis.h"
#include "Array.h"
#include "ButtonMatrix.h"
#include "error.h"

// NOLINTBEGIN(cppcoreguidelines-avoid-non-const-global-variables) // Embedded platform, globals are OK.
// NOLINTBEGIN(cppcoreguidelines-pro-bounds-array-to-pointer-decay) // Requires the use of c functions like sprintf. Using static_cast everywhere is annoying
// NOLINTBEGIN(cppcoreguidelines-pro-type-vararg) // No alternatives to c-style function since no standard library

namespace
{
enum class EModes : uint8_t
{
    INIT_MODE,
    SERIAL_MODE,
    GAMEPAD_MODE
};

enum class EAxis : uint8_t
{
    BRT,
    VID,
    SIZE_    // NOLINT(readability-identifier-naming)
};

const size_t  BUFFER_SIZE                 = 128;
// NOLINTNEXTLINE(modernize-avoid-c-arrays) // must be a char array for sprintf and Serial.Write.
char          g_serialBuffer[BUFFER_SIZE] = {};
EModes        g_mode                      = EModes::INIT_MODE;
const uint8_t PIN_ANALOG_BRT = PIN_A1;
const uint8_t PIN_ANALOG_VID = PIN_A0;

AnalogAxis    g_brtAxis(PIN_ANALOG_BRT);
AnalogAxis    g_vidAxis(PIN_ANALOG_VID);
auto          g_axis = Array<AnalogAxis* const, static_cast<size_t>(EAxis::SIZE_)>({&g_brtAxis, &g_vidAxis});
}    // namespace

// Entry point
void setup()
{
    pinMode(LED_BUILTIN_RX, OUTPUT);
    digitalWrite(LED_BUILTIN_RX, LOW);
    const uint16_t RESET_DELAY = 250;
    delay(RESET_DELAY);

    ButtonMatrix::Inst().read();

    // Check if FAV and FCR are pressed at the same time
    // If they are during startup, this enables serial mode for debugging

    g_mode = (ButtonMatrix::Inst().get(ButtonMatrix::EButtons::FCR) == ButtonMatrix::EButtonStates::CLOSED
              && ButtonMatrix::Inst().get(ButtonMatrix::EButtons::FAV) == ButtonMatrix::EButtonStates::CLOSED)
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

void handleAxisGamepadMode()
{
    uint8_t axisIdx = 0;
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
}

void handleAxisSerialMode()
{
    uint8_t axisIdx = 0;

    for (AnalogAxis* const ptr_axis : g_axis)
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
        axisIdx++;
    }
}

// Reads an axis and handles the value according to g_mode.
[[nodiscard]]
auto handleAxis() -> bool
{
    static Array<uint16_t, static_cast<size_t>(EAxis::SIZE_)> lastAxisValues;
    bool                                                      hasChanged = false;

    uint8_t                                                   axisIdx    = 0;
    for (AnalogAxis* const ptr_axis : g_axis)
    {
        ptr_axis->read();

        if (abs(ptr_axis->getRaw() - lastAxisValues[axisIdx]) > 1U)
        {
            lastAxisValues[axisIdx] = ptr_axis->getRaw();
            hasChanged              = true;
        }

        axisIdx++;
    }

    if (g_mode == EModes::GAMEPAD_MODE)
    {
        handleAxisGamepadMode();
    }
    else if (g_mode == EModes::SERIAL_MODE)
    {
        if (hasChanged)
        {
            handleAxisSerialMode();
        }
    }
    else
    {
        // empty
    }

    return hasChanged;
}

// Outputs the button states to the serial console if they have changed since the last time.
void debugOutputButtonStates()
{
    static uint32_t lastStateDisplayed;
    const uint32_t  STATE = ButtonMatrix::Inst().get();

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
    ButtonMatrix::Inst().read();

    static uint32_t lastButtonStateSent;

    bool axisUpdate = handleAxis();

    if (g_mode == EModes::GAMEPAD_MODE)
    {
        Gamepad.buttons(ButtonMatrix::Inst().get());

        static uint32_t lastReportSentTime = 0;
        // clang-format off
        static const uint32_t MAX_TIME_BETWEEN_REPORTS_IN_MS = 5000;
        // clang-format on
        const uint32_t ELAPSED_TIME = millis();
        // only send a new report when something has actually changed.
        if (axisUpdate || lastButtonStateSent != ButtonMatrix::Inst().get() || (ELAPSED_TIME - lastReportSentTime >= MAX_TIME_BETWEEN_REPORTS_IN_MS))
        {
            Gamepad.write();    // extremely slow
            lastButtonStateSent = ButtonMatrix::Inst().get();
            lastReportSentTime = ELAPSED_TIME;
        }
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
