
#ifndef _BRACZ_CUSTOM_DCC_CONTROL_HXX_
#define _BRACZ_CUSTOM_DCC_CONTROL_HXX_

extern "C" {
  /** Turns on DCC track output power. */
extern void enable_dcc();
  /** Turns off DCC track output power. */
extern void disable_dcc();
  /** @returns whether dcc track power is enabled or not. */
extern bool query_dcc();
}

#endif
