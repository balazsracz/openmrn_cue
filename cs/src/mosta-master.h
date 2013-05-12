// Copyright 2010 Google Inc. All Rights Reserved.
// Author: bracz@google.com (Balazs Racz)
//
// Routines used to serve as a slave for a Marklin Mobile Station acting as a
// master.

#ifndef PICV2_MOSTA_MASTER_H_
#define PICV2_MOSTA_MASTER_H_

class PacketBase;

void MoStaMaster_HandlePacket(const PacketBase& can_buf);
void MoStaMaster_Timer();
void MoStaMaster_EmergencyStop();

#endif  // PICV2_MOSTA_MASTER_H_
