#include <Arduino.h>

#define PIN_MASK 0b111100
#define PIN_MAX 6

#define PIN_NOT_USED 0xff

#define IR_LEN_MIN 2
#define IR_LEN_MAX 10

#define BYPASS_LEN_MAX 1024

#define CLOCK_HALF_CYCLE_US 32

#define IDLE_TO_SHIFT_DR_CMD 0b001
#define IDLE_TO_SHIFT_DR_LEN 3

#define IDLE_TO_SHIFT_IR_CMD 0b0011
#define IDLE_TO_SHIFT_IR_LEN 4

#define EXIT_IR_TO_SHIFT_DR_CMD 0b0011
#define EXIT_IR_TO_SHIFT_DR_LEN 4

#define PROMPT "> "
#define ROW_FORMAT "tck: %2d | tms: %2d | tdo: %2d | tdi:  X | value: %6lx\r\n"
#define ROW_FORMAT_TDI "tck: %2d | tms: %2d | tdo: %2d | tdi: %2d | value: %6lx\r\n"

#define getVersion(value) ((value >> 28) & 0xf)
#define getPartNo(value) ((value >> 12) & 0xffff)
#define getManufacturer(value) ((value >> 1) & 0x7ff)

uint32_t bypass_pattern = 0b10011101001101101000010111001001;

uint8_t ir_length = 2;

uint8_t tck_pin = 2;
uint8_t tms_pin = 3;
uint8_t tdo_pin = 4;
uint8_t tdi_pin = 5;

uint32_t pin_blacklist = 0;

enum debug_level {
    quiet,
    normal,
    verbose
};

enum debug_level debug = quiet;

/**
* Write a single bit to TMS, TDI and read TDO
*
* @param: bit to write to TMS
* @param: bit to write to TDI
* @returns: bit read from TDO
*/
bool moveBit(bool tms_val, bool tdi_val)
{
    bool tdo_val = false;
    digitalWrite(tms_pin, tms_val);
    digitalWrite(tdi_pin, tdi_val);
    digitalWrite(tck_pin, HIGH);
    delayMicroseconds(CLOCK_HALF_CYCLE_US);
    digitalWrite(tck_pin, LOW);
    tdo_val = digitalRead(tdo_pin);
    delayMicroseconds(CLOCK_HALF_CYCLE_US);

    if (debug == verbose)
    {
        char buffer[34];
        sprintf(buffer, "TMS: %d | TDI: %d | TDO: %d\r\n", tms_val, tdi_val, tdo_val);
        Serial.print(buffer);
    }

    return tdo_val;
}

/**
 * Transfer values over TMS and TDI reading TDO
 * 
 * @param: value to write to TMS
 * @param: value to write to TDI
 * @param: value read from TDO (nullable)
 * @param: number of bits to read and write
 */
void moveBits(uint32_t tms_val, uint32_t tdi_val, uint32_t * tdo_val, uint8_t width)
{
    uint32_t tdo_temp_val = 0;
    for (uint8_t i=0; i<width; i++)
    {
        bitWrite(tdo_temp_val, i, moveBit(bitRead(tms_val, i), bitRead(tdi_val, i)));
    }

    if (NULL != tdo_val)
    {
        *tdo_val = tdo_temp_val;
    }
}

/**
 * Reset test logic
 * 
 * Uses only the TMS line
 */ 
void resetTestLogic()
{
    moveBits(0b011111, 0, NULL, 6);
}

/**
 * Configure JTAG pins
 */
void setupPins()
{
    for (int idx=0; idx<PIN_MAX; idx++)
    {
        if (bitRead(PIN_MASK, idx))
        {
            pinMode(idx, INPUT);
        }
    }

    pinMode(tck_pin, OUTPUT);
    pinMode(tms_pin, OUTPUT);
    pinMode(tdo_pin, INPUT_PULLUP);

    digitalWrite(tck_pin, LOW);
    digitalWrite(tms_pin, LOW);

    if (tdi_pin != PIN_NOT_USED)
    {
        digitalWrite(tdi_pin, LOW);
        pinMode(tdi_pin, OUTPUT);
    }
}

/**
 * Reset pins to input state
 */
void resetPins()
{
    pinMode(tck_pin, INPUT);
    pinMode(tms_pin, INPUT);
    pinMode(tdi_pin, INPUT);
    pinMode(tdo_pin, INPUT);
}

/**
 * Check presence of bit in specified value
 * 
 * @param: value
 * @param: bit
 * @returns: number of occurences
 */
uint32_t bitCount(uint32_t value, uint8_t bitState)
{
    uint32_t count = 0;
    for (uint8_t i=0; i<32; i++)
    {
        count += (bitRead(value, i) == bitState) ? 1 : 0;
    }
    return count;
}

/**
 * Check if id code looks correct
 * 
 * @params: ID CODE
 * @returns: check result
 */
bool verifyIdCode(uint32_t id_code)
{
    return !(
        bitRead(id_code, 0) == 0 || 
        getManufacturer(id_code) == 0 ||
        getManufacturer(id_code) == 0x7ff ||
        getPartNo(id_code) == 0 ||
        getPartNo(id_code) == 0xffff ||
        getVersion(id_code) == 0xf ||
        bitCount(id_code, HIGH) < 10 ||
        bitCount(id_code, LOW) < 10
    );
}

/**
 * Read device ID CODE
 * 
 * @returns: 32 bit id code value
 */
uint32_t readIdCode()
{
    uint32_t id_code = 0;
    setupPins();
    resetTestLogic();
    moveBits(IDLE_TO_SHIFT_DR_CMD, 0, NULL, IDLE_TO_SHIFT_DR_LEN - 1);
    moveBits(1UL << 31, 0, &id_code, 32);
    return verifyIdCode(id_code) ? id_code : 0;
}

/**
 * Transfer data in BYPASS mode
 * 
 * A 32-bit test pattern is shifted into TDI
 * and expected to occur on TDO.
 * 
 * @returns: number of shifts needed to read pattern
 */
uint32_t passthroughData()
{
    setupPins();
    resetTestLogic();
    moveBits(IDLE_TO_SHIFT_IR_CMD, 0, NULL, IDLE_TO_SHIFT_IR_LEN);
    moveBits(1 << 7, 0b11111111, NULL, 8);
    moveBits(EXIT_IR_TO_SHIFT_DR_CMD, 0, NULL, EXIT_IR_TO_SHIFT_DR_LEN);

    uint32_t bit_value = 0;
    uint32_t bypass_value = 0;
    for (uint32_t i=0; i<BYPASS_LEN_MAX; i++)
    {
        bit_value = moveBit(0, bitRead(bypass_pattern, i % 32));
        bypass_value = bypass_value >> 1;
        bypass_value |= bit_value << 31;

        if (bypass_value == bypass_pattern)
        {
            return i;
        }
    }

    return 0;
}

/**
 * Get next candidate pin
 * 
 * @param: current pin index in pinArray
 * @param: size of pinArray
 * @param: array of counter
 * @returns: candidate pin number
 */
int8_t getNextPin(uint8_t pinIndex, uint8_t pinCount, int8_t pinArray[])
{
    int8_t candidate = -1;
    bool duplicate = false;

    for (int idx=pinArray[pinIndex]; idx<PIN_MAX; idx++)
    {
        if (bitRead(PIN_MASK, idx) && !bitRead(pin_blacklist, idx))
        {
            duplicate = false;

            for (int j=0; j<pinCount; j++)
            {
                if (pinArray[j] == idx)
                {
                    duplicate = true;
                    break;
                }
            }

            if (!duplicate)
            {
                candidate = idx;
                break;
            }
        }
    }

    return candidate;
}

/**
 * Try to detect debug pins using provided evaluator
 * 
 * @param: number of pins to find
 * @param: evaluator function
 * @returns: boolean outcome
 */
bool identifyPins(uint8_t pinCount, bool (*evaluator) (uint8_t, int8_t[]))
{
    uint8_t counterIndex = 0;
    int8_t counters[pinCount];
    memset(counters, -1, pinCount);
    bool noMoreCandidates = false;
    
    // select initial set of pins
    for (uint8_t idx=0; idx<pinCount; idx++)
    {
        counters[idx] = getNextPin(idx, pinCount, counters);
    }

    counterIndex = pinCount - 1;
    while (true)
    {
        // test pin set
        bool result = (*evaluator)(pinCount, counters);
        if (result)
        {
            return result;
        }

        // next candidate
        while (true)
        {
            int8_t candidate = getNextPin(counterIndex, pinCount, counters);
            if (candidate < 0)
            {
                if (counterIndex == 0)
                {
                    noMoreCandidates = true;
                    break;
                }

                counters[counterIndex] = 0xff;
                counterIndex -= 1;
            }
            else
            {
                counters[counterIndex++] = candidate;

                for (uint8_t idx=counterIndex; idx<pinCount; idx++)
                {
                    counters[idx] = getNextPin(idx, pinCount, counters);
                }

                counterIndex = pinCount - 1;
                break;
            }
        }

        if (noMoreCandidates)
        {
            break;
        }
    }

    return false;
}

/**
 * Print a line with current pin mapping and outcome
 * 
 * @param: should tdi be included
 * @param: corresponding value
 */
void printResultRow(bool include_tdi, uint32_t value)
{
    char dbgBuffer[100];

    if (include_tdi)
    {
        sprintf(dbgBuffer, ROW_FORMAT_TDI, tck_pin, tms_pin, tdo_pin, tdi_pin, value);
    }
    else
    {
        sprintf(dbgBuffer, ROW_FORMAT, tck_pin, tms_pin, tdo_pin, value);
    }
    
    Serial.print(dbgBuffer);
}

/**
 * ID Code evaluator
 *
 * @param: number of pins
 * @param: array of counters
 * @returns: outcome 
 */
bool testIdCode(uint8_t pinCount, int8_t counters[])
{
    tck_pin = counters[0];
    tms_pin = counters[1];
    tdo_pin = counters[2];
    tdi_pin = PIN_NOT_USED;

    bool status = false;
    uint32_t id_code = readIdCode();
    uint32_t id_code_prev = id_code;

    if (id_code != 0 && id_code != 4294967295)
    {
        status = true;
        for (uint8_t i=0; i<2; i++)
        {
            id_code = readIdCode();
            if (id_code != id_code_prev)
            {
                status = false;
                break;
            }
        }
    }

    if (status || debug >= normal)
    {
        printResultRow(false, id_code);
    }

    return status;    
}

/**
 * Bypass evaluator 
 *
 * @param: number of pins
 * @param: array of counters
 * @returns: outcome 
 */
bool testBypass(uint8_t pinCount, int8_t counters[])
{
    if (pinCount == 1)
    {
        tdi_pin = counters[0];
    }
    else
    {
        tck_pin = counters[0];
        tms_pin = counters[1];
        tdo_pin = counters[2];
        tdi_pin = counters[3];
    }

    bool status = false;
    uint32_t width = passthroughData();
    if (width > 0)
    {
        status = true;
    }

    if (status || debug >= normal)
    {
        printResultRow(true, width);
    }

    return status;
}

/**
 * Read cli byte
 * 
 * Waits for input as long as necessary.
 *
 * @return: byte read from serial port
 */
byte readCliByte()
{
    while (true)
    {
        if (Serial.available() > 0)
        {
            byte input = Serial.read();
            Serial.write(input);
            Serial.println("");
            return input;
        }
        delay(100);
    }
}

/**
 * Print the prompt
 */
void printPrompt()
{
    Serial.print(PROMPT);
}

/**
 * Minimalistic command line interface
 */
void commandLineInterface()
{
    char selection = readCliByte();
    switch (selection)
    {
        case 'a':
            {
                bool id_hit = false;
                bool tdi_found = false;
                uint32_t id_code = 0;

                Serial.println("Automatically finding TCK, TMS, and TDO using IDCODE scan...");

                // find tck, tms and tdo using id code scan
                id_hit = identifyPins(3, &testIdCode);

                if (id_hit)
                {
                    // if id code is consistent search for tdi
                    id_code = readIdCode(); 
                    if (id_code == readIdCode())
                    {
                        bitWrite(pin_blacklist, tck_pin, HIGH);
                        bitWrite(pin_blacklist, tms_pin, HIGH);
                        bitWrite(pin_blacklist, tdo_pin, HIGH);
                        tdi_found = identifyPins(1, &testBypass);
                        pin_blacklist = 0;
                    }
                }

                // either id code not read or tdi still missing
                if (!id_hit || !tdi_found)
                {
                    identifyPins(4, &testBypass);
                }
            }
            break;
        case 'i':
            Serial.println("IDCODE searching for TCK, TMS, and TDO...");
            identifyPins(3, &testIdCode);
            break;
        case 'b':
            Serial.println("BYPASS searching for TCK, TMS, TDO, and TDI...");
            identifyPins(4, &testBypass);
            break;
        case 't':
            Serial.println("BYPASS searching for TDI with known TCK, TMS, and TDO...");
            bitWrite(pin_blacklist, tck_pin, HIGH);
            bitWrite(pin_blacklist, tms_pin, HIGH);
            bitWrite(pin_blacklist, tdo_pin, HIGH);
            identifyPins(1, &testBypass);
            pin_blacklist = 0;
            break;
        case 'd':
            {
                Serial.print("Choose debug level 0-2 ");
                byte choice = readCliByte() - 0x30;
                debug = (enum debug_level) choice;
            }
            break;
        case 'h':
        default:
            Serial.println("a - Automatically enumerate JTAG pins");
            Serial.println("i - IDCODE search for pins");
            Serial.println("b - BYPASS search for pins");
            Serial.println("h - print this help");
            Serial.println("t - TDI BYPASS search with known other pins");
            Serial.println("d - set debug level");
            break;
    }
}

/**
 * Arduino setup
 */
void setup()
{
    Serial.begin(115200);
}

/**
 * Main loop
 */
void loop()
{
    printPrompt();
    commandLineInterface();
}
