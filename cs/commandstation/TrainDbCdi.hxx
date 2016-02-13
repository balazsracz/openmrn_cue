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
#include "commandstation/TrainDb.hxx"

namespace commandstation {


static const char MOMENTARY_MAP[] =
  "<relation><property>0</property><value>Latching</value></relation>"
  "<relation><property>1</property><value>Momentary</value></relation>";

static const char FNDISPLAY_MAP[] =
  "<relation><property>0</property><value>Unavailable</value></relation>"
  "<relation><property>1</property><value>Light</value></relation>"
  "<relation><property>2</property><value>Beamer</value></relation>"
  "<relation><property>3</property><value>Bell</value></relation>"
  "<relation><property>4</property><value>Horn</value></relation>"
  "<relation><property>5</property><value>Shunting mode</value></relation>"
  "<relation><property>6</property><value>Pantograph</value></relation>"
  "<relation><property>7</property><value>Smoke</value></relation>"
  "<relation><property>8</property><value>Momentum off</value></relation>"
  "<relation><property>9</property><value>Whistle</value></relation>"
  "<relation><property>10</property><value>Sound</value></relation>"
  "<relation><property>11</property><value>F</value></relation>"
  "<relation><property>12</property><value>Announce</value></relation>"
  "<relation><property>13</property><value>Engine</value></relation>"
  "<relation><property>14</property><value>Light1</value></relation>"
  "<relation><property>15</property><value>Light2</value></relation>"
  "<relation><property>17</property><value>Uncouple</value></relation>";


CDI_GROUP(TrainDbCdiFunctionGroup, Name("Functions"), Description("Defines what each function button does."));
CDI_GROUP_ENTRY(icon, nmranet::Uint8ConfigEntry, Name("Display"), Description("Defines how throttles display this function."), Default(FN_NONEXISTANT));
CDI_GROUP_ENTRY(is_momentary, nmranet::Uint8ConfigEntry, Name("Momentary"), Description("Momentary functions are automatically turned off when you release the respective button on the throttles."), MapValues(MOMENTARY_MAP), Default(0));
CDI_GROUP_END();

using TrainDbCdiAllFunctionGroup = nmranet::RepeatedGroup<TrainDbCdiFunctionGroup, DCC_MAX_FN>;

static const char DCC_DRIVE_MODE_MAP[] =
"<relation><property>5</property><value>DCC 28-step</value></relation>"
"<relation><property>6</property><value>DCC 128-step</value></relation>"
"<relation><property>0</property><value>Marklin-Motorola I</value></relation>"
"<relation><property>1</property><value>Marklin-Motorola II</value></relation>"
"<relation><property>69</property><value>DCC 28-step (forced long address)</value></relation>"
"<relation><property>70</property><value>DCC 128-step (forced long address)</value></relation>";

CDI_GROUP(TrainDbCdiEntry, Description("Configures a single train"));
CDI_GROUP_ENTRY(address, nmranet::Uint16ConfigEntry, Name("Address"), Description("Track protocol address of the train."), Default(3));
CDI_GROUP_ENTRY(mode, nmranet::Uint8ConfigEntry, Name("Protocol"), Description("Protocol to use on the track for driving this train."), MapValues(DCC_DRIVE_MODE_MAP), Default(DCC_28));
CDI_GROUP_ENTRY(name, nmranet::StringConfigEntry<16>, Name("Name"), Description("Identifies the train node on the LCC bus."));
CDI_GROUP_ENTRY(all_functions, TrainDbCdiAllFunctionGroup);
CDI_GROUP_END();

CDI_GROUP(TrainConfigDef, MainCdi());
// We do not support ACDI and we do not support adding the <identification>
// information in here because both of these vary train by train.
CDI_GROUP_ENTRY(train, TrainDbCdiEntry);
CDI_GROUP_END();

};

#endif // _BRACZ_COMMANDSTATION_TRAINDBCDI_HXX_