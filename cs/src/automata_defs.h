// Common definitions between C and ASM parts of automata library.

#define INSN_OFFSET 0x0800

// 0b0.......
#define _IF_REG 0x00
#define _IF_REG_MASK 0x80


#define _REG_1 0x40

#define _IF_REG_0 _IF_REG
#define _IF_REG_1 _IF_REG | _REG_1

#define _IF_REG_BITNUM_MASK 0b111111


#define _REG_MASK 0b00111000

#define _REG_GLOBAL1  0x18 // 0b00011000
#define _REG_GLOBAL2  0x28 // 0b00101000


// 0b101.....
#define _IF_STATE 0xA0
#define _IF_STATE_MASK 0xE0

//#define _TRY_CALL 0x80
//#define _TRY_CALL_MASK 0xF8

// 0b111.....
#define _GET_LOCK 0xE0
#define _GET_LOCK_MASK 0xE0

#define _IF_LOCK_FREE (_IF_REG_0 | _REG_GLOBAL_U )

#define _ADDR_0 0
#define _ADDR_1 0x08
#define _ADDR_2 0x10
#define _ADDR_3 0x18

// 0b110.....
#define _REL_LOCK 0xC0
#define _REL_LOCK_MASK 0xE0
#define _SET_REMOTE_0 _REL_LOCK
#define _SET_REMOTE_1 _GET_LOCK

// 0b1000....
#define _IF_MISC_BASE 0x80
#define _IF_MISC_MASK 0xF0

#define _IF_EMERGENCY_STOP (_IF_MISC_BASE | 0)
#define _IF_EMERGENCY_START (_IF_MISC_BASE | 1)
#define _IF_CLEAR_TRAIN (_IF_MISC_BASE | 2)
#define _IF_TRAIN_IS_FORWARD (_IF_MISC_BASE | 3)
#define _IF_TRAIN_IS_REVERSE (_IF_MISC_BASE | 4)
#define _SET_TRAIN_FORWARD (_IF_MISC_BASE | 5)
#define _SET_TRAIN_REVERSE (_IF_MISC_BASE | 6)
#define _IF_TRAIN_IS_PUSHPULL (_IF_MISC_BASE | 7)
#define _SYNC_ARG (_IF_MISC_BASE | 8)
#define _IF_TRAIN_IS_NOT_PUSHPULL (_IF_MISC_BASE | 9)


// one-argument misc operations.
// 0b1001....
#define _IF_MISCA_BASE 0x90
#define _IF_MISCA_MASK 0xF0

#define _IF_STOP_TRAIN (_IF_MISCA_BASE | 0)
#define _IF_STOP_TRAIN_AT (_IF_MISCA_BASE | 1)
#define _IF_START_TRAIN (_IF_MISCA_BASE | 2)
#define _IF_START_TRAIN_AT (_IF_MISCA_BASE | 3)
#define _IF_MOVE_TRAIN (_IF_MISCA_BASE | 4)
// gives success if we don't have train id information at a particular place.
#define _IF_UNKNOWN_TRAIN_AT (_IF_MISCA_BASE | 5)

#define _SET_REMOTE__0 (_IF_MISCA_BASE | 6)
#define _SET_REMOTE__1 (_IF_MISCA_BASE | 7)
#define _SET_REMOTE _SET_REMOTE__0

#define _SET_TRAIN_REL_SPEED (_IF_MISCA_BASE | 8)

#define _IF_ASPECT (_IF_MISCA_BASE | 9)

#define _IF_EEPROM_0 (_IF_MISCA_BASE | 0xA)
#define _IF_EEPROM_1 (_IF_MISCA_BASE | 0xB)
#define _IF_G_0 (_IF_MISCA_BASE | 0xC)
#define _IF_G_1 (_IF_MISCA_BASE | 0xD)

#define _IF_ALIVE (_IF_MISCA_BASE | 0xE)


// ===== Actions =====

// 0b1.......
#define _ACT_REG 0x80
#define _ACT_REG_MASK 0x80

#define _ACT_REG_0 _ACT_REG
#define _ACT_REG_1 _ACT_REG | _REG_1



// 0b000.....
#define _ACT_TIMER 0x00
#define _ACT_TIMER_MASK 0xE0
#define _ACT_TIMER_TO_ADDRESS (_ACT_TIMER | 0x1f)

// 0b0010.....
#define _ACT_MISC_BASE 0x20
#define _ACT_MISC_MASK 0xF0

#define _ACT_UP_ASPECT (_ACT_MISC_BASE | 0)
// args: localvar_id, global_ofs_lsb, global_ofs_msb.
#define _ACT_IMPORT_VAR (_ACT_MISC_BASE | 1) 
// Arguments: 0btttccccc 0baaaaabbb, where ttt is the type (0=event-based),
// ccccc is the clientid, aaaaa is the byte offset and bbb is the bit in the
// byte.
#define _ACT_DEF_VAR (_ACT_MISC_BASE | 2)
// This command has arguments.
// Arg1: 0b0X0Y0bbb, where eventid X will be set from the current contents of
// event Y, replacing the last bbb+1 bytes. Then bbb+1 bytes will follow for the
// new content in big-endian form.
#define _ACT_SET_EVENTID (_ACT_MISCA_BASE | 5)

// 0b0011.....
#define _ACT_MISCA_BASE 0x30
#define _ACT_MISCA_MASK 0xF0


#define _ACT_SET_SRCPLACE (_ACT_MISCA_BASE | 0)
#define _ACT_SET_TRAINID (_ACT_MISCA_BASE | 1)
#define _ACT_SET_SIGNAL (_ACT_MISCA_BASE | 2)
#define _ACT_SET_ASPECT (_ACT_MISCA_BASE | 3)
#define _ACT_READ_GLOBAL_ASPECT (_ACT_MISCA_BASE | 4)


// 0b01......
#define _ACT_STATE 0x40
#define _ACT_STATE_MASK 0xC0



#define A_DARK  0
#define A_STOP  1
#define A_RED   A_STOP
#define A_40    2
#define A_F2    A_40
#define A_GO    3
#define A_F1    A_GO
#define A_SHUNT 4
#define A_F6    A_SHUNT
#define A_60    5
#define A_F3    A_60
#define A_90    6
#define A_F5    A_90
#define A_S40   7
#define A_F4    A_S40
