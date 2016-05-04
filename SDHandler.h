#ifndef SDHandler_h
#define SDHandler_h

#include "mbed.h"
#include "SDFileSystem.h"
//#include "BufferedSerial.h"
#include <string>
#include <vector>
#include "PinDetect.h"

class SDHandler
{
public:
    SDHandler(PinName input, PinName led);
    void logData(const std::string data);

private:
    SDFileSystem *_sd;
    std::vector<std::string> _filenames;
    int _fileNumber;
    PinDetect *_pin;
    DigitalOut *_led;
    bool _active;
    FILE *_fp;
    std::string _oldHour;
    bool _dateInitialized;
    bool _dateRecheck;
    std::string _sdBuffer;
    bool _firstTime;
    //bool _watchdogChecked;

    Timer *_t;

    void _dirTest();
    void _test();
    std::vector<std::string> _read_directory(const std::string &path);
    void _getFileNumber();
    bool _is_number(const std::string &s);
    void _keyPressed();
    void _keyReleased();
    //void _logWatchdogReset();
    void _openFile();
    void _closeFile();
    bool _checkNewDate(const std::string &data);
    bool _parseRMC(const std::string &data);
    bool _parseGGA(const std::string &data);
    void _parseDateTime(std::string time, std::string date);
    void _filterData(const std::string &data);
    void _saveData();
    unsigned long _unixTimestamp(int year, int month, int day, int hour, int min, int sec);
    int _log(const char *format, ...);
};

#endif
