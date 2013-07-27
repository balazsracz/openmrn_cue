
#include "logging.h"

char logbuffer[256];

#ifdef TARGET_LPC2368
#include "host_packet.h"
#include "usb_proto.h"

void log_output(char* buf, int size) {
    if (size <= 0) return;
    PacketBase pkt(size + 1);
    pkt[0] = CMD_VCOM1;
    memcpy(pkt.buf() + 1, buf, size);
    PacketQueue::instance()->TransmitPacket(pkt);
}
#endif


#ifdef TARGET_LPC1768
#include "host_packet.h"
#include "usb_proto.h"

extern "C" { void send_stdio_serial_message(const char* data); }

void log_output(char* buf, int size) {
    if (size <= 0) return;
    buf[size] = '\0';
    send_stdio_serial_message(buf);
}

#endif


#ifdef __linux__

#include <stdio.h>

void log_output(char* buf, int size) {
    if (size <= 0) return;
    fwrite(buf, size, 1, stderr);
}

#endif
