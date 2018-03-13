#ifndef _HARDWARE_HXX_
#define _HARDWARE_HXX_

#include "TivaGPIO.hxx"
#include "DummyGPIO.hxx"
#include "BlinkerGPIO.hxx"
#include "driverlib/rom_map.h"
#include "utils/GpioInitializer.hxx"

#define TIVADCC_TIVA

GPIO_PIN(SW1, GpioInputPU, F, 4);
GPIO_PIN(SW2, GpioInputPU, F, 0);

GPIO_PIN(LED_RED, LedPin, F, 1);
GPIO_PIN(LED_GREEN, LedPin, F, 3);
GPIO_PIN(LED_BLUE, LedPin, F, 2);

GPIO_HWPIN(UART0RX, GpioHwPin, A, 0, U0RX, UART);
GPIO_HWPIN(UART0TX, GpioHwPin, A, 1, U0TX, UART);

GPIO_PIN(USB1, GpioUSBAPin, D, 4);
GPIO_PIN(USB2, GpioUSBAPin, D, 5);

//GPIO_HWPIN(CAN0RX, GpioHwPin, E, 4, CAN0RX, CAN);
//GPIO_HWPIN(CAN0TX, GpioHwPin, E, 5, CAN0TX, CAN);

GPIO_HWPIN(SERVO, GpioHwPin, C, 4, WT0CCP0, Timer);

#define FAKEHW

#ifndef FAKEHW

GPIO_PIN(RLED_RAW, LedPin, B, 5);
GPIO_PIN(RSET, GpioInputPU, B, 0);

GPIO_PIN(RB1, GpioInputPU, E, 4);
GPIO_PIN(RB2, GpioInputPU, B, 4);
GPIO_PIN(RB3, GpioInputPU, A, 6);
GPIO_PIN(RB4, GpioInputPU, A, 7);
GPIO_PIN(RB5, GpioInputPU, B, 1);
GPIO_PIN(RB6, GpioInputPU, E, 5);
GPIO_PIN(RB7, GpioInputPU, A, 5);
#else

typedef LED_RED_Pin RLED_RAW_Pin;
//GPIO_PIN(RLED_RAW, LedPin, B, 5);
GPIO_PIN(RSET, GpioInputPU, B, 0);

GPIO_PIN(RB1, GpioInputPU, B, 1);
GPIO_PIN(RB2, GpioInputPU, E, 4);
GPIO_PIN(RB3, GpioInputPU, E, 5);
GPIO_PIN(RB4, GpioInputPU, B, 4);
GPIO_PIN(RB5, GpioInputPU, A, 5);
GPIO_PIN(RB6, GpioInputPU, A, 6);
GPIO_PIN(RB7, GpioInputPU, A, 7);

#endif

typedef RLED_RAW_Pin BLINKER_RAW_Pin;
typedef BLINKER_Pin RLED_Pin;

typedef GpioInitializer<                       //
    SW1_Pin, SW2_Pin,                          //
    LED_RED_Pin, LED_GREEN_Pin, LED_BLUE_Pin,  //
    USB1_Pin, USB2_Pin,                        //
    UART0RX_Pin, UART0TX_Pin,                  //
    RLED_RAW_Pin, RSET_Pin,                    //
    RB1_Pin, RB2_Pin, RB3_Pin, RB4_Pin,        //
    RB5_Pin, RB6_Pin, RB7_Pin,                 //
    SERVO_Pin,                                 //
                //    CAN0RX_Pin, CAN0TX_Pin,
    DummyPin>
    GpioInit;

namespace TDebug {
using Resync = DummyPin;
using NextPacket = DummyPin;
};


#endif // _HARDWARE_HXX_
