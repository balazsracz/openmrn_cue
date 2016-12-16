#ifndef _APPLICATIONS_IO_BOARD_TARGET_CONFIG_HXX_
#define _APPLICATIONS_IO_BOARD_TARGET_CONFIG_HXX_

#include "openlcb/ConfiguredConsumer.hxx"
#include "openlcb/ConfiguredProducer.hxx"
#include "openlcb/ConfigRepresentation.hxx"
#include "openlcb/MemoryConfig.hxx"
#include "custom/DetectorPortConfig.hxx"

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
    4,               "Balazs Racz", "Railcom IO board",
    "ek-tm4c123gxl", "2015-10-25"};

static const uint16_t EXPECTED_VERSION = 0xf37b;

#define NUM_OUTPUTS 3
#define NUM_INPUTS 2

/// Declares a repeated group of a given base group and number of repeats. The
/// ProducerConfig and ConsumerConfig groups represent the configuration layout
/// needed by the ConfiguredProducer and ConfiguredConsumer classes, and come
/// from their respective hxx file.
using AllConsumers = RepeatedGroup<ConsumerConfig, NUM_OUTPUTS>;
using AllProducers = RepeatedGroup<ProducerConfig, NUM_INPUTS>;

using EnableConsumers = RepeatedGroup<ConsumerConfig, 6>;

using DetectorPorts = RepeatedGroup<bracz_custom::DetectorPortConfig, 6>;

CDI_GROUP(CountingDebouncerOpts);
CDI_GROUP_ENTRY(count_total, Uint8ConfigEntry, Name("Window length"), Description("How many consecutive samples to count"));
CDI_GROUP_ENTRY(min_active, Uint8ConfigEntry, Name("Min Active"), Description("How many active samples in the window to consider the window active"));
CDI_GROUP_END();

CDI_GROUP(DacSettingsConfig, Description("Configures reference voltage"));
CDI_GROUP_ENTRY(nominator, Uint8ConfigEntry, Name("Nominator"), Description("Number of CPU cycles to drive output high"));
CDI_GROUP_ENTRY(denom, Uint8ConfigEntry, Name("Denominator"), Description("PWM length in cycle count"));
CDI_GROUP_ENTRY(divide, Uint8ConfigEntry, Name("DivBy100"), Description("If non-zero, divides output voltage by 100."));
CDI_GROUP_END();

CDI_GROUP(CurrentParams, Name("Tuning params"), Description("Parameters for tuning the current detector"));
CDI_GROUP_ENTRY(occupancy, CountingDebouncerOpts, Name("Occupancy debouncer"));
CDI_GROUP_ENTRY(overcurrent, CountingDebouncerOpts, Name("Overcurrent debouncer"));
CDI_GROUP_ENTRY(dac_occupancy, DacSettingsConfig, Name("DAC occupancy"));
CDI_GROUP_ENTRY(dac_overcurrent, DacSettingsConfig, Name("DAC overcurrent"));
CDI_GROUP_ENTRY(dac_railcom, DacSettingsConfig, Name("DAC railcom"));
CDI_GROUP_END();

/// Defines the main segment in the configuration CDI. This is laid out at
/// origin 128 to give space for the ACDI user data at the beginning.
CDI_GROUP(IoBoardSegment, Segment(MemoryConfigDefs::SPACE_CONFIG), Offset(128));
CDI_GROUP_ENTRY(internal, InternalConfigData);
/// Each entry declares the name of the current entry, then the type and then
/// optional arguments list.
CDI_GROUP_ENTRY(consumers, AllConsumers, Name("Output LEDs"));
CDI_GROUP_ENTRY(producers, AllProducers, Name("Input buttons"));
CDI_GROUP_ENTRY(current, CurrentParams);
//CDI_GROUP_ENTRY(enables, EnableConsumers, Name("Channel enable"),
//                Description("Consumers to turn channels on and off."));
CDI_GROUP_ENTRY(detectors, DetectorPorts, Name("Channels"));
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

}  // namespace openlcb

#endif // _APPLICATIONS_IO_BOARD_TARGET_CONFIG_HXX_
