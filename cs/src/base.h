#ifndef _BRACZ_TRAIN_BASE_H
#define _BRACZ_TRAIN_BASE_H

// Save offsets.
#define LEN_CLIENT 32

#define OFS_AUTOMATA 0
#define OFS_PORT 24
#define OFS_GLOBAL_BITS OFS_PORT + 6
#define OFS_GLOBAL2_BITS OFS_PORT + 4
#define OFS_IOA OFS_PORT + 0
#define OFS_IOB OFS_PORT + 2

#define LEN_AUTOMATA 3

#define OFS_STATE 0
#define OFS_TIMER 1
#define OFS_BITS 2


// ==== Signal handler flow definitions
#define CMD_SIGNALPACKET 0x10   // arg1... the signal packet (arg1=address arg2=len arg3... = payload. the payload has to be one less bytes than len). Maximum length value is 12.



#endif  // _BRACZ_TRAIN_BASE_H
