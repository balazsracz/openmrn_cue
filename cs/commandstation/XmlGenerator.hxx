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
 * \file XmlGenerator.hxx
 *
 * Interface for generating XML files on-the-fly on small-memory machines.
 *
 * @author Balazs Racz
 * @date 16 Jan 2016
 */

#ifndef _BRACZ_MOBILESTATION_XMLGENERATOR_HXX_
#define _BRACZ_MOBILESTATION_XMLGENERATOR_HXX_

#include <stdio.h>
#include <stdint.h>
#include "utils/SimpleQueue.hxx"

namespace commandstation {


class XmlGenerator {
 public:
  XmlGenerator() {}

  /// Reads from the buffer, or generates more data to read. Returns the number
  /// of bytes written to buf. Returns a short read (including 0) if and only
  /// if EOF is reached.
  ssize_t read(size_t offset, void* buf, size_t len);

  size_t file_offset() {
    return fileOffset_;
  }

 protected:
  struct GeneratorAction;

  /// This function will be called repeatedly in order to fill in the output
  /// buffer. Each call must call add_to_output at least once unless the EOF is
  /// reached.
  virtual void generate_more() = 0;

  /// Call this method from the driver API in order to
  void internal_reset();

  /// Call this function from generate_more to extend the output buffer.
  void add_to_output(GeneratorAction* action) {
    pendingActions_.push_front(action);
  }


  GeneratorAction* from_const_string(const char* data) {
    GeneratorAction* a = new GeneratorAction;
    a->type = CONST_LITERAL;
    a->pointer = data;
    return a;
  }

  GeneratorAction* from_integer(int data) {
    GeneratorAction* a = new GeneratorAction;
    a->type = RENDER_INT;
    a->integer = data;
    return a;
  }

  struct GeneratorAction : public QMember {
    uint8_t type;
    union {
      const void* pointer;
      int integer;
    };
  };

 private:
  friend class TestEmptyXmlGenerator;

  enum ActionType {
    CONST_LITERAL,
    RENDER_INT,
  };

  /// Sets up the internal structures needed based on the action in the front
  /// of the pendingQueue_.
  void init_front_action();

  /// Returns the pointer to the data representing the front action.
  const char* get_front_buffer();

  /// Actions that were generated by the last call of generate_more(). Note
  /// that the order of these action is REVERSED during the call to
  /// generate_more().
  TypedQueue<GeneratorAction> pendingActions_;

  /// The offset (in the file) of the first byte of the first Action in
  /// pendingActions_.
  size_t fileOffset_;

  unsigned bufferOffset_;
  /// For rendering integers.
  char buffer_[16];
};

}  // namespace commandstation

#endif // _BRACZ_MOBILESTATION_XMLGENERATOR_HXX_
