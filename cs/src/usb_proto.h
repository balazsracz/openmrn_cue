#ifndef _TRAIN2_USB_PROTO_H
#define _TRAIN2_USB_PROTO_H

#define CMD_RPCLOG 0x11
#define CMD_TICK 0x12

#define CMD_SYNC_LEN 15
#define CMD_SYNC 14
#define CMD_SYSLOG 0x10

#define CMD_PING 0x13
#define CMD_PONG 0x14

// Throws away all state in the USB microcontroller.
#define CMD_CLEAR_STATE 0x15

// Calls a OneWire target via RPC. next bytes: address, command, arg1, arg2.
#define CMD_RPC 0x16
// Response from a CMD_RPC call. one byte payload: the return value.
#define CMD_RPC_RESPONSE 0x16
// One byte argument: the new address to set to. Acks by sending current
// address. If called without argument, just returns current address.
#define CMD_GET_OR_SET_ADDRESS 0x17
#define CMD_CURRENT_ADDRESS 0x17

// In the following offset means: high 3 bits is client id, low 5 bits is
// offset within client.
// Arguments: offset, bits_to_set, bits_to_clear
#define CMD_BIT_MODIFY_STATE 0x18
// Arguments: offset, new value.
#define CMD_MOD_STATE 0x19

// Packet to/from the arduino, containing a CAN frame.
#define CMD_CAN_PKT 0x1a
// Clear-text log data coming from the arduino.
#define CMD_CAN_LOG 0x1b

// Arguments: offset, length.
#define CMD_GET_STATE 0x1c
// data: offset, then bytes.
#define CMD_STATE 0x1d

// data: offset, then bytes.
#define CMD_I2C_IRQ_LOG 0x1e

// data: command, arg...
// Returns CMD_UMISC, command, status (00==success), retval
#define CMD_UMISC 0x1f

#define CMD_CAN_SENT 0x20

// Random logging messages that jkust need to be dumped to the screen.
#define CMD_UNKNOWN 0x21

// Next track packet that to be sent.
#define CMD_DCCLOG 0x22
// Virtual COM port 0. Bytes in this packet will be forwarded to the
// virtual serial port.
#define CMD_VCOM0 0x23



#define CMDUM_POWERON 0x01
#define CMDUM_POWEROFF 0x02
#define CMDUM_GETPOWER 0x03
#define CMDUM_GETCURRENT 0x04
#define CMDUM_MEASUREREF 0x05
#define CMDUM_MEASURECUR 0x06
#define CMDUM_CURRENTSENSE_TICK_LIMIT 0x07
#define CMDUM_CURRENTSENSE_THRESHOLD 0x08
#define CMDUM_CURRENTSENSE_THRESHOLD_2 0x0e
#define CMDUM_GETCANSTATE 0x09
#define CMDUM_CANSETREG 0x0a
#define CMDUM_CANGETREG 0x0b
#define CMDUM_CANGETSTATUS 0x0c
#define CMDUM_CANREINIT 0x0d
// 0x0e used
#define CMDUM_FLASHBUF 0x0f   // args: offset (1 byte), then bytes.
#define CMDUM_FLASHERASE 0x10 // args: pointer (4 bytes little endian)
#define CMDUM_FLASHWRITE 0x11 // args: pointer (4 bytes little endian). Returns
                              // status 1 if there is a check failure after
                              // write.
#define CMDUM_FLASHCHECK 0x12 // args: pointer (4 bytes little endian), then
                              // length (4 bytes little endian), returns 4
                              // bytes of CRC32 for the given region.
#define CMDUM_FLASHBUFCHECK 0x13   // args: none. returns crc32 of the flash
                                   // buffer's 64 bytes.
#define CMDUM_FLASHINT 0x14   // args: pointer. Erases block, then writes
                              // buffer, then resets the chip. Never returns.
#define CMDUM_RESET 0x15
#define CMDUM_REBOOTFLASH 0x16  // runs the bootloader.
#define CMDUM_LOGI2C 0x17     // Turns on i2c irq logging.
#define CMDUM_NOLOGI2C 0x18   // Turns off i2c irq logging.
#define CMDUM_SETTRAINAT 0x19   // arg1: train position, arg2: train id.
#define CMDUM_GETTRAINAT 0x1a   // arg1: train position
#define CMDUM_SETRELSPEED 0x1b   // arg1: train id, arg2: rel speed.
#define CMDUM_GETRELSPEED 0x1c   // arg1: train id, arg2: rel speed.
#define CMDUM_WIISTATUS 0x1d   // arg1: X, arg2: Y.
#define CMDUM_WIIINIT 0x1e   // inits the wii. returns one byte: init status
#define CMDUM_WIIMEASURE 0x1f   // polls the wii. byte 4 = measure status, byte 5=read status, byte 6-7: X/Y axis value.
#define CMDUM_WII_SET_DELAY 0x20   // sets the delay between measure and read of the wii. arg 1 is the delay in units of 256*16*4 clock cycles.
#define CMDUM_WII_SET_PARAM 0x21   // arg1: decay multiplier, arg2 decay shift; arg3: downscale shift.
#define CMDUM_WII_START_LOG 0x22
#define CMDUM_WII_STOP_LOG 0x23
#define CMDUM_GET_LOCK_INFO 0x24
#define CMDUM_REL_ALL_LOCK 0x25

#define LOG_RECV_ERR 1
#define LOG_RECV_FERR 2
#define LOG_RECV_OERR 3
#define LOG_PKT_OVERFLOW 4
#define LOG_USB_PKT_TOO_LONG 5
#define LOG_USB_PKT_RECEIVED 6
#define LOG_ILLEGAL_ARGUMENT 7
#define LOG_STATE_CLEARED 8
#define LOG_PKT_CRC_ERROR 9
#define LOG_RECV_ERROR_UNKNOWN_ADDRESS 0x0a
#define LOG_RECV_CRCERROR 0x13

#define LOG_SHUTDOWN_OVERCURRENT 0x0b
#define LOG_SHUTDOWN_SHORT 0x0c

#define LOG_MMA_INTO_HANDSHAKE 0x0d
#define LOG_MMA_HS_SENT 0x0e
#define LOG_MMA_INTO_KEEPALIVE 0x0f

#define LOG_CAN_DEADLOCK 0x10

#define LOG_UNLOCK_WITH_WRONG_ID 0x11
#define LOG_SHA_CHANGE 0x12

// log values from 0x50 come from the canbus track controller slave

// We turned off power to the track for some reason.
#define CANLOG_POWEROFF 0x50
// We did not receive keepalive from the master.
#define CANLOG_KEEPALIVE_TIMEOUT 0x51
// An invalid command was received over can.
#define CANLOG_INVALID 0x52
// A packet was received but there was no free buffer to put it into. Master
// did not keep protocol.
#define CANLOG_OVERFLOW 0x53
// Short on the track.
#define CANLOG_SHORT 0x54



// Error codes from DCC service mode
#define DCC_SUCCESS 0
#define DCC_ERR_NOACK 0x80   // Did not receive ACK from decoder.
#define DCC_ERR_ID 0x81      // Unknown train ID.
#define DCC_ERR_SERVICE_NOT_DCC 0x82  // Service mode requested for non-dcc
                                      // engine.


#endif
