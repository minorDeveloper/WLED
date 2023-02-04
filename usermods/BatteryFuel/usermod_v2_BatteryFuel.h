#pragma once

#include "Wire.h"

#include "wled.h"
#include "battery_defaults.h"

//#include "./Adafruit/Adafruit_MAX1704X.h"

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
    //Adafruit_MAX17048 fuel_gauge;

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
    void lowPowerIndicator();
    void checkFlagAndClear(uint8_t status_flag, int check_flag, bool* _alert);


  public:
    // Key functions
    void setup();
    void connected();
    void loop();

    void addToJsonInfo(JsonObject& root);
    void addToConfig(JsonObject& root);
    void appendConfigData();
    bool readFromConfig(JsonObject& root);

    void readFuelGauge();

    // Getters and setters
    uint16_t getId();
    unsigned long getReadingInterval();
    void setReadingInterval(unsigned long newReadingInterval);

    // fixme: UNUSED
    void generateExamplePreset();

    virtual ~UsermodBatteryFuel() 
    {

    }
};


