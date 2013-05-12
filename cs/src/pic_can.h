#ifndef _BRACZ_PIC_CAN_H_
#define _BRACZ_PIC_CAN_H_


// Offsets into pic-compatible can buffers.
#define CAN_START 2
#define CAN_SIDH 2
#define CAN_SIDL 3
#define CAN_EIDH 4
#define CAN_EIDL 5
#define CAN_LEN  6
#define CAN_D0   7
#define CAN_D1   8
#define CAN_D2   9
#define CAN_D3   10
#define CAN_D4   11
#define CAN_D5   12
#define CAN_D6   13
#define CAN_D7   14

#define SIDMATCH(sid) (can_buf[CAN_SIDH] == (sid >> 8)           \
                       && can_buf[CAN_SIDL] == (sid & 0xff))

#define EIDMATCH(eid) (can_buf[CAN_EIDH] == (eid >> 8)              \
                       && can_buf[CAN_EIDL] == (eid & 0xff))

// EID match except the last bit which gets flip-flopped for different mobile
// stations.
#define EIDMMMATCH(eid) (can_buf[CAN_EIDH] == (eid >> 8)        \
                         && (can_buf[CAN_EIDL] & 0xfe) == (eid & 0xfe))


#define EIDLMATCH(eidl) (can_buf[CAN_EIDL] == eidl)


#define CANMATCH(sid, eid) (SIDMATCH(sid) && EIDMATCH(eid))

#define CANMMATCH(sid, eid) (SIDMATCH(sid) && EIDLMATCH((eid & 0xff)) && \
    ((can_buf[CAN_EIDH] & 0b10000011) == ((eid >> 8) & 0b10000011)))


// Not sure how to implement this.
void CANSetDoNotLogToHost(void);
void CANSetPending(uint8_t destination);
void CANSetNotPending(uint8_t destination);


#define CDST_CANBUS 1
#define CDST_HOST 2
#define CDST_DCCMASTER 4
#define CDST_MOSTACLIENT 8

#ifndef KILL_MOSTA
#define DST_CAN_INCOMING CDST_HOST | CDST_DCCMASTER | CDST_MOSTACLIENT
#else
#define DST_CAN_INCOMING CDST_HOST | CDST_DCCMASTER
#endif

#endif // _BRACZ_PIC_CAN_H_
