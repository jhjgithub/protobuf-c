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

#include <protoc-c/c_generator.h>

#include <memory>
#include <vector>
#include <utility>

#include <protoc-c/c_file.h>
#include <protoc-c/c_helpers.h>
#include <google/protobuf/io/printer.h>
#include <google/protobuf/io/zero_copy_stream.h>
#include <google/protobuf/descriptor.pb.h>

namespace google {
namespace protobuf {
namespace compiler {
namespace c {

// Parses a set of comma-delimited name/value pairs, e.g.:
//   "foo=bar,baz,qux=corge"
// parses to the pairs:
//   ("foo", "bar"), ("baz", ""), ("qux", "corge")
void ParseOptions(const string& text, std::vector<std::pair<string, string> >* output) {
  std::vector<string> parts;
  SplitStringUsing(text, ",", &parts);

  for (unsigned i = 0; i < parts.size(); i++) {
    string::size_type equals_pos = parts[i].find_first_of('=');
    std::pair<string, string> value;
    if (equals_pos == string::npos) {
      value.first = parts[i];
      value.second = "";
    } else {
      value.first = parts[i].substr(0, equals_pos);
      value.second = parts[i].substr(equals_pos + 1);
    }
    output->push_back(value);
  }
}

CGenerator::CGenerator() {}
CGenerator::~CGenerator() {}

bool CGenerator::Generate(const FileDescriptor* file,
                            const string& parameter,
                            OutputDirectory* output_directory,
                            string* error) const {
  std::vector<std::pair<string, string> > options;
  ParseOptions(parameter, &options);

  // -----------------------------------------------------------------
  // parse generator options

  // TODO(kenton):  If we ever have more options, we may want to create a
  //   class that encapsulates them which we can pass down to all the
  //   generator classes.  Currently we pass dllexport_decl down to all of
  //   them via the constructors, but we don't want to have to add another
  //   constructor parameter for every option.

  // If the dllexport_decl option is passed to the compiler, we need to write
  // it in front of every symbol that should be exported if this .proto is
  // compiled into a Windows DLL.  E.g., if the user invokes the protocol
  // compiler as:
  //   protoc --cpp_out=dllexport_decl=FOO_EXPORT:outdir foo.proto
  // then we'll define classes like this:
  //   class FOO_EXPORT Foo {
  //     ...
  //   }
  // FOO_EXPORT is a macro which should expand to __declspec(dllexport) or
  // __declspec(dllimport) depending on what is being compiled.
  string dllexport_decl;

  for (unsigned i = 0; i < options.size(); i++) {
    if (options[i].first == "dllexport_decl") {
      dllexport_decl = options[i].second;
    } else {
      *error = "Unknown generator option: " + options[i].first;
      return false;
    }
  }

  // -----------------------------------------------------------------


  string basename = PBC_FILE_PREFIX + StripProto(file->name()) + PBC_FILE_POSTFIX;
  //basename.append(".pb-c");

  FileGenerator file_generator(file, dllexport_decl);

  // Generate header.
  {
    std::unique_ptr<io::ZeroCopyOutputStream> output(
      output_directory->Open(basename + ".h"));
    io::Printer printer(output.get(), '$');
    file_generator.GenerateHeader(&printer);
  }

  // Generate cc file.
  {
    std::unique_ptr<io::ZeroCopyOutputStream> output(
      output_directory->Open(basename + ".c"));
    io::Printer printer(output.get(), '$');
    file_generator.GenerateSource(&printer);
  }

  return true;
}

void OutDescName(GenDescList *gen, const Descriptor *descriptor_)
{
	string desc;

	desc	= "{ ";
	desc	+= "\"" + descriptor_->full_name() + "\",\t" +
		"&" + PkgClassNameToLower() + "_descriptor" +
		" },";
	desc = ToLower(desc);
	gen->sl_desc->push_back(desc);
}

void PutDesc(GenDescList *gen, const Descriptor *descriptor)
{
	OutDescName(gen, descriptor);

	for (int i = 0; i < descriptor->nested_type_count(); i++) {
		PutDesc(gen, descriptor->nested_type(i));
	}
}

void GenerateDescList(GenDescList *gen, const FileDescriptor *file)
{
	int i;
    string full_path = PBC_FILE_PREFIX + StripProto(file->name()) + PBC_FILE_POSTFIX + ".h";

	gen->sl_include->push_back(full_path);

	for (i = 0; i < file->message_type_count(); i++) {
		const Descriptor *desc = file->message_type(i);

		PutDesc(gen, desc);
	}
}

GenDescList::GenDescList()
{
	sl_include = new list<string>;
	sl_desc = new list<string>;
}

GenDescList::~GenDescList()
{
	delete sl_include;
	delete sl_desc;
}

// desclist.inc: c file의 include 내용을 포함 한다.
// desclist.desc: descriptor list 배열의 각 항목 포함한다.
// 각 서브 디렉토리의 *.inc, *.desc 파일은 Makefile에서 desclist.c 파일로 병합 된다.
bool GenDescList::GenerateDescFile(void)
{
	std::list<std::string>::iterator it;
	string fname, desc;
	ofstream of_inc, of_desc;

	if (sl_desc->size() == 0) {
		return true;
	}

	fname = prefix + ".inc";
	of_inc.open(fname.c_str(), ios::out | ios::trunc);

	if (!of_inc.is_open()) {
		cout << "Cannot open output file: " << prefix << "\n";

		return false;
	}

	//of_inc << "/* Generated by the protocol buffer compiler.  DO NOT EDIT! */\n\n";

	for (it = sl_include->begin(); it != sl_include->end(); ++it) {
		of_inc << "#include \"" << *it << "\"\n";
	}

	//of_inc << "#include <desclist.h>\n";
	//of_inc << "\n\n\n";
	of_inc.close();

	fname = prefix + ".desc";
	of_desc.open(fname.c_str(), ios::out | ios::trunc);

	//of_desc << "desc_list_t g_desclist[] = {\n";

	for (it = sl_desc->begin(); it != sl_desc->end(); ++it) {
		of_desc << "	" << *it << "\n";
	}

#if 0
	of_desc << "\n";
	of_inc << "    { NULL, NULL } \n";
	of_inc << "};\n\n";
#endif

	of_inc.close();

	return true;
}

bool GenDescList::Generate(const FileDescriptor *file,
						  const string& parameter,
						  OutputDirectory *output_directory,
						  string *error) const
{
#if 0
	if (prefix.empty()) {
		vector<pair<string, string> > options;
		ParseOptions(parameter, &options);

		for (unsigned i = 0; i < options.size(); i++) {
			if (options[i].first == "prefix") {
				prefix = string(options[i].second);
				break;
			}
		}

		if (prefix.empty()) {
			prefix = "desclist";
		}
	}
#endif

	// add patrick
	GenerateDescList((GenDescList *)this, file);

	prefix = PBC_FILE_PREFIX + StripProto(file->name()) + PBC_FILE_POSTFIX;

	return true;
}

}  // namespace c
}  // namespace compiler
}  // namespace protobuf
}  // namespace google
