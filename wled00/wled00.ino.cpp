# 1 "C:\\Users\\samla\\AppData\\Local\\Temp\\tmp4bw8xzj1"
#include <Arduino.h>
# 1 "C:/dev/BTTF/WLED/wled00/wled00.ino"
# 13 "C:/dev/BTTF/WLED/wled00/wled00.ino"
#include "wled.h"
void setup();
void loop();
#line 15 "C:/dev/BTTF/WLED/wled00/wled00.ino"
void setup() {
  WLED::instance().setup();
}

void loop() {
  WLED::instance().loop();
}