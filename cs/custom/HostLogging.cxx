#include "custom/HostLogging.hxx"

#include "src/host_packet.h"
#include "src/usb_proto.h"

namespace bracz_custom {

void send_host_log_event(HostLogEvent e) __attribute__((weak));

void send_host_log_event(HostLogEvent e) {
  PacketBase pkt(2);
  pkt[0] = CMD_CAN_LOG;
  pkt[1] = (uint8_t)e;
  PacketQueue::instance()->TransmitPacket(pkt);
}

} // namespace bracz_custom
