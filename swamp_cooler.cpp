#include "swamp_cooler.h"
#include <Ticker.h>
#include "util.h"

String returnSwampStates() {
  return "{"+power_Btn.name()+pump_Btn.name()+fan_Btn.name()+"}";
}
swampCoolerButton* selectButton(String name) {
  if (name == PUMP_NAME) {
    return &pump_Btn;
  } else if (name == POWER_NAME) {
    return &power_Btn;
  } else if (name == FAN_NAME) {
    return &fan_Btn;
  } else {
    return NULL;
  }
}

void swampCoolerButton::click() {
  logM("Sending code: " + String(code,HEX));
  irsend1.sendNEC(realcode,32);
}

bool swampCoolerButton::verifyState(int newstate) {
  int nstate;
  if (newstate == DEVICE_ON)
    nstate = DEVICE_OFF;
  else
    nstate = DEVICE_ON;
  logM("attemping to switch device " + deviceName + " to " + String(newstate));
  if (state != -1) {
    if (state == nstate ) {
      logM("sending code: " + String(code,HEX));
      click();
      state = newstate;
      return true;
    } 
  } else {
    logM("State unset for device " + deviceName);
  }
  return false;
}

bool swampCoolerFanButton::verifyState(int newstate) {
  logM("attemping to switch device " + deviceName + " to " + String(newstate));
  if (state != -1) {
    int diff = newstate - state;
    click();
    if (diff == -1 or diff == 2) {
      delay(SWITCHDELAY);
      click();
    }
    state = newstate;
  } else {
    logM("State unset for device " + deviceName);
  }
  return false;
}

swampCoolerButton power_Btn = swampCoolerButton(POWER_BTN_CODE,POWER_NAME);
swampCoolerButton pump_Btn = swampCoolerButton(PUMP_BTN_CODE,PUMP_NAME);
swampCoolerFanButton fan_Btn = swampCoolerFanButton(FAN_BTN_CODE,FAN_NAME);
