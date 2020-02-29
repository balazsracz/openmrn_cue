
#ifndef _BRACZ_CS_TIVA_CONFIG_HXX_
#define _BRACZ_CS_TIVA_CONFIG_HXX_

#include "openlcb/ConfigRepresentation.hxx"
#include "openlcb/MemoryConfig.hxx"
#include "openlcb/SimpleNodeInfo.hxx"
#include "commandstation/TrainDbCdi.hxx"

namespace openlcb {

extern const SimpleNodeStaticValues SNIP_STATIC_DATA =
{
    4, "Balazs Racz", "CS.js", "v2020.02.29", "1.0.2"
};

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
CDI_GROUP_END();

} // namespace openlcb




#endif // _BRACZ_CS_TIVA_CONFIG_HXX_
