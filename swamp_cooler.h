#ifndef __SWAMPCOOLER_H
#define __SWAMPCOOLER_H
#include <IRsend.h>
#include <Arduino.h>
#include <time.h>
extern IRsend irsend1;
#define DEVICE_OFF 0
#define DEVICE_ON 1
#define DEVICE_HI 1
#define POWER_BTN_CODE 0x807F
#define PUMP_BTN_CODE 0x40BF
#define FAN_BTN_CODE 0xC03F
#define SWITCHDELAY 500
#define UPPER_CODE 0x80FF0000
#define POWER_NAME "power"
#define PUMP_NAME "pump"
#define FAN_NAME "fan"

class swampCoolerButton {
  public:
    swampCoolerButton(const uint32_t code, String name)
            :code(code), deviceName(name), state(-1), timestamp(-1) {
        realcode = UPPER_CODE + code;
    }
    ~swampCoolerButton() { }
    int32_t getState() {return state; }
    void setState(int newstate) {state = newstate; }
    String name() { return "\""+deviceName+"\""+":"+String(state); }
    void click();
    virtual bool verifyState(int state);
  protected:
    String deviceName;
    uint32_t code;
    uint64_t realcode;
    int32_t state;
    time_t timestamp;
};

class swampCoolerFanButton : public swampCoolerButton {
  public:
    swampCoolerFanButton(const uint32_t code, String name) : swampCoolerButton(code, name) { } 
    bool send(int state=DEVICE_ON);
    virtual bool verifyState(int state);

};

swampCoolerButton* selectButton(String name);
String returnSwampStates();
extern swampCoolerButton power_Btn;
extern swampCoolerButton pump_Btn;
extern swampCoolerFanButton fan_Btn;
#endif

