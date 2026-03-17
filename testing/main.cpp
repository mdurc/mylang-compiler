#define APPROVALS_GOOGLETEST_EXISTING_MAIN
#include "vendor/ApprovalTests.hpp"

#include <filesystem>
#include <fstream>
#include <sstream>

#include "../src/driver.h"
#include "util.h"

std::string rtrim(const std::string& s) {
  size_t end = s.size();
  while (end > 0 && std::isspace(static_cast<unsigned char>(s[end - 1]))) {
    --end;
  }
  return s.substr(0, end);
}

std::string generate_compiler_output(const std::string& flag, const std::string& input_filepath) {
  std::string temp_out = std::filesystem::temp_directory_path().string() + "/test_out.tmp";

  drive(flag, input_filepath, temp_out);

  std::ifstream ifs(temp_out);
  std::stringstream ss;
  ss << ifs.rdbuf();
  ifs.close();

  std::filesystem::remove(temp_out);

  return rtrim(ss.str());
}

int main(int argc, char** argv) {
  ::testing::InitGoogleTest(&argc, argv);
  ApprovalTests::initializeApprovalTestsForGoogleTests();

  auto directoryDisposer =
      ApprovalTests::Approvals::useApprovalsSubdirectory("snapshots");

  auto reporter = ApprovalTests::CustomReporter::createForegroundReporter(
      "nvim", "-d {Received} {Approved}");

  ApprovalTests::DefaultReporterDisposer defaultReporterDisposer =
      ApprovalTests::Approvals::useAsDefaultReporter(reporter);

  return RUN_ALL_TESTS();
}
