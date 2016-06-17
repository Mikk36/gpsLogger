#include "mbed.h"
#include "SDHandler.h"
#include "SDFileSystem.h"
//#include "BufferedSerial.h"
#include <string>
#include <vector>
#include "PinDetect.h"
#include "settings.h"
#include <stdarg.h>

//extern bool wdReset;
extern Serial BT;
extern bool writeFailed;

/**
    PinName input SD card enabled sensing, enabled if low
    PinName led SD card enabled LED
*/
SDHandler::SDHandler(PinName input, PinName led) {

  _active = false;
  _dateInitialized = false;
  _dateRecheck = false;
  _oldHour = "";
  _sdBuffer = "";
  _fp = 0;
  _firstTime = true;
  //_watchdogChecked = false;

  _pin = new PinDetect(input);
  _led = new DigitalOut(led);
  _pin->mode(PullUp);
  _pin->setAssertValue(0);
  _pin->setSamplesTillAssert(10);
  _pin->attach_asserted(this, &SDHandler::_keyPressed);
  _pin->attach_deasserted(this, &SDHandler::_keyReleased);
  _pin->setSampleFrequency(50000);

  _fileNumber = -1;


  _t = new Timer();

  _sd = new SDFileSystem(PB_15, PB_14, PB_13, PD_2, "sd"); // D34, D33, D32, D25

  //_sd->mount();
  //_dirTest();

  //_log("Calling pack_file\n");
  //Sixpack *packer = new Sixpack(BT);
  //int result = packer->pack_file(1, "/sd/11.log", "/sd/11.flz");
  //_log("pack_file result: %d\n", result);
  //test();
  //_sd->unmount();


  if((int) *_pin == 0) {
    _keyPressed();
  }
  _pin->setSampleFrequency();
}

void SDHandler::logData(const std::string data) {
  if(_checkNewDate(data)) {
    _log("logData: _checkNewDate is true\n");
    if(_active) {
      _log("logData: _active is true\n");
      if(_firstTime) {
        _firstTime = false;
      } else {
        ++_fileNumber;
      }
      _closeFile();
      _openFile();
    }
  }

  if(_fp != 0) {
    _filterData(data);
  }
}

void SDHandler::_filterData(const std::string &data) {
#if SD_TESTING
  if(!_active) {
#else
  if(!_active || !_dateInitialized) {
#endif
    return;
  }

#if REDUCED_SD_LOGGING
  if(data.compare(1, 5, "GPGGA", 0, 5) == 0 || data.compare(1, 5, "GPRMC", 0, 5) == 0) {
    if(data.compare(13, 3, ".00", 0, 3) == 0 || data.compare(13, 3, ".99", 0, 3) == 0) {
      //fprintf(_fp, data.c_str());
      _sdBuffer += data;
    }
  }
#else
  //fprintf(_fp, data.c_str());
  _sdBuffer += data;
#endif

#if SD_TESTING
  _log("_sdBuffer length: %i\n", _sdBuffer.length());
#endif
  if(_sdBuffer.length() > 2000) {
    _saveData();
  }
}

void SDHandler::_saveData() {
  _led->write(0);
#if DEBUG_LOGGING
  int blockSize = 16;
  int i = 1;
  _log("Checking memory with blocksize %d char ...\n", blockSize);
  while (true) {
    char *p = (char *) malloc(i * blockSize);
    if (p == NULL)
      break;
    free(p);
    ++i;
  }
  _log("Memory OK for %d char\n", (i - 1) * blockSize);
#endif

  if(_fp != 0) {
    _log("Saving data to SD\n");
    int wroteBytes = fprintf(_fp, _sdBuffer.c_str());
    BT.printf("Wrote %i bytes to SD\n", wroteBytes);
    if(wroteBytes < 0) {
      writeFailed = true;
    }
  }
  _sdBuffer.clear();

#if DEBUG_LOGGING
  //_log("Current Stack : 0x%08X\r\n", __current_sp());
  //_log("Current Heap : 0x%08X\r\n", __current_pc());

  blockSize = 16;
  i = 1;
  _log("Checking memory with blocksize %d char ...\n", blockSize);
  while (true) {
    char *p = (char *) malloc(i * blockSize);
    if (p == NULL)
      break;
    free(p);
    ++i;
  }
  _log("Memory OK for %d char\n", (i - 1) * blockSize);
#endif

  _led->write(1);
}

bool SDHandler::_checkNewDate(const std::string &data) {
  if(!_dateInitialized || _dateRecheck) {
    if(!_dateInitialized) {
      _log("Date not initialized\n");
    }
    if(data.compare(1, 5, "GPRMC", 0, 5) == 0) {
      _log("Found a GPRMC sentence\n");
      return _parseRMC(data);
    }
  } else if(data.compare(1, 5, "GPGGA", 0, 5) == 0) {
    _log("Found a GPGGA sentence\n");
    return _parseGGA(data);
  }
  return false;
}

bool SDHandler::_parseRMC(const std::string &data) {
  // $GPRMC ,121330.904 ,V ,          ,  ,           ,  ,0.00 ,0.00 ,080180 ,   ,   ,N  *43
  // $GPRMC ,121331.105 ,V ,          ,  ,           ,  ,0.00 ,0.00 ,131015 ,   ,   ,N  *4D
  // $GPRMC ,121349.693 ,A ,5926.2671 ,N ,02452.6428 ,E ,0.01 ,0.00 ,131015 ,   ,   ,A  *6B
  // 0      ,1          ,2 ,3         ,4 ,5          ,6 ,7    ,8    ,9      ,10 ,11 ,12

  short commaCounter = 0;
  short c;
  for(std::string::size_type i = 0; i < data.size(); ++i) {
    c = (unsigned char)data.at(i);
    if(c == ',') {
      //_log("Found a comma\n");
      ++commaCounter;
      //_log("commaCounter: %i\n", commaCounter);
      //_log("commaCounter: %i\n", commaCounter);
    }
    if(commaCounter == 9) {
      _log("We're at 9!\n");
      std::string year = data.substr(i + 5, 2);
      if(year.compare("15") >= 0 && year.compare("80") < 0) {
        _log("_parseRMC: Initialized!\n");
        _dateInitialized = true;
        _oldHour = data.substr(7, 2);

        _parseDateTime(data.substr(7, 6), data.substr(i + 1, 6));

        if(_dateRecheck) {
          _log("_parseRMC: _dateRecheck, returning false\n");
          _dateRecheck = false;
          return false;
        }
        _log("_parseRMC: Returning true\n");
        return true;
      }
      _log("_parseRMC: Not an ok year\n");
      return false;
    }
  }
  _log("_parseRMC: default return false\n");
  return false;
}

bool SDHandler::_parseGGA(const std::string &data) {
  // $GPGGA ,121348.899 ,          ,  ,           ,  ,0 ,0 ,     ,     ,M  ,     ,M  ,   ,   *4D
  // $GPGGA ,121349.493 ,          ,  ,           ,  ,0 ,5 ,     ,     ,M  ,     ,M  ,   ,   *4F
  // $GPGGA ,121349.693 ,5926.2671 ,N ,02452.6428 ,E ,1 ,5 ,1.36 ,74.2 ,M  ,19.7 ,M  ,   ,   *60
  // 0      ,1          ,2         ,3 ,4          ,5 ,6 ,7 ,8    ,9    ,10 ,11   ,12 ,13 ,14

  std::string newHour = data.substr(7, 2);
  //_log("Old: %s New: %s\n", _oldHour.c_str(), newHour.c_str());
  _log("Old: %s New: %s\n", _oldHour.c_str(), newHour.c_str());

  if(newHour.compare(_oldHour) < 0 || newHour.compare(_oldHour) > 0) {
    _dateRecheck = true;
  }

  if(newHour.compare(_oldHour) < 0) {
    _oldHour = newHour;
    _log("New day!\n");
    return true;
  }

  _oldHour = newHour;
  return false;
}

void SDHandler::_parseDateTime(std::string time, std::string date) {
  /*
  struct tm settm;

  settm.tm_hour = 10 * (cGETtime[0] - '0') + cGETtime[1] - '0'; // '0' = 30; '1' = 31 => e.g. cGETtime[0]= '1' => '1'-'0' = 31-30 = 1
  settm.tm_min  = 10 * (cGETtime[2] - '0') + cGETtime[3] - '0';
  settm.tm_sec  = 10 * (cGETtime[4] - '0') + cGETtime[5] - '0';

  settm.tm_mday  = 10 * (cGETdate[0] - '0') + cGETdate[1] - '0';
  settm.tm_mon   = 10 * (cGETdate[2] - '0') + cGETdate[3] - '0' -1; //0-11
  settm.tm_year  = 10 * (cGETdate[4] - '0') + cGETdate[5] - '0';
  */

  int hour = 10 * (time[0] - '0') + (time[1] - '0');
  int minute = 10 * (time[2] - '0') + (time[3] - '0');
  int second = 10 * (time[4] - '0') + (time[5] - '0');

  int day = 10 * (date[0] - '0') + (date[1] - '0');
  int month = 10 * (date[2] - '0') + (date[3] - '0');
  int year = 2000 + 10 * (date[4] - '0') + (date[5] - '0');

  //_log("Date: %02d:%02d:%02d %02d.%02d.%04d\n", hour, minute, second, day, month, year);
  _log("Date: %02d:%02d:%02d %02d.%02d.%04d\n", hour, minute, second, day, month, year);

  set_time(_unixTimestamp(year, month, day, hour, minute, second));
  _log("Date set\n");
}

unsigned long SDHandler::_unixTimestamp(int year, int month, int day, int hour, int min, int sec) {
  const short days_since_beginning_of_year[12] = {0, 31, 59, 90, 120, 151, 181, 212, 243, 273, 304, 334};

  int leap_years = ((year - 1) - 1968) / 4
                   - ((year - 1) - 1900) / 100
                   + ((year - 1) - 1600) / 400;

  long days_since_1970 = (year - 1970) * 365 + leap_years
                         + days_since_beginning_of_year[month - 1] + day - 1;

  if ( (month > 2) && (year % 4 == 0 && (year % 100 != 0 || year % 400 == 0)) )
    days_since_1970 += 1; /* +leap day, if year is a leap year */

  return sec + 60 * ( min + 60 * (hour + 24 * days_since_1970) );
}

void SDHandler::_keyPressed() {
  if(!_active) {
    _sd->mount();
    _getFileNumber();
    _led->write(1);
    _log("Pressed\n");
    _log("New file number: %i\n", _fileNumber);
    _dirTest();
    _openFile();
    _active = true;
    //_logWatchdogReset();
  }
}

void SDHandler::_keyReleased() {
  if(_active) {
    _active = false;
    _closeFile();
    _sd->unmount();
    _fileNumber = -1;
    _log("Unpressed\n");
    _led->write(0);
  }
}

//void SDHandler::_logWatchdogReset() {
//    if(!_watchdogChecked) {
//        if (wdReset) {
//            fprintf(_fp, "Watchdog caused reset.\n");
//        }
//        _watchdogChecked = true;
//    }
//}

void SDHandler::_closeFile() {
  if(_sdBuffer.length() > 0) {
    _log("_closeFile: buffer is not 0\n");
    _saveData();
  }
  if(_fp != 0) {
    _log("_closeFile: file is open\n");
    fclose(_fp);
    _fp = 0;
  }
  _log("_closeFile: file is now closed\n");
}

void SDHandler::_openFile() {
#if !SD_TESTING
  if(!_dateInitialized) {
    return;
  }
#endif

  char fileName[20];
  snprintf(fileName, sizeof(fileName), "/sd/%i.log", _fileNumber);
  _log("New file name: %s\n", fileName);
  _fp = fopen(fileName, "a");
  _log("File opened\n");
}

void SDHandler::_getFileNumber() {
  std::string path = "/sd/";
  _filenames = _read_directory(path);

  for(vector<std::string>::iterator it = _filenames.begin(); it < _filenames.end(); it++) {
    std::string extensionTest = (*it).substr((*it).find_last_of(".") + 1);
    if(strcmp(extensionTest.c_str(), "log") == 0) {
      std::string numberTest = (*it).substr(0, (*it).find_last_of("."));
      if(_is_number(numberTest)) {
        int value = atoi(numberTest.c_str());
        if(value > _fileNumber) {
          _fileNumber = value;
        }
      }
    }
  }
  ++_fileNumber;
}

bool SDHandler::_is_number(const std::string &s) {
  return s.find_first_not_of("0123456789") == std::string::npos;
}

void SDHandler::_dirTest() {
#if DEBUG_LOGGING
  std::string path = "/sd/";
  _filenames = _read_directory(path);
  for(vector<std::string>::iterator it = _filenames.begin(); it < _filenames.end(); it++) {
    BT.printf("%s\n", (*it).c_str());
  }
#endif
}

std::vector<std::string> SDHandler::_read_directory( const std::string &path = std::string() ) {
  std::vector <std::string> result;
  dirent *de;
  DIR *dp;
  dp = opendir( path.empty() ? "." : path.c_str() );
  if (dp) {
    while (true) {
      de = readdir( dp );
      if(de == NULL) {
        break;
      }

      result.push_back(std::string(de->d_name));
    }
    closedir(dp);
    //std::sort( result.begin(), result.end() );
  }
  return result;
}

void SDHandler::_test() {
  _log("\nWriting to SD card...\n");
  _t->start();
  _fp = fopen("/sd/sdtest.txt", "a");
  _t->stop();
  _log("It took %f seconds to open the file\n", _t->read());
  _t->reset();
  if (_fp != NULL) {
    _t->start();
    for(int i = 0; i < 10000; ++i) {
      fprintf(_fp, "We're writing to an SD card!\n");
    }
    _t->stop();
    _log("It took %f seconds to write to the file\n", _t->read());
    _t->reset();

    _t->start();
    fclose(_fp);
    _t->stop();
    _log("It took %f seconds to close the file\n", _t->read());
    _t->reset();

    _log("success!\n");
  } else {
    _log("failed!\n");
  }

  //Perform a read test
  _log("Reading from SD card...");
  _fp = fopen("/sd/sdtest.txt", "r");
  if (_fp != NULL) {
    char c = fgetc(_fp);
    if (c == 'W')
      _log("success!\n");
    else
      _log("incorrect char (%c)!\n", c);
    fclose(_fp);
  } else {
    _log("failed!\n");
  }
}

int SDHandler::_log(const char *format, ...) {
#if DEBUG_LOGGING
  int n;
  va_list argptr;
  va_start(argptr, format);
  n = BT.vprintf(format, argptr);
  va_end(argptr);
  return n;
#else
  return 0;
#endif
}
