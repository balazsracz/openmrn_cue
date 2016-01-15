/** \copyright
 * Copyright (c) 2015, Balazs Racz
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
 * \file DetectorPortConfig.hxx
 *
 * CDI declaration for the detector port logic.
 *
 * @author Balazs Racz
 * @date 19 Dec 2015
 */

#ifndef _BRACZ_CUSTOM_DETECTORPORTLOGIC_HXX_
#define _BRACZ_CUSTOM_DETECTORPORTLOGIC_HXX_

#include "nmranet/ConfigRepresentation.hxx"

namespace bracz_custom {

CDI_GROUP(DetectorPortOccupancy);
CDI_GROUP_ENTRY(
                occ_on, nmranet::EventConfigEntry,  //
    Name("Occupied"),
    Description(
        "A train showed up in the segment."));
CDI_GROUP_ENTRY(
    occ_off, nmranet::EventConfigEntry, Name(">Unoccupied"),  //
    Description(
        "All trains left the segment."));
CDI_GROUP_END();

CDI_GROUP(DetectorPortOvercurrent);
CDI_GROUP_ENTRY(
    over_on, nmranet::EventConfigEntry,  //
    Name("Shorted"),
    Description(
        "The output was turned off due to a short circuit."));
CDI_GROUP_ENTRY(
    over_off, nmranet::EventConfigEntry, Name(">Short cleared"),  //
    Description(
        "The output is not shorted anymore."));
CDI_GROUP_END();

CDI_GROUP(DetectorPortEnable);
CDI_GROUP_ENTRY(
    enable_on, nmranet::EventConfigEntry,  //
    Name("Turn On"),
    Description(
        "Turns on the output."));
CDI_GROUP_ENTRY(
    enable_off, nmranet::EventConfigEntry, Name("Turn Off"),  //
    Description(
        "Turns off the output."));
CDI_GROUP_END();


CDI_GROUP(DetectorPortConfig);
CDI_GROUP_ENTRY(name, nmranet::StringConfigEntry<16>, Name("Port name"), Description("This setting has no effect on the node operation."));
CDI_GROUP_ENTRY(occupancy, DetectorPortOccupancy, Name("Occupancy detector"), Description("Configures events produced by the occupancy detector."));
CDI_GROUP_ENTRY(overcurrent, DetectorPortOvercurrent, Name("Short detector"), Description("Configures events produced by the overcurrent detector."));
CDI_GROUP_ENTRY(enable, DetectorPortEnable, Name("Enable"), Description("Configures events consumed to turn the port on or off."));
CDI_GROUP_END();

}  // namespace bracz_custom

#endif  // _BRACZ_CUSTOM_DETECTORPORTLOGIC_HXX_
