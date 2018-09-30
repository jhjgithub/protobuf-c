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

// Copyright (c) 2008-2014, Dave Benson and the protobuf-c authors.
// All rights reserved.
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

#include <protoc-c/c_file.h>
#include <protoc-c/c_enum.h>
#include <protoc-c/c_service.h>
#include <protoc-c/c_extension.h>
#include <protoc-c/c_helpers.h>
#include <protoc-c/c_message.h>
#include <protoc-c/c_generator.h>
#include <google/protobuf/io/printer.h>
#include <google/protobuf/descriptor.pb.h>

#include "protobuf-c.h"

namespace google {
namespace protobuf {
namespace compiler {
namespace c {

// ===================================================================

FileGenerator::FileGenerator(const FileDescriptor* file,
                             const string& dllexport_decl)
  : file_(file),
    message_generators_(
      new std::unique_ptr<MessageGenerator>[file->message_type_count()]),
    enum_generators_(
      new std::unique_ptr<EnumGenerator>[file->enum_type_count()]),
    service_generators_(
      new std::unique_ptr<ServiceGenerator>[file->service_count()]),
    extension_generators_(
      new std::unique_ptr<ExtensionGenerator>[file->extension_count()]) {

  for (int i = 0; i < file->message_type_count(); i++) {
    message_generators_[i].reset(
      new MessageGenerator(file->message_type(i), dllexport_decl));
  }

  for (int i = 0; i < file->enum_type_count(); i++) {
    enum_generators_[i].reset(
      new EnumGenerator(file->enum_type(i), dllexport_decl));
  }

  for (int i = 0; i < file->service_count(); i++) {
    service_generators_[i].reset(
      new ServiceGenerator(file->service(i), dllexport_decl));
  }

  for (int i = 0; i < file->extension_count(); i++) {
    extension_generators_[i].reset(
      new ExtensionGenerator(file->extension(i), dllexport_decl));
  }

  SplitStringUsing(file_->package(), ".", &package_parts_);
}

FileGenerator::~FileGenerator() {}

void FileGenerator::GenerateHeader(io::Printer* printer) {
  string filename_identifier = FilenameIdentifier(file_->name());

  int min_header_version = 1000000;
#if defined(HAVE_PROTO3)
  if (file_->syntax() == FileDescriptor::SYNTAX_PROTO3) {
    min_header_version = 1003000;
  }
#endif

  // Generate top of header.
  printer->Print(
    "/* Generated by the protocol buffer compiler.  DO NOT EDIT! */\n"
    "/* Generated from: $filename$ */\n"
    "\n"
    "#ifndef PROTOBUF_C_$filename_identifier$_INCLUDED\n"
    "#define PROTOBUF_C_$filename_identifier$_INCLUDED\n"
    "\n"
	"#include <stdlib.h>\n"
	"#include <protobuf-c.h>\n"
	"#include <ulist.h>\n"
    "\n"
    "PROTOBUF_C__BEGIN_DECLS\n"
    "\n",
    "filename", file_->name(),
    "filename_identifier", filename_identifier);

  // Verify the protobuf-c library header version is compatible with the
  // protoc-c version before going any further.
  printer->Print(
    "#if PROTOBUF_C_VERSION_NUMBER < $min_header_version$\n"
    "# error This file was generated by a newer version of protoc-c which is "
    "incompatible with your libprotobuf-c headers. Please update your headers.\n"
    "#elif $protoc_version$ < PROTOBUF_C_MIN_COMPILER_VERSION\n"
    "# error This file was generated by an older version of protoc-c which is "
    "incompatible with your libprotobuf-c headers. Please regenerate this file "
    "with a newer version of protoc-c.\n"
    "#endif\n"
    "\n",
    "min_header_version", SimpleItoa(min_header_version),
    "protoc_version", SimpleItoa(PROTOBUF_C_VERSION_NUMBER));

  for (int i = 0; i < file_->dependency_count(); i++) {
    printer->Print(
	  "#include \"$pbc_file_prefix$$dependency$$pbc_file_postfix$.h\"\n",
      "pbc_file_prefix", PBC_FILE_PREFIX,
      "dependency", StripProto(file_->dependency(i)->name()),
      "pbc_file_postfix", PBC_FILE_POSTFIX);
  }

  printer->Print("\n");

#if 0
  // Generate forward declarations of classes.
  for (int i = 0; i < file_->message_type_count(); i++) {
    message_generators_[i]->GenerateStructTypedef(printer);
  }

  printer->Print("\n");
#endif

  map<string, string> vars;
  vars["pkg"] = file_->package();
  printer->Print(vars, "// Package: $pkg$ \n");

  // Generate enum definitions.
  printer->Print("\n/* --- enums --- */\n\n");
  for (int i = 0; i < file_->message_type_count(); i++) {
    message_generators_[i]->GenerateEnumDefinitions(printer);
  }
  for (int i = 0; i < file_->enum_type_count(); i++) {
    enum_generators_[i]->GenerateDefinition(printer);
  }

#if 0
  // FIXME: Allocating function of each message
  printer->Print("\n/* --- list function prototype --- */\n\n");

  for (int i = 0; i < file_->message_type_count(); i++) {
    map<string, string> vars;
    vars["typename"] = file_->message_type(i)->name();
    vars["fullname"] = FullNameToLower(file_->message_type(i)->full_name());

    printer->Print(vars, "void* $fullname$_new(void);\n");
  }
#endif

  // Generate class definitions.
  printer->Print("\n/* --- messages --- */\n\n");
  for (int i = 0; i < file_->message_type_count(); i++) {
    message_generators_[i]->GenerateStructDefinition(printer);
  }

  for (int i = 0; i < file_->message_type_count(); i++) {
    message_generators_[i]->GenerateHelperFunctionDeclarations(printer, false);
  }

  printer->Print("/* --- per-message closures --- */\n\n");
  for (int i = 0; i < file_->message_type_count(); i++) {
    message_generators_[i]->GenerateClosureTypedef(printer);
  }

#if 0
  // Generate service definitions.
  printer->Print("\n/* --- services --- */\n\n");
  for (int i = 0; i < file_->service_count(); i++) {
    service_generators_[i]->GenerateMainHFile(printer);
  }
#endif

  // Declare extension identifiers.
  for (int i = 0; i < file_->extension_count(); i++) {
    extension_generators_[i]->GenerateDeclaration(printer);
  }

  printer->Print("\n/* --- descriptors --- */\n\n");
  for (int i = 0; i < file_->enum_type_count(); i++) {
    enum_generators_[i]->GenerateDescriptorDeclarations(printer);
  }
  for (int i = 0; i < file_->message_type_count(); i++) {
    message_generators_[i]->GenerateDescriptorDeclarations(printer);
  }
  for (int i = 0; i < file_->service_count(); i++) {
    service_generators_[i]->GenerateDescriptorDeclarations(printer);
  }

  printer->Print(
    "\n"
    "PROTOBUF_C__END_DECLS\n"
    "\n\n#endif  /* PROTOBUF_C_$filename_identifier$_INCLUDED */\n",
    "filename_identifier", filename_identifier);
}

void FileGenerator::GenerateSource(io::Printer* printer) {
  printer->Print(
    "/* Generated by the protocol buffer compiler.  DO NOT EDIT! */\n"
    "/* Generated from: $filename$ */\n"
    "\n"
    "/* Do not generate deprecated warnings for self */\n"
    "#ifndef PROTOBUF_C_NO_DEPRECATED\n"
    "#define PROTOBUF_C_NO_DEPRECATED\n"
    "#endif\n"
    "\n"
    "#include \"$pbc_file_prefix$$basename$$pbc_file_postfix$.h\"\n\n\n",
    "filename", file_->name(),
    "pbc_file_prefix", PBC_FILE_PREFIX,
    "basename", StripProto(file_->name()),
    "pbc_file_postfix", PBC_FILE_POSTFIX);

#if 0
  // For each dependency, write a prototype for that dependency's
  // BuildDescriptors() function.  We don't expose these in the header because
  // they are internal implementation details, and since this is generated code
  // we don't have the usual risks involved with declaring external functions
  // within a .cc file.
  for (int i = 0; i < file_->dependency_count(); i++) {
    const FileDescriptor* dependency = file_->dependency(i);
    // Open the dependency's namespace.
    vector<string> dependency_package_parts;
    SplitStringUsing(dependency->package(), ".", &dependency_package_parts);
    // Declare its BuildDescriptors() function.
    printer->Print(
      "void $function$();",
      "function", GlobalBuildDescriptorsName(dependency->name()));
    // Close the namespace.
    for (int i = 0; i < dependency_package_parts.size(); i++) {
      printer->Print(" }");
    }
    printer->Print("\n");
  }
#endif

#if 0
	///////////////////////////////////////////////////
	// list function
    // FIXME: Allocation of each message

	for (int i = 0; i < file_->message_type_count(); i++) {
		std::map<string, string> vars;

		vars["typename"] = file_->message_type(i)->name();
		vars["fullname"] = FullNameToLower(file_->message_type(i)->full_name());

		printer->Print(vars, "$fullname$_t** $fullname$_rt_new(uint32_t cnt) \n{\n");
		printer->Indent();
		printer->Print(vars, "$fullname$_t **rtmsg = ($fullname$_t**)malloc(sizeof($fullname$_t*)*cnt);\n\n");
		printer->Print(vars, "int i;\n\n");
		printer->Print(vars, "for (i=0 ; i<cnt ; i++) {\n");
		printer->Indent();
		printer->Print(vars, "rtmsg[i] = ($fullname$_t*)malloc(sizeof($fullname$_t));\n\n");
		printer->Print(vars, "$fullname$_init(rtmsg[i]);\n");

		printer->Outdent();
		printer->Print(vars, "}\n\n");
		printer->Print(vars, "return rtmsg;\n\n");
		printer->Outdent();
		printer->Print(vars, "}\n\n");

		printer->Print(vars, "void* $fullname$_new(void) \n{\n");
		printer->Indent();
		printer->Print(vars, "$fullname$_t *m = ($fullname$_t *)malloc(sizeof($fullname$_t));\n");
		printer->Print(vars, "$fullname$_init(m);\n");
	    printer->Print(vars, "return m;\n");
		printer->Outdent();
		printer->Print(vars, "}\n\n");
	}
#endif

  for (int i = 0; i < file_->message_type_count(); i++) {
    message_generators_[i]->GenerateHelperFunctionDefinitions(printer, false);
  }
  for (int i = 0; i < file_->message_type_count(); i++) {
    message_generators_[i]->GenerateMessageDescriptor(printer);
  }
  for (int i = 0; i < file_->enum_type_count(); i++) {
    enum_generators_[i]->GenerateEnumDescriptor(printer);
  }
  for (int i = 0; i < file_->service_count(); i++) {
    service_generators_[i]->GenerateCFile(printer);
  }

}

}  // namespace c
}  // namespace compiler
}  // namespace protobuf
}  // namespace google
