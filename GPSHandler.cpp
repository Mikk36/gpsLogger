#include "mbed.h"
#include "GPSHandler.h"
#include "BufferedSerial.h"
#include "SDHandler.h"
#include <string>
#include <sstream>
#include "PinDetect.h"
#include "settings.h"
#include "stdarg.h"
#include "Watchdog.h"

extern Watchdog wd;
extern BufferedSerial GPS;
extern Serial BT;

/**
  PinName activityLed GPS activity LED
  PinName powerPin Car ACC power sensing, active if low
  PinName btPowerPin Bluetooth power MOSFET pin
*/
GPSHandler::GPSHandler(PinName activityLed, PinName powerPin, PinName btPowerPin, SDHandler *sd) {
  _sd = sd;

  _pin = new PinDetect(powerPin);
  _pin->mode(PullUp);
  _pin->setAssertValue(0);
  _pin->setSamplesTillAssert(5);
  _pin->setSamplesTillHeld(200);
  _pin->attach_asserted(this, &GPSHandler::_keyPressed);
  _pin->attach_deasserted_held(this, &GPSHandler::_keyReleased);
  _pin->setSampleFrequency(50000);

  _activity = new DigitalOut(activityLed);
  _btPower = new DigitalOut(btPowerPin);
  _bufferI = 0;
}

void GPSHandler::loop() {
  //_log("Loop start\n");

  _activity->write(0);
  if(GPS.readable()) {
    do {
      //_log("GPS is readable\n");
      char currentChar = GPS.getc();
      _gpsBuffer[_bufferI] = currentChar;
      ++_bufferI;
      //_log("Buffer length: %i\n", _bufferI);
      if(currentChar == '\n') {
        _activity->write(1);
        _gpsBuffer[_bufferI] = 0;
        if(validateLine(_gpsBuffer)) {
          wd.Service();
          BT.printf(_gpsBuffer);
          _sd->logData(_gpsBuffer);
        }
        _gpsBuffer[0] = 0;
        _bufferI = 0;
      }

      if(_bufferI > NMEA_LENGTH) {
        //wd.Service();
        _sd->logData(_gpsBuffer);
        _log("Buffer overflow, clearing\n");
        _bufferI = 0;
        _gpsBuffer[0] = 0;
      }
    } while(GPS.readable());
  }
}

void GPSHandler::_keyPressed() {
  _log("Faster rate\n");
  setUpdateRate(GPS_RATE_FAST);
  _btPower->write(1);
}

void GPSHandler::_keyReleased() {
  _log("Slower rate\n");
  setUpdateRate(GPS_RATE_SLOW);
  _btPower->write(0);
}

std::string GPSHandler::int_to_hex( int number ) {
  char buffer[10];
  sprintf(buffer, "%02X", number);
  std::string data = buffer;
  return data;
}

std::string GPSHandler::int_to_dec(int number) {
  char buffer[10];
  sprintf (buffer, "%d", number);
  std::string data = buffer;
  return data;
}

bool GPSHandler::validateLine(std::string command) {
  if(command.compare(0, 1, "$", 0, 1) != 0) {
    return false;
  }
  size_t asteriskPos = command.find_first_of("*");
  if(asteriskPos == std::string::npos) {
    return false;
  }
  if(getCheckSum(command.substr(1, asteriskPos - 1)).compare(command.substr(asteriskPos + 1, 2)) == 0) {
    return true;
  } else {
    return false;
  }
}

std::string GPSHandler::getCheckSum(std::string command) {
  unsigned int i;
  int XOR;
  int c;
  // Calculate checksum ignoring any $'s in the string
  for (XOR = 0, i = 0; i < command.length(); ++i) {
    c = (unsigned char)command.at(i);
    if (c == '*') break;
    if (c != '$') XOR ^= c;
  }
  return int_to_hex(XOR);
}

void GPSHandler::setBaud(unsigned int baud) {
  std::string command = "$PMTK251," + int_to_dec(baud) + "*";
  command += getCheckSum(command) + "\r\n";
  GPS.printf(command.c_str());

  _t.attach(this, &GPSHandler::baudStep2, 1.0f);
}

void GPSHandler::changeBaud(unsigned int baud) {
  GPS.baud(baud);

  _t.attach(this, &GPSHandler::baudStep3, 1.0f);
}

void GPSHandler::setUpdateRate(unsigned short rate) {
  std::string command = "$PMTK220," + int_to_dec(rate) + "*";
  command += getCheckSum(command) + "\r\n";

  GPS.printf(command.c_str());
}

void GPSHandler::baudSetup() {
  wd.Service();
  setBaud(115200);
}

void GPSHandler::baudStep2() {
  wd.Service();
  changeBaud(115200);
}

void GPSHandler::baudStep3() {
  wd.Service();
  if((int) *_pin == 0) {
    _keyPressed();
  } else {
    _keyReleased();
  }
}

int GPSHandler::_log(const char *format, ...) {
#if DEBUG_LOGGING
  int n;
  va_list ap;
  va_start(ap, format);
  n = BT.vprintf(format, ap);
  va_end(ap);
  return n;
#else
  return 0;
#endif
}
