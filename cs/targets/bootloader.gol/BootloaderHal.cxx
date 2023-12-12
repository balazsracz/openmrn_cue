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

GPIO_PIN(GLB, GpioInputPU, B, 9);

GPIO_PIN(LED_D15, GpioOutputSafeLow, B, 14);
GPIO_PIN(LED_D16, GpioOutputSafeLow, B, 13);


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
  // 3. flashing is incomplete, value is all FF's
  //
  // If the app was flashed directly the bootloader will not start at
  // startup. Therefore once we are here, the bootloader was jumped to by hand.
  return (p[13] != 0 && p[13] != 0xffffffffu && p[13] != b[1]);
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


TSC_HandleTypeDef TscHandle;
TSC_IOConfigTypeDef IoConfig;


/* Array used to store the three acquisition value (one per channel) */
__IO uint32_t uhTSCAcquisitionValue[3];

struct AcquisitionPhase {
  // Bitmask of which channel IOs to capture.
  uint32_t channel_ios;
  // Group2 channel: row or col.
  uint8_t group2_rc;
  // Group2 channel: which entry.
  uint8_t group2_num;
  // Group3 channel: row or col.
  uint8_t group3_rc;
  // Group3 channel: which entry.
  uint8_t group3_num;
  // Group5 channel: row or col.
  uint8_t group5_rc;
  // Group5 channel: which entry.
  uint8_t group5_num;
};

static constexpr unsigned kNumPhases = 3;

// Use this constant in the first index of channel_results[][] to see the output for rows.
static constexpr unsigned kChRow = 0;
// Use this constant in the first index of channel_results[][] to see the output for columns.
static constexpr unsigned kChCol = 1;


static const AcquisitionPhase kPhases[kNumPhases] = {
  { TSC_GROUP2_IO2 | TSC_GROUP3_IO3 | TSC_GROUP5_IO2, kChCol, 0, kChRow, 1, kChCol, 2 },
  { TSC_GROUP2_IO3 | TSC_GROUP3_IO4 | TSC_GROUP5_IO3, kChRow, 2, kChCol, 1, kChCol, 3 },
  { TSC_GROUP2_IO4 | TSC_GROUP3_IO3 | TSC_GROUP5_IO4, kChRow, 0, kChRow, 1, kChRow, 3 }
};


void Error_Handler(volatile unsigned line) {
  while(1) {
    (void)line;
    LED_GREEN_Pin::set(true);
    LED_OTHER_Pin::set(false);
    for (volatile unsigned i = 0; i < 200000; i++);
    LED_GREEN_Pin::set(false);
    LED_OTHER_Pin::set(true);
    for (volatile unsigned i = 0; i < 200000; i++);
  }
  (void)line;
}

void touch_io_start(unsigned phase) {
  if (phase >= kNumPhases) {
    phase = 0;
  }
  /*##-2- Configure the touch-sensing IOs ####################################*/
  IoConfig.ChannelIOs = kPhases[phase].channel_ios;
  IoConfig.SamplingIOs = TSC_GROUP2_IO1 | TSC_GROUP3_IO2 | TSC_GROUP5_IO1;
  IoConfig.ShieldIOs = 0;

  if (HAL_TSC_IOConfig(&TscHandle, &IoConfig) != HAL_OK) {
    /* Initialization Error */
    Error_Handler(__LINE__);
  }
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

    bootloader_reset_segments();
    
    /* enable peripheral clocks */
    __HAL_RCC_GPIOA_CLK_ENABLE();
    __HAL_RCC_GPIOB_CLK_ENABLE();
    __HAL_RCC_GPIOC_CLK_ENABLE();
    __HAL_RCC_GPIOF_CLK_ENABLE();
    __HAL_RCC_CAN1_CLK_ENABLE();
    __HAL_RCC_TSC_CLK_ENABLE();
    __HAL_RCC_TSC_FORCE_RESET();
    for (volatile unsigned i = 0; i < 100; i++);
    __HAL_RCC_TSC_RELEASE_RESET();

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

    /*##-2- Configure Sampling Capacitor IOs (Alternate-Function Open-Drain)
     * ###*/
    gpio_init.Mode = GPIO_MODE_AF_OD;
    gpio_init.Pull = GPIO_NOPULL;
    gpio_init.Speed = GPIO_SPEED_FREQ_LOW;

    gpio_init.Alternate = GPIO_AF3_TSC;
    // caps are A4 B0 B3
    gpio_init.Pin = GPIO_PIN_4;
    HAL_GPIO_Init(GPIOA, &gpio_init);
    gpio_init.Pin = GPIO_PIN_0;
    HAL_GPIO_Init(GPIOB, &gpio_init);
    gpio_init.Pin = GPIO_PIN_3;
    HAL_GPIO_Init(GPIOB, &gpio_init);

    /*##-3- Configure Channel IOs (Alternate-Function Output PP)
     * ###############*/
    gpio_init.Mode = GPIO_MODE_AF_PP;
    gpio_init.Pull = GPIO_NOPULL;
    gpio_init.Speed = GPIO_SPEED_FREQ_LOW;

    // A 5-6-7
    gpio_init.Pin = GPIO_PIN_5;  // group2 io2
    HAL_GPIO_Init(GPIOA, &gpio_init);
    gpio_init.Pin = GPIO_PIN_6;  // group2 io3
    HAL_GPIO_Init(GPIOA, &gpio_init);
    gpio_init.Pin = GPIO_PIN_7;  // group2 io4
    HAL_GPIO_Init(GPIOA, &gpio_init);

    // B 1-2 4-5-6
    gpio_init.Pin = GPIO_PIN_1;  // group3 io3
    HAL_GPIO_Init(GPIOB, &gpio_init);
    gpio_init.Pin = GPIO_PIN_2;  // group3 io4
    HAL_GPIO_Init(GPIOB, &gpio_init);
    gpio_init.Pin = GPIO_PIN_4;  // group5 io2
    HAL_GPIO_Init(GPIOB, &gpio_init);
    gpio_init.Pin = GPIO_PIN_6;  // group5 io3
    HAL_GPIO_Init(GPIOB, &gpio_init);
    // B5 is incorrectly wired to a TSC circuit. B7 is a spare pin.
    gpio_init.Pin = GPIO_PIN_7;  // group5 io4
    HAL_GPIO_Init(GPIOB, &gpio_init);

    LED_GREEN_Pin::hw_init();
    LED_OTHER_Pin::hw_init();
    LED_D15_Pin::hw_init();
    LED_D16_Pin::hw_init();
    SW1_Pin::hw_init();

    memset(&TscHandle, 0, sizeof(TscHandle));
    memset(&IoConfig, 0, sizeof(IoConfig));

    // Configures TSC peripheral
    TscHandle.Instance = TSC;
    TscHandle.Init.AcquisitionMode = TSC_ACQ_MODE_NORMAL;
    TscHandle.Init.CTPulseHighLength = TSC_CTPH_1CYCLE;
    TscHandle.Init.CTPulseLowLength = TSC_CTPL_1CYCLE;
    TscHandle.Init.IODefaultMode =
        TSC_IODEF_IN_FLOAT; /* Because the electrodes are interlaced on this
                               board */
    TscHandle.Init.MaxCountInterrupt = ENABLE;
    TscHandle.Init.MaxCountValue = TSC_MCV_16383;
    TscHandle.Init.PulseGeneratorPrescaler = TSC_PG_PRESC_DIV64;
    TscHandle.Init.SpreadSpectrum = DISABLE;
    TscHandle.Init.SpreadSpectrumDeviation = 127;
    TscHandle.Init.SpreadSpectrumPrescaler = TSC_SS_PRESC_DIV1;
    TscHandle.Init.SynchroPinPolarity = TSC_SYNC_POLARITY_FALLING;
    TscHandle.Init.ChannelIOs =
        0; /* Not needed yet. Will be set with HAL_TSC_IOConfig() */
    TscHandle.Init.SamplingIOs =
        0; /* Not needed yet. Will be set with HAL_TSC_IOConfig() */
    TscHandle.Init.ShieldIOs =
        0; /* Not needed yet. Will be set with HAL_TSC_IOConfig() */

    if (HAL_TSC_Init(&TscHandle) != HAL_OK) {
      /* Initialization Error */
      Error_Handler(__LINE__);
    }

    // B5 we switch to analog mode.
    gpio_init.Mode = GPIO_MODE_ANALOG;
    gpio_init.Pull = GPIO_NOPULL;
    gpio_init.Speed = GPIO_SPEED_FREQ_LOW;
    gpio_init.Pin = GPIO_PIN_5;
    HAL_GPIO_Init(GPIOB, &gpio_init);

    HAL_TSC_IODischarge(&TscHandle, ENABLE);
    touch_io_start(2);

    // Configures CAN peripheral
    
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

    extern char __flash_start;
    // Check if the bootloader reset vector is in the right place.
    uint32_t* p = (uint32_t*) &__flash_start;
    extern void reset_handler(void);
    uint32_t bootdata = reinterpret_cast<uint32_t>(&reset_handler);
    if (p[1] != bootdata) {
      // need to rewrite the first sector.
      memcpy(g_write_buffer, p, 0x800);
      erase_flash_page(p);
      write_flash(p, g_write_buffer, 0x800);
      flash_complete();
    }
}

void bootloader_led(enum BootloaderLed id, bool value) {
  switch (id) {
    case LED_ACTIVE:
      LED_GREEN_Pin::set(value);
      return;
    case LED_WRITING:
      LED_OTHER_Pin::set(value);
      return;
    case LED_CSUM_ERROR:
      if (value) {
        LED_D15_Pin::set(1);
        LED_D16_Pin::set(0);
      }
      return;
    case LED_REQUEST:
      if (value) {
        LED_D16_Pin::set(1);
        LED_D15_Pin::set(0);
      }
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

    bool pressed = true;
    
    HAL_TSC_IODischarge(&TscHandle, DISABLE);
    if (HAL_TSC_Start(&TscHandle) != HAL_OK) {
      /* Acquisition Error */
      Error_Handler(__LINE__);
    }
    HAL_TSC_PollForAcquisition(&TscHandle);
    if (HAL_TSC_GroupGetStatus(&TscHandle, TSC_GROUP2_IDX) ==
        TSC_GROUP_COMPLETED) {
      unsigned res = HAL_TSC_GroupGetValue(&TscHandle, TSC_GROUP2_IDX);
      // Calibration 0x2e0 (736), below 85% it is pressed.
      if (res > 625) pressed = false;
    } else {
      Error_Handler(__LINE__);
    }
    touch_io_start(1);
    HAL_TSC_IODischarge(&TscHandle, ENABLE);
    for (volatile unsigned i = 0; i < 200000; i++);

    HAL_TSC_IODischarge(&TscHandle, DISABLE);
    if (HAL_TSC_Start(&TscHandle) != HAL_OK) {
      /* Acquisition Error */
      Error_Handler(__LINE__);
    }
    HAL_TSC_PollForAcquisition(&TscHandle);
    if (HAL_TSC_GroupGetStatus(&TscHandle, TSC_GROUP2_IDX) ==
        TSC_GROUP_COMPLETED) {
      unsigned res = HAL_TSC_GroupGetValue(&TscHandle, TSC_GROUP2_IDX);
      // Calibration 0x3e0 (992), below 85% it is pressed.
      if (res > 843) pressed = false;
    } else {
      Error_Handler(__LINE__);
    }

    if (!SW1_Pin::get()) {
      pressed = true;
    }

    if (!pressed && !GLB_Pin::get()) {
      // waits for global pin. If the CAN-bus is transmitting , there will be a
      // recessive bit every 10 bits or so. This waits for 12 to 30 bits long.
      pressed = true;
      for (unsigned i = 0; i < 7000; ++i) {
        if (GLB_Pin::get()) {
          pressed = false;
          break;
        }
      }
    }
    // if pressed or the pin is low, forces the bootloader.
    if (pressed) {
      LED_GREEN_Pin::set(1);
      LED_OTHER_Pin::set(1);
      LED_D16_Pin::set(1);
      LED_D15_Pin::set(0);
      return true;
    } else {
      LED_GREEN_Pin::set(0);
      LED_OTHER_Pin::set(0);
      return false;
    }
}

} // extern "C"
