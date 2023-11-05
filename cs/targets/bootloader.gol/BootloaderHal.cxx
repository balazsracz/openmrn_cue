#include <string.h>

#define BOOTLOADER_STREAM
//#define BOOTLOADER_DATAGRAM
#define WRITE_BUFFER_SIZE 2048

#define _OPENLCB_APPLICATIONCHECKSUM_HXX_
extern "C" bool check_application_checksum();

#define BOOTLOADER_LOOP_HOOK custom_bload_hook
extern "C" void custom_bload_hook();

#include "BootloaderHal.hxx"
#include "bootloader_hal.h"

#include "nmranet_config.h"
#include "openlcb/Defs.hxx"
#include "Stm32Gpio.hxx"
#include "openlcb/Bootloader.hxx"
#include "openlcb/If.hxx"
#include "utils/GpioInitializer.hxx"
#include "freertos_drivers/common/GpioWrapper.hxx"

const uint8_t AHBPrescTable[16] = {0, 0, 0, 0, 0, 0, 0, 0, 1, 2, 3, 4, 6, 7, 8, 9};
const uint8_t APBPrescTable[8]  = {0, 0, 0, 0, 1, 2, 3, 4};
const uint32_t HSEValue = 8000000;

int g_death_lineno = 0;

extern "C" {

GPIO_PIN(LED_GREEN_RAW, LedPin, F, 0);
using LED_GREEN_Pin = ::InvertedGpio<LED_GREEN_RAW_Pin>;
GPIO_PIN(LED_OTHER_RAW, LedPin, F, 1);
using LED_OTHER_Pin = ::InvertedGpio<LED_OTHER_RAW_Pin>;
GPIO_PIN(SW1, GpioInputPU, B, 15);

static constexpr unsigned clock_hz = 48000000;

// Replaces checksum mechanism in the bootloader. @return true if there seems
// to be an application loaded.
bool check_application_checksum() {
  extern char __flash_start;
  extern char __bootloader_start;
  uint32_t* p = (uint32_t*) &__flash_start;
  uint32_t* b = (uint32_t*) &__bootloader_start;
  // The alt reset vector at ofs 13 should be the app entry. There are two cases:
  //
  // 1. the app was flashed directly. p[1] != p[13](==0) != b[1].
  // 2. the app was flashed by the bootloader. p[13] != p[1] == b[1].
  //
  // If the app was flashed directly the bootloader will not start at
  // startup. Therefore once we are here, the bootloader was jumped to by hand.
  return (p[13] != 0 && p[13] != b[1]);
}

void custom_bload_hook() {
  if (!state_.input_frame_full || !IS_CAN_FRAME_EFF(state_.input_frame))
  {
    return;
  }
  uint32_t can_id = GET_CAN_FRAME_ID_EFF(state_.input_frame);
  if ((can_id >> 12) != 0x195B4) {
    return;
  }
  const uint8_t kPrefix[] = {0x09, 0x00, 0x0D, 0xF9};
  const uint8_t kStart[] = {0x09, 0x00, 0x0D, 0xFF, 0, 0, 0, 15};
  const uint8_t kFinish[] = {0x09, 0x00, 0x0D, 0xFF, 0, 0, 0, 16};
  const uint8_t kResponse[] = {0x09, 0x00, 0x0D, 0xFF, 0, 0, 0, 17};
  if (memcmp(&state_.input_frame.data[0], kPrefix, 4) == 0) {
    // Got a firmware upgrade data event.
    memcpy(&g_write_buffer[state_.write_buffer_index],
        &state_.input_frame.data[4], 4);
    state_.write_buffer_index += 4;
    if (state_.write_buffer_index >= WRITE_BUFFER_SIZE)
    {
        flush_flash_buffer();
        set_can_frame_global(Defs::MTI_EVENT_REPORT);
        state_.output_frame.can_dlc = 8;
        memcpy(state_.output_frame.data, kResponse, 8);
    }
  } else if (memcmp(&state_.input_frame.data[0], kFinish, 8) == 0) {
    // Got a firmware upgrade finish.
    if (state_.write_buffer_index > 0)
    {
        flush_flash_buffer();
    }
    state_.request_reset = 1;
  } else if (memcmp(&state_.input_frame.data[0], kStart, 8) == 0) {
    // Got a firmware upgrade start.
    state_.write_buffer_offset = 0;
    normalize_write_buffer_offset();
    state_.write_buffer_index = 0;
  }
}

void bootloader_hw_set_to_safe(void)
{
    /* enable peripheral clocks */
    __HAL_RCC_GPIOA_CLK_ENABLE();
    __HAL_RCC_GPIOB_CLK_ENABLE();
    __HAL_RCC_GPIOC_CLK_ENABLE();
    __HAL_RCC_GPIOF_CLK_ENABLE();
    SW1_Pin::hw_set_to_safe();
    LED_GREEN_Pin::hw_set_to_safe();
    LED_OTHER_Pin::hw_set_to_safe();
}

extern void bootloader_reset_segments(void);
//extern unsigned long cm3_cpu_clock_hz;

/** Setup the system clock */
static void clock_setup(void)
{
    /* reset clock configuration to default state */
    RCC->CR = RCC_CR_HSITRIM_4 | RCC_CR_HSION;
    while (!(RCC->CR & RCC_CR_HSIRDY))
        ;

#define USE_EXTERNAL_8_MHz_CLOCK_SOURCE 0
/* configure PLL:  8 MHz * 6 = 48 MHz */
#if USE_EXTERNAL_8_MHz_CLOCK_SOURCE
    RCC->CR |= RCC_CR_HSEON | RCC_CR_HSEBYP;
    while (!(RCC->CR & RCC_CR_HSERDY))
        ;
    RCC->CFGR = RCC_CFGR_PLLMUL6 | RCC_CFGR_PLLSRC_HSE_PREDIV | RCC_CFGR_SW_HSE;
    while (!((RCC->CFGR & RCC_CFGR_SWS) == RCC_CFGR_SWS_HSE))
        ;
#else
    RCC->CFGR = RCC_CFGR_PLLMUL6 | RCC_CFGR_PLLSRC_HSI_PREDIV | RCC_CFGR_SW_HSI;
    while (!((RCC->CFGR & RCC_CFGR_SWS) == RCC_CFGR_SWS_HSI))
        ;
#endif
    /* enable PLL */
    RCC->CR |= RCC_CR_PLLON;
    while (!(RCC->CR & RCC_CR_PLLRDY))
        ;

    /* set PLL as system clock */
    RCC->CFGR = (RCC->CFGR & (~RCC_CFGR_SW)) | RCC_CFGR_SW_PLL;
    while (!((RCC->CFGR & RCC_CFGR_SWS) == RCC_CFGR_SWS_PLL))
        ;
}

void bootloader_hw_init()
{
    /* Globally disables interrupts until the FreeRTOS scheduler is up. */
    asm("cpsid i\n");

    /* these FLASH settings enable opertion at 48 MHz */
    __HAL_FLASH_PREFETCH_BUFFER_ENABLE();
    __HAL_FLASH_SET_LATENCY(FLASH_LATENCY_1);

    /* Reset HSI14 bit */
    RCC->CR2 &= (uint32_t)0xFFFFFFFEU;

    /* Disable all interrupts */
    RCC->CIR = 0x00000000U;
    NVIC->ICER[0] = 0xFFFFFFFFu;

    clock_setup();

    /* enable peripheral clocks */
    __HAL_RCC_GPIOA_CLK_ENABLE();
    __HAL_RCC_GPIOB_CLK_ENABLE();
    __HAL_RCC_GPIOC_CLK_ENABLE();
    __HAL_RCC_GPIOF_CLK_ENABLE();
    __HAL_RCC_CAN1_CLK_ENABLE();

    /* setup pinmux */
    GPIO_InitTypeDef gpio_init;
    memset(&gpio_init, 0, sizeof(gpio_init));

    /* CAN pinmux on PB8 and PB9 */
    gpio_init.Mode = GPIO_MODE_AF_OD;
    gpio_init.Speed = GPIO_SPEED_FREQ_HIGH;
    gpio_init.Pull = GPIO_PULLUP;
    gpio_init.Alternate = GPIO_AF4_CAN;
    gpio_init.Pin = GPIO_PIN_8;
    HAL_GPIO_Init(GPIOB, &gpio_init);
    gpio_init.Pin = GPIO_PIN_9;
    HAL_GPIO_Init(GPIOB, &gpio_init);

    LED_GREEN_Pin::hw_init();
    SW1_Pin::hw_init();

    /* disable sleep, enter init mode */
    CAN->MCR = CAN_MCR_INRQ;

    /* Time triggered tranmission off
     * Bus off state is left automatically
     * Auto-Wakeup mode disabled
     * automatic re-transmission enabled
     * receive FIFO not locked on overrun
     * TX FIFO mode on
     */
    CAN->MCR |= (CAN_MCR_ABOM | CAN_MCR_TXFP);

    /* Setup timing.
     * 125,000 Kbps = 8 usec/bit
     */
    CAN->BTR = (CAN_BS1_5TQ | CAN_BS2_2TQ | CAN_SJW_1TQ |
                ((clock_hz / 1000000) - 1));

    /* enter normal mode */
    CAN->MCR &= ~CAN_MCR_INRQ;

    /* Enter filter initialization mode.  Filter 0 will be used as a single
     * 32-bit filter, ID Mask Mode, we accept everything, no mask.
     */
    CAN->FMR |= CAN_FMR_FINIT;
    CAN->FM1R = 0;
    CAN->FS1R = 0x000000001;
    CAN->FFA1R = 0;
    CAN->sFilterRegister[0].FR1 = 0;
    CAN->sFilterRegister[0].FR2 = 0;

    /* Activeate filter and exit initialization mode. */
    CAN->FA1R = 0x000000001;
    CAN->FMR &= ~CAN_FMR_FINIT;
}

void bootloader_led(enum BootloaderLed id, bool value)
{
    switch(id)
    {
        case LED_ACTIVE:
            LED_GREEN_Pin::set(value);
            return;
        case LED_WRITING:
            LED_OTHER_Pin::set(value);
            return;
        case LED_CSUM_ERROR:
            return;
        case LED_REQUEST:
            return;
        case LED_FRAME_LOST:
            return;
        default:
            /* ignore */
            break;
    }
}

/// @return true if the bootloader should be started, false if the application
/// check should be performed.
bool request_bootloader()
{
    extern uint32_t __bootloader_magic_ptr;
    if (__bootloader_magic_ptr == REQUEST_BOOTLOADER) {
        __bootloader_magic_ptr = 0;
        LED_GREEN_Pin::set(true);
        return true;
    }
    LED_GREEN_Pin::set(SW1_Pin::get());
    // if the pin is low, forces the bootloader.
    return !SW1_Pin::get();
}

} // extern "C"
