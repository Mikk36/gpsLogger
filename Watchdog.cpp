/// @file Watchdog.cpp provides the interface to the Watchdog module
///
/// This provides basic Watchdog service for the mbed. You can configure
/// various timeout intervals that meet your system needs. Additionally,
/// it is possible to identify if the Watchdog was the cause of any
/// system restart.
///
/// Adapted from Simon's Watchdog code from http://mbed.org/forum/mbed/topic/508/
///
/// @note Copyright &copy; 2011 by Smartware Computing, all rights reserved.
///     This software may be used to derive new software, as long as
///     this copyright statement remains in the source file.
/// @author David Smart
///
/// @note Copyright &copy; 2015 by NBRemond, all rights reserved.
///     This software may be used to derive new software, as long as
///     this copyright statement remains in the source file.
///
///     Added support for STM32 Nucleo platforms
///
/// @author Bernaérd Remond
///


#include "mbed.h"
#include "Watchdog.h"
#include "settings.h"

IWDG_HandleTypeDef IwdgHandle;
static void Error_Handler(void);
extern Serial BT;

/// Watchdog gets instantiated at the module level
Watchdog::Watchdog() {
  // capture the cause of the previous reset
  /* Check if the system has resumed from IWDG reset */
  /*
      if (__HAL_RCC_GET_FLAG(RCC_FLAG_IWDGRST)) {
          wdreset = true;
      }
      else {
          wdreset = false;
      }
  */
  //wdreset = false;
  //wdreset = (RCC->CSR & (1<<29)) ? true : false;


}

/// Load timeout value in watchdog timer and enable
void Watchdog::Configure(float timeout) {
  // see http://embedded-lab.com/blog/?p=9662
#define LsiFreq (40000)

  if (__HAL_RCC_GET_FLAG(RCC_FLAG_IWDGRST) != RESET) {
    wdreset = true;
    __HAL_RCC_CLEAR_RESET_FLAGS();
  } else {
    wdreset = false;
  }

  uint16_t PrescalerCode;
  uint16_t Prescaler;
  uint16_t ReloadValue;
  float Calculated_timeout;

  if ((timeout * (LsiFreq/4)) < 0x7FF) {
    PrescalerCode = IWDG_PRESCALER_4;
    Prescaler = 4;
  } else if ((timeout * (LsiFreq/8)) < 0xFF0) {
    PrescalerCode = IWDG_PRESCALER_8;
    Prescaler = 8;
  } else if ((timeout * (LsiFreq/16)) < 0xFF0) {
    PrescalerCode = IWDG_PRESCALER_16;
    Prescaler = 16;
  } else if ((timeout * (LsiFreq/32)) < 0xFF0) {
    PrescalerCode = IWDG_PRESCALER_32;
    Prescaler = 32;
  } else if ((timeout * (LsiFreq/64)) < 0xFF0) {
    PrescalerCode = IWDG_PRESCALER_64;
    Prescaler = 64;
  } else if ((timeout * (LsiFreq/128)) < 0xFF0) {
    PrescalerCode = IWDG_PRESCALER_128;
    Prescaler = 128;
  } else {
    PrescalerCode = IWDG_PRESCALER_256;
    Prescaler = 256;
  }

  // specifies the IWDG Reload value. This parameter must be a number between 0 and 0x0FFF.
  ReloadValue = (uint32_t)(timeout * (LsiFreq/Prescaler));

  Calculated_timeout = ((float)(Prescaler * ReloadValue)) / LsiFreq;
  printf("WATCHDOG set with prescaler:%d reload value: 0x%X - timeout:%f\n",Prescaler, ReloadValue, Calculated_timeout);

  /**
  Original implementation
  IWDG->KR = 0x5555; //Disable write protection of IWDG registers
  IWDG->PR = PrescalerCode;      //Set PR value
  IWDG->RLR = ReloadValue;      //Set RLR value
  IWDG->KR = 0xAAAA;    //Reload IWDG
  IWDG->KR = 0xCCCC;    //Start IWDG - See more at: http://embedded-lab.com/blog/?p=9662

  Service();
  */

  IwdgHandle.Instance = IWDG;
  IwdgHandle.Init.Prescaler = PrescalerCode;
  IwdgHandle.Init.Reload = ReloadValue;

  if (HAL_IWDG_Init(&IwdgHandle) != HAL_OK) {
    /* Initialization Error */
    Error_Handler();
  }

  /*##-4- Start the IWDG #####################################################*/
  if (HAL_IWDG_Start(&IwdgHandle) != HAL_OK) {
    Error_Handler();
  }
}

/// "Service", "kick" or "feed" the dog - reset the watchdog timer
/// by writing this required bit pattern
void Watchdog::Service() {
  /**
  Original implementation
  IWDG->KR = 0xAAAA;         //Reload IWDG - See more at: http://embedded-lab.com/blog/?p=9662#sthash.6VNxVSn0.dpuf
  */
#if WATCHDOG_ENABLED
  if (HAL_IWDG_Refresh(&IwdgHandle) != HAL_OK) {
    /* Refresh Error */
    Error_Handler();
  }
#endif
}

/// get the flag to indicate if the watchdog causes the reset
bool Watchdog::WatchdogCausedReset() {
  return wdreset;

  //if (wdreset) {
  //    RCC->CSR |= (1<<24); // clear reset flag
  //}
  //return wdreset;
}


static void Error_Handler(void) {
  /* Turn LED3 on */
  //DigitalOut led(LED1);
  //led = 1;
  BT.printf("IWDG Error");
  while (1) {
  }
}
