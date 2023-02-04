#include "usermod_v2_BatteryFuel.h"


float UsermodBatteryFuel::dot2round(float x)
{
    float nx = (int)(x * 100 + .5);
    return (float)(nx / 100);
}


/*
* setup() is called once at boot. WiFi is not yet connected at this point.
* You can use it to initialize variables, sensors or similar.
*/
void UsermodBatteryFuel::setup() 
{
    Wire.begin(i2c_sda, i2c_scl);
    DEBUG_PRINTLN(F("I2C initialised"));

    if (!fuel_gauge.begin(&Wire)) {
        initSuccess = false;
    }

    fuel_gauge.setAlertVoltages(batteryLowVoltage, batteryHighVoltage);

    nextReadTime = millis() + readingInterval;
    lastReadTime = millis();

    initDone = true;
}


/*
* connected() is called every time the WiFi is (re)connected
* Use it to initialize network interfaces
*/
void UsermodBatteryFuel::connected() 
{
    //Serial.println("Connected to WiFi!");
}

/*
* Indicate low power by activating a configured preset for a given time and then switching back to the preset that was selected previously
*/
void UsermodBatteryFuel::lowPowerIndicator()
{
    if (!batteryLowPowerDim) return;
    if (!initSuccess) return;  // no measurement

    if(alert.lowVoltage || alert.lowSOC) {
        if (!alert.lowPowerActive) {
            lastPreset = currentPreset;
            //applyPreset(lowPowerIndicatorPreset);
            alert.lowPowerActive = true;
        }
    } else {
        if(alert.lowPowerActive) {
            //applyPreset(lastPreset);
            alert.lowPowerActive = false;
        }
    }
}

void UsermodBatteryFuel::checkFlagAndClear(uint8_t status_flag, int check_flag, bool* _alert) {
    *_alert = status_flag & check_flag;
    if (*_alert) fuel_gauge.clearAlertFlag(check_flag);
}


void UsermodBatteryFuel::readFuelGauge() {
    battery.voltage = fuel_gauge.cellVoltage();
    battery.percentage = fuel_gauge.cellPercent();
    battery.chargeRate = fuel_gauge.chargeRate();
    battery.hibernating = fuel_gauge.isHibernating();

    if (fuel_gauge.isActiveAlert()) {
        uint8_t status_flag = fuel_gauge.getAlertStatus();
        checkFlagAndClear(status_flag, MAX1704X_ALERTFLAG_VOLTAGE_HIGH, &alert.highVoltage);
        checkFlagAndClear(status_flag, MAX1704X_ALERTFLAG_VOLTAGE_LOW, &alert.lowVoltage);
        checkFlagAndClear(status_flag, MAX1704X_ALERTFLAG_SOC_LOW, &alert.lowSOC);
    }

    if (battery.capacity == 0 || battery.chargeRate == 0.0f) return;

    battery.chargeTime = 1.0f / battery.chargeRate;
}

/*
* loop() is called continuously. Here you can check for events, read sensors, etc.
* 
*/
void UsermodBatteryFuel::loop() 
{
    if(strip.isUpdating()) return;

    // check the battery level every USERMOD_BATTERYFUEL_MEASUREMENT_INTERVAL (ms)
    if (millis() < nextReadTime) return;

    if (!initSuccess) return;  // nothing to read

    initializing = false;

    readFuelGauge();
    
    lowPowerIndicator();

    nextReadTime = millis() + (battery.hibernating) ? 40000 : readingInterval;
    lastReadTime = millis();

    // if (calculateTimeLeftEnabled) {
    //   float currentBatteryCapacity = totalBatteryCapacity;
    //   estimatedTimeLeft = (currentBatteryCapacity/strip.currentMilliamps)*60;
    // }

    // Auto off -- Master power off
    //if (autoOffEnabled && (autoOffThreshold >= batteryLevel))
    //  turnOff();
}

/*
* addToJsonInfo() can be used to add custom entries to the /json/info part of the JSON API.
* Creating an "u" object allows you to add custom key/value pairs to the Info section of the WLED web UI.
* Below it is shown how this could be used for e.g. a light sensor
*/
void UsermodBatteryFuel::addToJsonInfo(JsonObject& root)
{
    JsonObject user = root["u"];
    if (user.isNull()) user = root.createNestedObject("u");

    if (!initSuccess) return;  // no GPIO - nothing to report
    

    JsonArray infoPercentage = user.createNestedArray(F("Battery level"));
    JsonArray infoVoltage = user.createNestedArray(F("Battery voltage"));
    JsonArray infoChargeRate = user.createNestedArray(F("Charge rate"));
    JsonArray infoChargeTime = user.createNestedArray(F("Charge time"));
    JsonArray infoNextUpdate = user.createNestedArray(F("Next update"));

    infoPercentage.add(battery.percentage);
    infoPercentage.add(" %");

    infoVoltage.add(battery.voltage);
    infoVoltage.add(" v");

    infoChargeRate.add(battery.chargeRate);
    infoChargeRate.add(" %/hr");

    infoChargeTime.add(battery.chargeTime);
    infoChargeTime.add(" hrs");

    infoNextUpdate.add(nextReadTime / 1000);
    infoChargeTime.add(" s");
}

void UsermodBatteryFuel::addToConfig(JsonObject& root)
{
    JsonObject user = root.createNestedObject(FPSTR(_name));   

    user[F("low-voltage")] = batteryLowVoltage;
    user[F("high-voltage")] = batteryHighVoltage;
    user[F("dim-percentage")] = batteryLowPowerDim;
    user[F("low-power-percentage")] = batteryLowPowerPercentage;
    user[F("reading-interval")] = readingInterval;
    
    DEBUG_PRINTLN(F("Battery fuel config saved."));
}

void UsermodBatteryFuel::appendConfigData()
{
    oappend(SET_F("addInfo('BatteryFuel:low-voltage', 1, 'v');"));
    oappend(SET_F("addInfo('BatteryFuel:high-voltage', 1, 'v');"));
    oappend(SET_F("addInfo('BatteryFuel:dim-percentage', 1, '%');"));
    oappend(SET_F("addInfo('BatteryFuel:low-power-percentage', 1, '%');"));
    oappend(SET_F("addInfo('BatteryFuel:reading-interval', 1, 'ms');"));
}

/*
* readFromConfig() can be used to read back the custom settings you added with addToConfig().
* This is called by WLED when settings are loaded (currently this only happens immediately after boot, or after saving on the Usermod Settings page)
* 
* readFromConfig() is called BEFORE setup(). This means you can use your persistent values in setup() (e.g. pin assignments, buffer sizes),
* but also that if you want to write persistent values to a dynamic buffer, you'd need to allocate it here instead of in setup.
* If you don't know what that is, don't fret. It most likely doesn't affect your use case :)
* 
* Return true in case the config values returned from Usermod Settings were complete, or false if you'd like WLED to save your defaults to disk (so any missing values are editable in Usermod Settings)
* 
* getJsonValue() returns false if the value is missing, or copies the value into the variable provided and returns true if the value is present
* The configComplete variable is true only if the "exampleUsermod" object and all values are present.  If any values are missing, WLED will know to call addToConfig() to save them
* 
* This function is guaranteed to be called on boot, but could also be called every time settings are updated
*/
bool UsermodBatteryFuel::readFromConfig(JsonObject& root)
{
    JsonObject user = root[FPSTR(_name)];
    DEBUG_PRINT(FPSTR(_name));
    {
        DEBUG_PRINTLN(F(": No config found. (Using defaults.)"));
        return false;
    }

    batteryLowVoltage = user[F("low-voltage")] | batteryLowVoltage;
    batteryHighVoltage = user[F("high-voltage")] | batteryHighVoltage;
    batteryLowPowerDim = user[F("dim-percentage")] | batteryLowPowerDim;
    batteryLowPowerPercentage = user[F("low-power-percentage")] | batteryLowPowerPercentage;
    readingInterval = user[F("reading-interval")] | readingInterval;
    DEBUG_PRINT("Config loaded");

    return !user[FPSTR(_readInterval)].isNull();
}


//***********************REWRITTEN ABOVE HERE*********************************



    // custom map function
    // https://forum.arduino.cc/t/floating-point-using-map-function/348113/2
    //double mapf(double x, double in_min, double in_max, double out_min, double out_max) 
    //{
    //  return (x - in_min) * (out_max - out_min) / (in_max - in_min) + out_min;
    //}


    /*
     * Turn off all leds
     */
    //void turnOff()
    //{
    //  bri = 0;
    //  stateUpdated(CALL_MODE_DIRECT_CHANGE);
    //}






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


// /*
// * Get lowest configured battery voltage
// */
// float UsermodBatteryFuel::getMinBatteryVoltage()
// {
//     return minBatteryVoltage;
// }

// /*
// * Set lowest battery voltage
// * can't be below 0 volt
// */
// void UsermodBatteryFuel::setMinBatteryVoltage(float voltage)
// {
//     minBatteryVoltage = max(0.0f, voltage);
// }

// /*
// * Get highest configured battery voltage
// */
// float UsermodBatteryFuel::getMaxBatteryVoltage()
// {
//     return maxBatteryVoltage;
// }

// /*
// * Set highest battery voltage
// * can't be below minBatteryVoltage
// */
// void UsermodBatteryFuel::setMaxBatteryVoltage(float voltage)
// {
//     #ifdef USERMOD_BATTERYFUEL_USE_LIPO
//     maxBatteryVoltage = max(getMinBatteryVoltage()+0.7f, voltage);
//     #else
//     maxBatteryVoltage = max(getMinBatteryVoltage()+1.0f, voltage);
//     #endif
// }


// /*
// * Get the capacity of all cells in parralel sumed up
// * unit: mAh
// */
// unsigned int UsermodBatteryFuel::getTotalBatteryCapacity()
// {
//     return totalBatteryCapacity;
// }

// void UsermodBatteryFuel::setTotalBatteryCapacity(unsigned int capacity)
// {
//     totalBatteryCapacity = capacity;
// }

// /*
// * Get the choosen adc precision
// * esp8266 = 10bit resolution = 1024.0f 
// * esp32 = 12bit resolution = 4095.0f
// */
// float UsermodBatteryFuel::getAdcPrecision()
// {
//     #ifdef ARDUINO_ARCH_ESP32
//     // esp32
//     return 4096.0f;
//     #else
//     // esp8266
//     return 1024.0f;
//     #endif
// }

// /*
// * Get the calculated voltage
// * formula: (adc pin value / adc precision * max voltage) + calibration
// */
// float UsermodBatteryFuel::getVoltage()
// {
//     return voltage;
// }

// /*
// * Get the mapped battery level (0 - 100) based on voltage
// * important: voltage can drop when a load is applied, so its only an estimate
// */
// int8_t UsermodBatteryFuel::getBatteryLevel()
// {
//     return batteryLevel;
// }

// /*
// * Get low-power-indicator feature enabled status
// * is the low-power-indicator enabled, true/false
// */
// bool UsermodBatteryFuel::getLowPowerIndicatorEnabled()
// {
//     return lowPowerIndicatorEnabled;
// }

// /*
// * Set low-power-indicator feature status 
// */
// void UsermodBatteryFuel::setLowPowerIndicatorEnabled(bool enabled)
// {
//     lowPowerIndicatorEnabled = enabled;
// }

// /*
// * Get low-power-indicator preset to activate when low power is detected
// */
// int8_t UsermodBatteryFuel::getLowPowerIndicatorPreset()
// {
//     return lowPowerIndicatorPreset;
// }

// /* 
// * Set low-power-indicator preset to activate when low power is detected
// */
// void UsermodBatteryFuel::setLowPowerIndicatorPreset(int8_t presetId)
// {
//     // String tmp = ""; For what ever reason this doesn't work :(
//     // lowPowerIndicatorPreset = getPresetName(presetId, tmp) ? presetId : lowPowerIndicatorPreset;
//     lowPowerIndicatorPreset = presetId;
// }

// /*
// * Get low-power-indicator threshold in percent (0-100)
// */
// int8_t UsermodBatteryFuel::getLowPowerIndicatorThreshold()
// {
//     return lowPowerIndicatorThreshold;
// }

// /*
// * Set low-power-indicator threshold in percent (0-100)
// */
// void UsermodBatteryFuel::setLowPowerIndicatorThreshold(int8_t threshold)
// {
//     lowPowerIndicatorThreshold = threshold;
//     // when auto-off is enabled the indicator threshold cannot be below auto-off threshold
//     //lowPowerIndicatorThreshold  = autoOffEnabled /*&& lowPowerIndicatorEnabled*/ ? max(autoOffThreshold+1, (int)lowPowerIndicatorThreshold) : max(5, (int)lowPowerIndicatorThreshold);
// }

// /*
// * Get low-power-indicator duration in seconds
// */
// int8_t UsermodBatteryFuel::getLowPowerIndicatorDuration()
// {
//     return lowPowerIndicatorDuration;
// }

// /*
// * Set low-power-indicator duration in seconds
// */
// void UsermodBatteryFuel::setLowPowerIndicatorDuration(int8_t duration)
// {
//     lowPowerIndicatorDuration = duration;
// }


// /*
// * Get low-power-indicator status when the indication is done thsi returns true
// */
// bool UsermodBatteryFuel::getLowPowerIndicatorDone()
// {
//     return lowPowerIndicationDone;
// }



// strings to reduce flash memory usage (used more than twice)
const char UsermodBatteryFuel::_name[]          PROGMEM = "BatteryFuel";
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