/** \copyright
 * Copyright (c) 2014-2016, Balazs Racz
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are  permitted provided that the following conditions are met:
 *
 *  - Redistributions of source code must retain the above copyright notice,
 *    this list of conditions and the following disclaimer.
 *
 *  - Redistributions in binary form must reproduce the above copyright notice,
 *    this list of conditions and the following disclaimer in the documentation
 *    and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 *
 * \file TrainDbCdi.hxx
 *
 * CDI entry defining the commandstation traindb entry.
 *
 * @author Balazs Racz
 * @date 8 Feb 2016
 */

#ifndef _BRACZ_COMMANDSTATION_TRAINDBCDI_HXX_
#define _BRACZ_COMMANDSTATION_TRAINDBCDI_HXX_

#include "nmranet/ConfigRepresentation.hxx"

namespace commandstation {

static const char DCC_DRIVE_MODE_MAP[] =
"<relation><property>5</property><value>DCC 28-step</value></relation>"
"<relation><property>6</property><value>DCC 128-step</value></relation>"
"<relation><property>0</property><value>Marklin-Motorola I</value></relation>"
"<relation><property>1</property><value>Marklin-Motorola II</value></relation>"
"<relation><property>69</property><value>DCC 28-step w/long address</value></relation>"
"<relation><property>70</property><value>DCC 128-step w/long address</value></relation>";

CDI_GROUP(TrainDbCdiEntry, Description("Configures a single train"));
CDI_GROUP_ENTRY(address, nmranet::Uint16ConfigEntry, Name("Address"), Description("Track protocol address of the train."), Default(3));
CDI_GROUP_ENTRY(mode, nmranet::Uint8ConfigEntry, Name("Protocol"), Description("Protocol to use on the track for driving this train."), MapValues(DCC_DRIVE_MODE_MAP), Default(DCC_28));
CDI_GROUP_ENTRY(name, nmranet::StringConfigEntry<16>, Name("Name"), Description("Identifies the train node on the LCC bus."));
CDI_GROUP_END();

};

#endif // _BRACZ_COMMANDSTATION_TRAINDBCDI_HXX_


