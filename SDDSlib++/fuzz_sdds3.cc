/**
 * @file fuzz_sdds3.cc
 * @brief In-process fuzz harness for the standalone C++ SDDS parser and decoders.
 *
 * @details Exercises layout parsing and page decoding from caller-owned memory
 * with conservative resource limits.
 *
 * @copyright
 *   - (c) 2026 The University of Chicago
 *
 * @license
 * This file is distributed under the terms of the Software License Agreement
 * found in the file LICENSE included with this distribution.
 */

#include "SDDS.hpp"

#include <cstddef>
#include <cstdint>

#if defined(SDDSPP_FUZZ_STANDALONE)
#  include <fstream>
#  include <iterator>
#  include <string>
#  include <vector>
#endif

extern "C" int LLVMFuzzerTestOneInput(const std::uint8_t *data, std::size_t size) {
  sdds::ReaderOptions options;
  options.recovery = sdds::RecoveryMode::Strict;
  options.compression = sdds::Compression::None;
  options.limits.maxRows = 10000;
  options.limits.maxElements = 100000;
  options.limits.maxStringBytes = 65536;
  options.limits.maxLayoutCommandBytes = 65536;
  options.limits.maxDecompressedBytes = 1024U * 1024U;
  options.limits.maxIncludeDepth = 0;
  options.limits.maxPageIndexEntries = 1024;
  try {
    auto reader = sdds::Reader::fromSource(sdds::inputFromMemory(data, size),
                                           "fuzz-input", options);
    sdds::ReadRequest request;
    request.rows.stride = 3;
    while (reader.next(request)) {}
    reader.close();
  } catch (const sdds::Error &) {
  } catch (const std::exception &) {
  }
  return 0;
}

#if defined(SDDSPP_FUZZ_STANDALONE)
int main(int argc, char **argv) {
  for (int argument = 1; argument < argc; ++argument) {
    std::ifstream input(argv[argument], std::ios::binary);
    const std::vector<std::uint8_t> bytes((std::istreambuf_iterator<char>(input)),
                                          std::istreambuf_iterator<char>());
    LLVMFuzzerTestOneInput(bytes.data(), bytes.size());
  }
  return 0;
}
#endif
