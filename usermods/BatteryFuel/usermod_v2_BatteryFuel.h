#pragma once

#include "wled.h"

#include "Wire.h"
#include "battery_defaults.h"

#include "./Adafruit/Adafruit_MAX1704X.h"

/*
 * Usermod by Sam Lane, based on the Battery usermod by Maximilian Mewes

 * Designed to work with the MAX17048/9 fuel gauge for precise lipo charge
 * state sensing and discharge detection
 * 
 * 
 * Mail: contact@samlane.tech
 * GitHub: minorDeveloper
 * Date: 2023.02.03
 * If you have any questions, please feel free to contact me.
 */
class UsermodBatteryFuel : public Usermod 
{
  private:
    Adafruit_MAX17048 fuel_gauge;

    float batteryLowVoltage = USERMOD_BATTERYFUEL_LOW_VOLTAGE;
    float batteryHighVoltage = USERMOD_BATTERYFUEL_HIGH_VOLTAGE;

    bool batteryLowPowerDim = USERMOD_BATTERYFUEL_LOW_POWER_OFF;
    float batteryLowPowerPercentage = USERMOD_BATTERYFUEL_LOW_POWER_PERCENTAGE;
    int8_t lastPreset = 0; // Tracking last preset for low power mode

    struct Alert {
      bool lowVoltage = false;
      bool highVoltage = false;
      bool lowSOC = false;
      bool lowPowerActive = false;
    };

    struct Battery {
      unsigned int capacity = USERMOD_BATTERYFUEL_CAPACITY;
      float voltage = 0.0f;
      float percentage = 0.0f;
      float chargeRate = 0.0f;
      float chargeTime = 0.0f;
      bool hibernating = false;
    };

    Alert alert;
    Battery battery;


    // Frequency with which we read the fuel gauge
    unsigned long readingInterval = USERMOD_BATTERYFUEL_MEASUREMENT_INTERVAL;
    unsigned long nextReadTime = 0;
    unsigned long lastReadTime = 0;

    bool initDone = false;
    bool initializing = true;
    bool initSuccess = true;

    // Strings to reduce flash memory usage (used more than twice)
    static const char _name[];
    static const char _readInterval[];
    static const char _enabled[];
    static const char _threshold[];
    static const char _preset[];
    static const char _duration[];
    static const char _init[];

    
    // Functions
    float dot2round(float x);
    
    
    /*
    * Indicate low power by activating a configured preset for a given time and then switching back to the preset that was selected previously
    */
    void lowPowerIndicator()
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
    
    void checkFlagAndClear(uint8_t status_flag, int check_flag, bool* _alert)
    {
      *_alert = status_flag & check_flag;
      if (*_alert) fuel_gauge.clearAlertFlag(check_flag);
    }

    void readFuelGauge() {
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


  public:
    //Key functions

    /*
    * addToJsonInfo() can be used to add custom entries to the /json/info part of the JSON API.
    * Creating an "u" object allows you to add custom key/value pairs to the Info section of the WLED web UI.
    * Below it is shown how this could be used for e.g. a light sensor
    */
    void addToJsonInfo(JsonObject& root)
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

    void addToConfig(JsonObject& root)
    {
      JsonObject user = root.createNestedObject(FPSTR(_name));   

      user[F("low-voltage")] = batteryLowVoltage;
      user[F("high-voltage")] = batteryHighVoltage;
      user[F("dim-percentage")] = batteryLowPowerDim;
      user[F("low-power-percentage")] = batteryLowPowerPercentage;
      user[F("reading-interval")] = readingInterval;

      DEBUG_PRINTLN(F("Battery fuel config saved."));
    }

    void appendConfigData()
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
    bool readFromConfig(JsonObject& root)
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

    /*
    * getId() allows you to optionally give your V2 usermod an unique ID (please define it in const.h!).
    * This could be used in the future for the system to determine whether your usermod is installed.
    */
    uint16_t getId()
    {
      return USERMOD_ID_BATTERY;
    }


    unsigned long getReadingInterval()
    {
      return readingInterval;
    }

    /*
    * minimum repetition is 3000ms (3s) 
    */
    void setReadingInterval(unsigned long newReadingInterval)
    {
      readingInterval = max((unsigned long)3000, newReadingInterval);
    }




  /*
  * setup() is called once at boot. WiFi is not yet connected at this point.
  * You can use it to initialize variables, sensors or similar.
  */
  void setup() 
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
    * loop() is called continuously. Here you can check for events, read sensors, etc.
    * 
    */

  void loop() 
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

    //UNUSED BELOW

    // if (calculateTimeLeftEnabled) {
    //   float currentBatteryCapacity = totalBatteryCapacity;
    //   estimatedTimeLeft = (currentBatteryCapacity/strip.currentMilliamps)*60;
    // }

    // Auto off -- Master power off
    //if (autoOffEnabled && (autoOffThreshold >= batteryLevel))
    //  turnOff();
  }

};



float UsermodBatteryFuel::dot2round(float x)
{
  float nx = (int)(x * 100 + .5);
  return (float)(nx / 100);
}


// strings to reduce flash memory usage (used more than twice)
const char UsermodBatteryFuel::_name[]          PROGMEM = "BatteryFuel";
const char UsermodBatteryFuel::_readInterval[]  PROGMEM = "interval";
const char UsermodBatteryFuel::_enabled[]       PROGMEM = "enabled";
const char UsermodBatteryFuel::_threshold[]     PROGMEM = "threshold";
const char UsermodBatteryFuel::_preset[]        PROGMEM = "preset";
const char UsermodBatteryFuel::_duration[]      PROGMEM = "duration";
const char UsermodBatteryFuel::_init[]          PROGMEM = "init";