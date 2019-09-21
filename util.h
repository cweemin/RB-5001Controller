#ifndef __util_h
#define __util_h
#include <Arduino.h>
#include "defines.h"
#ifdef SYSLOG
#include <Syslog.h>
extern Syslog syslog;
void logM(String, uint16_t pri=LOG_INFO);
#endif
#endif
