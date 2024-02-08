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

#include <cstddef>

#include "openlcb/ConfigRepresentation.hxx"
#include "openlcb/MemoryConfigDefs.hxx"

namespace commandstation {

static const char OPERATING_MODE_MAP_VALUES[] = R"(
<relation><property>0</property><value>Disabled</value></relation>
<relation><property>1</property><value>Direct mode</value></relation>
<relation><property>2</property><value>POM mode</value></relation>
<relation><property>3</property><value>Paged mode</value></relation>
<relation><property>10</property><value>Advanced mode</value></relation>
)";

static const char EXT_MODE_MAP_VALUES[] = R"(
<relation><property>0</property><value>Disabled</value></relation>
<relation><property>256</property><value>Direct mode</value></relation>
<relation><property>513</property><value>POM mode (Short address)</value></relation>
<relation><property>514</property><value>POM mode (Long address)</value></relation>
<relation><property>516</property><value>POM mode (Basic accy)</value></relation>
<relation><property>517</property><value>POM mode (Ext accy)</value></relation>
<relation><property>768</property><value>Paged mode</value></relation>
<relation><property>2560</property><value>Advanced mode</value></relation>
)";


static const char ADDRESS_TYPE_MAP_VALUES[] = R"(
<relation><property>0</property><value>Service mode</value></relation>
<relation><property>1</property><value>Short address (DCC Mobile decoder)</value></relation>
<relation><property>2</property><value>Long address (DCC Mobile decoder)</value></relation>
<relation><property>4</property><value>Basic Accessory Decoder</value></relation>
<relation><property>5</property><value>Extended Accessory Decoder</value></relation>
)";

CDI_GROUP(ProgrammingTrackSpaceConfigAdvanced);
CDI_GROUP_ENTRY(
    repeat_verify, openlcb::Uint32ConfigEntry,
    Name("Repeat count for verify packets"),
    Description("How many times a direct mode bit verify packet needs to be "
                "repeated for an acknowledgement to be generated."),
    Default(3), Min(0), Max(255));
CDI_GROUP_ENTRY(
    repeat_cooldown_reset, openlcb::Uint32ConfigEntry,
    Name("Repeat count for reset packets after verify"),
    Description("How many reset packets to send after a verify."),
    Default(6), Min(0), Max(255));
CDI_GROUP_END();

CDI_GROUP(ProgrammingTrackSpaceConfig, Segment(openlcb::MemoryConfigDefs::SPACE_DCC_CV), Offset(0x7F100000),
          Name("Programming Track Operation"),
          Description("Use this component to read and write CVs on the "
                      "programming track of the command station."));

enum OperatingMode : uint8_t {
  DISABLED = 0,
  DIRECT_MODE = 1,
  POM_MODE = 2,
  PAGED_MODE = 3,
  POM_ACCY_BASIC_MODE = 4,
  POM_ACCY_EXT_MODE = 5,
  ADVANCED = 10,
};

CDI_GROUP_ENTRY(mode, openlcb::Uint16ConfigEntry, Name("Operating Mode"), MapValues(EXT_MODE_MAP_VALUES));
CDI_GROUP_ENTRY(address_type, openlcb::Uint8ConfigEntry, Offset(-1), Name("Address type"), MapValues(ADDRESS_TYPE_MAP_VALUES));
CDI_GROUP_ENTRY(dcc_address, openlcb::Uint16ConfigEntry,
                Name("DCC address"),
                Description("For Accessory POM, this contains the accessory "
                            "address to program (user address), 1..2048."),
                Default(3), Min(0), Max(10239));
CDI_GROUP_ENTRY(cv, openlcb::Uint32ConfigEntry, Name("CV Number"), Description("Number of CV to read or write (1..1024)."), Default(0), Min(0), Max(1024));
CDI_GROUP_ENTRY(
    value, openlcb::Uint8ConfigEntry, Name("CV Value"),
    Description(
        "Set 'Operating Mode' and 'CV Number' first, then: hit 'Refresh' to "
        "read the entire CV, or enter a value and hit 'Write' to set the CV."),
    Default(0), Min(0), Max(255));
CDI_GROUP_ENTRY(reserved1, openlcb::EmptyGroup<1>);
CDI_GROUP_ENTRY(
    bit_write_value, openlcb::Uint16ConfigEntry, Name("Bit Change"),
    Description(
        "Set 'Operating Mode' and 'CV Number' first, then: write 1064 to set "
        "the single bit whose value is 64, or 2064 to clear that bit. Write "
        "100 to 107 to set bit index 0 to 7, or 200 to 207 to clear bit 0 to "
        "7. Values outside of these two ranges do nothing."),
    Default(1000), Min(100), Max(2128));
CDI_GROUP_ENTRY(bit_value_string, openlcb::StringConfigEntry<24>,
                Name("Read Bits Decomposition"),
                Description("Hit Refresh on this line after reading a CV value "
                            "to see which bits are set."));
CDI_GROUP_ENTRY(advanced, ProgrammingTrackSpaceConfigAdvanced,
                Name("Advanced Settings"));
struct Shadow;
CDI_GROUP_END();

/// This shadow structure is declared to be parallel to the CDI entries.
struct ProgrammingTrackSpaceConfig::Shadow {
  /// operating mode, of enum OperatingMode
  uint8_t mode;
  /// DCC address type, enum dcc::TrainAddressType
  uint8_t address_type;
  // DCC address, user facing (accy has 1..2048)
  uint16_t dcc_address;
  // CV number to access (user-facing, 1..1024)
  uint32_t cv;
  // CV value to read/write
  uint8_t value;
  uint8_t reserved[1];
  uint16_t bit_write_value;
  char bit_value_string[24];
  uint32_t verify_repeats;
  uint32_t verify_cooldown_repeats;
};

#if (__GNUC__ > 6) || defined(__EMSCRIPTEN__)
#define SHADOW_OFFSETOF(entry)                                  \
    (offsetof(ProgrammingTrackSpaceConfig::Shadow, entry))
#else
#define SHADOW_OFFSETOF(entry)                                          \
    ((uintptr_t) & ((ProgrammingTrackSpaceConfig::Shadow*)nullptr)->entry)
#endif

static_assert(SHADOW_OFFSETOF(cv) ==
                  ProgrammingTrackSpaceConfig::zero_offset_this().cv().offset(),
              "Offset of CV field does not match.");

static_assert(SHADOW_OFFSETOF(verify_cooldown_repeats) ==
                  ProgrammingTrackSpaceConfig::zero_offset_this()
                      .advanced()
                      .repeat_cooldown_reset()
                      .offset(),
              "Offset of repeat cooldown reset field does not match.");

} // namespace commandstation



#endif // _COMMANDSTATION_PROGRAMMINGTRACKSPACECONFIG_HXX_
