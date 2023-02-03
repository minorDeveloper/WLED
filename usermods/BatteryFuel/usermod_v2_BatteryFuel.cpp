#include "usermod_v2_BatteryFuel.h"




















//-----------------------------------------------------------------
//                       GETTERS and SETTERS 
//-----------------------------------------------------------------

/*
* getId() allows you to optionally give your V2 usermod an unique ID (please define it in const.h!).
* This could be used in the future for the system to determine whether your usermod is installed.
*/
uint16_t UsermodBatteryFuel::getId()
{
    return USERMOD_ID_BATTERY;
}


unsigned long UsermodBatteryFuel::getReadingInterval()
{
    return readingInterval;
}

/*
* minimum repetition is 3000ms (3s) 
*/
void UsermodBatteryFuel::setReadingInterval(unsigned long newReadingInterval)
{
    readingInterval = max((unsigned long)3000, newReadingInterval);
}


/*
* Get lowest configured battery voltage
*/
float UsermodBatteryFuel::getMinBatteryVoltage()
{
    return minBatteryVoltage;
}

/*
* Set lowest battery voltage
* can't be below 0 volt
*/
void UsermodBatteryFuel::setMinBatteryVoltage(float voltage)
{
    minBatteryVoltage = max(0.0f, voltage);
}

/*
* Get highest configured battery voltage
*/
float UsermodBatteryFuel::getMaxBatteryVoltage()
{
    return maxBatteryVoltage;
}

/*
* Set highest battery voltage
* can't be below minBatteryVoltage
*/
void UsermodBatteryFuel::setMaxBatteryVoltage(float voltage)
{
    #ifdef USERMOD_BATTERYFUEL_USE_LIPO
    maxBatteryVoltage = max(getMinBatteryVoltage()+0.7f, voltage);
    #else
    maxBatteryVoltage = max(getMinBatteryVoltage()+1.0f, voltage);
    #endif
}


/*
* Get the capacity of all cells in parralel sumed up
* unit: mAh
*/
unsigned int UsermodBatteryFuel::getTotalBatteryCapacity()
{
    return totalBatteryCapacity;
}

void UsermodBatteryFuel::setTotalBatteryCapacity(unsigned int capacity)
{
    totalBatteryCapacity = capacity;
}

/*
* Get the choosen adc precision
* esp8266 = 10bit resolution = 1024.0f 
* esp32 = 12bit resolution = 4095.0f
*/
float UsermodBatteryFuel::getAdcPrecision()
{
    #ifdef ARDUINO_ARCH_ESP32
    // esp32
    return 4096.0f;
    #else
    // esp8266
    return 1024.0f;
    #endif
}

/*
* Get the calculated voltage
* formula: (adc pin value / adc precision * max voltage) + calibration
*/
float UsermodBatteryFuel::getVoltage()
{
    return voltage;
}

/*
* Get the mapped battery level (0 - 100) based on voltage
* important: voltage can drop when a load is applied, so its only an estimate
*/
int8_t UsermodBatteryFuel::getBatteryLevel()
{
    return batteryLevel;
}

/*
* Get low-power-indicator feature enabled status
* is the low-power-indicator enabled, true/false
*/
bool UsermodBatteryFuel::getLowPowerIndicatorEnabled()
{
    return lowPowerIndicatorEnabled;
}

/*
* Set low-power-indicator feature status 
*/
void UsermodBatteryFuel::setLowPowerIndicatorEnabled(bool enabled)
{
    lowPowerIndicatorEnabled = enabled;
}

/*
* Get low-power-indicator preset to activate when low power is detected
*/
int8_t UsermodBatteryFuel::getLowPowerIndicatorPreset()
{
    return lowPowerIndicatorPreset;
}

/* 
* Set low-power-indicator preset to activate when low power is detected
*/
void UsermodBatteryFuel::setLowPowerIndicatorPreset(int8_t presetId)
{
    // String tmp = ""; For what ever reason this doesn't work :(
    // lowPowerIndicatorPreset = getPresetName(presetId, tmp) ? presetId : lowPowerIndicatorPreset;
    lowPowerIndicatorPreset = presetId;
}

/*
* Get low-power-indicator threshold in percent (0-100)
*/
int8_t UsermodBatteryFuel::getLowPowerIndicatorThreshold()
{
    return lowPowerIndicatorThreshold;
}

/*
* Set low-power-indicator threshold in percent (0-100)
*/
void UsermodBatteryFuel::setLowPowerIndicatorThreshold(int8_t threshold)
{
    lowPowerIndicatorThreshold = threshold;
    // when auto-off is enabled the indicator threshold cannot be below auto-off threshold
    //lowPowerIndicatorThreshold  = autoOffEnabled /*&& lowPowerIndicatorEnabled*/ ? max(autoOffThreshold+1, (int)lowPowerIndicatorThreshold) : max(5, (int)lowPowerIndicatorThreshold);
}

/*
* Get low-power-indicator duration in seconds
*/
int8_t UsermodBatteryFuel::getLowPowerIndicatorDuration()
{
    return lowPowerIndicatorDuration;
}

/*
* Set low-power-indicator duration in seconds
*/
void UsermodBatteryFuel::setLowPowerIndicatorDuration(int8_t duration)
{
    lowPowerIndicatorDuration = duration;
}


/*
* Get low-power-indicator status when the indication is done thsi returns true
*/
bool UsermodBatteryFuel::getLowPowerIndicatorDone()
{
    return lowPowerIndicationDone;
}



// strings to reduce flash memory usage (used more than twice)
const char UsermodBatteryFuel::_name[]          PROGMEM = "Battery";
const char UsermodBatteryFuel::_readInterval[]  PROGMEM = "interval";
const char UsermodBatteryFuel::_enabled[]       PROGMEM = "enabled";
const char UsermodBatteryFuel::_threshold[]     PROGMEM = "threshold";
const char UsermodBatteryFuel::_preset[]        PROGMEM = "preset";
const char UsermodBatteryFuel::_duration[]      PROGMEM = "duration";
const char UsermodBatteryFuel::_init[]          PROGMEM = "init";











/*
* Generate a preset sample for low power indication 
*/
void UsermodBatteryFuel::generateExamplePreset()
{
    // StaticJsonDocument<300> j;
    // JsonObject preset = j.createNestedObject();
    // preset["mainseg"] = 0;
    // JsonArray seg = preset.createNestedArray("seg");
    // JsonObject seg0 = seg.createNestedObject();
    // seg0["id"] = 0;
    // seg0["start"] = 0;
    // seg0["stop"] = 60;
    // seg0["grp"] = 0;
    // seg0["spc"] = 0;
    // seg0["on"] = true;
    // seg0["bri"] = 255;

    // JsonArray col0 = seg0.createNestedArray("col");
    // JsonArray col00 = col0.createNestedArray();
    // col00.add(255);
    // col00.add(0);
    // col00.add(0);

    // seg0["fx"] = 1;
    // seg0["sx"] = 128;
    // seg0["ix"] = 128;

    // savePreset(199, "Low power Indicator", preset);
}


/*
* addToJsonState() can be used to add custom entries to the /json/state part of the JSON API (state object).
* Values in the state object may be modified by connected clients
*/
/*
void addToJsonState(JsonObject& root)
{

}
*/


/*
* readFromJsonState() can be used to receive data clients send to the /json/state part of the JSON API (state object).
* Values in the state object may be modified by connected clients
*/
/*
void readFromJsonState(JsonObject& root)
{
}
*/