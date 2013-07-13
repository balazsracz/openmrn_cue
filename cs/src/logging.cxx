
#include "logging.h"

char logbuffer[256];

#ifdef __FreeRTOS__
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

#ifdef __linux__

#include <stdio.h>

void log_output(char* buf, int size) {
    if (size <= 0) return;
    fwrite(buf, size, 1, stderr);
}

#endif


