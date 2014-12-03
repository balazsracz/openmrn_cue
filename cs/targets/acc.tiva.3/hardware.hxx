#ifndef _ACC_TIVA_3_HARDWARE_HXX_
#define _ACC_TIVA_3_HARDWARE_HXX_

#include "TivaGPIO.hxx"

#include "inc/hw_memmap.h"
#include "inc/hw_ints.h"
#include "driverlib/timer.h"

#define LED_GREEN GPIO_PORTD_BASE, GPIO_PIN_5
#define LED_BLUE GPIO_PORTG_BASE, GPIO_PIN_1
#define LED_YELLOW GPIO_PORTB_BASE, GPIO_PIN_0

struct DCCDecode
{
    static const auto TIMER_BASE = WTIMER4_BASE;
    static const auto TIMER_PERIPH = SYSCTL_PERIPH_WTIMER4;
    static const auto TIMER_INTERRUPT = INT_WTIMER4A;
    static const auto TIMER = TIMER_A;
    static const auto CFG_CAP_TIME_UP =
        TIMER_CFG_SPLIT_PAIR | TIMER_CFG_A_CAP_TIME_UP | TIMER_CFG_B_ONE_SHOT;
    // Interrupt bits.
    static const auto TIMER_CAP_EVENT = TIMER_CAPA_EVENT;
    static const auto TIMER_TIM_TIMEOUT = TIMER_TIMA_TIMEOUT;

    static const auto OS_INTERRUPT = INT_WTIMER4B;
    DECL_HWPIN(NRZPIN, D, 4, WT4CCP0);

    static const uint32_t TIMER_MAX_VALUE = 0x8000000UL;
    static const uint32_t PS_MAX = 0;

    static const int Q_SIZE = 32;
};

#endif // _ACC_TIVA_3_HARDWARE_HXX_
