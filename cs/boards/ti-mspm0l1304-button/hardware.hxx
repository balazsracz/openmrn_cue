#ifndef _HARDWARE_HXX_
#define _HARDWARE_HXX_

#include "freertos_drivers/ti/MSPM0GPIO.hxx"
#include "utils/GpioInitializer.hxx"


#include <ti/devices/msp/msp.h>



static const auto SIG_UART = UART0;

static const auto USEC_TIMER = TIMG1;

GPIO_PIN(BUTTON, GpioInputPU, 25, A, 24); 

typedef GpioInitializer<BUTTON_Pin> GpioInit;

#endif // _HARDWARE_HXX_
