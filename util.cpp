#include <Arduino.h>
#include "util.h"
#include "defines.h"

void logM(String msg, uint16_t pri) {
#ifdef SYSLOG
  syslog.log(LOG_INFO, msg);
#endif
  Serial.println(msg);
}
