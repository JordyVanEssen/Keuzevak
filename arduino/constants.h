#include <OneWire.h>
#include <DallasTemperature.h>

const int TEMP_SENSORS = 5;
const int DIGITAL_SENSORS = 3;
const int ACTUATORS = 7;
const int DIGITAL_ONLY_BUTTONS = 1;
const int BUTTONS = 5 + DIGITAL_ONLY_BUTTONS;
const int LEDS = 6;
const int SETTING_VALUES = 5;
const int SAMPLE_READINGS = 25;

const int FURNACE_MAX = 90;
const int CONVERSIONTABLE_SIZE = 110;
const int DEBOUCE = 2000;

const unsigned long PUMP_INTERVAL = 60000;                    // 1 minute
const unsigned long PELLET_FURNACE_INTERVAL = 1800000;        // 30 minutes
const unsigned long FLOWSWITCH_INTERVAL = 10000;              // 10 seconds
const unsigned long THREEWAY_INTERVAL = 60000;                // 1 minute
const unsigned long DELTA_INTERVAL = 600000;                  // 10 minutes
const unsigned long VALID_VALUE_INTERVAL = 120000;            // 2 minutes
const unsigned long MEASURE_INTERVAL = 5000;                  // 500 milliseconds
const unsigned long FURNACE_STARTUP_PUMP_INTERVAL = 60000;    // 1 minute
const unsigned long WIRE_RECEIVE_INTERVAL = 2000;             // 2 seconds

const int I2C_DATA_SIZE = TEMP_SENSORS + 3;

const int ONEWIRE_BUS = 5;
OneWire oneWire(ONEWIRE_BUS);
DallasTemperature sensors(&oneWire);

float conversionTable[CONVERSIONTABLE_SIZE] = { 
                100.00, 100.39, 100.78, 101.17, 101.56, 101.95, 102.34, 102.73, 103.12, 103.51,
                103.90, 104.29, 104.68, 105.07, 105.46, 105.85, 106.24, 106.63, 107.02, 107.40,
                107.79, 108.18, 108.57, 108.96, 109.35, 109.73, 110.12, 110.51, 110.90, 111.29,
                111.67, 112.06, 112.45, 112.83, 113.22, 113.61, 114.00, 114.38, 114.77, 115.15,
                115.54, 115.93, 116.31, 116.70, 117.08, 117.47, 117.86, 118.24, 118.63, 119.01,
                119.40, 119.78, 120.17, 120.55, 120.94, 121.32, 121.71, 122.09, 122.47, 122.86,
                123.24, 123.63, 124.01, 124.39, 124.78, 125.16, 125.54, 125.93, 126.31, 126.69,
                127.08, 127.46, 127.84, 128.22, 128.61, 128.99, 129.37, 129.75, 130.13, 130.52,
                130.90, 131.28, 131.66, 132.04, 132.42, 132.80, 133.18, 133.57, 133.95, 134.33,
                134.71, 135.09, 135.47, 135.85, 136.23, 136.61, 136.99, 137.37, 137.75, 138.13,
                138.51, 138.88, 139.26, 139.64, 140.02, 140.40, 140.78, 141.16, 141.54, 141.91  
            };

const int tempSensors[TEMP_SENSORS] = {A0, 0, 1, 3, 2};
const int digitalSensors[DIGITAL_SENSORS] = {53, 52, 51};
const int buttons[BUTTONS - DIGITAL_ONLY_BUTTONS] = {43, 44, 45, 42, 46};
const int output[ACTUATORS] = {38, 39, 37, 33, 35, 34, 32};
const int leds[LEDS] = {26, 27, 28, 29, 30, 31};

const int resistance[TEMP_SENSORS] = {216, 216, 219, 1116, 218};
const float resistanceDeviation[TEMP_SENSORS] = {0.0, 0.0, 4.5, 0.0, 0.0};

int settings[SETTING_VALUES] = {75, 80, 50, 75, 57};

enum Settings {
    manualHeatMax,
    maxTemp,
    absoluteMinTemp,
    boilerMinTemp,
    boilerTankAbsoluteMinTemp
};

enum TempSensor {
    boilerTankSensor,           // A0
    cvTankSensor,               // 0
    furnaceSensor,              // 1
    roofSensor,                 // 2
    outside                     // 3
};

enum DigitalSensor {
    digitalFurnaceSensor,       // 53
    digitalRoomSensor,          // 52
    digitalFlowswitch           // 51
};

enum Buttons {
    manualThreeway,             // 43
    manualHeatBoilerTank,       // 44
    manualHeatAllTanks,         // 45
    manualIgnorePelletfurnace,  // 42
    manualElectricalElement,    // 46
    manualSummerMode            // Digital only
};

enum Output {
    cvPump,                     // 38 - 6
    furnacePump,                // 39 - 7
    pelletFurnace,              // 37 - 8
    electricalHeatingElement,   // 33 - 9
    collectorSecPump,           // 35 - 10
    collectorMainPump,          // 34 - 11
    threeway                    // 32, 0 == boiler/sanitair, 1 == cv tank - 12
};

enum StatusLeds {
    summerModeLed,              // 26
    threewayLed,                // 27
    cvPumpLed,                  // 28
    furnacePumpLed,             // 29
    pelletFurnaceLed,           // 30
    alarmLed                    // 31
};

// same value as the boiler sensor and cv sensor
enum Tank {
    boilerTank = boilerTankSensor,
    cvTank = cvTankSensor
};
