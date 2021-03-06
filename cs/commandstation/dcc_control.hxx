
#ifndef _BRACZ_COMMANDSTATION_DCC_CONTROL_HXX_
#define _BRACZ_COMMANDSTATION_DCC_CONTROL_HXX_

extern "C" {
/** Turns on DCC track output power. */
extern void enable_dcc();
/** Sets the track output to a 10% fill rate for output shorted condition. */
extern void setshorted_dcc();
/** Turns off DCC track output power. */
extern void disable_dcc();
/** @returns whether dcc track power is enabled or not. */
extern bool query_dcc();
}

#endif
