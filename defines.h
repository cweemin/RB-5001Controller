#ifndef __local_h
#define __local_h

//#define SYSLOG
#define FS_BROWSER
#define ARDUINO_OTA
#define SWAMP_COOLER
#define SERIAL_DEBUG
#define AM2320_SENSOR
#define SYSLOG_SERVER "logs.papertrailapp.com"
#define SYSLOG_PORT 514
// Maximum number of item to GET
#define MAXGET 10

// TimeZone Stuff
#define TZ              -7       // (utc+) TZ in hours
#define TZ_SEC          ((TZ)*3600)
#define DST_SEC 0

#define UPDATE_PATH "/firmware"

#define HOST_NAME "irled"
#define DBG_OUTPUT_PORT Serial


//WIFI STUFF
#define WiFiSSID  ""
#define WiFiPSK   ""

// Temperature sensorpin (analog)
#define SENSORPIN 0
#endif
