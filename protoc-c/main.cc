#include <string>

#include <google/protobuf/compiler/plugin.h>
#include <google/protobuf/compiler/command_line_interface.h>
#include <protoc-c/c_generator.h>

int main(int argc, char* argv[]) {
  google::protobuf::compiler::c::CGenerator c_generator;

  std::string invocation_name = argv[0];
  std::string invocation_basename = invocation_name.substr(invocation_name.find_last_of("/") + 1);
  const std::string legacy_name = "protoc-c";

  if (invocation_basename == legacy_name) {
    google::protobuf::compiler::CommandLineInterface cli;
	google::protobuf::compiler::c::GenDescList c_desclist;
    cli.RegisterGenerator("--c_out", &c_generator, "Generate C/H files.");
	cli.RegisterGenerator("--desc_out", &c_desclist, "Generate Descriptor files.");
    cli.SetVersionInfo(PACKAGE_STRING);
	int ret;
    ret = cli.Run(argc, argv);
	c_desclist.GenerateDescFile();
	return ret;
  }

  return google::protobuf::compiler::PluginMain(argc, argv, &c_generator);
}
