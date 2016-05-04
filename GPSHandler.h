#ifndef GPSHandler_h
#define GPSHandler_h

#include "mbed.h"
#include "BufferedSerial.h"
#include "SDHandler.h"
#include <string>
#include "PinDetect.h"

#define NMEA_LENGTH 90

class GPSHandler
{
public:
    GPSHandler(PinName activityLed, PinName powerPin, PinName btPowerPin, SDHandler *sd);

    void baudSetup();

    void loop();

private:
    Timeout _t;
    SDHandler *_sd;
    DigitalOut *_activity;
    DigitalOut *_btPower;
    char _gpsBuffer[NMEA_LENGTH + 1];
    int _bufferI;
    PinDetect *_pin;

    std::string int_to_hex( int number );
    std::string int_to_dec(int number);
    std::string getCheckSum(std::string command);
    bool validateLine(std::string command);
    void setBaud(unsigned int baud);
    void changeBaud(unsigned int baud);
    void setUpdateRate(unsigned short rate);
    void baudStep2();
    void baudStep3();
    void _keyPressed();
    void _keyReleased();
    int _log(const char *format, ...);
};

#endif
