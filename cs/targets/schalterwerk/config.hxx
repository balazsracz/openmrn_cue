#ifndef _APPLICATIONS_IO_BOARD_TARGET_CONFIG_HXX_
#define _APPLICATIONS_IO_BOARD_TARGET_CONFIG_HXX_

#include "openlcb/ConfigRepresentation.hxx"
#include "openlcb/ConfiguredConsumer.hxx"
#include "openlcb/ConfiguredProducer.hxx"
#include "openlcb/MemoryConfig.hxx"
#include "openlcb/MultiConfiguredPC.hxx"
#include "openlcb/ServoConsumerConfig.hxx"

namespace openlcb
{

/// Defines the identification information for the node. The arguments are:
///
/// - 4 (version info, always 4 by the standard
/// - Manufacturer name
/// - Model name
/// - Hardware version
/// - Software version
///
/// This data will be used for all purposes of the identification:
///
/// - the generated cdi.xml will include this data
/// - the Simple Node Ident Info Protocol will return this data
/// - the ACDI memory space will contain this data.
extern const SimpleNodeStaticValues SNIP_STATIC_DATA = {
    4, "OpenMRN", "OpenLCB DevKit + Nucleo F303 dev board", "Rev A", "1.02"};

#define NUM_OUTPUTS 0
#define NUM_INPUTS 1
#define NUM_EXT_OUTPUTS 24
#define NUM_EXT_INPUTS 24
  
/// Declares a repeated group of a given base group and number of repeats. The
/// ProducerConfig and ConsumerConfig groups represent the configuration layout
/// needed by the ConfiguredProducer and ConfiguredConsumer classes, and come
/// from their respective hxx file.
using AllConsumers = RepeatedGroup<ConsumerConfig, NUM_OUTPUTS>;
using AllProducers = RepeatedGroup<ProducerConfig, NUM_INPUTS>;

using ExtConsumers = RepeatedGroup<ConsumerConfig, NUM_EXT_OUTPUTS>;
using ExtProducers = RepeatedGroup<ProducerConfig, NUM_EXT_INPUTS>;
  
using DirectConsumers = RepeatedGroup<ConsumerConfig, 8>;
#ifdef PORTD_SNAP
using PortDEConsumers = RepeatedGroup<ConsumerConfig, 8>;
#else
using PortDEConsumers = RepeatedGroup<ConsumerConfig, 16>;
#endif
using PortABProducers = RepeatedGroup<ProducerConfig, 16>;
using PulseConsumers = RepeatedGroup<PulseConsumerConfig, 8>;
using ServoConsumers = RepeatedGroup<ServoConsumerConfig, 4>;


/// Modify this value every time the EEPROM needs to be cleared on the node
/// after an update.
static constexpr uint16_t CANONICAL_VERSION = 0x1536;

CDI_GROUP(NucleoGroup, Name("Nucleo peripherals"), Description("These are physically located on the nucleo CPU daughterboard."));
CDI_GROUP_ENTRY(green_led, ConsumerConfig, Name("Nucleo user LED"), Description("Green led (LD2)."));
CDI_GROUP_ENTRY(user_btn, ProducerConfig, Name("USER button"), Description("Button with blue cap."));
CDI_GROUP_END();

/// Defines the main segment in the configuration CDI. This is laid out at
/// origin 128 to give space for the ACDI user data at the beginning.
CDI_GROUP(IoBoardSegment, Segment(MemoryConfigDefs::SPACE_CONFIG), Offset(128));
/// Each entry declares the name of the current entry, then the type and then
/// optional arguments list.
CDI_GROUP_ENTRY(internal_config, InternalConfigData);
CDI_GROUP_ENTRY(nucleo_onboard, NucleoGroup);
//#ifdef PORTD_SNAP
//CDI_GROUP_ENTRY(portde_consumers, PortDEConsumers, Name("Port E outputs"), Description("Line 1-4 is port E 5 - 8; offset due to Snap Switches"), RepName("Line"));
//#endif
CDI_GROUP_ENTRY(portab_producers, PortABProducers, Name("Port A/B inputs"), Description("Line 1-8 is port A, Line 9-16 is port B"), RepName("Line"));

CDI_GROUP_ENTRY(ext_producers, ExtProducers, Name("External inputs"), Description("Line 1-8 is first IO board, Line 9-16 is second, etc."), RepName("Line"));

CDI_GROUP_ENTRY(ext_consumers, ExtConsumers, Name("External outputs"), Description("Line 1-8 is first IO board, Line 9-16 is second, etc."), RepName("Line"));
  
CDI_GROUP_END();

/// This segment is only needed temporarily until there is program code to set
/// the ACDI user data version byte.
CDI_GROUP(VersionSeg, Segment(MemoryConfigDefs::SPACE_CONFIG),
    Name("Version information"));
CDI_GROUP_ENTRY(acdi_user_version, Uint8ConfigEntry,
    Name("ACDI User Data version"), Description("Set to 2 and do not change."));
CDI_GROUP_END();

/// The main structure of the CDI. ConfigDef is the symbol we use in main.cxx
/// to refer to the configuration defined here.
CDI_GROUP(ConfigDef, MainCdi());
/// Adds the <identification> tag with the values from SNIP_STATIC_DATA above.
CDI_GROUP_ENTRY(ident, Identification);
/// Adds an <acdi> tag.
CDI_GROUP_ENTRY(acdi, Acdi);
/// Adds a segment for changing the values in the ACDI user-defined
/// space. UserInfoSegment is defined in the system header.
CDI_GROUP_ENTRY(userinfo, UserInfoSegment);
/// Adds the main configuration segment.
CDI_GROUP_ENTRY(seg, IoBoardSegment);
/// Adds the versioning segment.
CDI_GROUP_ENTRY(version, VersionSeg);
CDI_GROUP_END();

} // namespace openlcb

#endif // _APPLICATIONS_IO_BOARD_TARGET_CONFIG_HXX_
