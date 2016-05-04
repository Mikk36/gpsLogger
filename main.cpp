#include "mbed.h"
#include "BufferedSerial.h"
#include <string>
#include <sstream>
#include "GPSHandler.h"
#include "SDHandler.h"
#include "settings.h"
#include "Watchdog.h"

Watchdog wd;

BufferedSerial GPS(PA_2, PA_3); // D1, D0
Serial BT(PA_9, PA_10); // D7, D8

Timeout t;
bool wdReset = false;
bool writeFailed = false;

SDHandler *sdHandler;
GPSHandler *gpsHandler;
DigitalOut led(LED1);

int main() {
  BT.baud(115200);

#if WATCHDOG_ENABLED
  wd.Configure(WATCHDOG_TIMEOUT);
  if (wd.WatchdogCausedReset()) {
    wdReset = true;
    BT.printf("\nWatchdog caused reset.\n");
  }
  wd.Service();
#endif

  for(int i = 0; i < 21; i++) {
    led = !led;
    wait(0.1);
  }

#if DEBUG_LOGGING || DEBUG_LOGGING_2
  BT.printf("Hello World !\n");
#endif

  // SD pins: D34, D33, D32, D25
  sdHandler = new SDHandler(PB_5, PA_5); // D4 (input, pullup), D13 (LED1, output)
  gpsHandler = new GPSHandler(PA_1, PB_6, PA_0, sdHandler); // D3 (LED2, output), D5 (input, pullup), D2 (output)
  t.attach(gpsHandler, &GPSHandler::baudSetup, 3.0f);

  while(1) {
#if DEBUG_LOGGING_2
    BT.printf("Main loop\n");
#endif
  if(!writeFailed) {
    gpsHandler->loop();
  }
    //wait(0.01);
    sleep();
  }
}
