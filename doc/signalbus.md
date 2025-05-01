# Signal bus

The Signal bus is a 3-wire local bus that is intended to connect a single
master to a number of slaves. The master is an OpenLCB node, whereas the slaves
are very simple individual microcontrollers with I/O functionality and very
little to no logic. The target is to have each signal head / signal mast an
individual MCU, which is then receiving the desired aspects or indications
through the signal bus.

The bus is a polled repeating bus, where the master is running a polling loop,
continuously sending out the current indication to the slaves. The encoding is
a 9-bit addressed UART bus running at 15625 baud.

## Physical layer

The bus runs over three wires: GND, 5V, DATA.

The DATA line is a 5V-tolerant data line, running an open-drain signaling. The
voltage levels are set for 3V3 CMOS receivers. (`Vlo_max` = 0.35 * 3.3V,
`Vhi_min` = 0.65 * 3.3 V)

The master sends UART signals at 15625 baud. At certain periods of time the
master leaves the DATA line floating with a 1k pull-up.

The slaves are either receiving the UART data or at the designated period of
time they are pulling the line to low.

## Data link layer

The master is sending UART data with 15625 baud 9N1. The ninth bit determines
whether the byte is an address byte (1) or a data byte (0).

There are 256 addresses on the bus.

A Frame is:
- an address byte
- a length byte (0 - 255), indicatingthe number of bytes in the rest of this
  packet including this length byte.
- a command byte
- optional argument byte(s)

Address zero is broadcast, and all slaves shall process these.

## Packet layer

The following commands are available:

### Reset (cmd = 0x01)

Format: Address 0x03 0x01 0x55

- Length = 3 bytes
- Cmd = 0x01
- One payload byte, which is fixed to 0x55.

Effect: The targeted slave verifies the payload being 0x55 and if correct, then
performs a soft reset (reboot).

### Configuration write

Format: Address 0x04 0x02 Cfg-Address Cfg-Value

- Length = 4 bytes
- Cmd = 0x02
- Payload = eeprom address byte (0-255), eeprom value byte (0-255).

Effect: The targeted slave writes the given value to the eeprom at a given
address.

### Set Aspect

Format: Address 0x03 0x03 Aspect-Value

- Length = 3 bytes
- Cmd = 0x03
- Payload = Aspect value (one byte).

Effect: The targeted slave sets its output to the logical state indicated by
the aspect value. This may include a transition animation.

The master keeps continuously sending out refresh packets for aspect values of
all known slaves.

The valid Aspect values are defined by the slave. For the SBB L-type signals,
the following aspect values exist:

- 0 = emergency stop (dark in advance signal, emergency red in main signal).
- 1 = F0 (stop; double yellow in advance signal, red in main signal).
- 2 = F2 (slow 40 km/h; green-yellow in advance signal, green-yellow in main
  signal).
- 3 = F1 (clear; green-green in advance signal, green in main signal).
- 4 = F6 (short run; dark in advance signal, yellow-yellow in main signal).
- 5 = F3 (medium 60 km/h; green-green-yellow in advance signal, green-green in
main signal).
- 6 = F6 (90 km/h; green-green-yellow in advance signal, green-green-green in
main signal).

### Start main program (bootloader)

Format: Address 0x03 0x03 ignored-byte

- Length = 3 bytes
- Cmd = 0x03
- Payload = One byte (ignored)

Effect: When received by the bootloader, this command switches to the main
firmware.

Since the master periodically sends this command to all known slaves, the
slaves are typically automtically switching from bootloader firmware to main
firmware.

### Reflash (bootloader)

Format: Address NN 0x04 Offset-Low Offset-High data...

- Length = 4 + num data bytes
- Cmd = 0x04
- Payload = two byte offset where to write the data (LSB first), and 1-12 bytes
  of data payload to write there.
  
Maximum length of packet is 16 bytes (12 bytes of data).

Effect: the bootloader writes the commanded data to the specified offset. If
the given offset is at the beginning of a flash sector, the sector is erased
before writing the payload (i.e., auto-erase forward).

### LED test (bootloader)

Format: Address 0x04 0x05 Output-LSB Output-MSB

- Length = 4
- Cmd = 0x05
- Payload = two byte, where each bit will directly control an output line. LSB
  byte first.

Effect: the bootloader sets each output line to the specified state (0 bit is
low, 1 bit is high).

### CRC (bootloader)

Format: Address 10 0x06 Offset-low Offset-High Len-Low Len-High CRC-LSB CRC-b2
CRC-b3 CRC-MSB

- Length = 10
- Cmd = 0x06
- Payload = <2-byte offset, LSB-first> <2-byte length, LSB-first> <4-byte
  CRC32, LSB-first>

Effect: The bootloader computes the CRC32 of the given data range from the
program flash. If the supplies CRC matches the computed CRC, a success is
displayed on the outputs. If the computed CRC does not match the supplied CRC,
a failure is displayed on the outputs.

Success and failure pictures are device-specific. For the signals success is
Red 1, failure is both reds together. (TODO: this does not work for advance
signals only heads).

CRC32 is defined like this:
```
void CRC32_Init() {
  crc32_state = 0xFFFFFFFFUL;
}

void CRC32_Add(uint8 data) {
  crc32_state ^= ((crc32_t)data) << 24;
  for (uint8 b = 8; b > 0; --b) {
    if (crc32_state & (1UL<<31)) {
      crc32_state = (crc32_state << 1) ^ 0x04C11DB7UL;
    } else {
      crc32_state <<= 1;
    }
  }
}

void CRC32_Get(crc32_t* dest) {
  crc32_state ^= 0xFFFFFFFFUL;
  *dest = crc32_state;
}
```

### Alt Reset (second bootloader)

Format: Address 2 0x07

- Length = 2
- Cmd = 0x07
- Payload = nothing

Used by a second stage bootloader (?). Reboots the device.

# OpenLCB interface for signal bus

This section describes the communication of OpenLCB nodes through the master
with the signalbus slaves.

The signalbus interface has two APIs: 

- A datagram based service to control the signal loop, including sending
  arbitrary frames to the bus. This is a peer to peer message from a
  controlling node (typically a user interface node) to the particular OpenLCB
  node that is the master of the signalbus.

- An event based service that sets the expected signal addresses and the
  aspects of the particular signals to show.

## Signalbus control datagram

The datagram ID is 0x2F.

Datagram format: 0x2F <datagram_command> <payload>

### Pause refresh loop

Datagram: 0x2F 0x18

Effect: The signal bus master stops sending out the refresh packets.

### Resume refresh loop

Datagram: 0x2F 0x19

Effect: The signal bus master restarts sending out the refresh packets.

### Send frame

Datagram: 0x2F 0x10 <address> <len> <cmd> <payload>

Effect: The signal bus master sends one frame to the signal bus. The frame will
be addressed to slave <address>. This byte will have the 9th bit set. The
contents of the frame will be <len> <cmd> <payload> unmodified. These bytes
will have the ninth bit clear.


## Signalbus aspect events

This is a block of 
