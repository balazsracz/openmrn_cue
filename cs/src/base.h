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
#define CMD_SIGNALPACKET_WITH_ACK 0x12   // arg1,arg2 = timeout msec (big endian) arg3... the signal packet (arg3=address arg4=len arg5... = payload. the payload has to be one less bytes than len). Maximum length value is 12.

#define CMD_SIGNAL_PAUSE 0x18
#define CMD_SIGNAL_RESUME 0x19

// ==== Signal commands
#define SCMD_RESET 0x1
#define SCMD_ASPECT 0x3
#define SCMD_FLASH 0x4  // arguments: ofsl ofsh data... max packet length = 16.
#define SCMD_FLASH_SUM 0xa  // arguments: ofsl ofsh data... csum max packet length = 32. csum is a negative of the sum of all bytes from the command onwards MOD 256.
#define SCMD_CRC 0x6  // argument: ofsl ofsh lenl lenh crcb1 crcb2 crcb3 crcb4 (little-endian)
#define SCMD_LED 0x5  // argument: out bits

#define SCMD_CRCRESULT 0x8  // No argument. ACK-ed if the previous CRC result
                            // was OK.
#define SCMD_PING 0x9  // No argument. ACK-ed always with return pulse.
#define SCMD_INZERO 0x10  // arg: input number. ACK-ed if the given input number is false.
#define SCMD_INONE 0x11  // arg: input number. ACK-ed if the given input number is true.



#endif  // _BRACZ_TRAIN_BASE_H
