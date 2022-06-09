
#include <Wire.h>
#include <MemoryFree.h>
#include "constants.h"

float temperatures[TEMP_SENSORS];
float delta[2];

int digitalSensorsIn[DIGITAL_SENSORS];
int out[ACTUATORS];
int ledOut[LEDS];
int in[BUTTONS];
int data[I2C_DATA_SIZE];
int manualOverride[BUTTONS + ACTUATORS];
float temperatureReadings[TEMP_SENSORS][SAMPLE_READINGS];

byte outputActuators = 0b00000000;
byte inputButtons = 0b00000000;
byte inputDigitalSensors = 0b00000000;

int sampleIndex = 0;
int deltaIndex = 0;
int deviceCount = 0;

unsigned long currentMillis = millis();
unsigned long previousPumpMillis = currentMillis - PUMP_INTERVAL;
unsigned long previousPelletMillis = currentMillis - PELLET_FURNACE_INTERVAL;
unsigned long previousDeltaMillis = currentMillis - DELTA_INTERVAL;
unsigned long previousThreewayMillis = currentMillis - THREEWAY_INTERVAL;
unsigned long previousFlowswitchMillis = currentMillis - FLOWSWITCH_INTERVAL;
unsigned long previousMeasureMillis = currentMillis - MEASURE_INTERVAL;
unsigned long previousButtonMillis = currentMillis - DEBOUCE;
unsigned long previousFurnaceStartupMillis = currentMillis - FURNACE_STARTUP_PUMP_INTERVAL;
unsigned long previousWireReceive = currentMillis - WIRE_RECEIVE_INTERVAL;

unsigned long previousPelletFurnaceLedStateMillis = currentMillis - 500;

Tank currentTank = boilerTank;
Tank manualTankToHeat = boilerTank;
Tank switchedTank = boilerTank;

bool manual = false;
bool startup = true;
bool switched = false;
bool sendButtonStates = false;
bool pelletFurnaceLedState = false;
bool furnaceStartup = false;

int freememory = 0;

void setup()
{
  Wire.begin(0x8);
  Wire.onRequest(onRequest);
  Wire.onReceive(onReceive);

  Serial.begin(9600);

  for (int i = 0; i < DIGITAL_SENSORS; i++)
    pinMode(digitalSensors[i], INPUT_PULLUP);

  for (int i = 0; i < TEMP_SENSORS; i++)
    pinMode(tempSensors[i], INPUT);

  for (int i = 0; i < BUTTONS - DIGITAL_ONLY_BUTTONS; i++)
    pinMode(buttons[i], INPUT_PULLUP);

  for (int i = 0; i < ACTUATORS; i++)
    pinMode(output[i], OUTPUT);

  for (int i = 0; i < LEDS; i++)
    pinMode(leds[i], OUTPUT);

  sensors.begin();
  deviceCount = sensors.getDeviceCount();
}

void onReceive(int bytes)
{
  while (Wire.available())
  {
    int received = Wire.read();

    if (received == 100 && !(currentMillis - previousWireReceive < WIRE_RECEIVE_INTERVAL)) {
      previousWireReceive = currentMillis;
      return;
    }

    if (currentMillis - previousWireReceive < WIRE_RECEIVE_INTERVAL && received > 0)
    {
      int id = received - 48;
      if (id < 0 || id > ACTUATORS + BUTTONS)
        return; 

      if (id < BUTTONS) // 0 1 2 3 4 5
        in[id] = in[id] == 1 ? 0 : 1;
      else if (id >= BUTTONS && id < ACTUATORS + BUTTONS) // 6 7 8 9 10 11 12
        manualOverride[id] = manualOverride[id] == 1 ? 0 : 1;
    }
  }
}

void onRequest()
{
  data[0] = (int)outputActuators;
  data[1] = (int)inputButtons;
  data[2] = (int)inputDigitalSensors;
  for (int i = 3; i < I2C_DATA_SIZE; i++)
  {
    data[i] = (int)temperatures[i - 3];
  }

  Wire.write((byte *)&data, I2C_DATA_SIZE * sizeof(int));
}

float calculateTemperature(float val)
{
  // calculate if value is out of range
  if (val < conversionTable[0])
    return -99.99;
  if (val > conversionTable[CONVERSIONTABLE_SIZE - 1])
    return 99.99;

  //  search for 'value' in _in array to get the position No.
  uint8_t pos = 0;
  while (val > conversionTable[pos])
    pos++;

  // handles the 'rare' equality case
  if (val == conversionTable[pos])
    return pos;

  float r1 = conversionTable[pos - 1];
  float r2 = conversionTable[pos];
  int c1 = pos - 1;
  int c2 = pos;

  return c1 + (val - r1) / (r2 - r1) * (c2 - c1);
}

float calculateAverage(int sensor)
{
  float total = 0;
  for (int i = 0; i < SAMPLE_READINGS; i++)
    total += temperatureReadings[sensor][i];
  return total / SAMPLE_READINGS;
}

void measureTemperature()
{
  int val;
  float Vout, R2, temp;
  sensors.requestTemperatures();
  for (int i = 0; i < TEMP_SENSORS; i++)
  {
    if (i == 0)
    {
      val = analogRead(tempSensors[i]);

      Vout = val * (5.0 / 1023.0);
      R2 = (resistance[i] * 1 / (5.0 / Vout - 1)) - resistanceDeviation[i];

      temp = calculateTemperature(R2);
    }
    else
    {
      temp = sensors.getTempCByIndex(tempSensors[i]);
    }

    sampleIndex = sampleIndex < SAMPLE_READINGS ? sampleIndex : 0;
    temperatureReadings[i][sampleIndex] = temp;

    if (!startup)
    {
      float previous = temperatureReadings[i][sampleIndex > 0 ? sampleIndex - 1 : SAMPLE_READINGS - 1];
      temperatureReadings[i][sampleIndex] = (temp > previous + 50.0 || temp < -30.0) ? previous : temp;
    }

    temperatures[i] = startup ? 65 : calculateAverage(i);
  }
  sampleIndex++;
  if (sampleIndex == SAMPLE_READINGS)
    startup = false;
}

void measureDigitalSensors()
{
  for (int i = 0; i < DIGITAL_SENSORS; i++)
  {
    digitalSensorsIn[i] = !digitalRead(digitalSensors[i]);
    int temp = digitalSensorsIn[digitalRoomSensor];
    digitalSensorsIn[digitalRoomSensor] = in[manualSummerMode] == 1 ? 0 : temp;

    bitWrite(inputDigitalSensors, i, digitalSensorsIn[i]);
  }
}

void readButtons()
{
  for (int i = 0; i < BUTTONS - DIGITAL_ONLY_BUTTONS; i++)
  {
    int sum = 0;
    if (currentMillis - previousButtonMillis > DEBOUCE && digitalRead(buttons[i]) == 0)
    {
      for (int x = 0; x < 10; x++)
        sum += digitalRead(buttons[i]);
      if (sum > 3)
        return;

      in[i] = in[i] == 1 ? 0 : 1;
      previousButtonMillis = currentMillis;
      return;
    }
  }

  for (int i = 0; i < BUTTONS; i++)
    bitWrite(inputButtons, i, in[i]);
}

void setOutput()
{
  for (int i = 0; i < ACTUATORS; i++)
  {
    int result = manualOverride[i + BUTTONS] == 1 ? 1 : out[i];
    digitalWrite(output[i], result);
    bitWrite(outputActuators, i, result);
  }

  for (int i = 0; i < LEDS; i++)
  {
    digitalWrite(leds[i], ledOut[i]);
  }
}

void setLeds()
{
  ledOut[summerModeLed] = in[manualSummerMode];
  ledOut[threewayLed] = in[manualThreeway];
  ledOut[cvPumpLed] = out[cvPump];
  ledOut[furnacePumpLed] = out[furnacePump];
  ledOut[pelletFurnaceLed] = out[pelletFurnace];

  if (in[manualIgnorePelletfurnace] == 1)
  {
    if (currentMillis - previousPelletFurnaceLedStateMillis > 500)
    {
      pelletFurnaceLedState = !pelletFurnaceLedState;
      previousPelletFurnaceLedStateMillis = currentMillis;
    }

    ledOut[pelletFurnaceLed] = pelletFurnaceLedState;
  }
}

void controlPelletFurnace(int mode)
{
  if (currentMillis - previousPelletMillis > PELLET_FURNACE_INTERVAL && out[pelletFurnace] != mode && in[manualIgnorePelletfurnace] == 0)
  {
    out[pelletFurnace] = mode;
    previousPelletMillis = currentMillis;
  }
}

bool controlThreeway(int mode)
{
  if (currentMillis - previousThreewayMillis > THREEWAY_INTERVAL && mode != out[threeway])
  {
    out[threeway] = mode;
    previousThreewayMillis = currentMillis;
    return true;
  }
  return false;
}

void calculateFurnaceStartup()
{
  delta[0] = temperatures[furnaceSensor];
  if (currentMillis - previousDeltaMillis > DELTA_INTERVAL)
  {
    previousDeltaMillis = currentMillis;
    delta[1] = temperatures[furnaceSensor];

    if (delta[0] != 0)
    {
      if (100.0 * (delta[1] - delta[0]) / delta[0] >= 50.0)
      {
        furnaceStartup = true;
        out[furnacePump] = 1;
        previousFurnaceStartupMillis = currentMillis;
      }
    }
  }
}

void maxTankTemperature()
{
  // if max temperature is exceeded in one of the tanks
  if (temperatures[boilerTank] > (switched ? 85 : (in[manualSummerMode] == 1 ? 85 : 80)))
  {
    currentTank = cvTank;
    switched = false;
  }

  if (temperatures[cvTankSensor] > 85)
  {
    ledOut[alarmLed] = 1;
    out[cvPump] = 1;
  }

  if (temperatures[cvTankSensor] < 80)
  {
    if (digitalSensorsIn[digitalRoomSensor] == 0)
      out[cvPump] = 0;
    ledOut[alarmLed] = 0;
  }
}
// ========================================================= //

void controlPumps()
{
  if (currentMillis - previousFlowswitchMillis > FLOWSWITCH_INTERVAL && out[furnacePump] == 1 && digitalSensorsIn[digitalFlowswitch] == 0)
    ledOut[alarmLed] = 1;

  if (temperatures[furnaceSensor] > temperatures[currentTank] + 10 || digitalSensorsIn[digitalFurnaceSensor] == 1)
  {
    previousPumpMillis = currentMillis;
    previousFlowswitchMillis = currentMillis;
    out[furnacePump] = 1;
  }
  else if (currentMillis - previousPumpMillis > PUMP_INTERVAL && temperatures[furnaceSensor] < temperatures[currentTank] + 8 && temperatures[furnaceSensor] < FURNACE_MAX)
    out[furnacePump] = 0;

  out[cvPump] = digitalSensorsIn[digitalRoomSensor];

  if (temperatures[roofSensor] > temperatures[currentTank] + 20)
    out[collectorMainPump] = 1;
  else if (temperatures[roofSensor] < temperatures[currentTank] + 15)
    out[collectorMainPump] = 0;
  out[collectorSecPump] = out[collectorMainPump];
}

void determineTankToHeat()
{
  if (temperatures[currentTank] > settings[maxTemp])
    currentTank = currentTank == boilerTank ? cvTank : boilerTank;

  if (temperatures[boilerTank] < settings[boilerMinTemp])
    currentTank = boilerTank;

  if (temperatures[boilerTankSensor] > settings[manualHeatMax] && temperatures[cvTankSensor] < 60 && digitalSensorsIn[digitalRoomSensor] == 1 && (out[furnacePump] == 1 || out[pelletFurnace] == 1))
    currentTank = cvTank;

  if (temperatures[boilerTankSensor] < settings[manualHeatMax] && temperatures[cvTankSensor] < 60 && digitalSensorsIn[digitalRoomSensor] == 1)
    currentTank = boilerTank;

  if (in[manualSummerMode] == 1)
  {
    in[manualHeatAllTanks] = 0;
    if (temperatures[boilerTankSensor] < settings[boilerTankAbsoluteMinTemp])
      in[manualHeatBoilerTank] = 1;
  }
}

void determineTankToManualHeat()
{
  if (in[manualHeatBoilerTank] == 1 || in[manualHeatAllTanks] == 1)
  {
    controlPelletFurnace(1);

    if (in[manualHeatAllTanks] == 1)
    {
      if (temperatures[boilerTankSensor] < settings[manualHeatMax] && manualTankToHeat == boilerTank)
        currentTank = boilerTank;
      else if (temperatures[cvTankSensor] < settings[manualHeatMax])
      {
        currentTank = cvTank;
        manualTankToHeat = currentTank;
      }
      else
      {
        in[manualHeatAllTanks] = 0;
        manualTankToHeat = boilerTank;
      }
    }
    else if (in[manualHeatBoilerTank] == 1)
    {
      if (temperatures[boilerTankSensor] < settings[manualHeatMax])
        currentTank = boilerTank;
      else
        in[manualHeatBoilerTank] = 0;
    }
  }

  if ((temperatures[boilerTankSensor] < settings[boilerTankAbsoluteMinTemp] || temperatures[cvTankSensor] < settings[absoluteMinTemp]))
  {
    in[manualHeatAllTanks] = 1;
    manualTankToHeat = temperatures[boilerTankSensor] < settings[boilerTankAbsoluteMinTemp] ? boilerTank : cvTank;
  }

  if (temperatures[manualTankToHeat] > settings[manualHeatMax] || (in[manualHeatAllTanks] == 0 && in[manualHeatBoilerTank] == 0))
    controlPelletFurnace(0);
}

// ========================================================= //

void loop()
{
  currentMillis = millis();

  if (currentMillis - previousMeasureMillis > MEASURE_INTERVAL && !startup)
  {
    previousMeasureMillis = currentMillis;
    measureTemperature();
    measureDigitalSensors();
  }
  else if (startup)
  {
    measureTemperature();
    measureDigitalSensors();
  }

  if (!startup)
  {
    out[electricalHeatingElement] = in[manualElectricalElement];
    controlPumps();
    determineTankToHeat();
    determineTankToManualHeat();

    if (in[manualSummerMode] == 1)
    {
      currentTank = boilerTank;
    }

    if (in[manualThreeway] == 1 && !switched)
    {
      switchedTank = currentTank == boilerTank ? cvTank : boilerTank;
      switched = true;
    }
    else if (in[manualThreeway] == 0 && switched)
    {
      switched = false;
    }

    if (switched)
      currentTank = switchedTank;

    maxTankTemperature();

    calculateFurnaceStartup();
    if (currentMillis - previousFurnaceStartupMillis > FURNACE_STARTUP_PUMP_INTERVAL && furnaceStartup)
    {
      out[furnacePump] = 0;
      furnaceStartup = false;
    }

    controlThreeway(currentTank == boilerTank ? 0 : 1);
    readButtons();
    setLeds();
    setOutput();
  }

  /*   Serial.println();
  for (int i = 0; i < TEMP_SENSORS; i++){
    Serial.print(temperatures[i]);Serial.print(", ");
  }

  Serial.println();
  for (int i = 0; i < DIGITAL_SENSORS; i++){
    Serial.print(digitalSensorsIn[i]);Serial.print(", ");
  } 
  Serial.println();
  for (int i = 0; i < BUTTONS; i++){
    Serial.print(in[i]);Serial.print(", ");
  }

  Serial.println();
  for (int i = 0; i < ACTUATORS; i++){
    Serial.print(out[i]);Serial.print(", ");
  }*/
}
