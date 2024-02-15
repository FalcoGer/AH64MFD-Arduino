#include <HID-Project.h>
#include <HID-Settings.h>

// NOLINTBEGIN(cppcoreguidelines-avoid-non-const-global-variables) // Embedded platform, globals are OK.
// NOLINTBEGIN(cppcoreguidelines-pro-bounds-array-to-pointer-decay) // Requires the use of c functions like sprintf. Using static_cast everywhere is annoying
// NOLINTBEGIN(cppcoreguidelines-pro-type-vararg) // No alternatives to c-style function since no standard library

enum class EModes : uint8_t
{
    INIT_MODE,
    SERIAL_MODE,
    GAMEPAD_MODE,
    CALIBRATION_MODE,
    FAILURE_MODE
};

const size_t BUFFER_SIZE                 = 128;
// NOLINTNEXTLINE(modernize-avoid-c-arrays) // must be a char array for sprintf and Serial.Write.
char         g_serialBuffer[BUFFER_SIZE] = {};
EModes       g_mode                      = EModes::INIT_MODE;

enum class EButtonStates : uint8_t
{
    OPEN,
    CLOSED
};

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

const uint8_t NUMBER_OF_BUTTONS = static_cast<uint8_t>(EButtons::SIZE_);

enum class EAxis : uint8_t
{
    AXIS_VIDEO,
    AXIS_BRIGHTNESS,
    SIZE_    // NOLINT(readability-identifier-naming)
};

void error(const char* const ptr_msg)
{
    EModes lastMode = g_mode;
    g_mode          = EModes::FAILURE_MODE;
    while (true)
    {
        // clang-format off
        const uint16_t ONE_SECOND = 1000;
        // clang-format on
        delay(ONE_SECOND);
        if (lastMode == EModes::SERIAL_MODE)
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

template <typename T, size_t n>
class Array
{
  private:
    // NOLINTNEXTLINE(modernize-avoid-c-arrays) // This is the array class to get rid of arrays. No choice.
    T m_data[n];

  public:
    Array() { static_assert(n > 0); }

    template <typename... Args>
    Array(Args... args) : m_data {args...}    // NOLINT(google-explicit-constructor)
    {
        static_assert(sizeof...(args) == n, "Incorrect number of arguments");
        static_assert(n > 0);
    }

    using ValueType     = T;
    using SizeType      = size_t;
    using Iterator      = T*;
    using ConstIterator = const T*;

    [[nodiscard]]
    constexpr auto size() const -> SizeType
    {
        return n;
    }
    [[nodiscard]]
    auto begin() -> Iterator
    {
        return static_cast<Iterator>(m_data);
    }
    [[nodiscard]]
    auto end() -> Iterator
    {
        return static_cast<Iterator>(m_data) + n;
    }
    [[nodiscard]]
    auto begin() const -> ConstIterator
    {
        return static_cast<ConstIterator>(m_data);
    }
    [[nodiscard]]
    auto end() const -> ConstIterator
    {
        return static_cast<ConstIterator>(m_data) + n;
    }

    auto operator[] (const SizeType INDEX) -> T&
    {
        if (INDEX < 0 || INDEX >= n)
        {
            (void)sprintf(g_serialBuffer, "Index out of range. Valid: 0 .. %d. Actual: %d\n", n - 1, INDEX);
            error(g_serialBuffer);
        }
        // // NOLINTNEXTLINE(cppcoreguidelines-pro-bounds-constant-array-index) // No choice
        return m_data[INDEX];
    }
    auto operator[] (const SizeType INDEX) const -> const T&
    {
        // NOLINTNEXTLINE(cppcoreguidelines-pro-type-const-cast) // Using const cast to avoid code duplication. Non const version doesn't modify.
        return const_cast<Array*>(this)->operator[] (INDEX);
    }
};

// clang-format off
const uint16_t          ANALOG_MAX_VALUE     = 0b1111111111;    // 10 bit max value = 1023
// clang-format on
// calibration data. Set to actual raw reading for min/max on the pot.
uint16_t                g_calibrationLowBrt  = 0;
uint16_t                g_calibrationHighBrt = ANALOG_MAX_VALUE;
uint16_t                g_calibrationLowVid  = 0;
uint16_t                g_calibrationHighVid = ANALOG_MAX_VALUE;
// clang-format off
const uint16_t          GAMEPAD_ANALOG_MAX   = 0xFFFF;
// clang-format on

const uint8_t           PIN_MUX_INHIBIT      = 6;     // D6 goes to 4051 Inhibit (NOT_ENABLE) input
const uint8_t           PIN_MUX_COMMON       = 10;    // D10 goes to 4051 X/Common input/output.
const Array<uint8_t, 3> MUX_ADDR_PINS        = {
  static_cast<uint8_t>(9),                     // D9 goes to 4051 A input
  static_cast<uint8_t>(8),                     // D8 goes to 4051 B input
  static_cast<uint8_t>(7)                      // D7 goes to 4051 C input
};

const Array<uint8_t, 4> BTN_MATRIX_PINS = {
  static_cast<uint8_t>(2),    // D2 connects to First row of button matrix (T1-T6, R1-R2)
  static_cast<uint8_t>(3),    // D3 connects to Second row of button matrix (R3-R6, B1-B4)
  static_cast<uint8_t>(4),    // D4 connects to Third row of button matrix (B5-B6, L1-L6)
  static_cast<uint8_t>(5)     // D5 connects to Fourth row of button matrix
                              // (Fav, Vid, Com, AC, TSD, WPN, FCR, notConnected)
};

const uint8_t PIN_ANALOG_BRT          = PIN_A1;
const uint8_t PIN_ANALOG_VID          = PIN_A0;

const uint8_t NUMBER_OF_MUX_ADDRESSES = 0b1000;

void          setMuxAddr(const uint8_t ADDR)
{
    digitalWrite(PIN_MUX_INHIBIT, HIGH);

    uint8_t idx = 0;
    for (const auto PIN : MUX_ADDR_PINS)
    {
        const bool STATE = (ADDR & (0b001U << idx++)) != 0;
        digitalWrite(PIN, static_cast<uint8_t>(STATE));
    }

    digitalWrite(PIN_MUX_INHIBIT, LOW);

    const uint16_t MUX_SETTLE_DELAY = 1;    // Datasheet timing is in ns.
    delay(MUX_SETTLE_DELAY);
}

// Returns a 16 bit normalized value for a gamepad axis from a given value and a low and high end for those values.
auto getCalibratedAnalogValue(const uint16_t RAW_VALUE, const uint16_t LOW_VALUE, const uint16_t HIGH_VALUE) -> uint16_t
{
    if (LOW_VALUE > HIGH_VALUE)
    {
        (void)sprintf(g_serialBuffer, "LOW_VALUE (%d) not lower than HIGH_VALUE %d", LOW_VALUE, HIGH_VALUE);
        error(g_serialBuffer);
    }

    if (LOW_VALUE == HIGH_VALUE)
    {
        return GAMEPAD_ANALOG_MAX;
    }

    if (RAW_VALUE < LOW_VALUE)
    {
        return 0;
    }
    if (RAW_VALUE > HIGH_VALUE)
    {
        return GAMEPAD_ANALOG_MAX;
    }

    // normalize value from value in [low .. high] to [0 .. GAMEPAD_ANALOG_MAX]
    // div by 2 because only need positive values.
    return static_cast<uint16_t>(
      ((static_cast<int32_t>(RAW_VALUE - LOW_VALUE) * GAMEPAD_ANALOG_MAX) / (HIGH_VALUE - LOW_VALUE)) - ((static_cast<int32_t>(GAMEPAD_ANALOG_MAX) / 2) + 1)
    );
}

// Entry point
void setup()
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

    const uint16_t RESET_DELAY = 250;
    delay(RESET_DELAY);

    // Check if FAV and FCR are pressed at the same time
    // If they are during startup, this enables serial mode for debugging
    const uint8_t BTN_ROW = BTN_MATRIX_PINS[3];    // FCR and WPN on the same row
    digitalWrite(BTN_ROW, HIGH);

    const uint8_t ADDR_OF_FCR = 0b001U;
    setMuxAddr(ADDR_OF_FCR);

    const bool    FCR_PRESSED = digitalRead(PIN_MUX_COMMON) != LOW;

    const uint8_t ADDR_OF_FAV = 0b111U;
    setMuxAddr(ADDR_OF_FAV);
    const bool FAV_PRESSED = digitalRead(PIN_MUX_COMMON) != LOW;
    digitalWrite(BTN_ROW, LOW);

    g_mode = (FCR_PRESSED && FAV_PRESSED) ? EModes::SERIAL_MODE : EModes::GAMEPAD_MODE;
    // g_mode = EModes::SERIAL_MODE;
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

auto getButtonName(const uint8_t LOGICAL_BTN_IDX) -> const char*
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
        default:                      error("Bad button idx"); return "FATAL ERROR";
    }
}

using ButtonStateArray = Array<EButtonStates, NUMBER_OF_BUTTONS>;
// Reads hardware button states into the buttonStates argument.
void readButtons(ButtonStateArray& buttonStates)
{
    // used for debouncing
    static ButtonStateArray lastStates;

    // walk backwards because wireing has logically first buttons on last address
    for (uint8_t muxAddr = 0; muxAddr < NUMBER_OF_MUX_ADDRESSES; muxAddr++)
    {
        setMuxAddr(muxAddr);
        uint8_t btnRowIdx = 0;
        for (const uint8_t BTN : BTN_MATRIX_PINS)
        {
            const uint16_t READ_DELAY = 1;
            // Set the corresponding button pin to high and then check the MUX common output if the signal arrives.
            digitalWrite(BTN, HIGH);
            delay(READ_DELAY);
            const EButtonStates STATE =
              (digitalRead(PIN_MUX_COMMON) != LOW) ? EButtonStates::CLOSED : EButtonStates::OPEN;
            digitalWrite(BTN, LOW);
            delay(READ_DELAY);

            const uint8_t BTN_IDX_LOGICAL = static_cast<uint8_t>((NUMBER_OF_MUX_ADDRESSES - 1 - muxAddr))
                                            | static_cast<uint8_t>(btnRowIdx << MUX_ADDR_PINS.size());
            btnRowIdx++;

            // Button state must be the same two reads in a row for debouncing.
            if (STATE == lastStates[BTN_IDX_LOGICAL])
            {
                buttonStates[BTN_IDX_LOGICAL] = STATE;
            }
            lastStates[BTN_IDX_LOGICAL] = STATE;
        }
    }
}

Array<uint16_t, static_cast<size_t>(EAxis::SIZE_)> g_lastAxisValues;
// Reads an axis and handles the value according to g_mode.
template <EAxis axis>
void readAxis()
{
    const auto     PIN        = axis == EAxis::AXIS_BRIGHTNESS ? PIN_ANALOG_BRT : PIN_ANALOG_VID;
    const uint16_t RAW        = analogRead(PIN);    // 0 - 1023 (10bit)

    const uint16_t LOW_VALUE  = axis == EAxis::AXIS_BRIGHTNESS ? g_calibrationLowBrt : g_calibrationLowVid;
    const uint16_t HIGH_VALUE = axis == EAxis::AXIS_BRIGHTNESS ? g_calibrationHighBrt : g_calibrationHighVid;

    const uint16_t VAL        = getCalibratedAnalogValue(RAW, LOW_VALUE, HIGH_VALUE);
    if (g_mode == EModes::GAMEPAD_MODE)
    {
        if (axis == EAxis::AXIS_BRIGHTNESS)
        {
            Gamepad.rxAxis(VAL);
        }
        else
        {
            Gamepad.ryAxis(VAL);
        }
    }
    else if (g_mode == EModes::SERIAL_MODE && g_lastAxisValues[static_cast<size_t>(axis)] != VAL)
    {
        const double   PERCENT = 100.0 * (static_cast<double>(VAL) / GAMEPAD_ANALOG_MAX);
        const uint16_t WRITTEN = sprintf(
          g_serialBuffer,
          "%s Calibrated Value: %x / %x (%.2f %%)  |  RAW: %x\n",
          (axis == EAxis::AXIS_BRIGHTNESS ? "BRT" : "VID"),
          VAL,
          GAMEPAD_ANALOG_MAX,
          PERCENT,
          RAW
        );
        if (WRITTEN >= BUFFER_SIZE)
        {
            error("Buffer overrun.");
        }
        Serial.write(g_serialBuffer);
        g_lastAxisValues[static_cast<size_t>(axis)] = VAL;
    }
    else
    {
        // Empty
    }
}

// Sets the calibration values to the extremes of the inputs.
void calibrate()
{
    // set the calibration values to the min/max of the analog inputs.
    {
        const uint16_t VAL   = analogRead(PIN_ANALOG_BRT);
        g_calibrationLowBrt  = min(VAL, g_calibrationLowBrt);
        g_calibrationHighBrt = max(VAL, g_calibrationHighBrt);
    }
    {
        const uint16_t VAL   = analogRead(PIN_ANALOG_VID);
        g_calibrationLowVid  = min(VAL, g_calibrationLowVid);
        g_calibrationHighVid = max(VAL, g_calibrationHighVid);
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

        // Reset calibration values to current value to ensure that they can be
        // adjusted to the actual values in the calibrate() function.
        {
            const uint16_t VAL   = analogRead(PIN_ANALOG_BRT);
            g_calibrationLowBrt  = VAL;
            g_calibrationHighBrt = VAL;
        }
        {
            const uint16_t VAL   = analogRead(PIN_ANALOG_VID);
            g_calibrationLowVid  = VAL;
            g_calibrationHighVid = VAL;
        }
    }
    else
    {
        g_mode = lastMode;
        if (g_mode == EModes::SERIAL_MODE)
        {
            const uint16_t WRITTEN = sprintf(
              g_serialBuffer,
              "Leaving calibration mode.\nNew calibration data:\nBRT: %d .. %d\nVID: %d .. %d\n",
              g_calibrationLowBrt,
              g_calibrationHighBrt,
              g_calibrationLowVid,
              g_calibrationHighVid
            );

            if (WRITTEN >= BUFFER_SIZE)
            {
                error("Buffer overrun.");
            }
            Serial.write(g_serialBuffer);
        }
    }
}

using ButtonStateArray = Array<EButtonStates, NUMBER_OF_BUTTONS>;
// Outputs the button states to the serial console if they have changed since the last time.
void debugOutputButtonStates(ButtonStateArray& buttonStates)
{
    static ButtonStateArray lastStateDisplayed;

    uint8_t                 btnIdx = -1;
    for (const EButtonStates STATE : buttonStates)
    {
        btnIdx++;
        if (lastStateDisplayed[btnIdx] != STATE)
        {
            const uint16_t WRITTEN = sprintf(
              g_serialBuffer,
              "Button %d (%s) now %s\n",
              btnIdx,
              getButtonName(btnIdx),
              (STATE == EButtonStates::CLOSED) ? "pressed" : "released"
            );

            if (WRITTEN >= BUFFER_SIZE)
            {
                error("Buffer overrun.");
            }
            Serial.write(g_serialBuffer);
            lastStateDisplayed[btnIdx] = STATE;
        }
    }
}

void loop()
{
    // actual states after debouncing
    ButtonStateArray buttonStates;

    const uint16_t   DEBOUNCE_DELAY = 1;

    // Read buttons
    readButtons(buttonStates);

    // when WPN and FAV buttons are pressed at the same time, enter calibration procedure
    auto isCalibrationButtonsPressed = [&buttonStates]() -> bool
    {
        return buttonStates[static_cast<uint8_t>(EButtons::FAV)] == EButtonStates::CLOSED
               && buttonStates[static_cast<uint8_t>(EButtons::WPN)] == EButtonStates::CLOSED;
    };

    if (isCalibrationButtonsPressed())
    {
        // wait for the buttons to no longer be pressed
        while (isCalibrationButtonsPressed())
        {
            delay(DEBOUNCE_DELAY);    // delay first for debouncing
            readButtons(buttonStates);
        }
        toggleCalibrationMode();
    }

    uint8_t btnIdx = -1;
    if (g_mode == EModes::CALIBRATION_MODE)
    {
        calibrate();
    }
    else
    {
        readAxis<EAxis::AXIS_BRIGHTNESS>();
        readAxis<EAxis::AXIS_VIDEO>();
    }

    if (g_mode == EModes::GAMEPAD_MODE)
    {
        for (const EButtonStates STATE : buttonStates)
        {
            btnIdx++;
            if (STATE == EButtonStates::CLOSED)
            {
                Gamepad.press(btnIdx);
            }
            else
            {
                Gamepad.release(btnIdx);
            }
        }

        Gamepad.write();
    }
    else if (g_mode == EModes::SERIAL_MODE)
    {
        debugOutputButtonStates(buttonStates);
    }
    else
    {
        // Empty
    }
}

// NOLINTEND(cppcoreguidelines-pro-type-vararg)
// NOLINTEND(cppcoreguidelines-pro-bounds-array-to-pointer-decay)
// NOLINTEND(cppcoreguidelines-avoid-non-const-global-variables)
