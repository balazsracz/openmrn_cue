/** \copyright
 * Copyright (c) 2018, Balazs Racz
 * All rights reserved
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are strictly prohibited without written consent.
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
 * \file ProgrammingTrackSpaceConfig.hxx
 *
 * CDI configuration for the CV space to access the programming track flow.
 *
 * @author Balazs Racz
 * @date 2 June 2018
 */

#ifndef _COMMANDSTATION_PROGRAMMINGTRACKSPACECONFIG_HXX_
#define _COMMANDSTATION_PROGRAMMINGTRACKSPACECONFIG_HXX_

#include "openlcb/ConfigRepresentation.hxx"

namespace commandstation {

static const char OPERATING_MODE_MAP_VALUES[] = R"(
<relation><property>0</property><value>Disabled</value></relation>
<relation><property>1</property><value>Direct mode</value></relation>
<relation><property>10</property><value>Advanced mode</value></relation>
)";


CDI_GROUP(ProgrammingTrackSpaceConfig, Segment(0x15), Offset(0x7F100000),
          Name("Programming track operation"),
          Description("Use this component to read and write CVs on the "
                      "programming track of the command station."));

enum OperatingMode {
  DISABLED = 0,
  DIRECT_MODE = 1,
  ADVANCED = 10,
};

CDI_GROUP_ENTRY(mode, openlcb::Uint32ConfigEntry, Name("Operating mode"), MapValues(OPERATING_MODE_MAP_VALUES));
CDI_GROUP_ENTRY(cv, openlcb::Uint32ConfigEntry, Name("CV number"), Description("Number of CV to read or write (1..1024)."), Default(0), Min(0), Max(1024));
CDI_GROUP_ENTRY(value, openlcb::Uint32ConfigEntry, Name("CV value"), Description("Read or write this field to access the given CV."), Default(0), Min(0), Max(255));
struct Shadow;
CDI_GROUP_END();

/// This shadow structure is declared to be parallel to the CDI entries.
struct ProgrammingTrackSpaceConfig::Shadow {
  uint32_t mode;
  uint32_t cv;
  uint32_t value;
};

#define SHADOW_OFFSETOF(entry)                                          \
  ((uintptr_t) & ((ProgrammingTrackSpaceConfig::Shadow*)nullptr)->entry)

static_assert(SHADOW_OFFSETOF(cv) == ProgrammingTrackSpaceConfig::zero_offset_this().cv().offset(), "Offset of CV field does not match.");


} // namespace commandstation



#endif // _COMMANDSTATION_PROGRAMMINGTRACKSPACECONFIG_HXX_

