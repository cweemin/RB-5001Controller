#ifndef __local_h
#define __local_h

#define SYSLOG
#define FS_BROWSER
#define ARDUINO_OTA
#define SWAMP_COOLER

#define SYSLOG_SERVER "logs.papertrailapp.com"
#define SYSLOG_PORT 514

// TimeZone Stuff
#define TZ              1       // (utc+) TZ in hours
#define TZ_SEC          ((TZ)*3600)
#define DST_SEC 0

#define UPDATE_PATH "/firmware"

#define HOST_NAME "irled"
#define DBG_OUTPUT_PORT Serial


//WIFI STUFF
#define WiFiSSID  ""
#define WiFiPSK   ""
#endif
