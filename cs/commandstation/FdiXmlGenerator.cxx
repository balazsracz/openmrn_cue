/** \copyright
 * Copyright (c) 2016, Balazs Racz
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
 * \file FdiXmlGenerator.hxx
 *
 * Train FDI generator.
 *
 * @author Balazs Racz
 * @date 16 Jan 2016
 */

#include "mobilestation/FdiXmlGenerator.hxx"

namespace mobilestation {

static const char kFdiXmlHead[] = R"(<?xml version='1.0' encoding='UTF-8'?>
<?xml-stylesheet type='text/xsl' href='xslt/fdi.xsl'?>
<fdi xmlns:xsi='http://www.w3.org/2001/XMLSchema-instance' xsi:noNamespaceSchemaLocation='http://openlcb.org/trunk/prototypes/xml/schema/fdi.xsd'>
<segment space='249'><group><name/>
)";
static const char kFdiXmlTail[] = "</group></segment></fdi>";

static const char kFdiXmlBinaryFunction[] =
    R"(<function size='1' kind='binary'>
)";

static const char kFdiXmlMomentaryFunction[] =
    R"(<function size='1' kind='momentary'>
)";

struct FunctionLabel {
  uint8_t fn;
  const char* label;
};

static const FunctionLabel labels[] = {  //
    {1, "Light"},      {2, "Beamer"},  {5, "Shunt"},     {6, "Coupler"},
    {8, "ABV"},        {10, "Smoke"},  {13, "Engine"},   {14, "Light1"},
    {15, "Light2"},    {132, "Honk"},  {133, "Whistle"}, {139, "P"},
    {140, "Announce"}, {141, "Sound"}, {0, nullptr}};

static const char* label_for_function(uint8_t type) {
  const FunctionLabel* r = labels;
  while (r->fn) {
    if (r->fn == type) return r->label;
    ++r;
  }
  return nullptr;
}

void FdiXmlGenerator::reset() {
  state_ = STATE_START;
  internal_reset();
}

void FdiXmlGenerator::generate_more() {
  while (true) {
    switch (state_) {
      case STATE_XMLHEAD: {
        add_to_output(from_const_string(kFdiXmlHead));
        nextFunction_ = 0;
        state_ = STATE_START_FN;
        return;
      }
      case STATE_START_FN: {
        if (((unsigned)nextFunction_) >= sizeof(entry_.function_mapping) ||
            entry_.function_mapping[nextFunction_] == 0xff) {
          state_ = STATE_NO_MORE_FN;
          continue;
        }
        if (entry_.function_labels[nextFunction_] & 0x80) {
          add_to_output(from_const_string(kFdiXmlMomentaryFunction));
        } else {
          add_to_output(from_const_string(kFdiXmlBinaryFunction));
        }
        state_ = STATE_FN_NAME;
        return;
      }
      case STATE_FN_NAME: {
        add_to_output(from_const_string("<name>"));
        const char* label =
            label_for_function(entry_.function_labels[nextFunction_]);
        if (!label) {
          add_to_output(from_const_string("F"));
          add_to_output(from_integer(entry_.function_mapping[nextFunction_]));
        } else {
          add_to_output(from_const_string(label));
        }
        add_to_output(from_const_string("</name>\n"));
        state_ = STATE_FN_NUMBER;
        return;
      }
      case STATE_FN_NUMBER: {
        add_to_output(from_const_string("<number>"));
        add_to_output(from_integer(entry_.function_mapping[nextFunction_]));
        add_to_output(from_const_string("</number>\n</function>\n"));
        state_ = STATE_FN_END;
        return;
      }
      case STATE_FN_END: {
        ++nextFunction_;
        state_ = STATE_START_FN;
        continue;
      }
      case STATE_NO_MORE_FN: {
        add_to_output(from_const_string(kFdiXmlTail));
        state_ = STATE_EOF;
        return;
      }
      case STATE_EOF: {
        return;
      }
    }
  }
}

}  // namespace mobilestation
