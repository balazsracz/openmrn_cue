/** \copyright
 * Copyright (c) 2019, Balazs Racz
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
 * \file OlcbBindingsConfig.hxx
 *
 * CDI configuration structures for the OpenLCB bindings of the logic language.
 *
 * @author Balazs Racz
 * @date 25 June 2019
 */

#ifndef _LOGIC_OLCBBINDINGSCONFIG_HXX_
#define _LOGIC_OLCBBINDINGSCONFIG_HXX_

#include "openlcb/ConfigRepresentation.hxx"
#include "openlcb/MemoryConfig.hxx"

namespace logic {

static const char ENABLED_MAP_VALUES[] = R"(
<relation><property>0</property><value>Disabled</value></relation>
<relation><property>1</property><value>Running</value></relation>
)";

CDI_GROUP(LogicBlockImport);
CDI_GROUP_ENTRY(name, openlcb::StringConfigEntry<32>,
                Name("Import variable name. Do not edit."));
CDI_GROUP_ENTRY(
    event_on, openlcb::EventConfigEntry, //
    Name("Event On/Active/Thrown"),
    Description("This event ID means 'true'."));
CDI_GROUP_ENTRY(
    event_off, openlcb::EventConfigEntry, //
    Name("Event Off/Inactive/Closed"),
    Description("This event ID means 'false'."));
CDI_GROUP_END();

using ReptImports = openlcb::RepeatedGroup<LogicBlockImport, 12>;

CDI_GROUP(LogicBlockBody);
CDI_GROUP_ENTRY(imports, ReptImports, Name("Imported variables for the logic block."), RepName("Variable"));
CDI_GROUP_ENTRY(text, openlcb::StringConfigEntry<1500>,
                Name("Program text."));
CDI_GROUP_ENTRY(status, openlcb::StringConfigEntry<256>,
                Name("Status of program."));
CDI_GROUP_END();

CDI_GROUP(LogicBlock);
CDI_GROUP_ENTRY(filename, openlcb::StringConfigEntry<32>,
                Name("Filename of this logic block."));
CDI_GROUP_ENTRY(enabled, openlcb::Uint8ConfigEntry, Default(0), Min(0), Max(1),
                MapValues(ENABLED_MAP_VALUES), Name("Enabled"),
                Description("Allows disabling the block in case of problems."));
CDI_GROUP_ENTRY(body, LogicBlockBody);
CDI_GROUP_END();

using ReptBlock = openlcb::RepeatedGroup<LogicBlock, 4>;

CDI_GROUP(LogicConfig);
CDI_GROUP_ENTRY(blocks, ReptBlock, RepName("Block"));
CDI_GROUP_END();

} // namespace logic


#endif // _LOGIC_OLCBBINDINGSCONFIG_HXX_
