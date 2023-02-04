#ifndef USERMOD_BATTERYFUEL_CAPACITY
  #define USERMOD_BATTERYFUEL_CAPACITY 5200
#endif

#ifndef USERMOD_BATTERYFUEL_LOW_VOLTAGE
  #define USERMOD_BATTERYFUEL_LOW_VOLTAGE 3.2
#endif

#ifndef USERMOD_BATTERYFUEL_HIGH_VOLTAGE
  #define USERMOD_BATTERYFUEL_HIGH_VOLTAGE 4.2
#endif

#ifndef USERMOD_BATTERYFUEL_LOW_POWER_OFF
  #define USERMOD_BATTERYFUEL_LOW_POWER_OFF true
#endif

#ifndef USERMOD_BATTERYFUEL_LOW_POWER_PERCENTAGE
  #define USERMOD_BATTERYFUEL_LOW_POWER_PERCENTAGE 20;
#endif

// Default time period between checking the fuel cell
// In hibernation mode this is automatically set to 40 seconds
#ifndef USERMOD_BATTERYFUEL_MEASUREMENT_INTERVAL
  #define USERMOD_BATTERYFUEL_MEASUREMENT_INTERVAL 20000
#endif

//***********************REWRITTEN ABOVE HERE*********************************
 


// pin defaults
// for the esp32 it is best to use the ADC1: GPIO32 - GPIO39
// https://docs.espressif.com/projects/esp-idf/en/latest/esp32/api-reference/peripherals/adc.html
#ifndef USERMOD_BATTERYFUEL_MEASUREMENT_PIN
  #ifdef ARDUINO_ARCH_ESP32
    
  #else //ESP8266 boards
    
  #endif
#endif

