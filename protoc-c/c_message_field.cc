// Protocol Buffers - Google's data interchange format
// Copyright 2008 Google Inc.  All rights reserved.
// http://code.google.com/p/protobuf/
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are
// met:
//
//     * Redistributions of source code must retain the above copyright
// notice, this list of conditions and the following disclaimer.
//     * Redistributions in binary form must reproduce the above
// copyright notice, this list of conditions and the following disclaimer
// in the documentation and/or other materials provided with the
// distribution.
//     * Neither the name of Google Inc. nor the names of its
// contributors may be used to endorse or promote products derived from
// this software without specific prior written permission.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
// "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
// LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
// A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
// OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
// SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
// LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
// DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
// THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
// (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
// OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

// Author: kenton@google.com (Kenton Varda)
//  Based on original Protocol Buffers design by
//  Sanjay Ghemawat, Jeff Dean, and others.

// Copyright (c) 2008-2013, Dave Benson.  All rights reserved.
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are
// met:
//
//     * Redistributions of source code must retain the above copyright
// notice, this list of conditions and the following disclaimer.
//
//     * Redistributions in binary form must reproduce the above
// copyright notice, this list of conditions and the following disclaimer
// in the documentation and/or other materials provided with the
// distribution.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
// "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
// LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
// A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
// OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
// SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
// LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
// DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
// THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
// (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
// OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

// Modified to implement C code by Dave Benson.

#include <protoc-c/c_message_field.h>
#include <protoc-c/c_helpers.h>
#include <google/protobuf/io/printer.h>
#include <google/protobuf/wire_format.h>

namespace google {
namespace protobuf {
namespace compiler {
namespace c {

using internal::WireFormat;

// ===================================================================

MessageFieldGenerator::
MessageFieldGenerator(const FieldDescriptor* descriptor)
  : FieldGenerator(descriptor) {
}

MessageFieldGenerator::~MessageFieldGenerator() {}

void MessageFieldGenerator::GenerateStructMembers(io::Printer* printer) const
{
  std::map<string, string> vars;
  vars["name"] = FieldName(descriptor_);
  string full_name = descriptor_->message_type()->full_name();
  string classname = full_name.substr(0, full_name.find("."));
  vars["type"] = ToLower(classname + string("_") + CamelToLower(descriptor_->message_type()->name()));
  vars["deprecated"] = FieldDeprecated(descriptor_);
  switch (descriptor_->label()) {
    case FieldDescriptor::LABEL_REQUIRED:
    case FieldDescriptor::LABEL_OPTIONAL:
      printer->Print(vars, "$type$_t *$name$$deprecated$;\n");
      break;
    case FieldDescriptor::LABEL_REPEATED:
      printer->Print(vars, "size_t n_$name$$deprecated$;\n");
      printer->Print(vars, "$type$_t **$name$$deprecated$;\n");

//#define USE_REPEATED_ALLOCATOR
#if 1 //def USE_REPEATED_ALLOCATOR
      printer->Print(vars, "list_head_t l_$name$$deprecated$;\n");
      //printer->Print(vars, "void* (*l_$name$$deprecated$_new)(void);\n");
#endif
      break;
  }
}
string MessageFieldGenerator::GetDefaultValue(void) const
{
  /* XXX: update when protobuf gets support
   *   for default-values of message fields.
   */
  return "NULL";
}
void MessageFieldGenerator::GenerateStaticInit(io::Printer* printer) const
{
  switch (descriptor_->label()) {
    case FieldDescriptor::LABEL_REQUIRED:
    case FieldDescriptor::LABEL_OPTIONAL:
      printer->Print("NULL");
      break;
    case FieldDescriptor::LABEL_REPEATED:
      printer->Print("0, NULL");
#if 1 //def USE_REPEATED_ALLOCATOR
      //printer->Print(", list_head_t l_$name$$deprecated$\n");
      printer->Print(", {NULL, NULL}");
      //printer->Print(", NULL");
#endif
      break;
  }
}
void MessageFieldGenerator::GenerateDescriptorInitializer(io::Printer* printer) const
{
  string full_name = descriptor_->message_type()->full_name();
  string classname = full_name.substr(0, full_name.find("."));
  string addr = "&" +  ToLower(classname + string("_"));
  addr += CamelToLower(descriptor_->message_type()->name());
  addr += "_descriptor";

  GenerateDescriptorInitializerGeneric(printer, false, "MESSAGE", addr);
}

}  // namespace c
}  // namespace compiler
}  // namespace protobuf
}  // namespace google
